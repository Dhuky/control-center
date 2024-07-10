#include "main.h"

static const char *LCD_TFT = "painel_LCD", *SPIFFS = "spiffs", *LORA = "LoRa";
uint8_t *last_conection, *work_status, *temperature, *humidity;
//uint8_t temperature = 0, humidity = 0;
char memory_info[50], uptime_info[50], datetime[50];
double temp_double, humi_double;
QueueHandle_t xQueueDatetime;
uint32_t total_heap_size;
bool SET_TIME = false; 
int rssi;

//double param1_double = 35; // Substitua pelos dados do seu sensor

void app_main() {

    nvs_flash_init();

    wifi_connection();

	total_heap_size = esp_get_free_heap_size();
	xQueueDatetime = xQueueCreate(10, sizeof(uint8_t)*256);

	ESP_ERROR_CHECK(i2cdev_init());

	initialize_spiffs();

	vTaskDelay(2000 / portTICK_PERIOD_MS);

	initialize_lora_module();

	//xTaskCreate(ILI9341, "ILI9341", 1024*6, NULL, 5, NULL);

    xTaskCreate(&task_rx, "task_rx", 1024*3, NULL, 2, NULL);

	//xTaskCreate(task_send_led_status, "task_send_led_status", 4096, NULL, 2, NULL);

	xTaskCreate(task_ds1307, "ds1307_test", 4096, NULL, 5, NULL);

	xTaskCreate(&DHT11_sensor, "DHT11_sensor", 1024*2, NULL, 2, NULL);

	xTaskCreate(&patch_rest_function, "post_rest_task", 4096, NULL, 5, NULL);

	display_system_info();

}

void initialize_spiffs(){

	ESP_LOGI(SPIFFS, "Initialize NVS");
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK( err );

	ESP_LOGI(SPIFFS, "Initializing SPIFFS");
	esp_err_t ret;
	ret = mountSPIFFS("/spiffs", "storage", 30);
	if (ret != ESP_OK) return;
	vTaskDelay(100 / portTICK_PERIOD_MS);
}

void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        ESP_LOGI("WIFI","WiFi connecting ... \n");
        break;
    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI("WIFI","WiFi connected ... \n");
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        ESP_LOGI("WIFI","WiFi lost connection ... \n");
        break;
    case IP_EVENT_STA_GOT_IP:
        ESP_LOGI("WIFI","WiFi got IP ... \n\n");
        break;
    default:
        break;
    }
}

void wifi_connection(){

    esp_err_t err;

    // 1 - Wi-Fi/LwIP Init Phase
    err = esp_netif_init(); // TCP/IP initiation
    if (err != ESP_OK) {
        ESP_LOGI("WIFI","Error in esp_netif_init: %d\n", err);
        return;
    }
	vTaskDelay(100 / portTICK_PERIOD_MS); // Delay for 1 second

    err = esp_event_loop_create_default(); // event loop
    if (err != ESP_OK) {
        ESP_LOGI("WIFI","Error in esp_event_loop_create_default: %d\n", err);
        return;
    }
	vTaskDelay(100 / portTICK_PERIOD_MS); // Delay for 1 second

    esp_netif_create_default_wifi_sta(); // WiFi station
	vTaskDelay(100 / portTICK_PERIOD_MS); // Delay for 1 second

    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wifi_initiation);
    if (err != ESP_OK) {
        ESP_LOGI("WIFI","Error in esp_wifi_init: %d\n", err);
        return;
    }
	vTaskDelay(100 / portTICK_PERIOD_MS); // Delay for 1 second

    // 2 - Wi-Fi Configuration Phase
    err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGI("WIFI","Error in esp_event_handler_register for WIFI_EVENT: %d\n", err);
        return;
    }

    err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGI("WIFI","Error in esp_event_handler_register for IP_EVENT: %d\n", err);
        return;
    }

    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD
        }
    };

    err = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);
    if (err != ESP_OK) {
        ESP_LOGI("WIFI","Error in esp_wifi_set_config: %d\n", err);
        return;
    }
	vTaskDelay(100 / portTICK_PERIOD_MS); // Delay for 1 second

    // 3 - Wi-Fi Start Phase
    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGI("WIFI","Error in esp_wifi_start: %d\n", err);
        return;
    }
	vTaskDelay(100 / portTICK_PERIOD_MS); // Delay for 1 second

    // 4- Wi-Fi Connect Phase
    err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGI("WIFI","Error in esp_wifi_connect: %d\n", err);
        return;
    }
}

void task_send_led_status(void *pvParameters) {
    while (1) {
        // Mensagem a ser enviada
        char message[] = "teste";

        // Enviar a mensagem via LoRa
        lora_send_packet((uint8_t *)message, strlen(message));

        // Logar a mensagem enviada
        ESP_LOGI("LORA", "Test message sent: %s", message);

        // Aguardar 5 segundos antes de enviar a próxima mensagem
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

esp_err_t client_event_get_handler(esp_http_client_event_handle_t evt){

    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        ESP_LOGI("SERVER", "HTTP_EVENT_ON_DATA: %.*s\n", evt->data_len, (char *)evt->data);
        break;

    default:
        break;
    }
    return ESP_OK;
}

void patch_rest_function(void *pvParameters){

	while(1){
		vTaskDelay(60000 / portTICK_PERIOD_MS);

		patch_sensors();
		patch_stats();
		patch_lora_data();
		get_control_status();
	}
}

void patch_sensors(void){

    esp_http_client_config_t config_patch = {
        .url = "https://firestore.googleapis.com/v1/projects/central-data-b7907/databases/(default)/documents/central_data/sensors",
        .method = HTTP_METHOD_PATCH,
        .cert_pem = (const char *)certificate_pem,
        .event_handler = client_event_get_handler
    };
    esp_http_client_handle_t client = esp_http_client_init(&config_patch);

	char post_data[128];
    sprintf(post_data, "{\"fields\":{\"temperature (°C)\":{\"doubleValue\":\"%.2f\"},\"humidity (%%)\":{\"doubleValue\":\"%.2f\"}}}", 
	temp_double, humi_double);

    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_http_client_set_header(client, "Content-Type", "application/json");

    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

void patch_stats (void){

	esp_http_client_config_t config_patch = {
		.url = "https://firestore.googleapis.com/v1/projects/central-data-b7907/databases/(default)/documents/central_data/stats",
		.method = HTTP_METHOD_PATCH,
		.cert_pem = (const char *)certificate_pem,
		.event_handler = client_event_get_handler
	};
	esp_http_client_handle_t client = esp_http_client_init(&config_patch);

	char post_data[256];
	sprintf(post_data, "{\"fields\":{\"Timestamp\":{\"stringValue\":\"%s\"},\"Uptime\":{\"stringValue\":\"%s\"}}}", 
	datetime, uptime_info);

	esp_http_client_set_post_field(client, post_data, strlen(post_data));
	esp_http_client_set_header(client, "Content-Type", "application/json");

	esp_http_client_perform(client);
	esp_http_client_cleanup(client);
}

void patch_lora_data (void){

	esp_http_client_config_t config_patch_no_d1 = {
		.url = "https://firestore.googleapis.com/v1/projects/central-data-b7907/databases/(default)/documents/central_data/no_d1",
		.method = HTTP_METHOD_PATCH,
		.cert_pem = (const char *)certificate_pem,
		.event_handler = client_event_get_handler
	};
	esp_http_client_handle_t client = esp_http_client_init(&config_patch_no_d1);

	char post_data_no_d1[256];
	sprintf(post_data_no_d1, "{\"fields\":{\"Last_conection\":{\"stringValue\":\"%s\"},\"Status\":{\"stringValue\":\"%s\"}}}", 
	last_conection, work_status);

	esp_http_client_set_post_field(client,post_data_no_d1, strlen(post_data_no_d1));
	esp_http_client_set_header(client, "Content-Type", "application/json");

	esp_http_client_perform(client);
	esp_http_client_cleanup(client);
}

void get_control_status(void){

	esp_http_client_config_t config_get = {
		.url = "https://firestore.googleapis.com/v1/projects/central-data-b7907/databases/(default)/documents/central_data/schedules",
		.method = HTTP_METHOD_GET,
		.cert_pem = (const char *)certificate_pem,
		.event_handler = custom_event_handler   //client_event_get_handler // custom_event_handler
	};

	esp_http_client_handle_t client = esp_http_client_init(&config_get);
	esp_http_client_perform(client);
	esp_http_client_cleanup(client);
}

esp_err_t custom_event_handler(esp_http_client_event_handle_t evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);

            // Allocate or reallocate buffer to accommodate new data
            response_buffer = realloc(response_buffer, response_buffer_len + evt->data_len + 1);
            if (response_buffer == NULL) {
                ESP_LOGE(TAG, "Failed to allocate memory for response buffer");
                return ESP_FAIL;
            }

            // Append new data to the buffer
            memcpy(response_buffer + response_buffer_len, evt->data, evt->data_len);
            response_buffer_len += evt->data_len;
            response_buffer[response_buffer_len] = '\0'; // Null-terminate the buffer

            break;

        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");

            // Parse the complete JSON response
            cJSON *json = cJSON_Parse(response_buffer);
            if (json != NULL) {
                // Verificar se "fields" existe
                cJSON *fields = cJSON_GetObjectItemCaseSensitive(json, "fields");
                if (fields != NULL) {
                    // Verificar se "led_status" existe dentro de "fields"
                    cJSON *led_status = cJSON_GetObjectItemCaseSensitive(fields, "led_status");
                    if (led_status != NULL) {
                        // Verificar se "integerValue" existe dentro de "led_status"
                        cJSON *integerValue = cJSON_GetObjectItemCaseSensitive(led_status, "integerValue");
                        if (integerValue != NULL) {
                            led_status_value = atoi(integerValue->valuestring);
                            ESP_LOGI("led_status", "led_status_value: %d", led_status_value);
                        } else {
                            ESP_LOGE("led_status", "Campo 'integerValue' não encontrado");
                        }
                    } else {
                        ESP_LOGE("led_status", "Campo 'led_status' não encontrado");
                    }
                } else {
                    ESP_LOGE("get_control_status", "Campo 'fields' não encontrado");
                }
                cJSON_Delete(json);
            } else {
                ESP_LOGE("get_control_status", "Falha ao interpretar JSON");
            }

            // Clean up response buffer
            free(response_buffer);
            response_buffer = NULL;
            response_buffer_len = 0;

            break;

        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            if (response_buffer != NULL) {
                free(response_buffer);
                response_buffer = NULL;
                response_buffer_len = 0;
            }
            break;

        default:
            break;
    }
    return ESP_OK;
}

void initialize_lora_module(){

	ESP_LOGI(LORA, "Initializing LoRa module");
	if (lora_init() == 0) {
		ESP_LOGE(LORA, "Does not recognize the module");
		while(1) {
			ESP_LOGI(LORA, "Stuck in error loop - LoRa module not recognized");
			vTaskDelay(pdMS_TO_TICKS(1000)); //vTaskDelay(1);
		}
	}
    ESP_LOGI(LORA, "Frequency is 915MHz");
	lora_set_frequency(915e6); // 915MHz
	//esp_task_wdt_reset();
	vTaskDelay(pdMS_TO_TICKS(100));

    lora_enable_crc();
	//esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(100));  // Adiciona um pequeno delay de 100ms
	int cr = 1;
	int bw = 7;
	int sf = 7;

    lora_set_coding_rate(cr);
	ESP_LOGI(LORA, "coding_rate=%d", cr);
	//esp_task_wdt_reset();
	vTaskDelay(pdMS_TO_TICKS(100));

	lora_set_bandwidth(bw);
	ESP_LOGI(LORA, "bandwidth=%d", bw);
	//esp_task_wdt_reset();
	vTaskDelay(pdMS_TO_TICKS(100));
	
	lora_set_spreading_factor(sf);
	ESP_LOGI(LORA, "spreading_factor=%d", sf);
	//esp_task_wdt_reset();
	vTaskDelay(pdMS_TO_TICKS(100));

	ESP_LOGI(LORA, "LoRa module initialized successfully");
}

void DHT11_sensor(){ //Configurações do sensor de temperatura e umidade

	temperature = pvPortMalloc(50);  // Aloca memória para a mensagem
	humidity = pvPortMalloc(50);
	DHT11_init(GPIO_NUM_4);

	while(1){
		sprintf((char *)temperature, "%dC", DHT11_read().temperature);
		temp_double = DHT11_read().temperature;
		sprintf((char *)humidity, "%d%%", DHT11_read().humidity);
		humi_double = DHT11_read().humidity;
		vTaskDelay(pdMS_TO_TICKS(5000));
	}
}

char* getMessageId(uint8_t *buf){//Tratamento das mensagens recebidas via ID

    static char messageId[50];

    if(strncmp((char *)buf, "Ult.conexao:", 5) == 0){
        strcpy(messageId, "Ult.conexao");
		ESP_LOGI("MESSAGE_ID", "getMessageId: 'Ult.conexao' found");

    }else if(strncmp((char *)buf, "Status:", 7) == 0){
        strcpy(messageId, "Status");
		ESP_LOGI("MESSAGE_ID", "getMessageId: 'Status' found");

    }else{
        strcpy(messageId, "Unknown");
		ESP_LOGI("MESSAGE_ID", "getMessageId: Unknown message");

    }
    return messageId;
}

void send_led_status_via_lora() {
    // Formatar a mensagem
    uint8_t buf[256];
    int send_len = snprintf((char *)buf, sizeof(buf), "LED: %d", led_status_value);

    // Enviar o pacote via LoRa
    lora_send_packet(buf, send_len);

    // Logar a mensagem enviada
    ESP_LOGI("LORA", "%d byte packet sent: [%.*s]", send_len, send_len, buf);
}

void task_rx(void *pvParameters){//Recepção de pacotes LoRa

	ESP_LOGI(LORA, "Start");
	uint8_t buf[256]; // Maximum Payload size of SX1276/77/78/79 is 255
	last_conection = pvPortMalloc(256);  // Aloca memória para a mensagem
	work_status = pvPortMalloc(256);

	if (last_conection == NULL) {
    		ESP_LOGE(LORA, "Failed to allocate memory for message");
    	return;
	}
    strcpy((char *)last_conection, "N/A"); 
    strcpy((char *)work_status, "N/A");

	while(1) {
		memset(buf, 0, sizeof(buf));  // Limpa o buffer antes de receber os dados
		lora_receive(); 
		if (lora_received()) {
			int rxLen = lora_receive_packet(buf, sizeof(buf));
			ESP_LOGI(LORA, "%d byte packet received:[%.*s]", rxLen, rxLen, buf);

			ESP_LOGI(LORA, "Buffer: %s", buf);

			char* messageId = getMessageId(buf);
			ESP_LOGI(LORA, "Message ID: %s", messageId);

			if(strcmp(messageId, "Status") == 0){
				char *ptr = strchr((char *)buf, ':');
				send_led_status_via_lora();//////////////////////////////
				if(ptr != NULL){
					memmove(work_status, ptr + 1, strlen(ptr + 1) + 1);
					ESP_LOGI(LORA, "message1 %s", work_status);
				}
			}

			if(strcmp(messageId, "Ult.conexao") == 0){
				char *ptr = strchr((char *)buf, ':');
				if(ptr != NULL){
					memmove(last_conection, ptr + 1, strlen(ptr + 1) + 1);
					ESP_LOGI(LORA, "message %s", last_conection);
				}
			}
			rssi = lora_packet_rssi();
			ESP_LOGI(LORA, "RSSI: %d", rssi);

            // Verificar se o status é 'online' e enviar a mensagem do LED
            // if (strcmp((char *)work_status, "online") == 0) { // Converter para char*
            //     send_led_status_via_lora();
			// }
		}
		vTaskDelay(1); // Avoid WatchDog alerts
	} // end while
}

void task_ds1307(){//Configurações de data e hora locais

	char *days_of_the_week[] = {"Dom", "Seg", "Ter", "Qua", "Qui", "Sex","Sab"};
	char *months[] = {"", "Jan", "Fev", "Mar", "Abr", "Mai", "Jun", "Jul", "Ago", "Set", "Out", "Nov", "Dez"};

	strcpy((char *)datetime, "Aguarde...");//char datetime[256];
    i2c_dev_t i2c_dev;
    memset(&i2c_dev, 0, sizeof(i2c_dev_t));
    ESP_ERROR_CHECK(ds1307_init_desc(&i2c_dev, 0, 21, 22));//lolin_s3_pro 9 sda, 10scl - - devkit v1 21sda 22scl

	//#ifdef CONFIG_SET_TIME
	if(SET_TIME){
        struct tm time = {
            .tm_year = 124, //(2022 - 1900)
            .tm_mon  = 7,   //(1 a 12)
            .tm_mday = 8,   
            .tm_hour = 17, 
			.tm_wday = 2,  //(1 a 7)
            .tm_min  = 11,
            .tm_sec  = 0
        };
        ESP_ERROR_CHECK(ds1307_set_time(&i2c_dev, &time));
    }
	//#endif

    while (1){
        struct tm time;
        ds1307_get_time(&i2c_dev, &time);
        sprintf(datetime, "%s, %02d de %s %02d:%02d", days_of_the_week[time.tm_wday], time.tm_mday, months[time.tm_mon], time.tm_hour,
        time.tm_min);

		xQueueSend(xQueueDatetime, &datetime, 0);
		ESP_LOGI("DATE_AND_TIME", "Data successfully sent to the queue");
		vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

esp_err_t mountSPIFFS(char * path, char * label, int max_files) {//Inicialização do sistema de arquivos internos do Esp32
	esp_vfs_spiffs_conf_t conf = {
		.base_path = path,
		.partition_label = label,
		.max_files = max_files,
		.format_if_mount_failed =true
	};

	// Use settings defined above toinitialize and mount SPIFFS filesystem.
	// Note: esp_vfs_spiffs_register is anall-in-one convenience function.
	esp_err_t ret = esp_vfs_spiffs_register(&conf);

	if (ret != ESP_OK) {
		if (ret ==ESP_FAIL) {
			ESP_LOGE(SPIFFS, "Failed to mount or format filesystem");
		} else if (ret== ESP_ERR_NOT_FOUND) {
			ESP_LOGE(SPIFFS, "Failed to find SPIFFS partition");
		} else {
			ESP_LOGE(SPIFFS, "Failed to initialize SPIFFS (%s)",esp_err_to_name(ret));
		}
		return ret;
	}

	size_t total = 0, used = 0;
	ret = esp_spiffs_info(conf.partition_label, &total, &used);
	if (ret != ESP_OK) {
		ESP_LOGE(SPIFFS,"Failed to get SPIFFS partition information (%s)",esp_err_to_name(ret));
	} else {
		ESP_LOGI(SPIFFS,"Mount %s to %s success", path, label);
		ESP_LOGI(SPIFFS,"Partition size: total: %d, used: %d", total, used);
	}

	return ret;
}

bool TouchGetCalibration(TFT_t * dev){//Verificação da existência de arquivos de calibração salvos na memória interna
	// Open NVS
	ESP_LOGI(__FUNCTION__, "Opening Non-Volatile Storage (NVS) handle... ");
	nvs_handle_t my_handle;
	esp_err_t err;
	err = nvs_open("storage", NVS_READWRITE, &my_handle);
	if (err != ESP_OK) {
		ESP_LOGE(__FUNCTION__, "Error (%s) opening NVS handle!", esp_err_to_name(err));
		return false;
	}
	ESP_LOGI(__FUNCTION__, "Opening Non-Volatile Storage (NVS) handle done");

	// Read NVS
	int16_t calibration;
	err = nvs_get_i16(my_handle, "calibration", &calibration);
	ESP_LOGI(__FUNCTION__, "nvs_get_i16(calibration)=%d", err);
	if (err != ESP_OK) {
		nvs_close(my_handle);
		return false;
	}
		
	err = nvs_get_i16(my_handle, "touch_min_xc", &dev->_min_xc);
	if (err != ESP_OK) {
		nvs_close(my_handle);
		return false;
	}
	err = nvs_get_i16(my_handle, "touch_min_yc", &dev->_min_yc);
	if (err != ESP_OK) {
		nvs_close(my_handle);
		return false;
	}
	err = nvs_get_i16(my_handle, "touch_max_xc", &dev->_max_xc);
	if (err != ESP_OK) {
		nvs_close(my_handle);
		return false;
	}
	err = nvs_get_i16(my_handle, "touch_max_yc", &dev->_max_yc);
	if (err != ESP_OK) {
		nvs_close(my_handle);
		return false;
	}
	err = nvs_get_i16(my_handle, "touch_min_xp", &dev->_min_xp);
	if (err != ESP_OK) {
		nvs_close(my_handle);
		return false;
	}
	err = nvs_get_i16(my_handle, "touch_min_yp", &dev->_min_yp);
	if (err != ESP_OK) {
		nvs_close(my_handle);
		return false;
	}
	err = nvs_get_i16(my_handle, "touch_max_xp", &dev->_max_xp);
	if (err != ESP_OK) {
		nvs_close(my_handle);
		return false;
	}
	err = nvs_get_i16(my_handle, "touch_max_yp", &dev->_max_yp);
	if (err != ESP_OK) {
		nvs_close(my_handle);
		return false;
	}

	// Close NVS
	nvs_close(my_handle);

	return true;
}

void TouchCalibration(TFT_t * dev, FontxFile *fx, int width, int height){//Função de calibração do touch do display LCD
	if (dev->_calibration == false) return;

	// Read Touch Calibration from NVS
	bool ret = TouchGetCalibration(dev);
	ESP_LOGI(__FUNCTION__, "TouchGetCalibration=%d", ret);
	if (ret) {
		ESP_LOGI(__FUNCTION__, "Restore Touch Calibration from NVS");
		dev->_calibration = false;
		return;
	}

	// get font width & height
	uint8_t buffer[FontxGlyphBufSize];
	uint8_t fontWidth;
	uint8_t fontHeight;
	GetFontx(fx, 0, buffer, &fontWidth, &fontHeight);
	ESP_LOGD(__FUNCTION__,"fontWidth=%d fontHeight=%d",fontWidth,fontHeight);

	uint8_t ascii[24];
	int xpos = 0;
	int ypos = 0;

	// Calibration
	lcdFillScreen(dev, BLACK);
	//dev->_min_xc = 15; 
	dev->_min_xc = 10;                                                                                             // teste posição
	//dev->_min_yc = 15;
	dev->_min_yc = 10;                                                                                             // teste posição
	//lcdDrawFillCircle(dev, 10, 10, 10, CYAN);
	lcdDrawFillCircle(dev, dev->_min_xc, dev->_min_yc, 10, CYAN);
	strcpy((char *)ascii, "Calibration");
	ypos = ((height - fontHeight) / 2) - 1;
	xpos = (width + (strlen((char *)ascii) * fontWidth)) / 2;
	lcdSetFontDirection(dev, DIRECTION180);
	lcdDrawString(dev, fx, xpos, ypos, ascii, WHITE);
	ypos = ypos - fontHeight;
	int _xpos = xpos;
	for(int i=0;i<10;i++) {
		lcdDrawFillCircle(dev, _xpos, ypos, fontWidth/2, RED);
		_xpos = _xpos - fontWidth - 5;
	}

	int16_t xp = INT16_MIN;
	int16_t yp = INT16_MAX;
	int counter = 0;

	// Clear XPT2046
	int _xp;
	int _yp;
	while(1) {
		if (touch_getxy(dev, &_xp, &_yp) == false) break;
	} // end while

	while(1) {
		if (touch_getxy(dev, &_xp, &_yp) == false) continue;
		ESP_LOGI(__FUNCTION__, "counter=%d _xp=%d _yp=%d xp=%d yp=%d", counter, _xp, _yp, xp, yp);
		if (_xp > xp) xp = _xp;
		if (_yp < yp) yp = _yp;
		counter++;
		if (counter == 100) break;
		if ((counter % 10) == 0) {
			lcdDrawFillCircle(dev, xpos, ypos, fontWidth/2, GREEN);
			xpos = xpos - fontWidth - 5;
		vTaskDelay(100 / portTICK_PERIOD_MS);                                                                      //teste
		}
	} // end while
	ESP_LOGI(__FUNCTION__, "_min_xp=%d _min_yp=%d", xp, yp);
	//dev->_min_xp = xp;                                                                                             // teste posição
	dev->_min_xp = 160; //
	//dev->_min_yp = yp;                                                                                             // teste posição
	dev->_min_yp = 1880; //dev->_min_yp = 1896;


	// Clear XPT2046
	lcdFillScreen(dev, BLACK);
	while(1) {
		if (touch_getxy(dev, &_xp, &_yp) == false) break;
		vTaskDelay(100 / portTICK_PERIOD_MS);                                                                      //teste
	} // end while

	lcdFillScreen(dev, BLACK);
	dev->_max_xc = width-10;
	dev->_max_yc = height-10;
	//lcdDrawFillCircle(dev, width-10, height-10, 10, CYAN);
	lcdDrawFillCircle(dev, dev->_max_xc, dev->_max_yc, 10, CYAN);
	ypos = ((height - fontHeight) / 2) - 1;
	xpos = (width + (strlen((char *)ascii) * fontWidth)) / 2;
	lcdDrawString(dev, fx, xpos, ypos, ascii, WHITE);
	ypos = ypos - fontHeight;
	_xpos = xpos;
	for(int i=0;i<10;i++) {
		lcdDrawFillCircle(dev, _xpos, ypos, fontWidth/2, RED);
		_xpos = _xpos - fontWidth - 5;
	}

	xp = INT16_MAX;
	yp = INT16_MIN;
	counter = 0;
	while(1) {
		if (touch_getxy(dev, &_xp, &_yp) == false) continue;
		ESP_LOGI(__FUNCTION__, "counter=%d _xp=%d _yp=%d xp=%d yp=%d", counter, _xp, _yp, xp, yp);
		if (_xp < xp) xp = _xp;
		if (_yp > yp) yp = _yp;
		counter++;
		if (counter == 100) break;
		if ((counter % 10) == 0) {
			lcdDrawFillCircle(dev, xpos, ypos, fontWidth/2, GREEN);
			xpos = xpos - fontWidth - 5;
			vTaskDelay(100 / portTICK_PERIOD_MS);                                                                   //teste
		}
	} // end while
	ESP_LOGI(__FUNCTION__, "max_xp=%d max_yp=%d", xp, yp);
	//dev->_max_xp = xp;                                                                                              //teste calibração
	dev->_max_xp = 1744;
	//dev->_max_yp = yp;
	dev->_max_yp = 239; //dev->_max_yp = 239;                                                                        //teste calibração

	// Clear XPT2046
	lcdFillScreen(dev, BLACK);
	while(1) {
		if (touch_getxy(dev, &_xp, &_yp) == false) break;
		vTaskDelay(100 / portTICK_PERIOD_MS);                                                                         //teste
	} // end while
	dev->_calibration = false;

 #if CONFIG_SAVE_CALIBRATION
	// Open NVS
	ESP_LOGI(__FUNCTION__, "Opening Non-Volatile Storage (NVS) handle... ");
	nvs_handle_t my_handle;
	esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
	if (err != ESP_OK) {
		ESP_LOGE(__FUNCTION__, "Error (%s) opening NVS handle!", esp_err_to_name(err));
		return;
	}
	ESP_LOGI(__FUNCTION__, "Opening Non-Volatile Storage (NVS) handle done");

	// Write Touch Calibration to NVS
	err = nvs_set_i16(my_handle, "touch_min_xc", dev->_min_xc);
	if (err != ESP_OK) {
		ESP_LOGE(__FUNCTION__, "Error (%s) writing NVS handle!", esp_err_to_name(err));
		nvs_close(my_handle);
		return;
	}
	err = nvs_set_i16(my_handle, "touch_min_yc", dev->_min_yc);
	if (err != ESP_OK) {
		ESP_LOGE(__FUNCTION__, "Error (%s) writing NVS handle!", esp_err_to_name(err));
		nvs_close(my_handle);
		return;
	}
	err = nvs_set_i16(my_handle, "touch_max_xc", dev->_max_xc);
	if (err != ESP_OK) {
		ESP_LOGE(__FUNCTION__, "Error (%s) writing NVS handle!", esp_err_to_name(err));
		nvs_close(my_handle);
		return;
	}
	err = nvs_set_i16(my_handle, "touch_max_yc", dev->_max_yc);
	if (err != ESP_OK) {
		ESP_LOGE(__FUNCTION__, "Error (%s) writing NVS handle!", esp_err_to_name(err));
		nvs_close(my_handle);
		return;
	}
	err = nvs_set_i16(my_handle, "touch_min_xp", dev->_min_xp);
	if (err != ESP_OK) {
		ESP_LOGE(__FUNCTION__, "Error (%s) writing NVS handle!", esp_err_to_name(err));
		nvs_close(my_handle);
		return;
	}
	err = nvs_set_i16(my_handle, "touch_min_yp", dev->_min_yp);
	if (err != ESP_OK) {
		ESP_LOGE(__FUNCTION__, "Error (%s) writing NVS handle!", esp_err_to_name(err));
		nvs_close(my_handle);
		return;
	}
	err = nvs_set_i16(my_handle, "touch_max_xp", dev->_max_xp);
	if (err != ESP_OK) {
		ESP_LOGE(__FUNCTION__, "Error (%s) writing NVS handle!", esp_err_to_name(err));
		nvs_close(my_handle);
		return;
	}
	err = nvs_set_i16(my_handle, "touch_max_yp", dev->_max_yp);
	if (err != ESP_OK) {
		ESP_LOGE(__FUNCTION__, "Error (%s) writing NVS handle!", esp_err_to_name(err));
		nvs_close(my_handle);
		return;
	}
	err = nvs_set_i16(my_handle, "calibration", 1);
	if (err != ESP_OK) {
		ESP_LOGE(__FUNCTION__, "Error (%s) writing NVS handle!", esp_err_to_name(err));
		nvs_close(my_handle);
		return;
	}
	err = nvs_commit(my_handle);
	if (err != ESP_OK) {
		ESP_LOGE(__FUNCTION__, "Error (%s) commit NVS handle!", esp_err_to_name(err));
	}


	// Close NVS
	nvs_close(my_handle);
	ESP_LOGI(__FUNCTION__, "Write Touch Calibration done");
 #endif
}

void display_system_info(){//Tratamento de informações do sistema

	uint32_t free_heap_size;
	int64_t time_since_reset;

	while(1){
	free_heap_size = esp_get_free_heap_size();
	time_since_reset = esp_timer_get_time();
	double time_since_reset_seconds = time_since_reset / 1e6;

	int days = (int)time_since_reset_seconds / (24 *3600);
	time_since_reset_seconds = (int)time_since_reset_seconds % (24 *3600);
	int hours = (int)time_since_reset_seconds / 3600;
	time_since_reset_seconds = (int)time_since_reset_seconds % 3600;
	int minutes = (int)time_since_reset_seconds / 60;

	uint32_t used_heap_size = total_heap_size - free_heap_size;
	float memory_usage_percentage = ((float)used_heap_size / total_heap_size) * 100;

	sprintf(memory_info, "%.2f%%", memory_usage_percentage);
	sprintf(uptime_info, "%dd, %dh, %dm", days, hours, minutes);

	vTaskDelay(5000);
	}
}

void TouchMakeMenu(TFT_t * dev, int page, FontxFile *fx, int width, int height, AREA_t *area, /*uint8_t *message, uint8_t *message1,*/ 
					uint8_t *datetime) {//Desenho da interface gráfica no display LCD

	// get font width & height
	uint8_t buffer[FontxGlyphBufSize];
	uint8_t fontWidth;
	uint8_t fontHeight;
	GetFontx(fx, 0, buffer, &fontWidth, &fontHeight);
	ESP_LOGD(__FUNCTION__,"fontWidth=%d fontHeight=%d",fontWidth,fontHeight);

	uint8_t ascii[256];
	int x_center = width - (fontHeight * 2);
	lcdSetFontDirection(dev, DIRECTION90);
	lcdFillScreen(dev, BLACK);

	int y_center = height / 8 * 1; //Página inicial
	strcpy((char *)ascii, "Inicio");
	int x_text = x_center;
	int y_text = y_center - (strlen((char *)ascii) * fontWidth / 2);
	int x_rect = x_text + fontHeight;
	int y_rect = y_text + (strlen((char *)ascii) * fontWidth);
	area[0].x_low = x_text;
	area[0].x_high = x_rect;
	area[0].y_low = y_text;
	area[0].y_high = y_rect;
	ESP_LOGD(__FUNCTION__, "x_low=%d x_high=%d y_low=%d y_high=%d", area[0].x_low, area[0].x_high, area[0].y_low, area[0].y_high);
	if (page == 0) {
		lcdDrawFillRect(dev, x_text, y_text, x_rect, y_rect, RED);
		lcdDrawString(dev, fx, x_text, y_text, ascii, BLACK);

		//Escopo de coordenadas: lcdDrawRect(dev, x0, y0, x+, y+, COR);
		lcdDrawRect(dev, 145, 0, 180, 317, WHITE); //Caixa de data

		lcdDrawRect(dev, 72, 0, 140, 160, WHITE);//retângulo superior esquerdo
		lcdDrawString(dev, fx, 115, 60, (uint8_t *)"RAM", WHITE);

		lcdDrawRect(dev, 72, 162, 140, 317, WHITE);//retângulo superior direito
		lcdDrawString(dev, fx, 115, 205, (uint8_t *)"Uptime", WHITE);

		lcdDrawRect(dev, 0, 0, 70, 160, WHITE);//retângulo inferior esquerdo
		lcdDrawString(dev, fx, 45, 15, (uint8_t *)"Temperatura", WHITE);

		lcdDrawRect(dev, 0, 162, 70, 317, WHITE);//retângulo inferior direito
		lcdDrawString(dev, fx, 45, 200, (uint8_t *)"Umidade", WHITE);

		strcpy((char *)ascii, (char *)datetime);//Data e hora da central
		x_text = (width / 2) + 30; //-1
		y_text = (height / 2) - (strlen((char *)ascii) * fontWidth / 2);
		lcdDrawString(dev, fx, x_text, y_text, ascii, CYAN);

		strcpy((char *)ascii, (char *)memory_info);//Uso de RAM da central
		x_text = 90 - (fontWidth / 2); //-1
		y_text = 82 - (strlen((char *)ascii) * fontWidth / 2);
		lcdDrawString(dev, fx, x_text, y_text, ascii, CYAN);
		
		strcpy((char *)ascii, (char *)uptime_info);//Tempo desde o último reset da central
		x_text = 90 - (fontWidth / 2); //-1
		y_text = 240 - (strlen((char *)ascii) * fontWidth / 2);
		lcdDrawString(dev, fx, x_text, y_text, ascii, CYAN);

		strcpy((char *)ascii, (char *)temperature);//Temperatura local na central
		x_text = 15 - (fontWidth / 2); //-1
		y_text = 82 - (strlen((char *)ascii) * fontWidth / 2);
		lcdDrawString(dev, fx, x_text, y_text, ascii, CYAN);

		strcpy((char *)ascii, (char *)humidity);//Umidade local na central
		x_text = 15 - (fontWidth / 2); //-1
		y_text = 240 - (strlen((char *)ascii) * fontWidth / 2);
		lcdDrawString(dev, fx, x_text, y_text, ascii, CYAN);

	 } else {
		lcdDrawRect(dev, x_text, y_text, x_rect, y_rect, WHITE);
		lcdDrawString(dev, fx, x_text, y_text, ascii, WHITE);
	}

	y_center = height / 8 * 3;
	strcpy((char *)ascii, "No-D1");//Nó de automação número 1
	x_text = x_center;
	y_text = y_center - (strlen((char *)ascii) * fontWidth / 2);
	x_rect = x_text + fontHeight;
	y_rect = y_text + (strlen((char *)ascii) * fontWidth);
	area[1].x_low = x_text;
	area[1].x_high = x_rect;
	area[1].y_low = y_text;
	area[1].y_high = y_rect;
	ESP_LOGD(__FUNCTION__, "x_low=%d x_high=%d y_low=%d y_high=%d", area[0].x_low, area[0].x_high, area[0].y_low, area[0].y_high);
	if (page == 1) {
		lcdDrawFillRect(dev, x_text, y_text, x_rect, y_rect, RED);
		lcdDrawString(dev, fx, x_text, y_text, ascii, BLACK);

		//Escopo de coordenadas: lcdDrawRect(dev, x0, y0, x+, y+, COR);
		lcdDrawRect(dev, 100, 0, 180, 317, WHITE);//Caixa última conexão
		lcdDrawString(dev, fx, 155, 75, (uint8_t *)"Ultima conexao", WHITE);

		lcdDrawRect(dev, 10, 0, 90, 160, WHITE);//Retângulo inferior esquerdo
		lcdDrawString(dev, fx, 65, 43, (uint8_t *)"Status", WHITE);

		lcdDrawRect(dev, 10, 162, 90, 317, WHITE);//Retângulo inferior direito
		lcdDrawString(dev, fx, 65, 217, (uint8_t *)"RSSI", WHITE);

		strcpy((char *)ascii, (char *)last_conection); //Informações de hora local do nó
		x_text = (width / 2) - 5;
		y_text = 155 - (strlen((char *)ascii) * fontWidth / 2);
		lcdDrawString(dev, fx, x_text, y_text, ascii, CYAN);

		strcpy((char *)ascii, (char *)work_status); //Informações de status do nó
		x_text = 30; //-1
		y_text = 75 - (strlen((char *)ascii) * fontWidth / 2);
		lcdDrawString(dev, fx, x_text, y_text, ascii, CYAN);

		sprintf((char *)ascii, "%d", rssi); //Informações de RSSI da última mensagem LoRa
		x_text = 30; //-1
		y_text = 240 - (strlen((char *)ascii) * fontWidth / 2);
		lcdDrawString(dev, fx, x_text, y_text, ascii, CYAN);
	} else {
		lcdDrawRect(dev, x_text, y_text, x_rect, y_rect, WHITE);
		lcdDrawString(dev, fx, x_text, y_text, ascii, WHITE);
	}

	y_center = height / 8 * 5;
	strcpy((char *)ascii, "No-D2");
	x_text = x_center;
	y_text = y_center - (strlen((char *)ascii) * fontWidth / 2);
	x_rect = x_text + fontHeight;
	y_rect = y_text + (strlen((char *)ascii) * fontWidth);
	area[2].x_low = x_text;
	area[2].x_high = x_rect;
	area[2].y_low = y_text;
	area[2].y_high = y_rect;
	ESP_LOGD(__FUNCTION__, "x_low=%d x_high=%d y_low=%d y_high=%d", area[0].x_low, area[0].x_high, area[0].y_low, area[0].y_high);
	if (page == 2) {
		lcdDrawFillRect(dev, x_text, y_text, x_rect, y_rect, RED);
		lcdDrawString(dev, fx, x_text, y_text, ascii, BLACK);

		//Escopo de coordenadas: lcdDrawRect(dev, x0, y0, x+, y+, COR);
		lcdDrawRect(dev, 100, 0, 180, 317, WHITE);//Caixa última conexão
		lcdDrawString(dev, fx, 155, 75, (uint8_t *)"Ultima conexao", WHITE);

		lcdDrawRect(dev, 10, 0, 90, 160, WHITE);//Retângulo inferior esquerdo
		lcdDrawString(dev, fx, 65, 43, (uint8_t *)"Status", WHITE);

		lcdDrawRect(dev, 10, 162, 90, 317, WHITE);//Retângulo inferior direito
		lcdDrawString(dev, fx, 65, 217, (uint8_t *)"RSSI", WHITE);
	} else {
		lcdDrawRect(dev, x_text, y_text, x_rect, y_rect, WHITE);
		lcdDrawString(dev, fx, x_text, y_text, ascii, WHITE);
	}

	y_center = height / 8 * 7;
	strcpy((char *)ascii, "No-D3");
	x_text = x_center;
	y_text = y_center - (strlen((char *)ascii) * fontWidth / 2);
	x_rect = x_text + fontHeight;
	y_rect = y_text + (strlen((char *)ascii) * fontWidth);
	area[3].x_low = x_text;
	area[3].x_high = x_rect;
	area[3].y_low = y_text;
	area[3].y_high = y_rect;
	if (page == 3) {
		lcdDrawFillRect(dev, x_text, y_text, x_rect, y_rect, RED);
		lcdDrawString(dev, fx, x_text, y_text, ascii, BLACK);

		//Escopo de coordenadas: lcdDrawRect(dev, x0, y0, x+, y+, COR);
		lcdDrawRect(dev, 100, 0, 180, 317, WHITE);//Caixa última conexão
		lcdDrawString(dev, fx, 155, 75, (uint8_t *)"Ultima conexao", WHITE);

		lcdDrawRect(dev, 10, 0, 90, 160, WHITE);//Retângulo inferior esquerdo
		lcdDrawString(dev, fx, 65, 43, (uint8_t *)"Status", WHITE);

		lcdDrawRect(dev, 10, 162, 90, 317, WHITE);//Retângulo inferior direito
		lcdDrawString(dev, fx, 65, 217, (uint8_t *)"RSSI", WHITE);

		// strcpy((char *)ascii, "Hello Africa");
		// x_text = (width / 2) - 1;
		// y_text = (height / 2) - (strlen((char *)ascii) * fontWidth / 2);
		// lcdDrawString(dev, fx, x_text, y_text, ascii, CYAN);
	} else {
		lcdDrawRect(dev, x_text, y_text, x_rect, y_rect, WHITE);
		lcdDrawString(dev, fx, x_text, y_text, ascii, WHITE);
	}
}

esp_err_t ConvertCoordinate(TFT_t * dev, int xp, int yp, int *xpos, int *ypos){//Conversão da área do touch em pontos de toque
	float _xd = dev->_max_xp - dev->_min_xp;
	float _yd = dev->_max_yp - dev->_min_yp;
	float _xs = dev->_max_xc - dev->_min_xc;
	float _ys = dev->_max_yc - dev->_min_yc;
	ESP_LOGD(__FUNCTION__, "_xs=%f _ys=%f", _xs, _ys);

	// Determine if within range
	if (dev->_max_xp > dev->_min_xp) {
		if (xp < dev->_min_xp && xp > dev->_max_xp) return ESP_FAIL;
	} else {
		if (xp < dev->_max_xp && xp > dev->_min_xp) return ESP_FAIL;
	}
	if (dev->_max_yp > dev->_min_yp) {
		if (yp < dev->_min_yp && yp > dev->_max_yp) return ESP_FAIL;
	} else {
		if (yp < dev->_max_yp && yp > dev->_min_yp) return ESP_FAIL;
	}

	// Convert from position to coordinate
	//_xpos = ( (float)(_xp - dev->_min_xp) / _xd * _xs ) + 10;
	//_ypos = ( (float)(_yp - dev->_min_yp) / _yd * _ys ) + 10;
	*xpos = ( (float)(xp - dev->_min_xp) / _xd * _xs ) + dev->_min_xc;
	*ypos = ( (float)(yp - dev->_min_yp) / _yd * _ys ) + dev->_min_yc;
	ESP_LOGD(__FUNCTION__, "*xpos=%d *ypos=%d", *xpos, *ypos);
	return ESP_OK;
}

void TouchMenuTest(TFT_t *dev, FontxFile *fx, int width, int height, TickType_t timeout){//Teste de menu touch (apenas para testes mesmo)
    AREA_t area[4];
    int page = 0;
	//uint8_t message[256];
	//uint8_t message1[256];
	uint8_t datetime[256];
    TouchMakeMenu(dev, page, fx, width, height, area, /*message, message1,*/ datetime);

    // Inicializar uma estrutura de configuração para o TWDT
    esp_task_wdt_config_t config = {
        .timeout_ms = timeout * 1000, // Tempo limite em milissegundos
        .idle_core_mask = 0, // Máscara dos núcleos cuja tarefa ociosa deve ser inscrita na inicialização
        .trigger_panic = true // Indicar se deve causar pânico em caso de timeout do TWDT
    };
    esp_task_wdt_init(&config);// Inicializar e habilitar o TWDT com a configuração
    esp_task_wdt_add(NULL);// Adicionar a tarefa atual ao monitoramento do TWDT

    TickType_t lastTouched = xTaskGetTickCount();

    while (1) {
        esp_task_wdt_reset();  // Alimentar o watchdog

        int _xp = 0;
        int _yp = 0;
        if (touch_getxy(dev, &_xp, &_yp)) {
            int _xpos = 0;
            int _ypos = 0;

            // Corrigindo o número de argumentos para ConvertCoordinate
            esp_err_t ret = ConvertCoordinate(dev, _xp, _yp, &_xpos, &_ypos);
            if (ret == ESP_OK) {
                ESP_LOGD(LCD_TFT, "_xpos=%d _ypos=%d", _xpos, _ypos);
                lastTouched = xTaskGetTickCount();

                int newPage = -1;  // Inicializando com um valor impossível
                for (int index = 0; index < 4; index++) {
                    if (_xpos >= area[index].x_low && _xpos <= area[index].x_high &&
                        _ypos >= area[index].y_low && _ypos <= area[index].y_high) {
                        newPage = index;
                        break;  // Saia do loop assim que encontrar uma correspondência
                    }
                }

                // Verificar se há uma mudança de página antes de chamar TouchMakeMenu
                if (newPage != -1 && newPage != page) {
                    page = newPage;
                    TouchMakeMenu(dev, page, fx, width, height, area, /*message, message1,*/ datetime);
					ESP_LOGI(LCD_TFT, "Page changed to %d", page + 1);
                }
            }
        } else {
            TickType_t current = xTaskGetTickCount();
            if (current - lastTouched > timeout * 1000)//{ 
			break;  // Verificar o timeout em milissegundos
        }

		if(xQueueReceive(xQueueDatetime, &datetime, 0)){
        	ESP_LOGI("TouchMakeMenu", "Mensagem recebida da fila: %s", datetime);
        	TouchMakeMenu(dev, page, fx, width, height, area, /*message, message1,*/ datetime); // Atualiza o menu com a nova mensagem
    	}
        vTaskDelay(20 / portTICK_PERIOD_MS);  // Adicionar um pequeno atraso para evitar sobrecarga
    }
}

void ILI9341(void *pvParameters){//Task de gerenciamento do diplay LCD

	FontxFile fx16G[2];
	FontxFile fx24G[2];
	FontxFile fx32G[2];
	FontxFile fx32L[2];
	InitFontx(fx16G,"/spiffs/ILGH16XB.FNT",""); // 8x16Dot Gothic
	InitFontx(fx24G,"/spiffs/ILGH24XB.FNT",""); // 12x24Dot Gothic
	InitFontx(fx32G,"/spiffs/ILGH32XB.FNT",""); // 16x32Dot Gothic
	InitFontx(fx32L,"/spiffs/LATIN32B.FNT",""); // 16x32Dot Latinc

	FontxFile fx16M[2];
	FontxFile fx24M[2];
	FontxFile fx32M[2];
	InitFontx(fx16M,"/spiffs/ILMH16XB.FNT",""); // 8x16Dot Mincyo
	InitFontx(fx24M,"/spiffs/ILMH24XB.FNT",""); // 12x24Dot Mincyo
	InitFontx(fx32M,"/spiffs/ILMH32XB.FNT",""); // 16x32Dot Mincyo
	
	TFT_t dev;
 #if CONFIG_XPT2046_ENABLE_SAME_BUS
	ESP_LOGI(LCD_TFT, "Enable Touch Contoller using the same SPI bus as TFT");
	int XPT_MISO_GPIO = CONFIG_XPT_MISO_GPIO;
	int XPT_CS_GPIO = CONFIG_XPT_CS_GPIO;
	int XPT_IRQ_GPIO = CONFIG_XPT_IRQ_GPIO;
	int XPT_SCLK_GPIO = -1;
	int XPT_MOSI_GPIO = -1;
 #elif CONFIG_XPT2046_ENABLE_DIFF_BUS
	ESP_LOGI(TAG, "Enable Touch Contoller using the different SPI bus from TFT");
	int XPT_MISO_GPIO = CONFIG_XPT_MISO_GPIO;
	int XPT_CS_GPIO = CONFIG_XPT_CS_GPIO;
	int XPT_IRQ_GPIO = CONFIG_XPT_IRQ_GPIO;
	int XPT_SCLK_GPIO = CONFIG_XPT_SCLK_GPIO;
	int XPT_MOSI_GPIO = CONFIG_XPT_MOSI_GPIO;
 #else
	ESP_LOGI(TAG, "Disable Touch Contoller");
	int XPT_MISO_GPIO = -1;
	int XPT_CS_GPIO = -1;
	int XPT_IRQ_GPIO = -1;
	int XPT_SCLK_GPIO = -1;
	int XPT_MOSI_GPIO = -1;
 #endif
	spi_master_init(&dev, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_TFT_CS_GPIO, CONFIG_DC_GPIO, 
		CONFIG_RESET_GPIO, CONFIG_BL_GPIO, XPT_MISO_GPIO, XPT_CS_GPIO, XPT_IRQ_GPIO, XPT_SCLK_GPIO, XPT_MOSI_GPIO);

 #if CONFIG_ILI9225
	uint16_t model = 0x9225;
 #endif
 #if CONFIG_ILI9225G
	uint16_t model = 0x9226;
 #endif
 #if CONFIG_ILI9340
	uint16_t model = 0x9340;
 #endif
 #if CONFIG_ILI9341
	uint16_t model = 0x9341;
 #endif
 #if CONFIG_ST7735
	uint16_t model = 0x7735;
 #endif
 #if CONFIG_ST7796
	uint16_t model = 0x7796;
 #endif
	lcdInit(&dev, model, CONFIG_WIDTH, CONFIG_HEIGHT, CONFIG_OFFSETX, CONFIG_OFFSETY);

 #if CONFIG_INVERSION
	ESP_LOGI(TAG, "Enable Display Inversion");
	lcdInversionOn(&dev);
 #endif

 #if CONFIG_RGB_COLOR
	ESP_LOGI(TAG, "Change BGR filter to RGB filter");
	lcdBGRFilter(&dev);
 #endif

 #if CONFIG_XPT2046_ENABLE_SAME_BUS || CONFIG_XPT2046_ENABLE_DIFF_BUS
 #if CONFIG_XPT_CHECK
	TouchPosition(&dev, fx24G, CONFIG_WIDTH, CONFIG_HEIGHT, 1000);
 #endif
 #endif

 #if 0
	// for test
	while(1) {
 #if CONFIG_XPT2046_ENABLE_SAME_BUS || CONFIG_XPT2046_ENABLE_DIFF_BUS
		TouchCalibration(&dev, fx24G, CONFIG_WIDTH, CONFIG_HEIGHT);
		TouchPenTest(&dev, fx24G, CONFIG_WIDTH, CONFIG_HEIGHT, 1000);
		TouchKeyTest(&dev, fx32G, CONFIG_WIDTH, CONFIG_HEIGHT, 1000);
		TouchMenuTest(&dev, fx24G, CONFIG_WIDTH, CONFIG_HEIGHT, 1000);
		TouchMoveTest(&dev, fx24G, CONFIG_WIDTH, CONFIG_HEIGHT, 1000);
		TouchIconTest(&dev, fx24G, CONFIG_WIDTH, CONFIG_HEIGHT, 1000);
 #endif

		ArrowTest(&dev, fx16G, model, CONFIG_WIDTH, CONFIG_HEIGHT);
		WAIT;
	}
 #endif

 while(1) {

 #if CONFIG_XPT2046_ENABLE_SAME_BUS || CONFIG_XPT2046_ENABLE_DIFF_BUS
		TouchCalibration(&dev, fx24G, CONFIG_WIDTH, CONFIG_HEIGHT);
		
		TouchMenuTest(&dev, fx24G, CONFIG_WIDTH, CONFIG_HEIGHT, 1000);
		

 #ifdef ENABLE_PNG
		TouchIconTest(&dev, fx24G, CONFIG_WIDTH, CONFIG_HEIGHT, 1000);
 #endif

 #endif
	} // end while

	// never reach here
	vTaskDelete(NULL);
}