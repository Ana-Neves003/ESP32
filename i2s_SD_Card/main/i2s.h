#ifndef I2S_H
#define I2S_H

#include "driver/i2s.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

// Definição de pinos
#define I2S_SAMPLE_RATE     (44100)
#define I2S_PORT_NUM        (0)
#define I2S_BCK_IO          (GPIO_NUM_21)
#define I2S_DATA_IN_IO      (GPIO_NUM_4)
#define DMA_BUF_COUNT       (64)
#define DMA_BUF_LEN         (1024)

extern QueueHandle_t xQueueData;
extern EventGroupHandle_t xEvents;

void i2s_setup(void);
void i2s_pause(void);
void i2s_resume(void);

//void vTaskSTART(void *pvParameters);
//void vTaskEND(void *pvParameters);
//void vTaskREC(void *pvParameters);


#endif // I2S_H
