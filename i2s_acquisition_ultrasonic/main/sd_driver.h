#ifndef SD_DRIVER_H
#define SD_DRIVER_H

#include "esp_err.h"
#include "esp_log.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

#define PIN_NUM_MISO   GPIO_NUM_19
#define PIN_NUM_MOSI   GPIO_NUM_23
#define PIN_NUM_CLK    GPIO_NUM_18
#define PIN_NUM_CS     GPIO_NUM_22
#define MOUNT_POINT    "/sdcard"
#define SPI_DMA_CHAN   1

esp_err_t init_sd_card(void);

#endif // SD_DRIVER_H
