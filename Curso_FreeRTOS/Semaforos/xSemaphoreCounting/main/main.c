/***************************************************
* Exemplo para demonstrar o uso de semáforo contador 
* dentro de uma ISR
* Por Ana Neves
***************************************************/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"

#define LED_PIN     (GPIO_NUM_2)
#define BUTTON_PIN  (GPIO_NUM_0)

SemaphoreHandle_t xSemaforoContador;
TaskHandle_t xTaskButtonHandle;

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

void vTask_BUTTON(void *pvParameters)
{
    UBaseType_t x;

    while(1)
    {
        xSemaphoreTake(xSemaforoContador, portMAX_DELAY);
        printf("Tratando a ISR do BUTTON: ");
        x = uxSemaphoreGetCount(xSemaforoContador);
        printf("%d\n", x);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void ISR_BUTTON(void *pvParameters)
{
    BaseType_t xHighPriorityTaskWoken = pdTRUE;

    xSemaphoreGiveFromISR(xSemaforoContador, &xHighPriorityTaskWoken);

    if(xHighPriorityTaskWoken == pdTRUE)
    {
        portYIELD_FROM_ISR();
    }
}

void app_main(void)
{
    gpio_pad_select_gpio(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    gpio_pad_select_gpio(BUTTON_PIN);
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);

    gpio_set_intr_type(BUTTON_PIN, GPIO_INTR_NEGEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_PIN, ISR_BUTTON, NULL);

    xSemaforoContador = xSemaphoreCreateCounting(255,0);


    xTaskCreate(vTask_BLINK, "Task Blink", configMINIMAL_STACK_SIZE + 1024, NULL, 1, NULL);
    xTaskCreate(vTask_BUTTON, "Task Button", configMINIMAL_STACK_SIZE + 1024, NULL, 3, &xTaskButtonHandle);
}
