#include "main.h"

static const char *TAG = "i2s_sd_card_test";

void app_main(void) {
    // Inicializa o cartão SD
    init_sd_card();

    // Configura o I2S

    printf("Entrando no modo Standard\n");
    i2s_setup_standard_mode();
    printf("Delay de 5s\n");
    vTaskDelay(pdMS_TO_TICKS(5000));
    printf("Entrando no modo Ultrasonic\n");
    i2s_setup_ultrasonic_mode();

    // Define o tempo de início da gravação
    recording_start_time = xTaskGetTickCount();
    recording_duration_ticks = pdMS_TO_TICKS(RECORDING_TIME_SECONDS * 1000);

    // Cria a tarefa para capturar áudio
    //xTaskCreatePinnedToCore(i2s_task, "i2s_task", 8192, NULL, 5, &vTask1Handle, PRO_CPU_NUM);

    
}

void i2s_task(void *pvParameter) {
    char data_buffer[DATA_BUFFER_SIZE];
    size_t bytes_read = 0;

    while (1) {
        // Verifica se já se passou o tempo de gravação
        ESP_LOGI(TAG, "Entrou na i2s_task.");
        if (xTaskGetTickCount() - recording_start_time >= recording_duration_ticks) {
            ESP_LOGI(TAG, "Recording stopped.");
            vTaskSuspend(NULL);
        } else {
            ESP_LOGI(TAG, "Entrou no else da i2s_task.");
            if (i2s_read(I2S_NUM, (void *)data_buffer, DATA_BUFFER_SIZE, &bytes_read, portMAX_DELAY) == ESP_OK) {
                    ESP_LOGI(TAG, "Entrou no if da i2s_task.");
                    FILE *file = fopen("/sdcard/audio.raw", "ab");
                    if (file != NULL) {
                        fwrite(data_buffer, bytes_read, 1, file);
                        printf("%x %x %x %x %x %x %x\n", data_buffer[0], data_buffer[1], data_buffer[2], data_buffer[3], data_buffer[4], data_buffer[5], data_buffer[6]);
                        fclose(file);
                    }
            } else {
                ESP_LOGE(TAG, "Failed to read from I2S.");
            }
        }
    }
}



void i2s_setup_standard_mode(void) {
    i2s_config_t i2s_config_std = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM,
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = BIT_DEPTH,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN_SMPL,
        .use_apll = I2S_CLK_APLL,
    };

    i2s_pin_config_t pin_config_std = {
        .bck_io_num = I2S_PIN_NO_CHANGE,
        .ws_io_num = MIC_CLOCK_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = MIC_DATA_PIN
    };

    // Configure o I2S para o modo standard
    ESP_ERROR_CHECK(i2s_driver_install(I2S_NUM, &i2s_config_std, 0, NULL));
    ESP_ERROR_CHECK(i2s_set_pin(I2S_NUM, &pin_config_std));
    ESP_ERROR_CHECK(i2s_stop(I2S_NUM));
}

// Configurações para o modo ultrassônico
void i2s_setup_ultrasonic_mode(void) {
    i2s_config_t i2s_config_ult = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX, 
        .sample_rate = ULTRASONIC_SAMPLE_RATE, 
        .bits_per_sample = BIT_DEPTH, 
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT, 
        .communication_format = I2S_COMM_FORMAT_STAND_I2S, 
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, 
        .dma_buf_count = DMA_BUF_COUNT, 
        .dma_buf_len = DMA_BUF_LEN_SMPL, 
        .use_apll = I2S_CLK_APLL, 
    };

    i2s_pin_config_t pin_config_ult = {
        .bck_io_num = I2S_PIN_NO_CHANGE, 
        .ws_io_num = MIC_CLOCK_PIN, 
        .data_out_num = I2S_PIN_NO_CHANGE, 
        .data_in_num = MIC_DATA_PIN 
    };

    // Configure o I2S para o modo ultrassônico
    ESP_ERROR_CHECK(i2s_driver_install(I2S_NUM, &i2s_config_ult, 0, NULL));
    ESP_ERROR_CHECK(i2s_set_pin(I2S_NUM, &pin_config_ult));
    ESP_ERROR_CHECK(i2s_stop(I2S_NUM)); 
}


