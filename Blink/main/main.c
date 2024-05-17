#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define CONFIG_LED_PIN 2

void blink_led(void *arg)
{
    bool led_status = false;
    
    while (1)
    {
        led_status = !led_status;
        gpio_set_level(CONFIG_LED_PIN, led_status);
        printf("LED %s", led_status ? "ON\n" : "OFF\n");
        vTaskDelay(pdMS_TO_TICKS(1000)); //Aguarda 1 segundo
    }
    
}

void app_main(void)
{
    gpio_pad_select_gpio(CONFIG_LED_PIN);
    gpio_set_direction(CONFIG_LED_PIN, GPIO_MODE_OUTPUT);
    
    xTaskCreate(blink_led, "blink_led", 4096, NULL, 5, NULL);

}
