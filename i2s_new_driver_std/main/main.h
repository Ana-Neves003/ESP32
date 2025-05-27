#ifndef MAIN_H
#define MAIN_H

#include "driver/i2s_std.h"
#include "freertos/queue.h"

// Pinos do microfone PDM
#define MIC_CLOCK_PIN     GPIO_NUM_21
#define MIC_DATA_PIN      GPIO_NUM_4

// Configurações I2S
#define I2S_PORT_NUM         I2S_NUM_0
//DMA_BUF_COUNT → é o número de buffers que o driver I2S do ESP32 vai criar e gerenciar 
//para realizar a transferência automática de dados da interface I2S para a memória RAM
//através da DMA (Direct Memory Access).
#define DMA_BUF_COUNT        16
//DMA_BUF_LEN_SMPL → é o número de amostras que cada buffer individual irá armazenar
#define DMA_BUF_LEN_SMPL     511 
//#define DMA_BUF_LEN_SMPL     1023

#define SAMPLE_RATE_STD      44100
#define BIT_DEPTH_STD        I2S_DATA_BIT_WIDTH_16BIT

#define SAMPLE_RATE_ULT      78125
#define BIT_DEPTH_ULT        I2S_DATA_BIT_WIDTH_32BIT
//#define BIT_DEPTH_ULT        I2S_DATA_BIT_WIDTH_16BIT

#define DATA_BUFFER_SIZE     (DMA_BUF_LEN_SMPL *2* BIT_DEPTH_ULT / 8)

#define ACQUISITION_TIME_MS 10000  


// Prototótipos
void i2s_init(bool modo_padrao);
void i2s_read_task(void *pvParameters);
void sd_write_task(void *pvParameters);
void stop_i2s();


extern i2s_chan_handle_t rx_handle;
extern uint8_t dataBuffer[DATA_BUFFER_SIZE];
extern QueueHandle_t xQueue;

#endif // MAIN_H
