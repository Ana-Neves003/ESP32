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


// Configuração do canal DMA (se necessário)
#define SPI_DMA_CHAN   1               // Definição do canal DMA para SPI

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

void vTask_REC(void *pvParameter) {
    uint8_t buffer[1024];
    size_t bytes_read = 0;
    const int recording_time_ms = 10000; // 10 segundos em milissegundos
    const int delay_time_ms = 100; // 100 milissegundos de delay
    int elapsed_time_ms = 0;

    // Abre o arquivo uma vez no início da task
    file = fopen(MOUNT_POINT "/gravacao.raw", "w"); // Abre o arquivo em modo texto para escrita
    if (file == NULL) { // Verifica se o arquivo foi aberto com sucesso
        ESP_LOGE(TAG, "Failed to open file for writing"); // Log de erro se falhou
        vTaskDelete(NULL); // Sai da task se falhou
        return;
    }

    ESP_LOGI(TAG, "Iniciando a gravação");

    while (elapsed_time_ms < recording_time_ms) {
        // Lê dados do I2S
        if (i2s_read(I2S_NUM, buffer, sizeof(buffer), &bytes_read, portMAX_DELAY) == ESP_OK) {
            // Escreve os dados lidos no arquivo em formato hexadecimal
            for (int i = 0; i < bytes_read; i++) {
                fprintf(file, "%02x ", buffer[i]);
            }
            fprintf(file, "\n");
            fflush(file); // Garante que os dados sejam escritos no arquivo

            // Imprime os dados no monitor serial
            ESP_LOGI(TAG, "Gravando %d bytes no arquivo", bytes_read);
            for (int i = 0; i < bytes_read; i++) {
                printf("%02x ", buffer[i]);
            }
            printf("\n");
        } else {
            ESP_LOGE(TAG, "Erro ao ler dados do I2S");
        }

        // Adiciona um delay para evitar saturação da task
        vTaskDelay(pdMS_TO_TICKS(delay_time_ms));
        elapsed_time_ms += delay_time_ms;
    }

    ESP_LOGI(TAG, "Gravação concluída");

    // Fecha o arquivo após 10 segundos
    fclose(file);
    ESP_LOGI(TAG, "Arquivo fechado");
    
    // Encerra a task
    vTaskDelete(NULL);
}


static esp_err_t init_sd_card(void) {
    esp_err_t ret;                          // Variável para armazenar o resultado das funções

    sdmmc_host_t host = SDSPI_HOST_DEFAULT(); // Configuração padrão do host SPI
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,        // Configuração do pino MOSI para SPI
        .miso_io_num = PIN_NUM_MISO,        // Configuração do pino MISO para SPI
        .sclk_io_num = PIN_NUM_CLK,         // Configuração do pino CLK para SPI
        .quadwp_io_num = -1,                // Pino não usado
        .quadhd_io_num = -1,                // Pino não usado
        .max_transfer_sz = 4000,            // Tamanho máximo de transferência
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CHAN); // Inicializa o barramento SPI
    if (ret != ESP_OK) {                    // Verifica se a inicialização foi bem-sucedida
        ESP_LOGE(TAG, "Failed to initialize bus."); // Log de erro se falhou
        return ret;                         // Retorna o código de erro
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT(); // Configuração padrão do dispositivo SPI
    slot_config.gpio_cs = PIN_NUM_CS;      // Configuração do pino CS para SPI
    slot_config.host_id = host.slot;       // ID do host

    sdmmc_card_t *card;                    // Ponteiro para a estrutura de cartão SD
    const esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = true,    // Formatar se a montagem falhar
        .max_files = 5,                    // Número máximo de arquivos abertos simultaneamente
        .allocation_unit_size = 16 * 1024  // Tamanho da unidade de alocação
    };

    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card); // Monta o sistema de arquivos FAT no cartão SD
    if (ret != ESP_OK) {                    // Verifica se a montagem foi bem-sucedida
        ESP_LOGE(TAG, "Failed to mount filesystem."); // Log de erro se falhou
        spi_bus_free(host.slot);            // Libera o barramento SPI
        return ret;                         // Retorna o código de erro
    }

    sdmmc_card_print_info(stdout, card);    // Imprime informações do cartão SD
    return ESP_OK; 
}

void app_main() {
    esp_log_level_set(TAG, ESP_LOG_INFO);
    ESP_LOGI(TAG, "Iniciando o app_main");

    i2s_setup(); 

    if (init_sd_card() != ESP_OK) {         // Inicializa o cartão SD e verifica se foi bem-sucedido
        ESP_LOGE(TAG, "Failed to initialize SD card"); // Log de erro se falhou
        return;                             // Sai da função se falhou
    }
    ESP_LOGI(TAG, "SD Card inicializado com sucesso");

    // Abertura do arquivo para escrita
    //file = fopen("/sdcard/gravacao.raw", "w");
    //if (file == NULL) {
        //ESP_LOGE(TAG, "Não foi possível abrir o arquivo para escrita");
        //return;
    //}       

    
    xTaskCreatePinnedToCore(vTask_REC, "vTask_REC", 8192, NULL, 5, NULL, 1);
}
