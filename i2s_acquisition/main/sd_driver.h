/*
 * Include section
 * --------------------
 * Importing all necessary libraries for the good maintenance of the code
 *
 *  C_POSIX_LIB_INCLUDED:         standard functionalities
 *  ESP_MANAGEMENT_LIBS_INCLUDED: log messages, error codes and SD filesystem support
 */

#ifndef ESP_MANAGEMENT_LIBS_INCLUDED
    #define ESP_MANAGEMENT_LIBS_INCLUDED
    #include "esp_err.h" // error codes and helper functions
    #include "esp_log.h" // logging library
    #include "esp_vfs_fat.h" // FAT filesystem support
#endif

#ifndef C_POSIX_LIB_INCLUDED
    #define C_POSIX_LIB_INCLUDED
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <sys/unistd.h>
    #include <sys/stat.h>
#endif //C_POSIX_LIB_INCLUDED

#define SD_DRIVER_TAG "sd_driver"
#define INIT_SPI_TAG   "init_spi"
#define DEINIT_SPI_TAG "deinit_spi"
#define INIT_SD_TAG    "init_sd"
#define DEINIT_SD_TAG  "deinit_sd"

// sd card
#define MOUNT_POINT "/sdcard" // SD card mounting directory
#define SPI_DMA_CHAN 1        // DMA channel to be used by the SPI peripheral

// spi bus
#ifndef USE_SPI_MODE
    #define USE_SPI_MODE    // define SPI mode
    #define PIN_NUM_MISO 19 // SDI - Serial Data In
    #define PIN_NUM_MOSI 23 // SDO - Serial Data Out
    #define PIN_NUM_CLK  18 // System clock
    #define PIN_NUM_CS   22 // Chip select
#endif

#ifndef _SD_DRIVER_H_
#define _SD_DRIVER_H_

/*
 * Function prototype section
 * --------------------
 * Initialize functions prototypes to later be defined
 */

char* merge_filename(const char *filename);
FILE* open_file(const char *filename, char *mode);
void close_file(FILE **file);
void rename_file(const char *actualfname, const char *targetfname);
int initialize_spi_bus(sdmmc_host_t* host);
int deinitialize_spi_bus(sdmmc_host_t* host);
int initialize_sd_card(sdmmc_host_t* host, sdmmc_card_t** card);
int deinitialize_sd_card(sdmmc_card_t** card);


#endif  // _SD_DRIVER_H_