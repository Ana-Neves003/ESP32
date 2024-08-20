#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define TRIGGER_PIN GPIO_NUM_12

void send_pulses() {
    for (int i = 0; i < 100; i++) {  // Enviar 100 pulsos (ajuste conforme necessário)
        gpio_set_level(TRIGGER_PIN, 1);
        printf("Pulso enviado");
        ets_delay_us(12.5);  // Meio período para 40 kHz (25 microsegundos por período)
        gpio_set_level(TRIGGER_PIN, 0);
        ets_delay_us(12.5);
    }
}

void app_main(void) {
    // Configure o pino Trigger como saída
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << TRIGGER_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    while (1) {
        send_pulses();
        vTaskDelay(pdMS_TO_TICKS(1000));  // Aguardar 1 segundo antes de enviar novamente
    }
}
