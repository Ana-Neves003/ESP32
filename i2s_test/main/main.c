#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "driver/i2s_std.h"
#include "driver/i2s_pdm.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"

#define SAMPLE_RATE_STD  44100
#define SAMPLE_RATE_ULT  78125   
#define DMA_BUF_COUNT    16
#define DMA_BUF_LEN_SMPL 511

#define BIT_DEPTH_STD        I2S_DATA_BIT_WIDTH_16BIT
#define BIT_DEPTH_ULT        I2S_DATA_BIT_WIDTH_32BIT
#define DATA_BUFFER_SIZE     (DMA_BUF_LEN_SMPL *2* BIT_DEPTH_ULT / 8)


// Pinos do microfone PDM
#define MIC_CLOCK_PIN     GPIO_NUM_21
#define MIC_DATA_PIN      GPIO_NUM_4

#define I2S_PORT_NUM    I2S_NUM_0

static const char *TAG = "I2S_TEST";


i2s_chan_handle_t rx_handle;
uint8_t dataBuffer[DATA_BUFFER_SIZE];
QueueHandle_t xQueue;


void wifi_init_sta(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "VIVOFIBRA-4870-EXT",
            //.ssid = "LASEM",
            .password = "CC99735551",
            //.password = "besourosuco",
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();
}

void i2s_init_std(int SAMPLE_RATE, i2s_data_bit_width_t BIT_DEPTH, bool modo_ult) {

    ESP_LOGI(TAG, "Initializing I2S STD"); 

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
            .sample_rate_hz = SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_APLL,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256
        },
        .slot_cfg = {
            .data_bit_width =  BIT_DEPTH,
            .slot_bit_width =  BIT_DEPTH,
                .slot_mode = I2S_SLOT_MODE_STEREO,
                .slot_mask = I2S_STD_SLOT_BOTH,
                .ws_width = BIT_DEPTH,
                .ws_pol = false,
                .bit_shift = true
            },
            .gpio_cfg = {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = modo_ult ? MIC_CLOCK_PIN: I2S_GPIO_UNUSED,
                .ws   = modo_ult ? I2S_GPIO_UNUSED: MIC_CLOCK_PIN,
                .dout = I2S_GPIO_UNUSED,
                .din  = MIC_DATA_PIN
            }
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
    ESP_LOGI(TAG, "I2S inicializado!");
}

void i2s_init_pdm(int SAMPLE_RATE, i2s_data_bit_width_t BIT_DEPTH)
{
    ESP_LOGI(TAG, "Inicializando I2S em modo PDM.");

    // Configuração do canal 
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0, // escolha do canal i2s 0 (no esp32 apenas esse canal suporta o modo pdm)
        .role = I2S_ROLE_MASTER, // o esp32 atua como o controlador
        .dma_desc_num = DMA_BUF_COUNT, // quantidade de buffers do DMA
        .dma_frame_num = DMA_BUF_LEN_SMPL, // tamanho dos buffers do DMA
        .auto_clear = false, // limpar automaticamente o buffer TX (desnecessario)
    };
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg,NULL, &rx_handle));                         // Create a new I2S channel  

    // Configuração do microfone em modo PDM RX
    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(BIT_DEPTH, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = MIC_CLOCK_PIN, 
            .din = MIC_DATA_PIN,
            .invert_flags = { // nao inverter bits
                .clk_inv = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(rx_handle, &pdm_rx_cfg));    // Initialize I2S channel in standard mode
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));                           // Enable I2S channel
    ESP_LOGI(TAG, "I2S inicializado!");
}

void app_main(void) {
    
    // Inicializa NVS (necessário para Wi-Fi funcionar)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // IP do computador e porta
    //const char* DEST_IP = "192.168.1.124";  
    const char* DEST_IP = "192.168.15.183";  
    const int DEST_PORT = 12345;

    size_t bytes_read = 0;

    // Inicializa Wi-Fi
    ESP_LOGI(TAG, "Iniciando conexão Wi-Fi...");
    wifi_init_sta();
    vTaskDelay(pdMS_TO_TICKS(3000));  // Aguarda conexão

    
    // Mostra IP do ESP32
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_get_ip_info(netif, &ip_info);
    ESP_LOGI(TAG, "IP do ESP32: " IPSTR, IP2STR(&ip_info.ip));
    
    // Cria socket UDP
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Não foi possível criar o socket");
        return;
    }

    // Configura destino
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(DEST_IP);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(DEST_PORT);
    
    // Inicializa em 44100 só pra ativar o microfone
    ESP_LOGI(TAG, "Inicializando I2S...");
    //i2s_init_pdm(SAMPLE_RATE_STD, BIT_DEPTH_STD);
    i2s_init_std(SAMPLE_RATE_STD, BIT_DEPTH_STD, false);
    vTaskDelay(pdMS_TO_TICKS(500));

    // Desliga e deleta o canal
    i2s_channel_disable(rx_handle);
    i2s_del_channel(rx_handle);

    //Reconfigura para 78125 Hz
    i2s_init_std(SAMPLE_RATE_ULT, BIT_DEPTH_ULT, true);
    vTaskDelay(pdMS_TO_TICKS(500));

    while (1) {
        esp_err_t res = i2s_channel_read(rx_handle, dataBuffer, DATA_BUFFER_SIZE, &bytes_read, portMAX_DELAY);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Erro na leitura do I2S: %s", esp_err_to_name(res));
            continue;
        }

        printf("%02x %02x %02x %02x %02x %02x %02x\n",
            dataBuffer[0], dataBuffer[1], dataBuffer[2],
            dataBuffer[3], dataBuffer[4], dataBuffer[5],
            dataBuffer[6]);

            sendto(sock, dataBuffer, bytes_read, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            //int sent = sendto(sock, dataBuffer, bytes_read, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            //ESP_LOGI(TAG, "UDP: Enviados %d bytes", sent);

        vTaskDelay(pdMS_TO_TICKS(100));  
    }

    /*
    esp_err_t res = i2s_channel_read(rx_handle, dataBuffer, DATA_BUFFER_SIZE, &bytes_read, portMAX_DELAY);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Erro na leitura do I2S: %s", esp_err_to_name(res));
        }

        printf("%02x %02x %02x %02x %02x %02x %02x\n",
            dataBuffer[0], dataBuffer[1], dataBuffer[2],
            dataBuffer[3], dataBuffer[4], dataBuffer[5],
            dataBuffer[6]);

        //sendto(sock, dataBuffer, bytes_read, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

        vTaskDelay(pdMS_TO_TICKS(100));
        */


}
