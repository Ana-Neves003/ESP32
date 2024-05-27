/********************************************************
* Exemplo que demonstra como enviar valores para uma fila 
* a partir de uma ISR 
* por: Ana Neves
********************************************************/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include <inttypes.h>

#define LED_PIN     (GPIO_NUM_2)
#define BUTTON_PIN  (GPIO_NUM_0)

QueueHandle_t xFila;
TaskHandle_t vTask1Handle,vTask2Handle;


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
    uint16_t valorRecebido;

    while(1)
    {
        xQueueReceive(xFila, &valorRecebido, portMAX_DELAY);
        printf("Botao pressionado: %d\n", valorRecebido);
    }
}

void trataISR_BUTTON(void *pvParameters)
{
    static uint16_t valor;
    valor++;
    xQueueSendFromISR(xFila,&valor,NULL);
}

void app_main(void)
{
    gpio_pad_select_gpio(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    gpio_pad_select_gpio(BUTTON_PIN);
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);

    gpio_set_intr_type(BUTTON_PIN, GPIO_INTR_NEGEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_PIN, trataISR_BUTTON, NULL);

    xFila = xQueueCreate(5, sizeof(uint16_t));

    xTaskCreate(vTask_BLINK, "Task BLINK", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    xTaskCreate(vTask1, "Task1", configMINIMAL_STACK_SIZE +1024, NULL, 1, &vTask2Handle);
}
