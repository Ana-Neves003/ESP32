#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2s_std.h"
#include "driver/i2s_pdm.h"

#include "esp_system.h"
#include "esp_netif.h"
#include "filtro.h"

#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "lwip/sockets.h"
#include <netinet/in.h>

#include "esp_http_client.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"



#define SAMPLE_RATE_STD  44100
#define SAMPLE_RATE_ULT  78125   
#define DMA_BUF_COUNT    16
#define DMA_BUF_LEN_SMPL 511

#define BIT_DEPTH_STD        I2S_DATA_BIT_WIDTH_16BIT
#define BIT_DEPTH_ULT        I2S_DATA_BIT_WIDTH_32BIT
#define DATA_BUFFER_SIZE     (DMA_BUF_LEN_SMPL *2* BIT_DEPTH_ULT / 8) //stereo x 32 bits = 4088 bytes

// Pinos do microfone PDM
#define MIC_CLOCK_PIN     GPIO_NUM_21
#define MIC_DATA_PIN      GPIO_NUM_4
#define I2S_PORT_NUM      I2S_NUM_0

#define CORE_TX             0  // TCP/Wi-Fi
#define CORE_I2S            1  // I2S

// -----------Variáveis Globais ----------------------------

uint8_t dataBuffer[DATA_BUFFER_SIZE];
static i2s_chan_handle_t rx_handle;
static QueueHandle_t xQueue;

//------------ Filas ------------
//#define QUEUE_IN_ITEMS    8
//#define QUEUE_OUT_ITEMS   8
#define QUEUE_ITEMS       16   // nº de blocos na fila

// ----------- Envio -------------
#define TX_DELAY_MS       8

//#define DEST_IP             "192.168.15.183"
#define DEST_IP             "192.168.1.133"
#define DEST_PORT           12345

const uint32_t aquisicao_segundos = 60;  
const TickType_t duracao = pdMS_TO_TICKS(aquisicao_segundos * 1000);

/*
//#define SEND_FILTERED     0 // 0 = envia RAW (igual antes), 1 = envia filtrado (int16) 

// --------- Parâmetros do Filtro --------
#define CIC_N  4
#define CIC_R  16
#define CIC_M  1


#define WORDS_PER_CHUNK   128


// ------- Coeficientes FIR -----------

static const float fir_coeffs[] = {
    0.000792054770933056f, -0.000442986869603646f, -0.000346715840607441f,
    0.00105672404464702f, -0.00102725323675075f, -3.97609216037946e-05f,
    0.00157304669437729f, -0.00223131365400529f, 0.000913291906289769f,
    0.00194681182232756f, -0.00410916320834437f, 0.00307212378248671f,
    0.00143941527024877f, -0.00629432948118691f, 0.00682235964803528f,
    -0.000929276275685085f, -0.00791664441517844f, 0.0121861344621779f,
    -0.00625357547552810f, -0.00757308458012768f, 0.0187253171630382f,
    -0.0157626522477365f, -0.00317130531504444f, 0.0255797271575543f,
    -0.0314843244086080f, 0.00912596811664657f, 0.0316404965881606f,
    -0.0600365491481145f, 0.0419241430420708f, 0.0358143540096918f,
    -0.151082412771185f, 0.254374980329710f, 0.703428798081828f,
    0.254374980329710f, -0.151082412771185f, 0.0358143540096918f,
    0.0419241430420708f, -0.0600365491481145f, 0.0316404965881606f,
    0.00912596811664657f, -0.0314843244086080f, 0.0255797271575543f,
    -0.00317130531504444f, -0.0157626522477365f, 0.0187253171630382f,
    -0.00757308458012768f, -0.00625357547552810f, 0.0121861344621779f,
    -0.00791664441517844f, -0.000929276275685085f, 0.00682235964803528f,
    -0.00629432948118691f, 0.00143941527024877f, 0.00307212378248671f,
    -0.00410916320834437f, 0.00194681182232756f, 0.000913291906289769f,
    -0.00223131365400529f, 0.00157304669437729f, -3.97609216037946e-05f,
    -0.00102725323675075f, 0.00105672404464702f, -0.000346715840607441f,
    -0.000442986869603646f, 0.000792054770933056f
};

static const size_t FIR_LEN = sizeof(fir_coeffs)/sizeof(fir_coeffs[0]);

*/

static const char *TAG = "I2S_TCP_RTOS";

typedef struct {
    size_t len_bytes;                     // bytes válidos em data[]
    uint8_t data[DATA_BUFFER_SIZE];       // conteúdo lido do I2S
} block_t;

/*

// ====== tipos de bloco p/ filas ======
typedef struct {
    size_t len_bytes;                     // bytes válidos em data[]
    uint8_t data[DATA_BUFFER_SIZE];       // conteúdo lido do I2S
} block_in_t;

// Saída filtrada (vamos quantizar para int16 p/ mandar por TCP quando SEND_FILTERED=1)
#define OUT_SAMPLES_MAX   ((WORDS_PER_CHUNK*32)/CIC_R + 64)  // margem
typedef struct {
    size_t nsamples;                      // nº de amostras válidas
    int32_t data[OUT_SAMPLES_MAX];        // amostras filtradas quantizadas
} block_out_t;

*/

//static QueueHandle_t q_in  = NULL;
//static QueueHandle_t q_out = NULL;
//static i2s_chan_handle_t rx_handle = NULL;


static void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            //.ssid = "VIVOFIBRA-4870-EXT",
            .ssid = "LASEM",
            //.ssid = "Cerberus",
            //.password = "CC99735551",
            .password = "besourosuco",
            //.password = "Lime@302",
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));  // reduzir latência
    ESP_ERROR_CHECK(esp_wifi_connect());
}

static void i2s_init_std(int SAMPLE_RATE, i2s_data_bit_width_t BIT_DEPTH, bool modo_ult) {

    //ESP_LOGI(TAG, "Initializing I2S STD"); 

    i2s_chan_config_t rx_channel_config = {
        .id = I2S_PORT_NUM,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = DMA_BUF_COUNT,
        .dma_frame_num = DMA_BUF_LEN_SMPL,
        .auto_clear = true
    };

    ESP_ERROR_CHECK(i2s_new_channel(&rx_channel_config, NULL, &rx_handle));

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_APLL,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256
        },
        .slot_cfg = {
            .data_bit_width =  BIT_DEPTH,
            .slot_bit_width =  BIT_DEPTH,
                .slot_mode = I2S_SLOT_MODE_STEREO,
                .slot_mask = I2S_STD_SLOT_BOTH,
                .ws_width = BIT_DEPTH,
                .ws_pol = false,
                .bit_shift = true
            },
            .gpio_cfg = {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = modo_ult ? MIC_CLOCK_PIN: I2S_GPIO_UNUSED,
                .ws   = modo_ult ? I2S_GPIO_UNUSED: MIC_CLOCK_PIN,
                .dout = I2S_GPIO_UNUSED,
                .din  = MIC_DATA_PIN
            }
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
    //ESP_LOGI(TAG, "I2S inicializado!");
}

static void i2s_init_pdm(int SAMPLE_RATE, i2s_data_bit_width_t BIT_DEPTH)
{
    ESP_LOGI(TAG, "Inicializando I2S em modo PDM.");

    // Configuração do canal 
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0, // escolha do canal i2s 0 (no esp32 apenas esse canal suporta o modo pdm)
        .role = I2S_ROLE_MASTER, // o esp32 atua como o controlador
        .dma_desc_num = DMA_BUF_COUNT, // quantidade de buffers do DMA
        .dma_frame_num = DMA_BUF_LEN_SMPL, // tamanho dos buffers do DMA
        .auto_clear = false, // limpar automaticamente o buffer TX (desnecessario)
    };
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg,NULL, &rx_handle));                         // Create a new I2S channel  

    // Configuração do microfone em modo PDM RX
    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(BIT_DEPTH, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = MIC_CLOCK_PIN, 
            .din = MIC_DATA_PIN,
            .invert_flags = { // nao inverter bits
                .clk_inv = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(rx_handle, &pdm_rx_cfg));    // Initialize I2S channel in standard mode
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));                           // Enable I2S channel
    ESP_LOGI(TAG, "I2S inicializado!");
}

static void task_i2s_capture(void *pvParameters)
{
    
    ESP_LOGI(TAG, "I2S 44.1 kHz...");
    i2s_init_std(SAMPLE_RATE_STD, BIT_DEPTH_STD, false);
    vTaskDelay(pdMS_TO_TICKS(1000));

    i2s_channel_disable(rx_handle);
    i2s_del_channel(rx_handle);
    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI(TAG, "I2S 78.125 kHz...");
    i2s_init_std(SAMPLE_RATE_ULT, BIT_DEPTH_ULT, true);
    vTaskDelay(pdMS_TO_TICKS(200));

    TickType_t start = xTaskGetTickCount();

    while ((xTaskGetTickCount() - start) < duracao) {
        block_t blk;
        size_t bytes_read = 0;
        esp_err_t res = i2s_channel_read(rx_handle, blk.data, DATA_BUFFER_SIZE, &bytes_read, portMAX_DELAY);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "I2S read err=%s", esp_err_to_name(res));
            continue;
        }
        blk.len_bytes = bytes_read;
        //ESP_LOG_BUFFER_HEXDUMP(TAG, blk.data, 32, ESP_LOG_INFO);
        vTaskDelay(1);
       
       // Envia o bloco para a fila, bloqueando se necessário (nunca descarta)
       xQueueSend(xQueue, &blk, portMAX_DELAY);  

    }

    vTaskDelay(pdMS_TO_TICKS(20));

    if (rx_handle) 
    { 
        i2s_channel_disable(rx_handle);    
        i2s_del_channel(rx_handle); 
        rx_handle = NULL; 
    }

    ESP_LOGI(TAG, "task_i2s_capture: fim");
    vTaskDelete(NULL);

}

static void task_https_send(void *pvParameters)
{
    // Espera o IP ficar válido
    esp_netif_ip_info_t ip_info;
    do {
        vTaskDelay(pdMS_TO_TICKS(300));
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (!netif) continue;
        esp_netif_get_ip_info(netif, &ip_info);
    } while (ip_info.ip.addr == 0);
    ESP_LOGI(TAG, "IP OK");

    // Configuração HTTPS
    esp_http_client_config_t config = {
    //.url = "http://192.168.15.183:8000/upload",
    .url = "http://192.168.1.133:8000/upload",
    //.transport_type = HTTP_TRANSPORT_OVER_SSL,
    .transport_type = HTTP_TRANSPORT_OVER_TCP,
    .cert_pem = NULL,  // Ignora certificado
    .skip_cert_common_name_check = true,
    .disable_auto_redirect = true,
    .event_handler = NULL,
    .is_async = false,
    .timeout_ms = 5000,
    .keep_alive_enable = false,
    .user_data = NULL,
    .crt_bundle_attach = NULL,  // Importante pra evitar erro de CA
};

    esp_http_client_handle_t client = esp_http_client_init(&config);

    TickType_t start = xTaskGetTickCount();

    while ((xTaskGetTickCount() - start) < duracao || uxQueueMessagesWaiting(xQueue) > 0) {
        block_t blk;
        if (xQueueReceive(xQueue, &blk, pdMS_TO_TICKS(50)) == pdTRUE) {
            // Envia o bloco via POST
            esp_http_client_set_method(client, HTTP_METHOD_POST);
            esp_http_client_set_header(client, "Content-Type", "application/octet-stream");

            esp_err_t err = esp_http_client_open(client, blk.len_bytes);
            if (err == ESP_OK) {
                esp_http_client_write(client, (const char *)blk.data, blk.len_bytes);
                esp_http_client_close(client);
                ESP_LOGI(TAG, "Enviado %d bytes via HTTP", blk.len_bytes);
            } else {
                ESP_LOGE(TAG, "Falha ao abrir conexão HTTP: %s", esp_err_to_name(err));
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }
    }

    esp_http_client_cleanup(client);
    ESP_LOGI(TAG, "task_https_send: fim");
    vTaskDelete(NULL);
}


/*
static void task_process_filter(void *arg)
{
    filtro_t *filt = (filtro_t*)arg;

    float *out_tmp = (float*)heap_caps_malloc(OUT_SAMPLES_MAX * sizeof(float), MALLOC_CAP_8BIT);
    static block_out_t outb;
    if (!out_tmp) {
        ESP_LOGE(TAG, "sem memória para out_tmp");
        vTaskDelete(NULL);
    }
    
    while (g_running || uxQueueMessagesWaiting(q_in) > 0) {
        block_in_t inb;
        if (xQueueReceive(q_in, &inb, pdMS_TO_TICKS(50)) != pdTRUE) continue;

        // processa em fatias para não segurar a CPU (evita WDT)
        size_t n_words_total = inb.len_bytes / 4;
        const uint32_t *wptr = (const uint32_t*)inb.data;
        //const size_t WORDS_PER_CHUNK = 128;  

        while (n_words_total > 0) {
            size_t this_chunk = (n_words_total > WORDS_PER_CHUNK) ? WORDS_PER_CHUNK : n_words_total;
            size_t n_out = 0;
            int r = filtro_process_words(filt, wptr, this_chunk, out_tmp, &n_out);
            if (r != 0) {
                ESP_LOGE(TAG, "filtro_process_words falhou (%d)", r);
                break;
            }

            if (n_out > 0) {
                //block_out_t outb;
                size_t ns = (n_out > OUT_SAMPLES_MAX) ? OUT_SAMPLES_MAX : n_out;
                outb.nsamples = ns;

                // normaliza (~ R^N = 16^4 = 65536) e quantiza p/ int16
                for (size_t i = 0; i < ns; ++i) {
                    //float v = out_tmp[i] / 65536.0f;
                    float v = out_tmp[i] ;
                    //if (v > 0.999f) v = 0.999f;
                    if (v > 2147483647.0f) v = 2147483647.0f;
                    //if (v < -0.999f) v = -0.999f;
                    if (v < -2147483648.0f) v = -2147483648.0f;
                    //outb.data[i] = (int16_t)(v * 32767.0f);
                    outb.data[i] = (int32_t)v;
                }

                xQueueSend(q_out, &outb, portMAX_DELAY);
            }

            wptr += this_chunk;
            n_words_total -= this_chunk;
            vTaskDelay(0);
        }
    }

    heap_caps_free(out_tmp);
    ESP_LOGI(TAG, "task_process_filter: fim");
    vTaskDelete(NULL);
}
*/

static void task_tcp_send(void *pvParameters) 
{
    // Aguarda IP válido
    esp_netif_ip_info_t ip_info;
    do 
    {
        vTaskDelay(pdMS_TO_TICKS(300));
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (!netif) continue;
        esp_netif_get_ip_info(netif, &ip_info);
    } while (ip_info.ip.addr == 0);
    ESP_LOGI(TAG, "IP OK");

    // Cria socket
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0)
    { 
        ESP_LOGE(TAG, "socket falhou"); 
        vTaskDelete(NULL); 
    }

    int nodelay = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

    struct sockaddr_in dest_addr = {0};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port   = htons(DEST_PORT);
    dest_addr.sin_addr.s_addr = inet_addr(DEST_IP);

    ESP_LOGI(TAG, "Conectando %s:%d...", DEST_IP, DEST_PORT);
    if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        ESP_LOGE(TAG, "connect falhou");
        close(sock);
        vTaskDelete(NULL);
    }
    ESP_LOGI(TAG, "Conectado");

    
    TickType_t start = xTaskGetTickCount();


    // envia RAW, igual ao seu fluxo original (sem filtro)
    while ((xTaskGetTickCount() - start) < duracao || uxQueueMessagesWaiting(xQueue) > 0) {
        block_t inb;
        if (xQueueReceive(xQueue, &inb, pdMS_TO_TICKS(50)) == pdTRUE) {
            int sent = send(sock, inb.data, inb.len_bytes, MSG_DONTWAIT);
            if (sent < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    vTaskDelay(pdMS_TO_TICKS(1));
                    continue;
                } else {
                    ESP_LOGE(TAG, "erro TCP (errno=%d)", errno);
                    break;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(TX_DELAY_MS));
        }
    }

    ESP_LOGI(TAG, "Encerrando TCP");
    close(sock);
    ESP_LOGI(TAG, "task_tcp_send: fim");
    vTaskDelete(NULL);
    
}

void app_main(void) {

    // Inicializa NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // IP do computador e porta
    //const char* DEST_IP = "192.168.1.124";  
    //const char* DEST_IP = "192.168.15.183";  
    //const char* DEST_IP = "192.168.0.106";  
    //const int DEST_PORT = 12345;

    // Conecta Wi-Fi
    ESP_LOGI(TAG, "Iniciando conexão Wi-Fi...");
    wifi_init_sta();
    ESP_LOGI(TAG, "Aguardando conexão Wi-Fi...");
    //vTaskDelay(pdMS_TO_TICKS(3000));  // Aguarda conexão

    /*
    // Mostra IP do ESP32
    esp_netif_ip_info_t ip_info;
    do {
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        esp_netif_get_ip_info(netif, &ip_info);
    } while (ip_info.ip.addr == 0);

    ESP_LOGI(TAG, "Conectado! IP do ESP32: " IPSTR, IP2STR(&ip_info.ip));
    
    // Cria socket TCP
    ESP_LOGI(TAG, "Criando socket TCP...");
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Não foi possível criar o socket");
        return;
    }

    //Desativar Nagle para menor latência
    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    // Configura destino
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(DEST_PORT);
    dest_addr.sin_addr.s_addr = inet_addr(DEST_IP);

    ESP_LOGI(TAG, "Conectando ao servidor %s:%d...", DEST_IP, DEST_PORT);
    if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        ESP_LOGE(TAG, "Falha na conexão com o servidor");
        close(sock);
        return;
    }
    ESP_LOGI(TAG, "Conectado!");
    
    // Inicializa em 44100 só pra ativar o microfone
    ESP_LOGI(TAG, "Inicializando I2S em 44.1 kHz...");
    //i2s_init_pdm(SAMPLE_RATE_STD, BIT_DEPTH_STD);
    i2s_init_std(SAMPLE_RATE_STD, BIT_DEPTH_STD, false);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Desliga e deleta o canal
    i2s_channel_disable(rx_handle);
    i2s_del_channel(rx_handle);
    vTaskDelay(pdMS_TO_TICKS(1000));

    //Reconfigura para 78125 Hz
    ESP_LOGI(TAG, "Reconfigurando I2S para 78.125 Hz...");
    i2s_init_std(SAMPLE_RATE_ULT, BIT_DEPTH_ULT, true);
    vTaskDelay(pdMS_TO_TICKS(1000));

    TickType_t start = xTaskGetTickCount();

    //while (1) {
    while ((xTaskGetTickCount() - start) < duracao) {
        size_t bytes_read = 0;
        esp_err_t res = i2s_channel_read(rx_handle, dataBuffer, DATA_BUFFER_SIZE, &bytes_read, portMAX_DELAY);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Erro na leitura do I2S: %s", esp_err_to_name(res));
            continue;
        }
        
        // Envia dados via TCP
          int sent = send(sock, dataBuffer, bytes_read, MSG_DONTWAIT);
        if (sent < 0) {
            ESP_LOGW(TAG, "Falha/sem espaço no envio TCP (errno=%d)", errno);
            vTaskDelay(1);  // dá um respiro e tenta de novo
            continue;
        }

        //printf("%02x %02x %02x %02x %02x %02x %02x\n",
        //    dataBuffer[0], dataBuffer[1], dataBuffer[2],
        //    dataBuffer[3], dataBuffer[4], dataBuffer[5],
        //    dataBuffer[6]);

            

        vTaskDelay(pdMS_TO_TICKS(10));  
    }

    ESP_LOGI(TAG, "Aquisição finalizada!");
    vTaskDelay(pdMS_TO_TICKS(100));
    close(sock);

    */

    // Fila (copia blocos inteiros)
    //xQueue = xQueueCreate(QUEUE_ITEMS, sizeof(block_t));
    //if (!xQueue) { ESP_LOGE(TAG, "Falha ao criar fila"); return; }

    // Cria fila de entrada (blocos de áudio)
    xQueue = xQueueCreate(QUEUE_ITEMS, sizeof(block_t));
    if (!xQueue) {
        ESP_LOGE(TAG, "Falha ao criar fila xQueue");
        return;
    }

    // Loga o heap livre após a criação das filas.
    // Útil para diagnóstico de consumo de memória e depuração durante o desenvolvimento.
    //ESP_LOGI(TAG, "heap livre pós filas: %u", (unsigned)esp_get_free_heap_size());

    /*
    // Cria filtro
    filtro_t *filt = filtro_create(CIC_R, CIC_N, CIC_M, fir_coeffs, FIR_LEN);
    if (!filt) {
        ESP_LOGE(TAG, "filtro_create falhou");
        return;
    }
    */

    // Tasks
    xTaskCreatePinnedToCore(task_i2s_capture,"i2s_cap",   8192, NULL, 10, NULL, CORE_I2S);
    //xTaskCreatePinnedToCore(task_process_filter,"proc_flt",  16384, filt, 10, NULL, CORE_I2S);
    //xTaskCreatePinnedToCore(task_tcp_send,"tcp_send",  8192, NULL, 9, NULL, CORE_TX);
    xTaskCreatePinnedToCore(task_https_send,"https_send",  12288, NULL, 9, NULL, CORE_TX);

}