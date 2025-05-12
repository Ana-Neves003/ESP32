// main.c
#include "main.h"
#include "sd_driver.h"
#include <unistd.h> // Necessário para usar fsync()


static const char *TAG = "I2S_PDM";

uint8_t dataBuffer[DATA_BUFFER_SIZE];
i2s_chan_handle_t rx_handle;
FILE* audio_file = NULL;

void i2s_init()
{
    ESP_LOGI(TAG, "Inicializando I2S em modo PDM.");

    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = DMA_BUF_COUNT,
        .dma_frame_num = DMA_BUF_LEN_SMPL,
        .auto_clear = false,
    };
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));

    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(BITS_PER_SAMPLE, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = MIC_CLOCK_PIN,
            .din = MIC_DATA_PIN,
            .invert_flags = {
                .clk_inv = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(rx_handle, &pdm_rx_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
    ESP_LOGI(TAG, "I2S inicializado...");
}

void i2s_read_task(void *pvParameters)
{
    size_t bytes_read;
    while (1)
    {
        ESP_ERROR_CHECK(i2s_channel_read(rx_handle, dataBuffer, DATA_BUFFER_SIZE, &bytes_read, portMAX_DELAY));

        printf("%02x %02x %02x %02x %02x %02x %02x\n",
               dataBuffer[0], dataBuffer[1], dataBuffer[2],
               dataBuffer[3], dataBuffer[4], dataBuffer[5],
               dataBuffer[6]);

        //ESP_LOGI(TAG, "Gravando %d bytes no SD", (int)bytes_read);

        if (audio_file) {
            fwrite(dataBuffer, 1, bytes_read, audio_file);
            fflush(audio_file);
            fsync(fileno(audio_file));
        }
    }
}

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(10000));
    sdcard_init(&audio_file);
    i2s_init();
    xTaskCreate(i2s_read_task, "i2s_read_task", 4096, NULL, 5, NULL);
}
