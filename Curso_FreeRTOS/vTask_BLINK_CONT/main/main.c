#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"


//Mapeamento de pinos
#define LED_PIN (GPIO_NUM_2)

//Variáeveis para armazenamento de handle das tasks
TaskHandle_t task1Handle_BLINK = NULL;
TaskHandle_t task2Handle_CONT = NULL;

//Protótipo das Tasks
//void vTask_BLINK(void *pvParameters);
//void vTask_CONT(void *pvParameters);

void vTask_BLINK(void *pvParameters)
{
    bool led_status = false;
    
    while (1)
    {
        led_status = !led_status;
        gpio_set_level(LED_PIN, led_status);
        vTaskDelay(pdMS_TO_TICKS(200)); 
    }
}

void vTask_CONT(void *pvParameters)
{
    int cont = 0;

    while(1)
    {
        printf("Task CONT: %d\n", cont++);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    gpio_pad_select_gpio(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    xTaskCreate(vTask_BLINK, "Task_BLINK", configMINIMAL_STACK_SIZE, NULL, 1, &task1Handle_BLINK);
    xTaskCreate(vTask_CONT, "Task_CONT", configMINIMAL_STACK_SIZE + 1024, NULL, 2, &task2Handle_CONT);
}
