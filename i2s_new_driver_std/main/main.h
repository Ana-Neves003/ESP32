#ifndef MAIN_H
#define MAIN_H

#include "driver/i2s_std.h"

// Pinos do microfone PDM
#define MIC_CLOCK_PIN     GPIO_NUM_21
#define MIC_DATA_PIN      GPIO_NUM_4

// Configurações I2S
#define I2S_PORT_NUM         I2S_NUM_0
#define DMA_BUF_COUNT        32
#define DMA_BUF_LEN_SMPL     1024

#define SAMPLE_RATE_STD      44100
#define BIT_DEPTH_STD        I2S_DATA_BIT_WIDTH_16BIT

#define SAMPLE_RATE_ULT      78125
#define BIT_DEPTH_ULT        I2S_DATA_BIT_WIDTH_32BIT

#define DATA_BUFFER_SIZE     (DMA_BUF_LEN_SMPL * BIT_DEPTH_ULT / 8)

// Prototótipos
void i2s_init(bool modo_padrao);
void i2s_read_task(void *pvParameters);

extern i2s_chan_handle_t rx_handle;
extern uint8_t dataBuffer[DATA_BUFFER_SIZE];

#endif // MAIN_H
