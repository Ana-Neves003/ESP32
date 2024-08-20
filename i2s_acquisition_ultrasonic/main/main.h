#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/i2s.h"
#include "sd_driver.h"

// Defines relacionados ao I2S e ao tempo de gravação
#define I2S_SAMPLE_RATE     (8000)
#define ULTRASONIC_SAMPLE_RATE  (44100)
#define I2S_NUM             (0)
#define MIC_CLOCK_PIN       (GPIO_NUM_21)
#define MIC_DATA_PIN        (GPIO_NUM_4)
#define DMA_BUF_COUNT       (64)
#define DMA_BUF_LEN_SMPL    (1024)
#define RECORDING_TIME_SECONDS (10)

#define BIT_DEPTH I2S_BITS_PER_SAMPLE_16BIT
#define DATA_BUFFER_SIZE (DMA_BUF_LEN_SMPL * BIT_DEPTH / 8)

// Declaração das funções
void i2s_task(void *pvParameter);
void i2s_setup_standard_mode(void);
void i2s_setup_ultrasonic_mode(void);

TickType_t recording_start_time;
TickType_t recording_duration_ticks;
TaskHandle_t vTask1Handle = NULL;
bool ultrasonic_mode_enabled = false;

#endif // MAIN_H


