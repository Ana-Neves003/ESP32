#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2s.h"
#include "sd_driver.h"

// Defines relacionados ao I2S e ao tempo de gravação
#define I2S_SAMPLE_RATE     (31250)
#define I2S_NUM             (0)
#define MIC_CLOCK_PIN       (GPIO_NUM_21)
#define MIC_DATA_PIN        (GPIO_NUM_4)
#define DMA_BUF_COUNT       (64)
#define DMA_BUF_LEN_SMPL    (1024)
#define RECORDING_TIME_SECONDS (50)

#define BIT_DEPTH I2S_BITS_PER_SAMPLE_16BIT
#define DATA_BUFFER_SIZE (DMA_BUF_LEN_SMPL * BIT_DEPTH / 8)

// Declaração das funções
void i2s_task(void *pvParameter);
void i2s_setup(void);

extern TickType_t recording_start_time;
extern TickType_t recording_duration_ticks;

#endif // MAIN_H


