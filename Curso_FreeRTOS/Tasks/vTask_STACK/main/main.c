/*****************************************************************************
* Exemplo para verificação do consumo de memória pela tarefa - High Water Mark
* Por: Ana Neves
*****************************************************************************/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
//#include "esp_log.h"


//Mapeamento de pinos
#define LED_PIN_1 (GPIO_NUM_2)      //LED DO ESP32
#define LED_PIN_2 (GPIO_NUM_14)     //LED EXTERNO

//Variáeveis para armazenamento de handle das tasks
TaskHandle_t task1Handle_BLINK1 = NULL;
TaskHandle_t task2Handle_BLINK2 = NULL;
TaskHandle_t task3Handle_CONT = NULL;

//static const char* TAG = "vTaskDelete_BLINK_CONT";

//Protótipo das Tasks
//void vTask_BLINK(void *pvParameters);
//void vTask_CONT(void *pvParameters);

//Variáveis auxiliares
uint16_t valor = 500;

void vTask_BLINK(void *pvParameters)
{
    UBaseType_t uxHighWaterMark;
    bool led_status = false;
    gpio_num_t pin = *(gpio_num_t *)pvParameters;
    
    while (1)
    {
        led_status = !led_status;
        gpio_set_level(pin, led_status);
        vTaskDelay(pdMS_TO_TICKS(200)); 

        uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
        //printf("%s : %d\n", pcTaskGetTaskName(NULL), uxHighWaterMark);
    }
}

void vTask_CONT(void *pvParameters)
{
    UBaseType_t uxHighWaterMark;
    uint16_t cont = *(uint16_t *)pvParameters;

    while(1)
    {
        //printf("Task CONT: %d\n", cont++);
        //ESP_LOGI(TAG, "Task CONT: %d\n", cont++);
        vTaskDelay(pdMS_TO_TICKS(1000));

        uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
        printf("%s : %d\n", pcTaskGetTaskName(NULL), uxHighWaterMark);
    }
}

void app_main(void)
{
    static gpio_num_t led_pin_1 = LED_PIN_1;
    static gpio_num_t led_pin_2 = LED_PIN_2;

    gpio_pad_select_gpio(LED_PIN_1);
    gpio_pad_select_gpio(LED_PIN_2);
    gpio_set_direction(LED_PIN_1, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_PIN_2, GPIO_MODE_OUTPUT);

    xTaskCreatePinnedToCore(vTask_BLINK, "Task_BLINK 1", configMINIMAL_STACK_SIZE, &led_pin_1, 1, &task1Handle_BLINK1, APP_CPU_NUM);    //Núcleo 1
    //xTaskCreate(vTask_BLINK, "Task_BLINK 1", configMINIMAL_STACK_SIZE, &led_pin_1, 1, &task1Handle_BLINK1);
    xTaskCreatePinnedToCore(vTask_BLINK, "Task_BLINK 2", configMINIMAL_STACK_SIZE, &led_pin_2, 1, &task2Handle_BLINK2, APP_CPU_NUM);    //Núcleo 1
    //xTaskCreate(vTask_BLINK, "Task_BLINK 2", configMINIMAL_STACK_SIZE, &led_pin_2, 1, &task2Handle_BLINK2);
    xTaskCreatePinnedToCore(vTask_CONT, "Task_CONT", configMINIMAL_STACK_SIZE + 1024, &valor, 2, &task3Handle_CONT, PRO_CPU_NUM);       //Núcleo 0
    //xTaskCreate(vTask_CONT, "Task_CONT", configMINIMAL_STACK_SIZE + 1024, &valor, 2, &task3Handle_CONT);
}
