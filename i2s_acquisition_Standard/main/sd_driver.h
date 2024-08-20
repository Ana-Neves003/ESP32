#ifndef SD_DRIVER_H
#define SD_DRIVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#define SD_DRIVER_TAG "sd_driver"
#define INIT_SPI_TAG   "init_spi"
#define DEINIT_SPI_TAG "deinit_spi"
#define INIT_SD_TAG    "init_sd"
#define DEINIT_SD_TAG  "deinit_sd"

sdmmc_card_t* card;
sdmmc_host_t host;
FILE* session_file = NULL;

const char mount_point[] = MOUNT_POINT;
const char* fname = "rec.raw";


//esp_err_t init_sd_card(void);

char* merge_filename(const char *filename);
FILE* open_file(const char *filename, char *mode);
void close_file(FILE **file);
void rename_file(const char *actualfname, const char *targetfname);
//int initialize_spi_bus(void);
//int initialize_spi_bus(sdmmc_host_t* host);
int deinitialize_spi_bus(sdmmc_host_t* host);
//int initialize_sd_card(void);
//int initialize_sd_card(sdmmc_host_t* host, sdmmc_card_t** card);
int deinitialize_sd_card(sdmmc_card_t** card);
static esp_err_t init_sd_card(void);



#endif // SD_DRIVER_H
