#include "sd_driver.h"


char* merge_filename(const char *filename)
{
    const char* bar = "/";
    const char* fileformat = "";
    char *name = malloc(strlen(MOUNT_POINT)+strlen(bar)+strlen(filename)+strlen(fileformat));
    strcpy(name, MOUNT_POINT);
    strcat(name, bar); 
    strcat(name, filename);
    //strcat(name, fileformat);
    return name;
}

FILE* open_file(const char *filename, char *mode)
{
    FILE* f;
    
    if(filename != NULL)
    {
        char * name = merge_filename(filename);        

        // Open file
        ESP_LOGI(SD_DRIVER_TAG, "Opening file.");
        f = fopen(name, mode);
        free(name);
        if (f == NULL) {
            ESP_LOGE(SD_DRIVER_TAG, "Failed to open file.");
            return NULL;
        }
        ESP_LOGI(SD_DRIVER_TAG, "File opened.");
        return f;
    } else 
    {
        ESP_LOGE(SD_DRIVER_TAG, "Invalid file name.");
        return NULL;
    }
}

void close_file(FILE **file)
{
    if(*file != NULL){
        fclose(*file);
        *file = NULL;
    }
    ESP_LOGI(SD_DRIVER_TAG, "File closed.");
    return;
}

void rename_file(const char *actualfname, const char *targetfname)
{
    /*
    char * actualName = merge_filename(actualfname); 
    char * targetName = merge_filename(targetfname); 
    // Check if destination file exists before renaming
    struct stat st;
    if (stat(targetName, &st) == 0) {
        // Delete it if it exists
        unlink(targetName);
    }
    // Rename original file
    ESP_LOGI(SD_DRIVER_TAG, "Renaming file.");
    if (rename(targetName, actualName) != 0) {
        ESP_LOGE(SD_DRIVER_TAG, "Rename failed.");
        free(targetName);
        free(actualName);
        return;
    } else
    {
        free(targetName);
        free(actualName);
        return;
    }
    return;
    */
}

/*
int initialize_spi_bus(void)
{
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
        ESP_LOGE(INIT_SPI_TAG, "Failed to initialize bus."); // Log de erro se falhou
        return ret;                         // Retorna o código de erro
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT(); // Configuração padrão do dispositivo SPI
    slot_config.gpio_cs = PIN_NUM_CS;      // Configuração do pino CS para SPI
    slot_config.host_id = host.slot;

     return 1; // initialization complete
}
*/

int deinitialize_spi_bus(sdmmc_host_t* host)
{
    ESP_LOGI(DEINIT_SPI_TAG, "Deinitializing SPI bus!");
    
    esp_err_t ret = spi_bus_free((*host).slot); //deinitialize the bus after all devices are removed
    if(ret != ESP_OK) {
        ESP_LOGI(DEINIT_SPI_TAG, "SPI not freed.");
        return 0;
    }
    
    ESP_LOGI(DEINIT_SPI_TAG, "SPI bus freed.");
    return 1;
}

/*
int initialize_sd_card(void)
{
   esp_err_t ret;

   sdmmc_card_t *card;                    // Ponteiro para a estrutura de cartão SD
    const esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = true,    // Formatar se a montagem falhar
        .max_files = 5,                    // Número máximo de arquivos abertos simultaneamente
        .allocation_unit_size = 16 * 1024  // Tamanho da unidade de alocação
    };

    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card); // Monta o sistema de arquivos FAT no cartão SD
    if (ret != ESP_OK) {                    // Verifica se a montagem foi bem-sucedida
        ESP_LOGE(INIT_SD_TAG, "Failed to mount filesystem."); // Log de erro se falhou
        spi_bus_free(host.slot);            // Libera o barramento SPI
        return ret;                         // Retorna o código de erro
    }

    sdmmc_card_print_info(stdout, card);    // Imprime informações do cartão SD
    return ESP_OK;  
}

*/

int deinitialize_sd_card(sdmmc_card_t** card)
{
    ESP_LOGI(DEINIT_SD_TAG, "Demounting SD card!");
    esp_err_t ret = esp_vfs_fat_sdcard_unmount(MOUNT_POINT, *card); // unmount partition and disable SDMMC or SPI peripheral
    
    if(ret != ESP_OK) {
        ESP_LOGI(DEINIT_SD_TAG, "Card still mounted.");
        return 0;
    }
    
    ESP_LOGI(DEINIT_SD_TAG, "Card unmounted.");
    return 1;
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
        ESP_LOGE(INIT_SPI_TAG, "Failed to initialize bus."); // Log de erro se falhou
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
        ESP_LOGE(INIT_SD_TAG, "Failed to mount filesystem."); // Log de erro se falhou
        spi_bus_free(host.slot);            // Libera o barramento SPI
        return ret;                         // Retorna o código de erro
    }

    sdmmc_card_print_info(stdout, card);    // Imprime informações do cartão SD
    return ESP_OK;                          // Retorna ESP_OK se bem-sucedido
}
