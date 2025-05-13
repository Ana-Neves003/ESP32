#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/i2s_pdm.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define MIC_CLOCK_PIN     GPIO_NUM_21
#define MIC_DATA_PIN      GPIO_NUM_4
#define SAMPLE_RATE       44100
#define BITS_PER_SAMPLE   16
#define DMA_BUF_COUNT     64
#define DMA_BUF_LEN_SMPL  1024
#define I2S_PORT          I2S_NUM_0
#define DATA_BUFFER_SIZE  (DMA_BUF_LEN_SMPL * BITS_PER_SAMPLE / 8)

// Botão de gravação (BOOT)
#define BUTTON_GPIO       GPIO_NUM_0

// Variáveis globais
extern uint8_t dataBuffer[DATA_BUFFER_SIZE];
extern i2s_chan_handle_t rx_handle;
extern FILE* audio_file;
extern SemaphoreHandle_t xButtonSemaphore;
extern SemaphoreHandle_t xFileMutex;
extern volatile bool is_recording;

// Funções
void i2s_init(void);
void task_gerenciar_gravacao(void *pvParameters);
void task_gravar_audio(void *pvParameters);

#endif // MAIN_H
