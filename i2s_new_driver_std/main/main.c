#include <stdio.h>
#include <unistd.h> // fsync
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"

// Pinos do microfone PDM
#define MIC_CLOCK_PIN  GPIO_NUM_21
#define MIC_DATA_PIN   GPIO_NUM_4

// Configurações I2S
#define SAMPLE_RATE_STD  44100
#define SAMPLE_RATE_ULT  78125
#define BITS_PER_SAMPLE  32
#define DMA_BUF_COUNT    64 
#define DMA_BUF_LEN_SMPL 1024
#define DATA_BUFFER_SIZE (DMA_BUF_LEN_SMPL * BITS_PER_SAMPLE / 8) // 2048

// Pinos e configuração do SD
#define MOUNT_POINT     "/sdcard"
#define SPI_DMA_CHAN    1
#define PIN_NUM_MISO    19
#define PIN_NUM_MOSI    23
#define PIN_NUM_CLK     18
#define PIN_NUM_CS      22


static const char *TAG = "I2S_STD";
uint8_t dataBuffer[DATA_BUFFER_SIZE];
i2s_chan_handle_t rx_handle;
FILE* audio_file = NULL;

void i2s_init(uint32_t sample_rate)
{
    
    ESP_LOGI(TAG, "Initializing I2S...");
    i2s_chan_config_t rx_channel_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);    // I2S channel configuration
    ESP_ERROR_CHECK(i2s_new_channel(&rx_channel_config,NULL, &rx_handle));                         // Create a new I2S channel 

    i2s_std_config_t rx_std_config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
        /*
        .clk_cfg= {
            .sample_rate_hz = sample_rate,
            .clk_src = I2S_CLK_SRC_APLL,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256
        },
        */
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(BITS_PER_SAMPLE,I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,    // MCLK is not used in this example
            .bclk = MIC_CLOCK_PIN,        // BCLK pin
            .ws = I2S_GPIO_UNUSED,           // WS pin
            .dout = I2S_GPIO_UNUSED,    // DOUT is not used in this example
            .din = MIC_DATA_PIN           // SD pin
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &rx_std_config));    // Initialize I2S channel in standard mode
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));                           // Enable I2S channel
    ESP_LOGI(TAG, "I2S initialized");
}

void sdcard_init() {
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
        ESP_LOGE(TAG, "Erro ao abrir arquivo para gravação.");
    } else {
        ESP_LOGI(TAG, "Arquivo aberto com sucesso.");
    }
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

            //ESP_LOGI(TAG, "Gravando %d bytes no SD", bytes_read);

            if (audio_file) {
                fwrite(dataBuffer, 1, bytes_read, audio_file);
                fflush(audio_file);
                fsync(fileno(audio_file));

            }
    }
}

void app_main(void)
{

    vTaskDelay(pdMS_TO_TICKS(500));                                 // Delay to open the serial port
    
    i2s_init(SAMPLE_RATE_STD);                                                             // Initialize I2S
    vTaskDelay(pdMS_TO_TICKS(500));

    sdcard_init();
    vTaskDelay(pdMS_TO_TICKS(500));

    i2s_channel_disable(rx_handle);
    i2s_del_channel(rx_handle);
    vTaskDelay(pdMS_TO_TICKS(500));

    i2s_init(SAMPLE_RATE_ULT);
    vTaskDelay(pdMS_TO_TICKS(500));

    xTaskCreate(i2s_read_task, "i2s_read_task", 4096, NULL, 5, NULL);       // Create I2S read task
}