#include "main.h"
#include "sd_driver.h"
#include <unistd.h> // Necessário para usar fsync()


static const char *TAG = "I2S_PDM";

uint8_t dataBuffer[DATA_BUFFER_SIZE];
i2s_chan_handle_t rx_handle;
FILE* audio_file = NULL;


SemaphoreHandle_t xButtonSemaphore;
SemaphoreHandle_t xFileMutex;
TaskHandle_t xHandleGravacao = NULL;



void i2s_init()
{
    ESP_LOGI(TAG, "Inicializando I2S em modo PDM.");

    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = DMA_BUF_COUNT,
        .dma_frame_num = DMA_BUF_LEN_SMPL,
        .auto_clear = false,
    };
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));

    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(BITS_PER_SAMPLE, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = MIC_CLOCK_PIN,
            .din = MIC_DATA_PIN,
            .invert_flags = {
                .clk_inv = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(rx_handle, &pdm_rx_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
    ESP_LOGI(TAG, "I2S inicializado...");
}

void task_gravar_audio(void *pvParameters) {
    size_t bytes_read;
    while (1) {
        //ESP_LOGI(TAG, "[GRAVANDO]");
        ESP_ERROR_CHECK(i2s_channel_read(rx_handle, dataBuffer, DATA_BUFFER_SIZE, &bytes_read, portMAX_DELAY));

        printf("%02x %02x %02x %02x %02x %02x %02x\n",
               dataBuffer[0], dataBuffer[1], dataBuffer[2],
               dataBuffer[3], dataBuffer[4], dataBuffer[5],
               dataBuffer[6]);
        /*    
        xSemaphoreTake(xFileMutex, portMAX_DELAY);
        if (audio_file != NULL) {
            fwrite(dataBuffer, 1, bytes_read, audio_file);
            fflush(audio_file);
            fsync(fileno(audio_file));
        }
        xSemaphoreGive(xFileMutex);
        */
    }
}

void task_gerenciar_gravacao(void *pvParameters) {
    static bool em_gravacao = false;

    while (1) {
        if (xSemaphoreTake(xButtonSemaphore, portMAX_DELAY)) {
            vTaskDelay(pdMS_TO_TICKS(200)); // debounce

            xSemaphoreTake(xFileMutex, portMAX_DELAY);
            if (!em_gravacao) {
                if (!sd_is_mounted()) {
                    sd_mount();
                }
                audio_file = sd_open_file();
                if (audio_file) {
                    vTaskResume(xHandleGravacao);
                    em_gravacao = true;
                    ESP_LOGI(TAG, ">>> Gravação iniciada.");
                }
            } else {
                if (audio_file) {
                    fclose(audio_file);
                    audio_file = NULL;
                }
                vTaskSuspend(xHandleGravacao);
                em_gravacao = false;
                ESP_LOGI(TAG, ">>> Gravação finalizada.");
            }
            xSemaphoreGive(xFileMutex);
        }
    }
}

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    xSemaphoreGiveFromISR(xButtonSemaphore, NULL);
}

void app_main(void) {
    i2s_init();

    xFileMutex = xSemaphoreCreateMutex();
    xButtonSemaphore = xSemaphoreCreateBinary();

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = 1ULL << BUTTON_GPIO,
        .pull_down_en = 1,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, gpio_isr_handler, NULL);

    xTaskCreate(task_gravar_audio, "task_gravar_audio", 4096, NULL, 8, &xHandleGravacao);
    //vTaskSuspend(xHandleGravacao); // inicia suspensa

   // xTaskCreate(task_gerenciar_gravacao, "task_gerenciar_gravacao", 4096, NULL, 10, NULL);
}