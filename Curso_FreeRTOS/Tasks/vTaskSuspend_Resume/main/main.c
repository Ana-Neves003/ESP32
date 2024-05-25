#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
//#include "esp_log.h"


//Mapeamento de pinos
#define LED_PIN (GPIO_NUM_2)

//Variáeveis para armazenamento de handle das tasks
TaskHandle_t task1Handle_BLINK = NULL;
TaskHandle_t task2Handle_CONT = NULL;

//static const char* TAG = "vTaskDelete_BLINK_CONT";

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
    uint8_t cont = 0;

    while(1)
    {
        printf("Task CONT: %d\n", cont++);
        //ESP_LOGI(TAG, "Task CONT: %d\n", cont++);

        if(cont==10)
        {
             printf("Suspendendo a Task 1\n");
             gpio_set_level(LED_PIN, false);
             vTaskSuspend(task1Handle_BLINK);
        }

        else if(cont==15)
        {
           printf("Reiniciando a Task 1\n");
           vTaskResume(task1Handle_BLINK);
           cont = 0; 
        }

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
