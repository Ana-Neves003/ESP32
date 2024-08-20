#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/i2s.h"
#include "sd_driver.c"

// ------------ Definições de Logs ----------------------------

#define SD_CARD_TAG    "sd_card"
#define MEMS_MIC_TAG   "mems_mic"
#define START_REC_TAG  "start_rec"
#define END_REC_TAG    "end_rec"
#define SETUP_APP_TAG  "setup_app"

// ----Definições relacionados ao I2S e ao tempo de gravação----

#define SAMPLE_RATE         (44100)
#define I2S_PORT_NUM        (0)
#define MIC_CLOCK_PIN       (GPIO_NUM_21)
#define MIC_DATA_PIN        (GPIO_NUM_4)
#define DMA_BUF_COUNT       (64)
#define DMA_BUF_LEN_SMPL    (1024)
#define BIT_DEPTH I2S_BITS_PER_SAMPLE_16BIT
#define DATA_BUFFER_SIZE (DMA_BUF_LEN_SMPL * BIT_DEPTH / 8)
#define BIT_(shift) (1<<shift)

//#define MIC_CLOCK_PIN  GPIO_NUM_21  // gpio 21 - MEMS MIC clock in
//#define MIC_DATA_PIN   GPIO_NUM_4   // gpio 4  - MEMS MIC data out
#define BTN_START_END  GPIO_NUM_0   // gpio 0  - button
#define GPIO_OUTPUT_IO GPIO_NUM_16  // gpio 16 - no use
#define GPIO_INPUT_PIN_SEL1   (1ULL<<BTN_START_END)  // | (1ULL<<ANOTHER_GPIO)
#define ESP_INTR_FLAG_DEFAULT 0

enum events{REC_STARTED};

// -----------------------Declaração das funções-----------------

void i2s_setup(void * pvParameters);
void vTaskREC(void * pvParameters);
void vTaskSTART(void * pvParameters);
void vTaskEND(void * pvParameters);
static void IRAM_ATTR ISR_BTN();

//---------------------------------------------------------------



//-------------------------Variáveis do FreeRTOS-----------------

TaskHandle_t xTaskRECHandle;   
TaskHandle_t xTaskSTARThandle; 
TaskHandle_t xTaskENDhandle;
QueueHandle_t xQueueData;            
EventGroupHandle_t xEvents;          
SemaphoreHandle_t xMutex;            
SemaphoreHandle_t xSemaphoreBTN_ON;  
SemaphoreHandle_t xSemaphoreBTN_OFF; 
SemaphoreHandle_t xSemaphoreTimer;

size_t bytes_read;
char dataBuffer[DATA_BUFFER_SIZE];


//----------------Configuração do I2S------------------------------

/*

i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM,
        .sample_rate = 31250,
        .bits_per_sample = BIT_DEPTH,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN_SMPL,
        .use_apll = I2S_CLK_APLL,
};

i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_PIN_NO_CHANGE,
        .ws_io_num = MIC_CLOCK_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = MIC_DATA_PIN
};


*/

//----------------Configuração do GPIO------------------------------

// Config pino de entrada - button (GPIO0 commanded by BOOT button)

gpio_config_t in_button = {
    .intr_type    = GPIO_INTR_POSEDGE,   // interrupt on rising edge
    .mode         = GPIO_MODE_INPUT,     // set as input mode
    .pin_bit_mask = GPIO_INPUT_PIN_SEL1, // bit mask of pins to set (GPIO00)
    .pull_down_en = 1,                   // enable pull-down mode
    .pull_up_en   = 0,                   // disable pull-up mode
};

struct timeval date = {// struct with date data
    .tv_sec = 0, // current date in seconds (to be fecthed from NTP)
};

#endif 


