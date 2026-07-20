#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "driver/i2s_std.h"

#include "lwip/inet.h"
#include "lwip/sockets.h"
#include <unistd.h>


#define SAMPLE_RATE_STD  44100
#define SAMPLE_RATE_ULT  75000

#define DMA_BUF_COUNT    16
#define DMA_BUF_LEN_SMPL 511

#define BIT_DEPTH_STD    I2S_DATA_BIT_WIDTH_16BIT
#define BIT_DEPTH_ULT    I2S_DATA_BIT_WIDTH_32BIT

#define DATA_BUFFER_SIZE (DMA_BUF_LEN_SMPL * 2 * BIT_DEPTH_ULT / 8)

#define QUEUE_LENGTH     24
#define NUM_CICLOS_TCP   50

#define MIC_CLOCK_PIN    GPIO_NUM_21
#define MIC_DATA_PIN     GPIO_NUM_4
#define I2S_PORT_NUM     I2S_NUM_0

//#define TCP_SERVER_IP    "192.168.15.78"
//#define TCP_SERVER_IP    "192.168.1.100"
#define TCP_SERVER_IP    "192.168.0.104"
//#define TCP_SERVER_IP      "192.168.0.107"
#define TCP_SERVER_PORT  8001


static const char *TAG = "TESTE_FILA";

static i2s_chan_handle_t rx_handle = NULL;
static QueueHandle_t fila_dados = NULL;

static volatile int overflows_i2s = 0;

static int blocos_lidos = 0;
static int blocos_enviados = 0;
static int blocos_descartados = 0;
static int ciclos_tcp_concluidos = 0;


/*
 * Estrutura de cada item armazenado na fila.
 * Cada bloco possui 4088 bytes de dados.
 */
typedef struct {
    size_t tamanho;
    uint8_t dados[DATA_BUFFER_SIZE];
} bloco_i2s_t;


/*
 * Blocos globais para evitar uso excessivo da pilha.
 */
static bloco_i2s_t bloco_captura_teste;
static bloco_i2s_t bloco_envio_teste;


/*
 * Detecta overflow interno do I2S/DMA.
 */
static bool IRAM_ATTR callback_overflow_i2s(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_data)
{
    overflows_i2s++;
    return false;
}


/*
 * Conexão Wi-Fi.
 */
static void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            //.ssid = "VIVOFIBRA-4870-EXT",
            //.ssid = "linksys",
            .ssid = "LARS-301-2.4GHz",
            //.password = "CC99735551",
            //.password = "",
            .password = "LARS@ROBOTICA",
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_connect());
}


/*
 * Inicialização do I2S.
 */
static void i2s_init_std(int sample_rate, i2s_data_bit_width_t bit_depth, bool modo_ult)
{
    i2s_chan_config_t rx_channel_config = {
        .id = I2S_PORT_NUM,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = DMA_BUF_COUNT,
        .dma_frame_num = DMA_BUF_LEN_SMPL,
        .auto_clear = true
    };

    ESP_ERROR_CHECK(i2s_new_channel(&rx_channel_config, NULL, &rx_handle));

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = sample_rate,
            .clk_src = I2S_CLK_SRC_APLL,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256
        },

        .slot_cfg = {
            .data_bit_width = bit_depth,
            .slot_bit_width = bit_depth,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = bit_depth,
            .ws_pol = false,
            .bit_shift = true
        },

        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = modo_ult ? MIC_CLOCK_PIN : I2S_GPIO_UNUSED,
            .ws = modo_ult ? I2S_GPIO_UNUSED : MIC_CLOCK_PIN,
            .dout = I2S_GPIO_UNUSED,
            .din = MIC_DATA_PIN
        }
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));

    i2s_event_callbacks_t callbacks = {
        .on_recv_q_ovf = callback_overflow_i2s
    };

    ESP_ERROR_CHECK(i2s_channel_register_event_callback(rx_handle, &callbacks, NULL));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
}


/*
 * Abre uma única conexão TCP com o computador.
 */
static int conectar_tcp(void)
{
    int socket_tcp = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

    if (socket_tcp < 0) {
        ESP_LOGE(TAG, "Erro ao criar socket TCP");
        return -1;
    }

    struct sockaddr_in destino = {
        .sin_family = AF_INET,
        .sin_port = htons(TCP_SERVER_PORT),
        .sin_addr.s_addr = inet_addr(TCP_SERVER_IP)
    };

    if (connect(socket_tcp, (struct sockaddr *)&destino, sizeof(destino)) != 0) {
        ESP_LOGE(TAG, "Erro ao conectar ao servidor TCP");
        close(socket_tcp);
        return -1;
    }

    ESP_LOGI(TAG, "Conectado ao servidor TCP");
    return socket_tcp;
}


/*
 * Garante que todos os bytes sejam enviados.
 */
static int enviar_tcp_completo(int socket_tcp, const uint8_t *dados, size_t tamanho)
{
    size_t total_enviado = 0;

    while (total_enviado < tamanho) {
        int enviado = send(socket_tcp, dados + total_enviado, tamanho - total_enviado, 0);

        if (enviado <= 0) {
            return -1;
        }

        total_enviado += enviado;
    }

    return (int)total_enviado;
}


void app_main(void)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_LOGI(TAG, "Iniciando conexao Wi-Fi...");
    wifi_init_sta();

    esp_netif_ip_info_t ip_info;

    do {
        vTaskDelay(pdMS_TO_TICKS(500));

        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        esp_netif_get_ip_info(netif, &ip_info);

    } while (ip_info.ip.addr == 0);

    ESP_LOGI(TAG, "Wi-Fi OK! IP: " IPSTR, IP2STR(&ip_info.ip));

    /*
     * Inicialização inicial em 44,1 kHz.
     */
    ESP_LOGI(TAG, "I2S 44.1 kHz...");
    i2s_init_std(SAMPLE_RATE_STD, BIT_DEPTH_STD, false);

    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_ERROR_CHECK(i2s_channel_disable(rx_handle));
    ESP_ERROR_CHECK(i2s_del_channel(rx_handle));
    rx_handle = NULL;

    vTaskDelay(pdMS_TO_TICKS(1000));

    /*
     * Configuração final ultrassônica.
     */
    ESP_LOGI(TAG, "I2S 75 kHz...");
    i2s_init_std(SAMPLE_RATE_ULT, BIT_DEPTH_ULT, true);

    vTaskDelay(pdMS_TO_TICKS(1000));

    /*
     * Cria uma única fila com capacidade para 24 blocos.
     */
    fila_dados = xQueueCreate(QUEUE_LENGTH, sizeof(bloco_i2s_t));

    if (fila_dados == NULL) {
        ESP_LOGE(TAG, "Erro ao criar fila");
        ESP_ERROR_CHECK(i2s_channel_disable(rx_handle));
        ESP_ERROR_CHECK(i2s_del_channel(rx_handle));
        rx_handle = NULL;
        return;
    }

    blocos_lidos = 0;
    blocos_enviados = 0;
    blocos_descartados = 0;
    ciclos_tcp_concluidos = 0;
    overflows_i2s = 0;

    /*
     * Abre uma única conexão TCP.
     */
    int socket_tcp = conectar_tcp();

    if (socket_tcp < 0) {
        ESP_ERROR_CHECK(i2s_channel_disable(rx_handle));
        ESP_ERROR_CHECK(i2s_del_channel(rx_handle));
        rx_handle = NULL;
        vQueueDelete(fila_dados);
        fila_dados = NULL;
        return;
    }

    bool erro_teste = false;

    while (ciclos_tcp_concluidos < NUM_CICLOS_TCP && !erro_teste) {
        ESP_LOGI(TAG, "Ciclo %d de %d: lendo %d blocos...", ciclos_tcp_concluidos + 1, NUM_CICLOS_TCP, QUEUE_LENGTH);

        for (int i = 0; i < QUEUE_LENGTH; i++) {
            bloco_captura_teste.tamanho = 0;

            esp_err_t res = i2s_channel_read(rx_handle, bloco_captura_teste.dados, DATA_BUFFER_SIZE, &bloco_captura_teste.tamanho, portMAX_DELAY);

            if (res != ESP_OK) {
                ESP_LOGE(TAG, "Erro na leitura I2S: %s", esp_err_to_name(res));
                erro_teste = true;
                break;
            }

            if (bloco_captura_teste.tamanho != DATA_BUFFER_SIZE) {
                ESP_LOGE(TAG, "Bloco incompleto: %u bytes", (unsigned int)bloco_captura_teste.tamanho);
                erro_teste = true;
                break;
            }

            if (xQueueSend(fila_dados, &bloco_captura_teste, portMAX_DELAY) != pdPASS) {
                ESP_LOGE(TAG, "Erro ao colocar bloco na fila");
                blocos_descartados++;
                erro_teste = true;
                break;
            }

            blocos_lidos++;
        }


        if (erro_teste) {
            break;
        }

        ESP_LOGI(TAG, "Fila preenchida. Iniciando envio TCP do ciclo %d...", ciclos_tcp_concluidos + 1);

        while (uxQueueMessagesWaiting(fila_dados) > 0) {
            bloco_envio_teste.tamanho = 0;

            if (xQueueReceive(fila_dados, &bloco_envio_teste, portMAX_DELAY) != pdPASS) {
                ESP_LOGE(TAG, "Erro ao retirar bloco da fila");
                erro_teste = true;
                break;
            }

            int bytes_enviados = enviar_tcp_completo(socket_tcp, bloco_envio_teste.dados, bloco_envio_teste.tamanho);

            if (bytes_enviados != (int)bloco_envio_teste.tamanho) {
                ESP_LOGE(TAG, "Erro TCP. Enviou %d de %u bytes", bytes_enviados, (unsigned int)bloco_envio_teste.tamanho);
                erro_teste = true;
                break;
            }

            blocos_enviados++;
        }

        if (erro_teste) {
            break;
        }

        ciclos_tcp_concluidos++;
        ESP_LOGI(TAG, "Ciclo TCP %d concluido.", ciclos_tcp_concluidos);

    }

    shutdown(socket_tcp, SHUT_WR);
    close(socket_tcp);

    ESP_ERROR_CHECK(i2s_channel_disable(rx_handle));
    ESP_ERROR_CHECK(i2s_del_channel(rx_handle));
    rx_handle = NULL;

    vQueueDelete(fila_dados);
    fila_dados = NULL;

    ESP_LOGI(TAG, "Teste encerrado.");
    ESP_LOGI(TAG, "Ciclos TCP concluidos: %d", ciclos_tcp_concluidos);
    ESP_LOGI(TAG, "Blocos lidos: %d", blocos_lidos);
    ESP_LOGI(TAG, "Blocos enviados: %d", blocos_enviados);
    ESP_LOGI(TAG, "Blocos descartados: %d", blocos_descartados);
    ESP_LOGI(TAG, "Overflows I2S: %d", overflows_i2s);
}