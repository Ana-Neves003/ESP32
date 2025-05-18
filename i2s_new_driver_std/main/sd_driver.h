#ifndef SD_DRIVER_H
#define SD_DRIVER_H

#include <stdio.h>

// === Pinos e configuração do SD Card (SPI) ===
#define MOUNT_POINT      "/sdcard"
#define SPI_DMA_CHAN     1

#define PIN_NUM_MISO     19
#define PIN_NUM_MOSI     23
#define PIN_NUM_CLK      18
#define PIN_NUM_CS       22

// === Prototipos ===
void sdcard_init(void);

// === Variáveis globais ===
extern FILE* audio_file;

#endif // SD_DRIVER_H
