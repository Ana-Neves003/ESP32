#include <stdio.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "driver/i2s_std.h"

#define SAMPLE_RATE_STD     44100
#define SAMPLE_RATE_ULT      78125
#define DMA_BUF_COUNT        16
#define DMA_BUF_LEN_SMPL     511

#define BIT_DEPTH_STD        I2S_DATA_BIT_WIDTH_16BIT
#define BIT_DEPTH_ULT        I2S_DATA_BIT_WIDTH_32BIT
#define DATA_BUFFER_SIZE     (DMA_BUF_LEN_SMPL *2* BIT_DEPTH_ULT / 8)

#define MOUNT_POINT      "/sdcard"
#define SPI_DMA_CHAN     1

#define PIN_NUM_MISO     19
#define PIN_NUM_MOSI     23
#define PIN_NUM_CLK      18
#define PIN_NUM_CS       22

// Pinos do microfone PDM
#define MIC_CLOCK_PIN     GPIO_NUM_21
#define MIC_DATA_PIN      GPIO_NUM_4

#define I2S_PORT_NUM    I2S_NUM_0

static const char *TAG = "I2S_SD";


FILE* audio_file = NULL;
i2s_chan_handle_t rx_handle;
uint8_t dataBuffer[DATA_BUFFER_SIZE];
QueueHandle_t xQueue;


// Inicialização do cartão SD 
void sdcard_init() {
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;
    host.flags = SDMMC_HOST_FLAG_SPI;
    //host.max_freq_khz = SDMMC_FREQ_DEFAULT;
    host.max_freq_khz = 30000; // O cartão opera com segurança até 30 MHz

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

    // Mostra a velocidade máxima suportada pelo cartão SD
    //sdmmc_card_print_info(stdout, card);
    //ESP_LOGI(TAG, "Velocidade máxima declarada pelo cartão (kbps): %d", card->csd.tr_speed);

    audio_file = fopen(MOUNT_POINT "/audio.raw", "wb");
    if (!audio_file) {
        ESP_LOGE(TAG, "Erro ao abrir arquivo para gravação.");
        return;
    }
}

void i2s_init() {
    i2s_chan_config_t rx_channel_config = {
        .id = I2S_PORT_NUM,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = DMA_BUF_COUNT,
        .dma_frame_num = DMA_BUF_LEN_SMPL,
        .auto_clear = true
    };

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = SAMPLE_RATE_STD,
            .clk_src = I2S_CLK_SRC_APLL,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256
        },
        .slot_cfg = {
            .data_bit_width = BIT_DEPTH_STD,
            .slot_bit_width = BIT_DEPTH_STD,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = BIT_DEPTH_STD,
            .ws_pol = false,
            .bit_shift = true
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_GPIO_UNUSED,
            .ws   = MIC_CLOCK_PIN,
            .dout = I2S_GPIO_UNUSED,
            .din  = MIC_DATA_PIN
        }
    };

    ESP_ERROR_CHECK(i2s_new_channel(&rx_channel_config, NULL, &rx_handle));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
}

void app_main(void) {
    size_t bytes_read = 0;

    sdcard_init();
    if (!audio_file) {
        ESP_LOGE(TAG, "Arquivo não foi criado. Encerrando.");
        return;
    }

    ESP_LOGI(TAG, "Inicializando I2S...");
    i2s_init();

    ESP_LOGI(TAG, "Leitura do I2S...");
    i2s_channel_read(rx_handle, dataBuffer, DATA_BUFFER_SIZE, &bytes_read, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    ESP_LOGI(TAG, "Escrita do SD...");
    fwrite(dataBuffer, 1, bytes_read, audio_file); 
    fflush(audio_file);     
    fsync(fileno(audio_file));  


    ESP_LOGI(TAG, "2 Leitura do I2S...");
    i2s_channel_read(rx_handle, dataBuffer, DATA_BUFFER_SIZE, &bytes_read, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI(TAG, "Escrita do SD...");
    fwrite(dataBuffer, 1, bytes_read, audio_file); 
    fflush(audio_file);     
    fsync(fileno(audio_file));  
    fclose(audio_file);

    ESP_LOGI(TAG, "Desabilita canal");
    i2s_channel_disable(rx_handle);
    ESP_LOGI(TAG, "Deleta canal");
    i2s_del_channel(rx_handle);

}
