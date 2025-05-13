#include "sd_driver.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"

#define MOUNT_POINT "/sdcard"
#define SPI_DMA_CHAN 1
#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   22

static const char *TAG = "SD_DRIVER";
static bool sd_initialized = false;


bool sd_is_mounted(){
    return sd_initialized;
}

void sd_mount() {
    if (sd_initialized) {
        ESP_LOGW(TAG, "Cartão SD já montado.");
        return;
    }

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

    esp_err_t ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CHAN);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Falha ao inicializar o barramento SPI (%s)", esp_err_to_name(ret));
        return;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5
    };

    sdmmc_card_t* card;
    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao montar o SD card (%s)", esp_err_to_name(ret));
        return;
    }

    sd_initialized = true;
    ESP_LOGI(TAG, "SD card montado com sucesso.");
}

FILE* sd_open_file(void) {
    if (!sd_initialized) {
        ESP_LOGW(TAG, "Cartão SD não está montado.");
        return NULL;
    }

    FILE* file = fopen(MOUNT_POINT "/audio.raw", "ab+");
    if (file) {
        ESP_LOGI(TAG, "Arquivo aberto no modo append.");
    } else {
        ESP_LOGE(TAG, "Erro ao abrir/criar arquivo para gravação.");
    }
    return file;
}

