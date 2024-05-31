#include "main.h"
#include "i2s.h"
#include "sd_card.h"

static const char *TAG = "i2s_sd_card";
static bool recording = false;

TaskHandle_t xTaskSTARTHandle, xTaskRECHandle, xTaskENDHandle;

//void vTask_REC(void *pvParameter) {
//    uint8_t buffer[1024];
//    size_t bytes_read = 0;
//    const int recording_time_ms = 10000;
//    const int delay_time_ms = 100;
//    int elapsed_time_ms = 0;
//
//    file = fopen(MOUNT_POINT "/gravacao.raw", "w");
//    if (file == NULL) {
//        ESP_LOGE(TAG, "Failed to open file for writing");
//        vTaskDelete(NULL);
//        return;
//    }
//
//    ESP_LOGI(TAG, "Iniciando a gravação");
//
//    while (elapsed_time_ms < recording_time_ms) {
//        if (i2s_read(I2S_NUM, buffer, sizeof(buffer), &bytes_read, portMAX_DELAY) == ESP_OK) {
//            for (int i = 0; i < bytes_read; i++) {
//                fprintf(file, "%02x ", buffer[i]);
//            }
//            fprintf(file, "\n");
//            fflush(file);
//
//            ESP_LOGI(TAG, "Gravando %d bytes no arquivo", bytes_read);
//            for (int i = 0; i < bytes_read; i++) {
//                printf("%02x ", buffer[i]);
//            }
//            printf("\n");
//        } else {
//            ESP_LOGE(TAG, "Erro ao ler dados do I2S");
//        }
//
//        vTaskDelay(pdMS_TO_TICKS(delay_time_ms));
//        elapsed_time_ms += delay_time_ms;
//    }
//
//    ESP_LOGI(TAG, "Gravação concluída");
//    fclose(file);
//    ESP_LOGI(TAG, "Arquivo fechado");
//    vTaskDelete(NULL);
//}

void vTaskSTART(void *pvParameters) 
{
    while (1) {
        // Espera o evento de início de gravação
        xEventGroupWaitBits(xEvents, BIT_0, pdTRUE, pdTRUE, portMAX_DELAY);
        ESP_LOGI(TAG, "Gravação iniciada");
        i2s_resume();
        vTaskResume(xTaskRECHandle);
        vTaskResume(xTaskENDHandle);
        vTaskSuspend(NULL);
    }
}

void vTaskEND(void *pvParameters) 
{
    while (1) {
        // Espera o evento de parada de gravação
        xEventGroupWaitBits(xEvents, BIT_0, pdFALSE, pdTRUE, portMAX_DELAY);
        ESP_LOGI(TAG, "Gravação parada");
        i2s_pause();
        vTaskSuspend(NULL);
    }
}

void vTaskREC(void *pvParameters) 
{
    uint8_t buffer[1024];
    size_t bytes_read = 0;

    while (1) {
        if (recording) {
            i2s_read(I2S_PORT_NUM, buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);
            for (int i = 0; i < bytes_read; i++) {
                printf("%02x ", buffer[i]);
            }
            printf("\n");
        }
    }
}

void vTaskBUTTON(void *pvParameters)
{
    gpio_pad_select_gpio(GPIO_BUTTON);
    gpio_set_direction(GPIO_BUTTON, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_BUTTON, GPIO_PULLUP_ONLY);

    bool last_button_state = false;

    while (1) {
        bool current_button_state = gpio_get_level(GPIO_BUTTON) == 0;

        if (current_button_state && !last_button_state) {
            if (!recording) {
                ESP_LOGI(TAG, "Iniciando gravação");
                recording = true;
                xEventGroupSetBits(xEvents, BIT_0);
            } else {
                ESP_LOGI(TAG, "Parando gravação");
                recording = false;
                xEventGroupClearBits(xEvents, BIT_0);
            }
        }

        last_button_state = current_button_state;
        vTaskDelay(pdMS_TO_TICKS(10)); // Pequeno atraso para reduzir o uso da CPU
    }
}

void app_main() {
    ESP_LOGI(TAG, "Inicializando I2S");

    // Criação da fila e grupo de eventos
    xQueueData = xQueueCreate(32, sizeof(uint32_t));
    xEvents = xEventGroupCreate();

    // Criação das tarefas
    xTaskCreatePinnedToCore(vTaskSTART, "vTaskSTART", 8192, NULL, configMAX_PRIORITIES-2, &xTaskSTARTHandle, PRO_CPU_NUM);
    xTaskCreatePinnedToCore(vTaskEND,   "vTaskEND",   8192, NULL, configMAX_PRIORITIES-1, &xTaskENDHandle,   PRO_CPU_NUM);
    xTaskCreatePinnedToCore(vTaskREC,   "vTaskREC",   8192, NULL, configMAX_PRIORITIES-3, &xTaskRECHandle,   APP_CPU_NUM);
    xTaskCreate(vTaskBUTTON, "button_task", 4096, NULL, 5, NULL);

    // Suspende tarefas para o modo de gravação
    vTaskSuspend(xTaskSTARTHandle);
    vTaskSuspend(xTaskENDHandle);
    vTaskSuspend(xTaskRECHandle);
}
