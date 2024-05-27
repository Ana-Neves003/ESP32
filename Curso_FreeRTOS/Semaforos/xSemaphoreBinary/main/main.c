/**************************************************
* Exemplo para demonstrar o uso de semáforo binário
* Por: Ana Neves
**************************************************/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
//#include "driver/adc.h" 

#define LED_PIN     (GPIO_NUM_2)
//#define POT_PIN     (GPIO_NUM_34)

TaskHandle_t xTaskADCHandle;

SemaphoreHandle_t xSemaphore;

void vTaskADC(void *pvParameters)
{
    uint16_t adcValue;

    while(1)
    {
        xSemaphoreTake(xSemaphore, portMAX_DELAY);
        // Simulação da leitura do ADC
        adcValue = esp_random() % 4096; // Valor aleatório entre 0 e 4095
        //adcValue = adc1_get_raw(ADC1_CHANNEL_6);  // Ler o valor do pino ADC1_6 (GPIO34)
        printf("Valor do ADC: %d\n", adcValue);
        vTaskDelay(pdMS_TO_TICKS(1000)); // Atraso para simular leitura periódica
    }
}

void vTask_BLINK()
{
    bool led_status = false;
    while(1)
    {
        gpio_set_level(LED_PIN, led_status);
        led_status = !led_status;
        printf("Valor LED: %d\n", led_status);
        vTaskDelay(pdMS_TO_TICKS(500));
        xSemaphoreGive(xSemaphore);
    }
}

void app_main(void)
{
    gpio_pad_select_gpio(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    //adc1_config_width(ADC_WIDTH_BIT_12); // Configura a resolução do ADC como 12 bits
    //adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_0); // Configura a atenuação do sinal do pino ADC1_6 (GPIO34) como 0dB


    xSemaphore = xSemaphoreCreateBinary();

    if(xSemaphore == NULL){
        printf("Nao foi possivel criar o semaforo");
        while(1);
    }

    xTaskCreate(vTaskADC, "Task ADC", configMINIMAL_STACK_SIZE + 1024, NULL, 1, &xTaskADCHandle);
    xTaskCreate(vTask_BLINK, "Task BLINK", configMINIMAL_STACK_SIZE + 1024, NULL, 1, NULL);
}
