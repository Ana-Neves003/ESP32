#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2s.h"

#define I2S_SAMPLE_RATE     (44100)    // Taxa de amostragem do I2S
#define I2S_NUM             (0)        // Número do módulo I2S
#define I2S_BCK_IO          (GPIO_NUM_21) // Pino BCK (Bit Clock)
#define I2S_DATA_IN_IO      (GPIO_NUM_4)  // Pino de entrada de dados I2S

static const char *TAG = "i2s_example";

void i2s_setup(void) {
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX, // Modo master e recebimento de dados
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // Nível de prioridade da interrupção
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

    i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM, &pin_config);
}

void i2s_task(void *pvParameter) {
    uint8_t buffer[1024];
    size_t bytes_read = 0;

    while (1) {
        i2s_read(I2S_NUM, buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);
        
        // Imprime os dados adquiridos
        for (int i = 0; i < bytes_read; i++) {
            printf("%02x ", buffer[i]);
        }
        printf("\n");
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


void app_main() {
    esp_log_level_set(TAG, ESP_LOG_INFO);

    i2s_setup();

    xTaskCreatePinnedToCore(i2s_task, "i2s_task", 4096, NULL, 5, NULL, 1);
}
