#include <stdio.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "main.h"
#include "sd_driver.h"

#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "I2S_STD";

i2s_chan_handle_t rx_handle;
uint8_t dataBuffer[DATA_BUFFER_SIZE];

void i2s_init(bool modo_padrao) {
    i2s_chan_config_t rx_channel_config = {
        .id = I2S_PORT_NUM,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = DMA_BUF_COUNT,
        .dma_frame_num = DMA_BUF_LEN_SMPL,
        .auto_clear = true
    };

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = modo_padrao ? SAMPLE_RATE_STD : SAMPLE_RATE_ULT,
            .clk_src = I2S_CLK_SRC_APLL,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256
        },
        .slot_cfg = {
            .data_bit_width = modo_padrao ? BIT_DEPTH_STD : BIT_DEPTH_ULT,
            .slot_bit_width = modo_padrao ? BIT_DEPTH_STD : BIT_DEPTH_ULT,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = modo_padrao ? BIT_DEPTH_STD : BIT_DEPTH_ULT,
            .ws_pol = false,
            .bit_shift = true
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = modo_padrao ? I2S_GPIO_UNUSED : MIC_CLOCK_PIN,
            .ws   = modo_padrao ? MIC_CLOCK_PIN : I2S_GPIO_UNUSED,
            .dout = I2S_GPIO_UNUSED,
            .din  = MIC_DATA_PIN
        }
    };

    ESP_ERROR_CHECK(i2s_new_channel(&rx_channel_config, NULL, &rx_handle));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
}

void i2s_read_task(void *pvParameters) {
    size_t bytes_read;

    while (1) {
        ESP_ERROR_CHECK(i2s_channel_read(rx_handle, dataBuffer, DATA_BUFFER_SIZE, &bytes_read, portMAX_DELAY));

        printf("%02x %02x %02x %02x %02x %02x %02x\n",
               dataBuffer[0], dataBuffer[1], dataBuffer[2],
               dataBuffer[3], dataBuffer[4], dataBuffer[5],
               dataBuffer[6]);

        if (audio_file) {
            fwrite(dataBuffer, 1, bytes_read, audio_file);
            fflush(audio_file);
            fsync(fileno(audio_file));
        }
    }
}

void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(500));

    sdcard_init();
    vTaskDelay(pdMS_TO_TICKS(500));

    i2s_init(true);
    vTaskDelay(pdMS_TO_TICKS(500));

    i2s_channel_disable(rx_handle);
    i2s_del_channel(rx_handle);
    vTaskDelay(pdMS_TO_TICKS(500));

    i2s_init(false);
    vTaskDelay(pdMS_TO_TICKS(500));

    xTaskCreate(i2s_read_task, "i2s_read_task", 4096, NULL, 5, NULL);
}
