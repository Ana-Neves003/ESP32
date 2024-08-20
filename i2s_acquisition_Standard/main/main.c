#include "main.h"


void app_main(void) {

    BaseType_t xReturnedTask[3];

    // Configurando os pinos da gpio 
    ESP_ERROR_CHECK(gpio_config(&in_button));                              // initialize input pin 1 configuration - on/off button
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT));     // install gpio isr service
    ESP_ERROR_CHECK(gpio_isr_handler_add(BTN_START_END, ISR_BTN, NULL));  // hook isr handler for specific gpio pin

    printf("Configurou os pinos\n");

    // Configurando o i2s
    //ESP_ERROR_CHECK(i2s_driver_install(I2S_PORT_NUM, &i2s_config, 32, &xQueueData));
    //ESP_ERROR_CHECK(i2s_set_pin(I2S_PORT_NUM, &pin_config));
    //ESP_ERROR_CHECK(i2s_stop(I2S_PORT_NUM));

    printf("Configurou o i2s\n");

    // Criando Semáforos e Grupos de Eventos
    xEvents           = xEventGroupCreate();
    xMutex            = xSemaphoreCreateMutex();
    xSemaphoreBTN_ON  = xSemaphoreCreateBinary();
    xSemaphoreBTN_OFF = xSemaphoreCreateBinary();

    printf("Configurou os semaforos, grupo e eventos\n");
    
    /*
    if(xQueueData == NULL){ // tests if queue creation fails
        ESP_LOGE(SETUP_APP_TAG, "Failed to create data queue.\n");
        while(1);
    }
    if(xEvents == NULL){ // tests if event group creation fails
        ESP_LOGE(SETUP_APP_TAG, "Failed to create event group.\n");
        while(1);
    }
    if((xSemaphoreBTN_ON == NULL) || (xSemaphoreBTN_OFF == NULL) || (xMutex == NULL)){ // tests if semaphore creation fails
        ESP_LOGE(SETUP_APP_TAG, "Failed to create semaphores.\n");
        while(1);
    }
    */
  

    // Criando as Tasks
    xReturnedTask[0] = xTaskCreatePinnedToCore(vTaskSTART, "taskSTART", 8192, NULL, configMAX_PRIORITIES-2, &xTaskSTARThandle, PRO_CPU_NUM);
    xReturnedTask[1] = xTaskCreatePinnedToCore(vTaskEND,   "taskEND",   8192, NULL, configMAX_PRIORITIES-1, &xTaskENDhandle,   PRO_CPU_NUM);
    xReturnedTask[2] = xTaskCreatePinnedToCore(vTaskREC,   "taskREC",   8192, NULL, configMAX_PRIORITIES-3, &xTaskRECHandle,   APP_CPU_NUM);
   
    printf("Criando as tasks\n");

    for(int itr=0; itr<3; itr++) // iterate over tasks 
    {
        if(xReturnedTask[itr] == pdFAIL){ // tests if task creation fails
            ESP_LOGE(SETUP_APP_TAG, "Failed to create task %d.\n", itr);
            while(1);
        }
    }

     printf("Tasks criadas\n");

    // Suspende Tasks do modo gravação 
    vTaskSuspend(xTaskRECHandle);
    vTaskSuspend(xTaskENDhandle);

    // set flag informing that the recording is stopped
    xEventGroupClearBits(xEvents, BIT_(REC_STARTED));

    ESP_LOGI(SETUP_APP_TAG, "Successful BOOT!");
    vTaskDelete(NULL);
    
}

void vTaskREC(void * pvParameters)
{
    i2s_event_t i2s_evt;
    while(1)
    {
        while(
            (xQueueData!=NULL)                               &&
            (xQueueReceive(xQueueData, &i2s_evt, 0)==pdTRUE) &&
            (i2s_evt.type == I2S_EVENT_RX_DONE)              &&
            (xMutex!=NULL)                                   &&
            (xSemaphoreTake(xMutex,portMAX_DELAY)==pdTRUE)
        ) // wait for data to be read
        {
            i2s_read(I2S_PORT_NUM, (void*) dataBuffer, DATA_BUFFER_SIZE, &bytes_read, portMAX_DELAY); // read bytes from DMA
            printf("%x %x %x %x %x %x %x\n", dataBuffer[0], dataBuffer[1], dataBuffer[2], dataBuffer[3], dataBuffer[4], dataBuffer[5], dataBuffer[6]);
            xSemaphoreGive(xMutex);
        }
        vTaskDelay(1);
    }
}

void vTaskEND(void * pvParameters)
{
    BaseType_t xHighPriorityTaskWoken = pdFALSE;
    
    while(1)
    {
        if(xSemaphoreBTN_OFF != NULL && xSemaphoreTakeFromISR(xSemaphoreBTN_OFF, &xHighPriorityTaskWoken) == pdTRUE) // button was pressed to turn OFF recording
        {
            // suspend tasks for recording mode
            vTaskSuspend(xTaskRECHandle);

            // i2s stop
            ESP_ERROR_CHECK(i2s_stop(I2S_PORT_NUM));

            // wait to get mutex indicating total end of rec task
            while(xSemaphoreTake(xMutex, portMAX_DELAY) == pdFALSE);

            ESP_LOGI(END_REC_TAG, "Recording session finished.");

            // close file in use
            close_file(&session_file);

            // dismount SD card and free SPI bus (in the given order) 
            deinitialize_sd_card(&card);
            deinitialize_spi_bus(&host);

            // clear flag informing that the recording is stopped
            xEventGroupClearBits(xEvents, BIT_(REC_STARTED));

            // resume task to wait for recording trigger
            vTaskResume(xTaskSTARThandle);

            ESP_LOGI(END_REC_TAG, "Returning to IDLE mode.");

            xSemaphoreGive(xMutex);

            // locking task
            vTaskSuspend(xTaskENDhandle); // suspend actual task
        } else { vTaskDelay(1); }
    }
}

void vTaskSTART(void * pvParameters)
{
    BaseType_t xHighPriorityTaskWoken = pdFALSE;
    while(1)
    {
        if(xSemaphoreBTN_ON!=NULL && xSemaphoreTakeFromISR(xSemaphoreBTN_ON,&xHighPriorityTaskWoken)==pdTRUE) // button was pressed to turn ON recording
        {
            ESP_LOGI(START_REC_TAG, "Starting record session.");

            // initialize SPI bus and mount SD card
            //while(initialize_spi_bus(&host)!=1){vTaskDelay(100);}
            //while(initialize_sd_card(&host, &card)!=1){vTaskDelay(100);}

             // Inicialização do cartão SD
        if (init_sd_card() != ESP_OK) {         // Inicializa o cartão SD e verifica se foi bem-sucedido
            ESP_LOGE(INIT_SD_TAG, "Failed to initialize SD card"); // Log de erro se falhou
            return;                             // Sai da função se falhou
        }

            vTaskDelay(100);

            printf("Passou pela inicialização sem problema");
            
            // reset RTC 
            settimeofday(&date, NULL); // update time

            printf("Passou pelo RTC sem problema");
            
            // open new file in append mode
            //while(session_file==NULL) session_file = open_file(fname, "a");

            while(session_file==NULL){
                esp_err_t ret;                          // Variável para armazenar o resultado das funções
                uint32_t io_num;                        // Variável para armazenar o número do GPIO
                uint32_t counter = 0;                   // Contador para o número de vezes que o botão foi pressionado
                FILE *f;
    
                session_file = f = fopen(MOUNT_POINT "/counter.txt", "a");

                if (f == NULL) {                // Verifica se o arquivo foi aberto com sucesso
                ESP_LOGE(SD_DRIVER_TAG, "Failed to open file for writing"); // Log de erro se falhou
                return;                     // Sai da função se falhou
                }
            }

            // set flag informing that the recording already started
            xEventGroupSetBits(xEvents, BIT_(REC_STARTED));

            // start i2s
            //ESP_ERROR_CHECK(i2s_start(I2S_PORT_NUM));        // start i2s clocking mic to low-power mode
            //vTaskDelay(pdMS_TO_TICKS(100));                 // structural delay to changes take place
            //i2s_set_sample_rates(I2S_PORT_NUM, SAMPLE_RATE); // change mic to ultrasonic mode

            ESP_LOGI(START_REC_TAG, "Recording session started.");

            // resume tasks for recording mode
            vTaskResume(xTaskRECHandle);
            vTaskResume(xTaskENDhandle);
        
            // locking task
            vTaskSuspend(xTaskSTARThandle); // suspend actual task
        } else {vTaskDelay(1);}
    }
}

static void IRAM_ATTR ISR_BTN()
{
    BaseType_t xHighPriorityTaskWoken = pdFALSE;
    if(xEventGroupGetBitsFromISR(xEvents) & BIT_(REC_STARTED)){xSemaphoreGiveFromISR(xSemaphoreBTN_OFF,&xHighPriorityTaskWoken);} // recording started, so stop recording
    else{xSemaphoreGiveFromISR(xSemaphoreBTN_ON,&xHighPriorityTaskWoken);} // recording stopd, so start recording
    portYIELD_FROM_ISR(xHighPriorityTaskWoken);
}

void i2s_setup(void * pvParameters) {
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM,
        .sample_rate = 31250,
        .bits_per_sample = BIT_DEPTH,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN_SMPL,
        .use_apll = I2S_CLK_APLL,
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_PIN_NO_CHANGE,
        .ws_io_num = MIC_CLOCK_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = MIC_DATA_PIN
    };

    i2s_driver_install(I2S_PORT_NUM, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_PORT_NUM, &pin_config);
    // configure i2s
    //ESP_ERROR_CHECK(i2s_driver_install(I2S_NUM, &i2s_config, 32, &xQueueData));
    //ESP_ERROR_CHECK(i2s_set_pin(I2S_NUM, &pin_config));
    //ESP_ERROR_CHECK(i2s_stop(I2S_NUM));
}