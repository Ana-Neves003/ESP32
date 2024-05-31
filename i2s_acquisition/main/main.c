#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2s.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

#define I2S_SAMPLE_RATE     (31250)    // Taxa de amostragem do I2S
#define I2S_NUM             (0)        // Número do módulo I2S
#define MIC_CLOCK_PIN       (GPIO_NUM_21) // Pino BCK (Bit Clock)
#define MIC_DATA_PIN        (GPIO_NUM_4)  // Pino de entrada de dados I2S
#define DMA_BUF_COUNT       (64)
#define DMA_BUF_LEN_SMPL    (1024)
#define RECORDING_TIME_SECONDS (50) // Duração da gravação em segundos

// Definições de pinos
#define PIN_NUM_MISO   GPIO_NUM_19     // Definição do pino MISO (Master In Slave Out) para SPI
#define PIN_NUM_MOSI   GPIO_NUM_23     // Definição do pino MOSI (Master Out Slave In) para SPI
#define PIN_NUM_CLK    GPIO_NUM_18     // Definição do pino CLK (Clock) para SPI
#define PIN_NUM_CS     GPIO_NUM_22     // Definição do pino CS (Chip Select) para SPI
#define MOUNT_POINT    "/sdcard"       // Ponto de montagem do sistema de arquivos no cartão SD
#define SPI_DMA_CHAN   1

#define DMA_BUF_COUNT    64
#define DMA_BUF_LEN_SMPL 1024
#define BIT_DEPTH I2S_BITS_PER_SAMPLE_16BIT
#define DATA_BUFFER_SIZE (DMA_BUF_LEN_SMPL*BIT_DEPTH/8)

// Variáveis globais para controle de tempo
TickType_t recording_start_time;
TickType_t recording_duration_ticks;


// Declaração das funções
static esp_err_t init_sd_card(void);
static void i2s_setup(void);
void i2s_task(void *pvParameter);
void sd_card_task(void *pvParameter);

static const char *TAG = "i2s_sd_card_test";

// Configura o I2S
static void i2s_setup(void) {
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM, // Modo master e recebimento de dados
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = BIT_DEPTH,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // Nível de prioridade da interrupção
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN_SMPL,
        .use_apll = I2S_CLK_APLL,
        //.use_apll = false,
        //.tx_desc_auto_clear = false,
        //.fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_PIN_NO_CHANGE,
        .ws_io_num = MIC_CLOCK_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = MIC_DATA_PIN
    };

    i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM, &pin_config);
}

// Tarefa para capturar áudio usando o I2S
void i2s_task(void *pvParameter) {
    //uint8_t buffer[1024];
    char data_buffer[DATA_BUFFER_SIZE];
    size_t bytes_read = 0;

    while (1) {
        //i2s_read(I2S_NUM, buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);
        i2s_read(I2S_NUM, data_buffer, DATA_BUFFER_SIZE, &bytes_read, portMAX_DELAY);
        
        // Grava os dados no cartão SD
        FILE *file = fopen("/sdcard/audio.raw", "ab");
        if (file != NULL) {
            //fwrite(buffer, 1, bytes_read, file);
            fwrite(data_buffer, 1, bytes_read, file);
            fclose(file);
        }

        // Imprime os dados adquiridos
        for (int i = 0; i < bytes_read; i++) {
            //printf("%02x ", buffer[i]);
            printf("%02x ", data_buffer[i]);
            //printf("%x %x %x %x %x %x %x\n", data_buffer[0], data_buffer[1], data_buffer[2], data_buffer[3], data_buffer[4], data_buffer[5], data_buffer[6]);
        }
        printf("\n");

        // Verifica se já se passou o tempo de gravação
        if (xTaskGetTickCount() - recording_start_time >= recording_duration_ticks) {
            ESP_LOGI(TAG, "Recording stopped.");
            vTaskSuspend(NULL); // Suspende esta tarefa
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // Atraso para controle do tempo de exibição
    }
}

// Inicialização do cartão SD
static esp_err_t init_sd_card(void) {
    static bool sd_initialized = false; // Variável estática para verificar se o cartão SD já foi inicializado
    if (sd_initialized) {
        ESP_LOGW(TAG, "SD card already initialized.");
        return ESP_OK; // Retorna com sucesso se o cartão SD já estiver inicializado
    }

    esp_err_t ret;                          // Variável para armazenar o resultado das funções

    sdmmc_host_t host = SDSPI_HOST_DEFAULT(); // Configuração padrão do host SPI
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,        // Configuração do pino MOSI para SPI
        .miso_io_num = PIN_NUM_MISO,        // Configuração do pino MISO para SPI
        .sclk_io_num = PIN_NUM_CLK,         // Configuração do pino CLK para SPI
        .quadwp_io_num = -1,                // Pino não usado
        .quadhd_io_num = -1,                // Pino não usado
        .max_transfer_sz = 8192,            // Tamanho máximo de transferência
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
        .format_if_mount_failed = false,    // Formatar se a montagem falhar
        .max_files = 5,                    // Número máximo de arquivos abertos simultaneamente
        .allocation_unit_size = 16 * 1024  // Tamanho da unidade de alocação
    };

    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card); // Monta o sistema de arquivos FAT no cartão SD
    if (ret != ESP_OK) {                    // Verifica se a montagem foi bem-sucedida
        ESP_LOGE(TAG, "Failed to mount filesystem."); // Log de erro se falhou
        spi_bus_free(host.slot);            // Libera o barramento SPI
        return ret;                         // Retorna o código de erro
    }

    sd_initialized = true; // Marca o cartão SD como inicializado com sucesso
    sdmmc_card_print_info(stdout, card);    // Imprime informações do cartão SD
    return ESP_OK;                          // Retorna ESP_OK se bem-sucedido
}

// Tarefa para gravar áudio no cartão SD
void sd_card_task(void *pvParameter) {
    esp_err_t ret;

    // Inicializa o cartão SD
    ret = init_sd_card();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SD card.");
        vTaskDelete(NULL);
        return;
    }

    // Cria o arquivo para gravação
    FILE *file = fopen("/sdcard/recording.raw", "wb");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to create file for recording.");
        vTaskDelete(NULL);
        return;
    }

    // Grava áudio por RECORDING_TIME_SECONDS segundos
    for (int i = 0; i < RECORDING_TIME_SECONDS; i++) {
        uint8_t buffer[1024];
        size_t bytes_read = 0;

        // Lê dados do buffer I2S
        i2s_read(I2S_NUM, buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read audio data from I2S.");
            break;
        }
        
        // Escreve os dados no arquivo
        //fwrite(buffer, 1, bytes_read, file);
        size_t bytes_written = fwrite(buffer, 1, bytes_read, file);
        if (bytes_written != bytes_read) {
            ESP_LOGE(TAG, "Failed to write audio data to file.");
            break;
        }
    }

    // Fecha o arquivo
    fclose(file);

    //ESP_LOGI(TAG, "Audio recording completed.");

    ESP_LOGI(TAG, "Recording completed. File saved as '/sdcard/recording.raw'.");

    vTaskDelete(NULL);
}


void app_main(void)
{
    // Inicializa o cartão SD
    init_sd_card();

    // Configura o I2S
    i2s_setup();

    // Define o tempo de início da gravação
    recording_start_time = xTaskGetTickCount();
    recording_duration_ticks = pdMS_TO_TICKS(RECORDING_TIME_SECONDS * 1000);

    // Cria a tarefa para capturar áudio
    xTaskCreatePinnedToCore(i2s_task, "i2s_task", 8192, NULL, 5, NULL, PRO_CPU_NUM);

    // Cria a tarefa para gravar no cartão SD
    xTaskCreatePinnedToCore(sd_card_task, "sd_card_task", 8192, NULL, 5, NULL, APP_CPU_NUM);
}
