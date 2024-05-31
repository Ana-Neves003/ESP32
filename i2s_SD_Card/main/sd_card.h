#ifndef SD_CARD_H
#define SD_CARD_H

#include "driver/gpio.h"
#include "esp_err.h"

#define PIN_NUM_MISO        (GPIO_NUM_19)
#define PIN_NUM_MOSI        (GPIO_NUM_23)
#define PIN_NUM_CLK         (GPIO_NUM_18)
#define PIN_NUM_CS          (GPIO_NUM_22)
#define MOUNT_POINT         "/sdcard/test.txt"
#define SPI_DMA_CHAN        (1)

esp_err_t init_sd_card(void);

#endif // SD_CARD_H
