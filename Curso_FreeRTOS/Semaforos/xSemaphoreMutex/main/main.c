/*
* Exemplo para demonstrar o uso de MUTEX para acesso
* a recursos e a inversão de prioridades
* Por Ana Neves
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"

#define LED_PIN (GPIO_NUM_2)

SemaphoreHandle_t Mutex;
TaskHandle_t vTask1Handle, vTask2Handle;

void vTask_BLINK(void *pvParameters)
{
    bool led_status = false;
    while(1)
    {
        gpio_set_level(LED_PIN, led_status);
        led_status = !led_status;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}


void vTask1(void *pvParameters)
{
    while(1)
    {
        xSemaphoreTake(Mutex, portMAX_DELAY);
        printf("Enviando informação da Task: 1\n");
        vTaskDelay(pdMS_TO_TICKS(2000));
        xSemaphoreGive(Mutex);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void vTask2(void *pvParameters)
{
    while(1)
    {
        xSemaphoreTake(Mutex, portMAX_DELAY);
        printf("Enviando informação da Task: 2\n");
        vTaskDelay(pdMS_TO_TICKS(500));
        xSemaphoreGive(Mutex);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}


void app_main(void)
{
    gpio_pad_select_gpio(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    Mutex = xSemaphoreCreateMutex();

    xTaskCreate(vTask_BLINK, "Task Blink", configMINIMAL_STACK_SIZE + 1024, NULL, 1, NULL);
    xTaskCreate(vTask1, "Task 1", configMINIMAL_STACK_SIZE + 1024, NULL, 2, &vTask1Handle);
    xTaskCreate(vTask2, "Task 2", configMINIMAL_STACK_SIZE + 1024, NULL, 4, &vTask2Handle);
}
