#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
//#include "esp_log.h"

#define LED 2

QueueHandle_t xFila;

TaskHandle_t xTask1Handle_CONT, xTask2Handle_FILA, xTask3Handle_BLINK;

void vTask_CONT(void *pvParameters)
{
    uint16_t cont = 0;

    while(1)
    {
        if(cont<10)
        {
            xQueueSend(xFila, &cont, portMAX_DELAY);
            cont++;
        }else
        {
            cont = 0;
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void vTask_FILA(void *pvParameters)
{
    uint16_t valorRecebido = 0;

    while(1)
    {
        if(xQueueReceive(xFila, &valorRecebido, pdMS_TO_TICKS(1000)) == pdTRUE)
        {
            printf("Valor Recebido: %d\n", valorRecebido);
        }else
        {
            printf("TIMEOUT\n");
        }
    }
}

void vTask_BLINK(void *pvParameters)
{
    bool led_status = false;

    while (1)
    {
        gpio_set_level(LED, led_status);
        led_status = !led_status;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
}

void app_main(void)
{
    BaseType_t xReturned;

    gpio_pad_select_gpio(LED);
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);

    xFila = xQueueCreate(5, sizeof(uint16_t));

    if(xFila == NULL)
    {
        printf("Nao foi possive criar uma fila\n");
        while(1);
    }

    xReturned = xTaskCreate(vTask_CONT, "Task1", configMINIMAL_STACK_SIZE + 1024, NULL, 1, &xTask1Handle_CONT);

    if(xReturned == pdFAIL)
    {
        printf("Nao foi possivel criar a Task 1\n");
        while(1);
    }

    xReturned = xTaskCreate(vTask_FILA, "Task2", configMINIMAL_STACK_SIZE + 1024, NULL, 1, &xTask2Handle_FILA);

    if(xReturned == pdFAIL)
    {
        printf("Nao foi possivel criar a Task 2\n");
        //ESP_LOGE("xQueue_CONT","Nao foi possivel criar a Task 2\n");
        while(1);
    }

    xReturned = xTaskCreate(vTask_BLINK, "Task3", configMINIMAL_STACK_SIZE, NULL, 1, &xTask3Handle_BLINK);

    if(xReturned == pdFAIL)
    {
        printf("Nao foi possivel criar a Task 3\n");
        //ESP_LOGE("xQueue_CONT","Nao foi possivel criar a Task 3\n");
        while(1);
    }
}

