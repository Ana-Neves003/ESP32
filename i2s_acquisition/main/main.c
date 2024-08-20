#include "main.h"

static const char *TAG = "i2s_sd_card_test";

void app_main(void) {

    i2s_config_t i2s_config;
    i2s_pin_config_t pin_config;


    // Inicializa o cartão SD
    init_sd_card();

    ESP_LOGI(TAG, "Cartão SD Inicializado!");

    // Configurando o i2s para modo padrão
    i2s_setup(&i2s_config, &pin_config);
    ESP_ERROR_CHECK(i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL));
    ESP_ERROR_CHECK(i2s_set_pin(I2S_NUM, &pin_config));

    // Start I2S in default mode
    ESP_ERROR_CHECK(i2s_start(I2S_NUM));
    vTaskDelay(100);  // Atraso para permitir que as mudanças tenham efeito

    i2s_setup(&i2s_config, &pin_config);
    
    ESP_ERROR_CHECK(i2s_set_sample_rates(I2S_NUM, 44100));  

    ESP_LOGI(TAG, "Mudando para modo ultrassônico...");
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_ERROR_CHECK(i2s_set_sample_rates(I2S_NUM, 200000));

    // Define o tempo de início da gravação
    recording_start_time = xTaskGetTickCount();
    recording_duration_ticks = pdMS_TO_TICKS(RECORDING_TIME_SECONDS * 1000);

    // Cria a tarefa para capturar áudio
    xTaskCreatePinnedToCore(i2s_task, "i2s_task", 8192, NULL, 5, &vTask1Handle, PRO_CPU_NUM);

    ESP_LOGI(TAG, "Successful BOOT!");

}

void i2s_task(void *pvParameter) {
    char data_buffer[DATA_BUFFER_SIZE_ULT];
    size_t bytes_read = 0;

    while (1) {
        // Verifica se já se passou o tempo de gravação
        if (xTaskGetTickCount() - recording_start_time >= recording_duration_ticks) {
            ESP_LOGI(TAG, "Recording stopped.");
            vTaskSuspend(NULL);
        } else {
            if (i2s_read(I2S_NUM, (void *)data_buffer, DATA_BUFFER_SIZE_STD, &bytes_read, portMAX_DELAY) == ESP_OK) {
                // Grava os dados no cartão SD
                FILE *file = fopen("/sdcard/audio.raw", "ab");
                if (file != NULL) {
                    fwrite(data_buffer, bytes_read, 1, file);
                    printf("%x %x %x %x %x %x %x\n", data_buffer[0], data_buffer[1], data_buffer[2], data_buffer[3], data_buffer[4], data_buffer[5], data_buffer[6]);
                    fclose(file);
                }
            }
        }
    }
}


void i2s_setup(i2s_config_t *i2s_config, i2s_pin_config_t *pin_config) {

     *i2s_config = (i2s_config_t) {
        //.mode = I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM,
        .mode = I2S_MODE_MASTER | I2S_MODE_RX,
        .sample_rate = 8000,
        .bits_per_sample = BIT_DEPTH_STD,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN_SMPL,
        .use_apll = I2S_CLK_APLL,
    };

    *pin_config = (i2s_pin_config_t) {
        .bck_io_num = I2S_PIN_NO_CHANGE,
        .ws_io_num = MIC_CLOCK_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = MIC_DATA_PIN
    };

   
}

void i2s_setup_ult(i2s_config_t *i2s_config, i2s_pin_config_t *pin_config) {

    *i2s_config = (i2s_config_t) {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX,
        .sample_rate = 312500,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN_SMPL,
        .use_apll = I2S_CLK_APLL,
    };

    *pin_config = (i2s_pin_config_t) {
        .bck_io_num = I2S_PIN_NO_CHANGE,
        .ws_io_num = MIC_CLOCK_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = MIC_DATA_PIN
    };

}