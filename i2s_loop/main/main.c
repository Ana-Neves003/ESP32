#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_wifi.h"

#include "driver/i2s_std.h"
#include "esp_http_client.h"


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

// REDE / ENVIO
//#define DEST_IP             "192.168.15.183"
//#define DEST_IP             "192.168.1.133"
//#define DEST_PORT 12345
//#define HTTP_URL             "http://192.168.1.133:8000/upload" 
#define HTTP_URL             "http://192.168.15.183:8000/upload" 


static const char *TAG = "I2S_LOOP_HTTP";

static i2s_chan_handle_t rx_handle = NULL;
static uint8_t dataBuffer[DATA_BUFFER_SIZE];
//static const TickType_t duracao = pdMS_TO_TICKS(1000 * 60);


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
            //.ssid = "LASEM",
            //.ssid = "Cerberus",
            .password = "CC99735551",
            //.password = "besourosuco",
            //.password = "Lime@302",
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_connect());
}

static void i2s_init_std(int SAMPLE_RATE, i2s_data_bit_width_t BIT_DEPTH, bool modo_ult)
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
            .bclk = modo_ult ? MIC_CLOCK_PIN : I2S_GPIO_UNUSED,
            .ws   = modo_ult ? I2S_GPIO_UNUSED : MIC_CLOCK_PIN,
            .dout = I2S_GPIO_UNUSED,
            .din  = MIC_DATA_PIN
        }
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
}

void app_main(void)
{
    // NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // Conecta Wi-Fi
    ESP_LOGI(TAG, "Iniciando conexão Wi-Fi...");
    wifi_init_sta();
    //ESP_LOGI(TAG, "Aguardando conexão Wi-Fi...");

    esp_netif_ip_info_t ip_info;
    do {
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        esp_netif_get_ip_info(netif, &ip_info);
    } while (ip_info.ip.addr == 0);

    ESP_LOGI(TAG, "Wifi OK! IP: " IPSTR, IP2STR(&ip_info.ip));

    // HTTP CLIENT
    esp_http_client_config_t http_cfg = {
        .url = HTTP_URL,
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
        //.keep_alive_enable = true
    };
    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);

    // ---- ENVIO HTTP ----
        esp_http_client_set_method(client, HTTP_METHOD_POST);
        esp_http_client_set_header(client, "Content-Type", "application/octet-stream");
   
    // Inicializa em 44100 só pra ativar o microfone
    ESP_LOGI(TAG, "I2S 44.1 kHz...");
    i2s_init_std(SAMPLE_RATE_STD, BIT_DEPTH_STD, false);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Desliga e deleta o canal
    i2s_channel_disable(rx_handle);
    i2s_del_channel(rx_handle);
    vTaskDelay(pdMS_TO_TICKS(1000));

    //Reconfigura para 78125 Hz
    ESP_LOGI(TAG, "I2S 78.125 kHz...");
    i2s_init_std(SAMPLE_RATE_ULT, BIT_DEPTH_ULT, true);
    vTaskDelay(pdMS_TO_TICKS(1000));

    /*
    TickType_t start = xTaskGetTickCount();

    while ((xTaskGetTickCount() - start) < duracao) {

        size_t bytes_read = 0;
        esp_err_t res = i2s_channel_read(rx_handle, dataBuffer, DATA_BUFFER_SIZE, &bytes_read, portMAX_DELAY);

        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Erro na leitura do I2S: %s", esp_err_to_name(res));
            continue;
        }

        esp_err_t err = esp_http_client_open(client, bytes_read);
        if (err == ESP_OK) {

            esp_http_client_write(client, (char *)dataBuffer, bytes_read);
            esp_http_client_close(client);

            ESP_LOGI(TAG, "HTTP: enviado %d bytes", bytes_read);

        } else {
            ESP_LOGE(TAG, "HTTP: erro ao abrir conexão: %s", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(10));  
    }
    */

    int pacotes_enviados = 0;
    const int PACOTES_MAX = 5;

    while (pacotes_enviados < PACOTES_MAX) {
        size_t bytes_read = 0;
        esp_err_t res = i2s_channel_read(rx_handle, dataBuffer, DATA_BUFFER_SIZE, &bytes_read, portMAX_DELAY);

        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Erro na leitura do I2S: %s", esp_err_to_name(res));
            continue;
        }

        // ---- ENVIO HTTP ----
        esp_http_client_set_method(client, HTTP_METHOD_POST);
        esp_http_client_set_header(client, "Content-Type", "application/octet-stream");

        esp_err_t err = esp_http_client_open(client, bytes_read);
        if (err == ESP_OK) {
            esp_http_client_write(client, (char *)dataBuffer, bytes_read);
            esp_http_client_close(client);

            ESP_LOGI(TAG, "[%d] HTTP: enviado %d bytes", pacotes_enviados + 1, bytes_read);
            pacotes_enviados++;
        } 
        else {
            ESP_LOGE(TAG, "HTTP: erro ao abrir conexão: %s", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(10));  // delay mínimo necessário
    }


    ESP_LOGI(TAG, "Aquisição finalizada.");
    esp_http_client_cleanup(client);

    if (rx_handle) {
        i2s_channel_disable(rx_handle);
        i2s_del_channel(rx_handle);
        rx_handle = NULL;
    }
}
