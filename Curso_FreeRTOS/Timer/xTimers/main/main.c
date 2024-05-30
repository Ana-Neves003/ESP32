#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/gpio.h"

#define LED_PIN1    (GPIO_NUM_2)
#define LED_PIN2    (GPIO_NUM_14)
#define BUTTON_PIN    (GPIO_NUM_0)


TaskHandle_t xTask1;
TaskHandle_t xTimer1, xTimer2;

bool led_status1 = true;
bool led_status2 = true;



void callBackTimer1(TimerHandle_t xTimer)
{
    led_status1 = !led_status1;
    gpio_set_level(LED_PIN1, led_status1);
    vTaskDelay(pdMS_TO_TICKS(500));
}


void callBackTimer2(TimerHandle_t xTimer)
{
    led_status2 = !led_status2;
    gpio_set_level(LED_PIN2, led_status2);
    vTaskDelay(pdMS_TO_TICKS(500));
    xTimerStart(xTimer1, 0);
}

void vTask1(void *pvParameters)
{
    uint8_t debouncingTime = 0;
    while(1)
    {
        if((gpio_get_level(BUTTON_PIN) == 0) && (xTimerIsTimerActive(xTimer2) == pdFALSE)) 
        {
            debouncingTime++;
            if(debouncingTime >= 10) 
            {
                debouncingTime = 0;
                led_status2 = !led_status2;
                gpio_set_level(LED_PIN2, led_status2);
                xTimerStart(xTimer2, 0);
                xTimerStop(xTimer1, 0);
                printf("Iniciando o Timer 2 ... \n");
            }
        } else
        {
            debouncingTime = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}

void app_main(void)
{    
    gpio_pad_select_gpio(LED_PIN1);
    gpio_set_direction(LED_PIN1, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN1, led_status1);

    gpio_pad_select_gpio(LED_PIN2);
    gpio_set_direction(LED_PIN2, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN2, led_status2);

    gpio_pad_select_gpio(BUTTON_PIN);
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_intr_type(BUTTON_PIN, GPIO_INTR_NEGEDGE);
    gpio_install_isr_service(0);

    xTimer1 = xTimerCreate("TIMER 1", pdMS_TO_TICKS(1000), pdTRUE, 0, callBackTimer1);
    xTimer2 = xTimerCreate("TIMER 2", pdMS_TO_TICKS(10000), pdFALSE, 0, callBackTimer2);

    //xTaskCreate(vTask_BLINK, "Task Blink", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    xTaskCreate(vTask1, "Task 1", configMINIMAL_STACK_SIZE + 1024, NULL, 1, NULL);

    xTimerStart(xTimer1, 0);
}
