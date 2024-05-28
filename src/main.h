#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>
#include <ds1307.h>
#include <stdbool.h>
#include <sys/param.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_task_wdt.h"
#include "driver/gpio.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "cJSON.h"

#include "dht11.h"
#include "lora.h"
#include "ili9340.h"
#include "fontx.h"
#include "lora.h"
#include "credentials.h"
#include "certificate.h"

static const char *TAG = "SERVER";
RTC_NOINIT_ATTR int led_status_value;
static char *response_buffer = NULL;
static int response_buffer_len = 0;

typedef struct {
	int x_center;
	int y_center;
	int x_low;
	int y_low;
	int x_high;
	int y_high;
	int radius;
	char text[32];
	bool selected;//teste
} AREA_t;

char* getMessageId(uint8_t *buf);
void DHT11_sensor();
void task_ds1307();
void initialize_spiffs();
void initialize_lora_module();
void wifi_connection();
void patch_rest_function(void *pvParameters);
void patch_sensors(void);
void patch_stats (void);
void patch_lora_data (void);
void get_control_status(void);
void display_system_info(void);
void send_led_status_via_lora(void);
void task_send_led_status(void *pvParameters);
void task_rx(void *pvParameters);
void TouchCalibration(TFT_t * dev, FontxFile *fx, int width, int height);
void TouchMenuTest(TFT_t *dev, FontxFile *fx, int width, int height, TickType_t timeout);
void ILI9341(void *pvParameters);
void TouchMakeMenu(TFT_t * dev, int page, FontxFile *fx, int width, int height, AREA_t *area, /*uint8_t *message, uint8_t *message1,*/ 
					uint8_t *datetime);
void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
bool TouchGetCalibration(TFT_t * dev);
esp_err_t custom_event_handler(esp_http_client_event_handle_t evt);
esp_err_t client_event_get_handler(esp_http_client_event_handle_t evt);
esp_err_t mountSPIFFS(char * path, char * label, int max_files);
esp_err_t ConvertCoordinate(TFT_t * dev, int xp, int yp, int *xpos, int *ypos);

#endif