#ifndef MAIN_H
#define MAIN_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2s.h"
#include "esp_log.h"
#include "esp_system.h"

// Definições de pinos
#define I2S_SAMPLE_RATE     (44100)
#define I2S_NUM             (0)
#define I2S_BCK_IO          (GPIO_NUM_21)
#define I2S_DATA_IN_IO      (GPIO_NUM_4)
#define GPIO_BUTTON         (GPIO_NUM_0)
#define BIT_0               (1 << 0)

// Funções externas
//void i2s_setup(void);
//esp_err_t init_sd_card(void);

void vTaskSTART(void *pvParameters);
void vTaskEND(void *pvParameters);
void vTaskREC(void *pvParameters);
void vTaskBUTTON(void *pvParameters);


#endif // MAIN_H
