#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

#define GPIO_BUTTON    GPIO_NUM_0
#define PIN_NUM_MISO   GPIO_NUM_19
#define PIN_NUM_MOSI   GPIO_NUM_23
#define PIN_NUM_CLK    GPIO_NUM_18
#define PIN_NUM_CS     GPIO_NUM_22
#define MOUNT_POINT    "/sdcard"
#define SPI_DMA_CHAN   1

static const char *TAG = "sd_card_example";
static xQueueHandle gpio_evt_queue = NULL;

static void IRAM_ATTR button_isr_handler(void *arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static esp_err_t init_sd_card(void) {
    esp_err_t ret;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    ESP_LOGI(TAG, "Initializing SPI bus...");
    ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CHAN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus: %s", esp_err_to_name(ret));
        return ret;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    sdmmc_card_t *card;
    const esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    ESP_LOGI(TAG, "Mounting filesystem...");
    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount filesystem. If you want the card to be formatted, set format_if_mount_failed = true.");
        spi_bus_free(host.slot);
        return ret;
    }

    sdmmc_card_print_info(stdout, card);
    return ESP_OK;
}

void app_main(void) {
    esp_err_t ret;
    uint32_t io_num;
    uint32_t counter = 0;
    FILE *f;

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_POSEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << GPIO_BUTTON),
        .pull_down_en = 0,
        .pull_up_en = 1,
    };
    gpio_config(&io_conf);

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_BUTTON, button_isr_handler, (void *) GPIO_BUTTON);

    if (init_sd_card() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SD card");
        return;
    }

    while (1) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            counter++;
            ESP_LOGI(TAG, "Button pressed. Counter: %d", counter);

            f = fopen(MOUNT_POINT "/counter.txt", "a");
            if (f == NULL) {
                ESP_LOGE(TAG, "Failed to open file for writing");
                return;
            }
            fprintf(f, "Counter: %d\n", counter);
            fclose(f);
            ESP_LOGI(TAG, "Counter value written to file");
        }
    }
}
