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

// Define variáveis relacionadas ao I2S e ao tempo de gravação
#define I2S_SAMPLE_RATE_STD         (80000)
#define SAMPLE_RATE_ULT_PDM         (312500) // sample rate used in i2s hack to record raw pdm during recording in ultrasonic mode
#define I2S_NUM                     (0)
#define MIC_CLOCK_PIN               (GPIO_NUM_21)
#define MIC_DATA_PIN                (GPIO_NUM_4)
#define DMA_BUF_COUNT               (32)
#define DMA_BUF_LEN_SMPL            (1024)
#define RECORDING_TIME_SECONDS      (20)
#define OSR                         (16) // pdm oversampling rate in ultrasound mode

#define BIT_DEPTH_STD I2S_BITS_PER_SAMPLE_16BIT
#define BIT_DEPTH_ULT I2S_BITS_PER_SAMPLE_32BIT
#define DATA_BUFFER_SIZE_STD (DMA_BUF_LEN_SMPL * BIT_DEPTH_STD / 8)
#define DATA_BUFFER_SIZE_ULT (DMA_BUF_LEN_SMPL * BIT_DEPTH_ULT / 8)
#define I2S_CLOCK_RATE       SAMPLE_RATE_ULT_PDM*2*BIT_DEPTH_ULT // i2s clock rate for ultrasonic mode (API adjusted to 5MHz)
#define SAMPLE_RATE_ULT_PCM  I2S_CLOCK_RATE/OSR                  // sample rate of resulting pcm audio after pdm2pcm software conversion
    


// Declaração das funções
void i2s_task(void *pvParameter);
void i2s_setup(i2s_config_t *i2s_config, i2s_pin_config_t *pin_config);
void i2s_setup_ult(i2s_config_t *i2s_config, i2s_pin_config_t *pin_config);
//void i2s_setup_ult(i2s_config_t *i2s_config);

TickType_t recording_start_time;
TickType_t recording_duration_ticks;
TaskHandle_t vTask1Handle = NULL;
i2s_config_t i2s_config;
i2s_pin_config_t pin_config;

#endif // MAIN_H

