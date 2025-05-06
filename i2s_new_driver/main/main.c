#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_pdm.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include <unistd.h> //Para funcionar o fsync. Sem ele o arquivo fica só em 0KB
#include "esp_err.h" 
#include "esp_log.h" 
#include "esp_vfs_fat.h"


// pins
#define MIC_CLOCK_PIN  GPIO_NUM_21  
#define MIC_DATA_PIN   GPIO_NUM_4   
#define SAMPLE_RATE    44100
#define BITS_PER_SAMPLE 16
#define DMA_BUF_COUNT    64
#define DMA_BUF_LEN_SMPL 1024
#define I2S_PORT       I2S_NUM_0
#define DATA_BUFFER_SIZE (DMA_BUF_LEN_SMPL * BITS_PER_SAMPLE / 8)


#define MOUNT_POINT "/sdcard"
#define SPI_DMA_CHAN 1
#define USE_SPI_MODE    
#define PIN_NUM_MISO 19 
#define PIN_NUM_MOSI 23 
#define PIN_NUM_CLK  18 
#define PIN_NUM_CS   22 

uint8_t dataBuffer[DATA_BUFFER_SIZE];

static const char *TAG = "I2S PDM";
i2s_chan_handle_t rx_handle;
FILE* audio_file = NULL;

void sdcard_init()
{
    ESP_LOGI(TAG, "Montando o sistema de arquivos SD...");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;
    host.flags = SDMMC_HOST_FLAG_SPI;
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CHAN));

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5
    };

    sdmmc_card_t* card;
    esp_err_t ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao montar o SD card (%s)", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "SD card montado com sucesso.");
    audio_file = fopen(MOUNT_POINT "/audio.raw", "wb");
    if (!audio_file) {
        ESP_LOGE(TAG, "Erro ao abrir arquivo para gravacao.");
    }else{
        ESP_LOGI(TAG, "Arquivo aberto com sucesso.");
    }
}

void i2s_init()
{
    ESP_LOGI(TAG, "Inicializando I2S em modo PDM.");

    // Configuração do canal 
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0, // escolha do canal i2s 0 (no esp32 apenas esse canal suporta o modo pdm)
        .role = I2S_ROLE_MASTER, // o esp32 atua como o controlador
        .dma_desc_num = DMA_BUF_COUNT, // quantidade de buffers do DMA
        .dma_frame_num = DMA_BUF_LEN_SMPL, // tamanho dos buffers do DMA
        .auto_clear = false, // limpar automaticamente o buffer TX (desnecessario)
    };
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg,NULL, &rx_handle));                         // Create a new I2S channel  

    // Configuração do microfone em modo PDM RX
    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(BITS_PER_SAMPLE, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = MIC_CLOCK_PIN, 
            .din = MIC_DATA_PIN,
            .invert_flags = { // nao inverter bits
                .clk_inv = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(rx_handle, &pdm_rx_cfg));    // Initialize I2S channel in standard mode
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));                           // Enable I2S channel
    ESP_LOGI(TAG, "I2S inicializado...");
}

void i2s_read_task(void *pvParameters)
{
    size_t bytes_read;                  // Number of bytes read from the I2S

    while(1)
    {
        ESP_ERROR_CHECK(i2s_channel_read(rx_handle, dataBuffer, DATA_BUFFER_SIZE, &bytes_read, portMAX_DELAY));    // Read data from the I2S

        //ESP_LOGI(TAG, "Bytes read: %d", bytes_read);  // Print the number of bytes read from the I2S
        
        // Exibe os primeiros 7 bytes em hexadecimal
        printf("%02x %02x %02x %02x %02x %02x %02x\n",
            dataBuffer[0], dataBuffer[1], dataBuffer[2],
            dataBuffer[3], dataBuffer[4], dataBuffer[5],
            dataBuffer[6]);

            ESP_LOGI(TAG, "Gravando %d bytes no SD", bytes_read);

            if (audio_file) {
                fwrite(dataBuffer, 1, bytes_read, audio_file);
                fflush(audio_file);
                fsync(fileno(audio_file));

            }
    }
}

void app_main(void)
{
    vTaskDelay(10000 / portTICK_PERIOD_MS);                                 // Delay to open the serial port
    sdcard_init();
    i2s_init();                                                             // Initialize I2S
    xTaskCreate(i2s_read_task, "i2s_read_task", 4096, NULL, 5, NULL);       // Create I2S read task
}