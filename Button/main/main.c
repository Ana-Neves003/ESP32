#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define ESP_INR_FLAG_DEFAULT 0
#define CONFIG_BUTTON_PIN 0
#define CONFIG_LED_PIN 2

TaskHandle_t ISR = NULL;

void IRAM_ATTR button_isr_handler(void *arg)
{
    xTaskResumeFromISR(ISR);
}

void button_task(void *arg)
{
    bool led_status = false;

    while(1){
        vTaskSuspend(NULL);
        led_status = !led_status;
        gpio_set_level(CONFIG_LED_PIN,led_status);
        printf("Button pressed!\n"); 
    }
}



void app_main()
{
    gpio_pad_select_gpio(CONFIG_BUTTON_PIN);
    gpio_pad_select_gpio(CONFIG_LED_PIN);

    //Set the correct direction
    gpio_set_direction(CONFIG_BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(CONFIG_LED_PIN, GPIO_MODE_OUTPUT);

    //Enable interrupt on falling (1 -> 0) edge for button pin
    gpio_set_intr_type(CONFIG_BUTTON_PIN, GPIO_INTR_NEGEDGE);

    //Install the driver's GPIO ISR handler service, which allows per-pin GPIO interrupt handlers
    //Install ISR service with default configuration
    gpio_install_isr_service(ESP_INR_FLAG_DEFAULT);

    //Attach the interrupt service routine
    gpio_isr_handler_add(CONFIG_BUTTON_PIN, button_isr_handler,NULL);

    //Create and star stats task
    xTaskCreate(button_task, "button_task", 4096, NULL, 10, &ISR);

}