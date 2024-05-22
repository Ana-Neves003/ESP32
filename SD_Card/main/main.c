#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

// Definições de pinos
#define GPIO_BUTTON    GPIO_NUM_0      // Definição do pino do botão
#define PIN_NUM_MISO   GPIO_NUM_19     // Definição do pino MISO (Master In Slave Out) para SPI
#define PIN_NUM_MOSI   GPIO_NUM_23     // Definição do pino MOSI (Master Out Slave In) para SPI
#define PIN_NUM_CLK    GPIO_NUM_18     // Definição do pino CLK (Clock) para SPI
#define PIN_NUM_CS     GPIO_NUM_22     // Definição do pino CS (Chip Select) para SPI
#define MOUNT_POINT    "/sdcard"       // Ponto de montagem do sistema de arquivos no cartão SD

// Configuração do canal DMA (se necessário)
#define SPI_DMA_CHAN   1               // Definição do canal DMA para SPI

// Tag para logging
static const char *TAG = "sd_card_example"; // Tag para mensagens de log

// Fila para eventos GPIO
static xQueueHandle gpio_evt_queue = NULL;  // Declaração de uma fila para eventos GPIO

// Manipulador de interrupção do botão
static void IRAM_ATTR button_isr_handler(void *arg) {
    uint32_t gpio_num = (uint32_t) arg;                 // Cast do argumento para uint32_t
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL); // Envia o número do GPIO para a fila a partir da ISR
}

// Inicialização do cartão SD
static esp_err_t init_sd_card(void) {
    esp_err_t ret;                          // Variável para armazenar o resultado das funções

    sdmmc_host_t host = SDSPI_HOST_DEFAULT(); // Configuração padrão do host SPI
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,        // Configuração do pino MOSI para SPI
        .miso_io_num = PIN_NUM_MISO,        // Configuração do pino MISO para SPI
        .sclk_io_num = PIN_NUM_CLK,         // Configuração do pino CLK para SPI
        .quadwp_io_num = -1,                // Pino não usado
        .quadhd_io_num = -1,                // Pino não usado
        .max_transfer_sz = 4000,            // Tamanho máximo de transferência
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CHAN); // Inicializa o barramento SPI
    if (ret != ESP_OK) {                    // Verifica se a inicialização foi bem-sucedida
        ESP_LOGE(TAG, "Failed to initialize bus."); // Log de erro se falhou
        return ret;                         // Retorna o código de erro
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT(); // Configuração padrão do dispositivo SPI
    slot_config.gpio_cs = PIN_NUM_CS;      // Configuração do pino CS para SPI
    slot_config.host_id = host.slot;       // ID do host

    sdmmc_card_t *card;                    // Ponteiro para a estrutura de cartão SD
    const esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = true,    // Formatar se a montagem falhar
        .max_files = 5,                    // Número máximo de arquivos abertos simultaneamente
        .allocation_unit_size = 16 * 1024  // Tamanho da unidade de alocação
    };

    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card); // Monta o sistema de arquivos FAT no cartão SD
    if (ret != ESP_OK) {                    // Verifica se a montagem foi bem-sucedida
        ESP_LOGE(TAG, "Failed to mount filesystem."); // Log de erro se falhou
        spi_bus_free(host.slot);            // Libera o barramento SPI
        return ret;                         // Retorna o código de erro
    }

    sdmmc_card_print_info(stdout, card);    // Imprime informações do cartão SD
    return ESP_OK;                          // Retorna ESP_OK se bem-sucedido
}

// Aplicação principal
void app_main(void) {
    esp_err_t ret;                          // Variável para armazenar o resultado das funções
    uint32_t io_num;                        // Variável para armazenar o número do GPIO
    uint32_t counter = 0;                   // Contador para o número de vezes que o botão foi pressionado
    FILE *f;                                // Ponteiro para arquivo

    // Configuração do GPIO do botão como entrada
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_POSEDGE,     // Configura interrupção no flanco de subida
        .mode = GPIO_MODE_INPUT,            // Configura o GPIO como entrada
        .pin_bit_mask = (1ULL << GPIO_BUTTON), // Máscara do pino do botão
        .pull_down_en = 0,                  // Desabilita pull-down
        .pull_up_en = 1,                    // Habilita pull-up
    };
    gpio_config(&io_conf);                  // Aplica a configuração do GPIO

    // Criação da fila para lidar com eventos GPIO da ISR
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t)); // Cria a fila com capacidade para 10 itens de uint32_t
    gpio_install_isr_service(0);            // Instala o serviço de ISR do GPIO
    gpio_isr_handler_add(GPIO_BUTTON, button_isr_handler, (void *) GPIO_BUTTON); // Adiciona a ISR para o GPIO do botão

    // Inicialização do cartão SD
    if (init_sd_card() != ESP_OK) {         // Inicializa o cartão SD e verifica se foi bem-sucedido
        ESP_LOGE(TAG, "Failed to initialize SD card"); // Log de erro se falhou
        return;                             // Sai da função se falhou
    }

    // Loop principal
    while (1) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) { // Aguarda eventos na fila
            counter++;                       // Incrementa o contador
            ESP_LOGI(TAG, "Button pressed. Counter: %d", counter); // Log do contador

            // Abrir arquivo para acrescentar
            f = fopen(MOUNT_POINT "/counter.txt", "a"); // Abre o arquivo em modo append
            if (f == NULL) {                // Verifica se o arquivo foi aberto com sucesso
                ESP_LOGE(TAG, "Failed to open file for writing"); // Log de erro se falhou
                return;                     // Sai da função se falhou
            }
            fprintf(f, "Counter: %d\n", counter); // Escreve o valor do contador no arquivo
            fclose(f);                     // Fecha o arquivo
            ESP_LOGI(TAG, "Counter value written to file"); // Log de sucesso
        }
    }
}
