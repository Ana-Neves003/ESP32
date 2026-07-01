#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_wifi.h"

//#include "esp_timer.h"
//#include "esp_heap_caps.h"

#include "driver/i2s_std.h"
#include "driver/gptimer.h"

#include "lwip/sockets.h"
#include "lwip/inet.h"
#include <unistd.h>

//#include "esp_http_client.h"

#define SAMPLE_RATE_STD  44100
#define SAMPLE_RATE_ULT  78125
#define DMA_BUF_COUNT    16
#define DMA_BUF_LEN_SMPL 511

#define BIT_DEPTH_STD        I2S_DATA_BIT_WIDTH_16BIT
#define BIT_DEPTH_ULT        I2S_DATA_BIT_WIDTH_32BIT
#define DATA_BUFFER_SIZE     (DMA_BUF_LEN_SMPL * 2 * BIT_DEPTH_ULT / 8)

#define MIC_CLOCK_PIN     GPIO_NUM_21
#define MIC_DATA_PIN      GPIO_NUM_4
#define I2S_PORT_NUM      I2S_NUM_0

//#define HTTP_URL          "http://192.168.15.78:8000/upload" 

#define DURACAO_US 5000000

#define QUEUE_LENGTH 24


#define TCP_SERVER_IP "192.168.15.78"
#define TCP_SERVER_PORT 8001

static const char *TAG = "I2S_TASKS";

static i2s_chan_handle_t rx_handle = NULL;
static QueueHandle_t fila_dados = NULL;
static gptimer_handle_t timer_captura = NULL;


static volatile bool capturando = false;
static volatile bool aquisicao_finalizada = false;

static int blocos_lidos = 0;
static int blocos_enviados = 0;
static int blocos_descartados = 0;
//static int erros_http = 0;

static int socket_tcp = -1;


static volatile int overflows_i2s = 0; //Medir se há overflow interno do I2S

// Bloco de dados lido do I2S para envio pela fila
typedef struct {
    size_t tamanho;
    uint8_t dados[DATA_BUFFER_SIZE];
} bloco_i2s_t;

//Verifica se o RX/DMA do I2s sobreescreveu dados antes da task conseguir ler
//Resultado: Cada bloco de I2S é produzido em cerac de 6,5 ms (511/78125 = 0,0065408 s ou 6,5 ms)
//Resultado: Média por envio HTTP -> 52,153 ms e Pior envio (máximo) -> 112,946 ms
//52,153/6,5 ~= 8 blocos, Enquanto envia 1 bloco o I2S produz aproximadanete 8 blocos
static bool IRAM_ATTR callback_overflow_i2s(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_data)
{
    overflows_i2s++;
    return false;
}

static void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "VIVOFIBRA-4870-EXT",
            .password = "CC99735551",
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_connect());
}

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
            //.slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mode = I2S_SLOT_MODE_MONO,
            //.slot_mask = I2S_STD_SLOT_BOTH,
            .slot_mask = I2S_STD_SLOT_RIGHT,
            .ws_width = bit_depth,
            .ws_pol = false,
            .bit_shift = true
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = modo_ult ? MIC_CLOCK_PIN : I2S_GPIO_UNUSED,
            .ws   = modo_ult ? I2S_GPIO_UNUSED : MIC_CLOCK_PIN,
            .dout = I2S_GPIO_UNUSED,
            .din  = MIC_DATA_PIN
        }
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));

    i2s_event_callbacks_t callbacks = {
        .on_recv_q_ovf = callback_overflow_i2s
    };

    ESP_ERROR_CHECK(i2s_channel_register_event_callback(rx_handle, &callbacks, NULL));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
}

// Interrompe a aquisição quando o tempo configurado termina
static bool IRAM_ATTR parar_aquisicao(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data)
{
    capturando = false;
    return false;
}


static void configurar_timer(void)
{
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000
    };

    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &timer_captura));

    gptimer_event_callbacks_t callbacks = {
        .on_alarm = parar_aquisicao
    };

    ESP_ERROR_CHECK(
        gptimer_register_event_callbacks(timer_captura, &callbacks, NULL)
    );

    gptimer_alarm_config_t alarm_config = {
        .alarm_count = DURACAO_US,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = false
    };

    ESP_ERROR_CHECK(
        gptimer_set_alarm_action(timer_captura, &alarm_config)
    );
}

/*
static void task_aquisicao(void *pvParameters)
{
    bloco_i2s_t bloco;

    while (capturando) {
        esp_err_t res = i2s_channel_read(
            rx_handle,
            bloco.dados,
            DATA_BUFFER_SIZE,
            &bloco.tamanho,
            portMAX_DELAY
        );

        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Erro na leitura do I2S: %s", esp_err_to_name(res));
            continue;
        }

        total_lido += bloco.tamanho;
        blocos_lidos++;

        if (xQueueSend(fila_dados, &bloco, 0) != pdPASS) {
            blocos_descartados++;
        }else{
            int ocupacao_atual = uxQueueMessagesWaiting(fila_dados);

            if(ocupacao_atual > maior_ocupacao_fila){
                maior_ocupacao_fila = ocupacao_atual;
            }
        }
    }

    aquisicao_finalizada = true;

    ESP_LOGI(TAG, "Aquisicao finalizada.");
    ESP_LOGI(TAG, "Blocos lidos: %d", blocos_lidos);
    ESP_LOGI(TAG, "Total lido: %u bytes", (unsigned int)total_lido);
    ESP_LOGI(TAG, "Blocos descartados: %d", blocos_descartados);
    ESP_LOGI(TAG, "Maior ocupacao da fila: %d blocos", maior_ocupacao_fila);
    ESP_LOGI(TAG, "Overflows internos I2S: %d", overflows_i2s);

    vTaskDelete(NULL);
}
*/

/*
static void task_envio(void *pvParameters)
{
    esp_http_client_config_t http_cfg = {
        .url = HTTP_URL,
        .transport_type = HTTP_TRANSPORT_OVER_TCP
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/octet-stream");

    bloco_i2s_t bloco;

    while (!aquisicao_finalizada || uxQueueMessagesWaiting(fila_dados) > 0) {
        size_t tamanho_total = 0;
        int blocos_no_envio = 0;

        if (xQueueReceive(fila_dados, &bloco, pdMS_TO_TICKS(100)) != pdPASS) {
            continue;
        }

        memcpy(buffer_envio + tamanho_total, bloco.dados, bloco.tamanho);
        tamanho_total += bloco.tamanho;
        blocos_no_envio++;

        while (blocos_no_envio < BLOCOS_POR_ENVIO &&
               xQueueReceive(fila_dados, &bloco, 0) == pdPASS) {

            memcpy(buffer_envio + tamanho_total, bloco.dados, bloco.tamanho);
            tamanho_total += bloco.tamanho;
            blocos_no_envio++;
        }

        esp_err_t err = esp_http_client_open(client, tamanho_total);

        if (err == ESP_OK) {
            int bytes_enviados = esp_http_client_write(
                client,
                (char *)buffer_envio,
                tamanho_total
            );

            esp_http_client_close(client);

            if (bytes_enviados == tamanho_total) {
                total_enviado += bytes_enviados;
                blocos_enviados += blocos_no_envio;
            } else {
                erros_http++;
                ESP_LOGE(TAG, "HTTP: enviou %d de %d bytes",
                         bytes_enviados, tamanho_total);
            }

        } else {
            erros_http++;
            ESP_LOGE(TAG, "HTTP: erro ao abrir conexao: %s",
                     esp_err_to_name(err));
        }
    }

    ESP_LOGI(TAG, "Envio finalizado.");
    ESP_LOGI(TAG, "Blocos enviados: %d", blocos_enviados);
    ESP_LOGI(TAG, "Total enviado: %u bytes", (unsigned int)total_enviado);
    ESP_LOGI(TAG, "Erros HTTP: %d", erros_http);

    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
}
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

static int enviar_tcp_completo(int socket_tcp, const uint8_t *dados, size_t tamanho)
{
    size_t total_enviado_socket = 0;

    while (total_enviado_socket < tamanho) {
        int enviado = send(
            socket_tcp,
            dados + total_enviado_socket,
            tamanho - total_enviado_socket,
            0
        );

        if (enviado <= 0) {
            return -1;
        }

        total_enviado_socket += enviado;
    }

    return total_enviado_socket;
}

/*
static void task_envio_HTTP(void *pvParameters)
{
    esp_http_client_config_t http_cfg = {
        .url = HTTP_URL,
        .transport_type = HTTP_TRANSPORT_OVER_TCP
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/octet-stream");

    bloco_i2s_t bloco;

    while (!aquisicao_finalizada || uxQueueMessagesWaiting(fila_dados) > 0) {
        size_t tamanho_total = 0;
        int blocos_no_envio = 0;

        if (xQueueReceive(fila_dados, &bloco, pdMS_TO_TICKS(100)) != pdPASS) {
            continue;
        }

        memcpy(buffer_envio + tamanho_total, bloco.dados, bloco.tamanho);
        tamanho_total += bloco.tamanho;
        blocos_no_envio++;

        while (blocos_no_envio < BLOCOS_POR_ENVIO &&
               xQueueReceive(fila_dados, &bloco, 0) == pdPASS) {
            memcpy(buffer_envio + tamanho_total, bloco.dados, bloco.tamanho);
            tamanho_total += bloco.tamanho;
            blocos_no_envio++;
        }

        int64_t inicio_http = esp_timer_get_time();

        esp_err_t err = esp_http_client_open(client, tamanho_total);

        if (err == ESP_OK) {
            int bytes_enviados = esp_http_client_write(
                client,
                (char *)buffer_envio,
                tamanho_total
            );

            esp_http_client_close(client);

            if (bytes_enviados == tamanho_total) {
                total_enviado += bytes_enviados;
                blocos_enviados += blocos_no_envio;
            } else {
                erros_http++;
                ESP_LOGE(TAG, "HTTP: enviou %d de %d bytes",
                         bytes_enviados, tamanho_total);
            }
        } else {
            erros_http++;
            ESP_LOGE(TAG, "HTTP: erro ao abrir conexao: %s",
                     esp_err_to_name(err));
        }

        int64_t tempo_http_us = esp_timer_get_time() - inicio_http;

        tempo_http_total_us += tempo_http_us;

        if (tempo_http_us > tempo_http_max_us) {
            tempo_http_max_us = tempo_http_us;
        }

        envios_http_medidos++;
    }

    ESP_LOGI(TAG, "Envio finalizado.");
    ESP_LOGI(TAG, "Blocos enviados: %d", blocos_enviados);
    ESP_LOGI(TAG, "Total enviado: %u bytes", (unsigned int)total_enviado);
    ESP_LOGI(TAG, "Erros HTTP: %d", erros_http);

    if (envios_http_medidos > 0) {
        ESP_LOGI(TAG, "Tempo medio HTTP: %lld us",
                 tempo_http_total_us / envios_http_medidos);

        ESP_LOGI(TAG, "Tempo maximo HTTP: %lld us",
                 tempo_http_max_us);
    }

    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
}
*/

/*
static void task_envio_TCP(void *pvParameters)
{
    bloco_i2s_t bloco;

    while (!aquisicao_finalizada || uxQueueMessagesWaiting(fila_dados) > 0) {
        if (xQueueReceive(fila_dados, &bloco, pdMS_TO_TICKS(100)) != pdPASS) {
            continue;
        }

        int bytes_enviados = enviar_tcp_completo(
            socket_tcp,
            bloco.dados,
            bloco.tamanho
        );

        if (bytes_enviados == bloco.tamanho) {
            total_enviado += bytes_enviados;
            blocos_enviados++;
        } else {
            erros_http++;
            ESP_LOGE(TAG, "Erro no envio TCP");
            break;
        }
    }

    close(socket_tcp);

    ESP_LOGI(TAG, "Envio TCP finalizado.");
    ESP_LOGI(TAG, "Blocos enviados: %d", blocos_enviados);
    ESP_LOGI(TAG, "Total enviado: %u bytes", (unsigned int)total_enviado);
    ESP_LOGI(TAG, "Erros TCP: %d", erros_http);

    vTaskDelete(NULL);
}
*/

static void task_envio_TCP(void *pvParameters)
{
    bloco_i2s_t bloco;

    while (!aquisicao_finalizada || uxQueueMessagesWaiting(fila_dados) > 0) {
        //size_t tamanho_total = 0;
        //int blocos_no_envio = 0;

        //Aguarda um bloco da fila. se houve timeout e a aquisição acabou, o loop principal encerra
        if (xQueueReceive(fila_dados, &bloco, pdMS_TO_TICKS(100)) != pdPASS) {
            continue;
        }

        //memcpy(buffer_envio + tamanho_total, bloco.dados, bloco.tamanho);
        //tamanho_total += bloco.tamanho;
        //blocos_no_envio++;

        /*
        while (blocos_no_envio < BLOCOS_POR_ENVIO) {
            if (xQueueReceive(fila_dados, &bloco, pdMS_TO_TICKS(10)) == pdPASS) {
                memcpy(buffer_envio + tamanho_total, bloco.dados, bloco.tamanho);
                tamanho_total += bloco.tamanho;
                blocos_no_envio++;
            } else if (aquisicao_finalizada) {
                break;
            }
        }
        */

        int bytes_enviados = enviar_tcp_completo(
            socket_tcp,
            bloco.dados,
            bloco.tamanho
            //buffer_envio,
            //tamanho_total
        );

        if (bytes_enviados == bloco.tamanho) {
            //total_enviado += bytes_enviados;
            blocos_enviados++;
        } else {
            //erros_http++;
            ESP_LOGE(TAG, "Erro no envio TCP. Conexão perdida");
            break;
        }
    }

    close(socket_tcp);

    ESP_LOGI(TAG, "Envio TCP finalizado.");
    ESP_LOGI(TAG, "Blocos enviados: %d", blocos_enviados);
    //ESP_LOGI(TAG, "Total enviado: %u bytes", (unsigned int)total_enviado);
    //ESP_LOGI(TAG, "Erros TCP: %d", erros_http);

    vTaskDelete(NULL);
}

/*
static void task_envio_TCP(void *pvParameters)
{
    int socket_tcp = conectar_tcp();

    if (socket_tcp < 0) {
        ESP_LOGE(TAG, "Nao foi possivel iniciar o envio TCP");
        vTaskDelete(NULL);
        return;
    }

    bloco_i2s_t bloco;

    while (true) {
        if (xQueueReceive(fila_dados, &bloco, pdMS_TO_TICKS(100)) != pdPASS) {
            if (aquisicao_finalizada) {
                break;
            }

            continue;
        }

        int64_t inicio_envio = esp_timer_get_time();

        int bytes_enviados = enviar_tcp_completo(
            socket_tcp,
            bloco.dados,
            bloco.tamanho
        );

        int64_t tempo_envio_us = esp_timer_get_time() - inicio_envio;

        tempo_http_total_us += tempo_envio_us;

        if (tempo_envio_us > tempo_http_max_us) {
            tempo_http_max_us = tempo_envio_us;
        }

        envios_http_medidos++;

        if (bytes_enviados == bloco.tamanho) {
            total_enviado += bytes_enviados;
            blocos_enviados++;
        } else {
            erros_http++;
            ESP_LOGE(TAG, "Erro no envio TCP");

            close(socket_tcp);

            do {
                socket_tcp = conectar_tcp();
            } while (socket_tcp < 0);

            bytes_enviados = enviar_tcp_completo(
                socket_tcp,
                bloco.dados,
                bloco.tamanho
            );

            if (bytes_enviados == bloco.tamanho) {
                total_enviado += bytes_enviados;
                blocos_enviados++;
            } else {
                erros_http++;
            }
        }
    }

    close(socket_tcp);

    ESP_LOGI(TAG, "Envio TCP finalizado.");
    ESP_LOGI(TAG, "Blocos enviados: %d", blocos_enviados);
    ESP_LOGI(TAG, "Total enviado: %u bytes", (unsigned int)total_enviado);
    ESP_LOGI(TAG, "Erros TCP: %d", erros_http);

    if (envios_http_medidos > 0) {
        ESP_LOGI(TAG, "Tempo medio TCP: %lld us",
                 tempo_http_total_us / envios_http_medidos);

        ESP_LOGI(TAG, "Tempo maximo TCP: %lld us",
                 tempo_http_max_us);
    }

    vTaskDelete(NULL);
}
    */

static void task_aquisicao_HTTP(void *pvParameters)
{
    bloco_i2s_t bloco;

    while (capturando) {
        esp_err_t res = i2s_channel_read(
            rx_handle,
            bloco.dados,
            DATA_BUFFER_SIZE,
            &bloco.tamanho,
            portMAX_DELAY
        );

        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Erro na leitura do I2S: %s", esp_err_to_name(res));
            continue;
        }

        //total_lido += bloco.tamanho;
        blocos_lidos++;

        if (xQueueSend(fila_dados, &bloco, 0) != pdPASS) {
            blocos_descartados++;
        }
    }

    aquisicao_finalizada = true;

    ESP_LOGI(TAG, "Aquisicao finalizada.");
    ESP_LOGI(TAG, "Blocos lidos: %d", blocos_lidos);
    //ESP_LOGI(TAG, "Total lido: %u bytes", (unsigned int)total_lido);
    ESP_LOGI(TAG, "Blocos descartados: %d", blocos_descartados);

    vTaskDelete(NULL);
}

/*
static void task_envio_HTTP(void *pvParameters)
{
    esp_http_client_config_t http_cfg = {
        .url = HTTP_URL,
        .transport_type = HTTP_TRANSPORT_OVER_TCP
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(
        client,
        "Content-Type",
        "application/octet-stream"
    );

    bloco_i2s_t bloco;

    while (!aquisicao_finalizada || uxQueueMessagesWaiting(fila_dados) > 0) {

        if (xQueueReceive(fila_dados, &bloco, pdMS_TO_TICKS(100)) != pdPASS) {
            continue;
        }

        esp_err_t err = esp_http_client_open(client, bloco.tamanho);

        if (err == ESP_OK) {
            int bytes_enviados = esp_http_client_write(
                client,
                (char *)bloco.dados,
                bloco.tamanho
            );

            esp_http_client_close(client);

            if (bytes_enviados == bloco.tamanho) {
                total_enviado += bytes_enviados;
                blocos_enviados++;
            } else {
                erros_http++;
                ESP_LOGE(
                    TAG,
                    "HTTP: enviou %d de %d bytes",
                    bytes_enviados,
                    bloco.tamanho
                );
            }

        } else {
            erros_http++;
            ESP_LOGE(
                TAG,
                "HTTP: erro ao abrir conexão: %s",
                esp_err_to_name(err)
            );
        }
    }

    ESP_LOGI(TAG, "Envio finalizado.");
    ESP_LOGI(TAG, "Blocos enviados: %d", blocos_enviados);
    ESP_LOGI(TAG, "Total enviado: %u bytes", (unsigned int)total_enviado);
    ESP_LOGI(TAG, "Erros HTTP: %d", erros_http);

    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
}
*/
void app_main(void)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_LOGI(TAG, "Iniciando conexão Wi-Fi...");
    wifi_init_sta();

    esp_netif_ip_info_t ip_info;

    do {
        vTaskDelay(pdMS_TO_TICKS(500));

        esp_netif_t *netif =
            esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

        esp_netif_get_ip_info(netif, &ip_info);

    } while (ip_info.ip.addr == 0);

    ESP_LOGI(TAG, "Wi-Fi OK! IP: " IPSTR, IP2STR(&ip_info.ip));

    ESP_LOGI(TAG, "I2S 44.1 kHz...");
    i2s_init_std(SAMPLE_RATE_STD, BIT_DEPTH_STD, false);

    vTaskDelay(pdMS_TO_TICKS(1000));

    i2s_channel_disable(rx_handle);
    i2s_del_channel(rx_handle);

    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "I2S 78.125 kHz...");
    i2s_init_std(SAMPLE_RATE_ULT, BIT_DEPTH_ULT, true);

    vTaskDelay(pdMS_TO_TICKS(1000));

    //ESP_LOGI(TAG, "RAM interna livre: %u bytes", (unsigned int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    //ESP_LOGI(TAG, "Maior bloco livre RAM interna: %u bytes", (unsigned int)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    //ESP_LOGI(TAG, "PSRAM livre: %u bytes", (unsigned int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
         
    fila_dados = xQueueCreate(QUEUE_LENGTH, sizeof(bloco_i2s_t));

    if (fila_dados == NULL) {
        ESP_LOGE(TAG, "Erro ao criar fila");
        return;
    }

    configurar_timer();

    overflows_i2s = 0;

    socket_tcp = conectar_tcp();

    if (socket_tcp < 0) {
        ESP_LOGE(TAG, "Nao foi possivel conectar ao servidor TCP");
        return;
    }

    capturando = true;
    aquisicao_finalizada = false;

    xTaskCreatePinnedToCore(task_aquisicao_HTTP, "Aquisicao", 8192, NULL, 10, NULL, 1);
    xTaskCreatePinnedToCore(task_envio_TCP, "Envio", 8192, NULL, 5, NULL, 0);

    ESP_ERROR_CHECK(gptimer_enable(timer_captura));
    ESP_ERROR_CHECK(gptimer_start(timer_captura));
}