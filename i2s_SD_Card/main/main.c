#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "esp_err.h"

// Definições de pinos
#define I2S_SAMPLE_RATE     (44100)         // Taxa de amostragem do I2S
#define I2S_NUM             (0)             // Número do módulo I2S
#define I2S_BCK_IO          (GPIO_NUM_21)   // Pino BCK (Bit Clock)
#define I2S_DATA_IN_IO      (GPIO_NUM_4)    // Pino de entrada de dados I2S
#define GPIO_BUTTON         (GPIO_NUM_0)    // Definição do pino do botão
#define PIN_NUM_MISO        (GPIO_NUM_19)   // Definição do pino MISO (Master In Slave Out) para SPI
#define PIN_NUM_MOSI        (GPIO_NUM_23)   // Definição do pino MOSI (Master Out Slave In) para SPI
#define PIN_NUM_CLK         (GPIO_NUM_18)   // Definição do pino CLK (Clock) para SPI
#define PIN_NUM_CS          (GPIO_NUM_22)   // Definição do pino CS (Chip Select) para SPI
#define MOUNT_POINT         "/sdcard"       // Ponto de montagem do sistema de arquivos no cartão SD

// Tag para logging
static const char *TAG = "i2s_sd_card"; 

FILE* file = NULL;

void i2s_setup(void) {
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX, 
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

    i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM, &pin_config);
}

void i2s_task(void *pvParameter) {
    uint8_t buffer[1024];
    size_t bytes_read = 0;


    while (1) {
        i2s_read(I2S_NUM, buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);
        fwrite(buffer, 1, bytes_read, file);

        for (int i = 0; i < bytes_read; i++) {
            printf("%02x ", buffer[i]);
        }
        printf("\n");

        vTaskDelay(pdMS_TO_TICKS(100));
    }
        
}

void sd_card_setup(void) {
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    sdmmc_card_t* card;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao montar a partição FAT no SD card. Código de erro = %d", ret);
        return;
    }

    ESP_LOGI(TAG, "Cartão SD inicializado e pronto para gravação.");
}

void app_main() {
    esp_log_level_set(TAG, ESP_LOG_INFO);

    // Configuração do cartão SD
    sd_card_setup();

    // Abertura do arquivo para escrita
    file = fopen("/sdcard/gravacao.raw", "w");
    if (file == NULL) {
        ESP_LOGE(TAG, "Não foi possível abrir o arquivo para escrita");
        return;
    }

    i2s_setup();

    
    xTaskCreatePinnedToCore(i2s_task, "i2s_task", 8192, NULL, 5, NULL, 1);
}
