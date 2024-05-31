#include "i2s.h"
#include "driver/i2s.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

static const char *SETUP_TAG = "setup_tag";

QueueHandle_t xQueueData;
EventGroupHandle_t xEvents;

void i2s_setup(void) {
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM,
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCK_IO,
        .ws_io_num = I2S_PIN_NO_CHANGE,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_DATA_IN_IO
    };

    //i2s_driver_install(I2S_PORT_NUM, &i2s_config, 32, NULL);
    ESP_LOGI(SETUP_TAG, "INSTALANDO DRIVER I2S");
    ESP_ERROR_CHECK(i2s_driver_install(I2S_PORT_NUM, &i2s_config, 32, &xQueueData));

    
    //i2s_set_pin(I2S_PORT_NUM, &pin_config);
    ESP_LOGI(SETUP_TAG, "DEFININDO PINOS I2S");
    ESP_ERROR_CHECK(i2s_set_pin(I2S_PORT_NUM, &pin_config));
}

void i2s_pause(void) {
    ESP_LOGI(SETUP_TAG, "PARANDO I2S");
    ESP_ERROR_CHECK(i2s_stop(I2S_PORT_NUM));
}

void i2s_resume(void) {
    ESP_LOGI(SETUP_TAG, "REINICIANDO I2S");
    ESP_ERROR_CHECK(i2s_start(I2S_PORT_NUM));
}

