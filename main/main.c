// https://github.com/espressif/ESP8266_RTOS_SDK
// https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"

// #include "esp8266/gpio_struct.h"
#include "driver/gpio.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_timer.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

// Config ===========================================================

#define LED_PIN 2
#define LIGHT_PIN 5
#define FAN_PIN 4
#define LIGHT_SWITCH_PIN 13
#define FAN_SWITCH_PIN 14

// default Access Point IP address: 192.168.4.1
#define AP_SSID  "light-and-fan"
#define AP_PASS  "automation"
#define MAX_CONN 4

#define MAX_CRED 8

// embedded static files
extern const uint8_t index_page[] asm("_binary_index_html_gz_start");
extern const uint8_t index_page_end[] asm("_binary_index_html_gz_end");

// Helpers ==========================================================

#define STR1(x) #x
#define STR(x) STR1(x)

// #define FIELD_SIZE(t,f) (sizeof(((t*)0)->f))

#define VERBOSITY 1

#define CHECK_OK_VERBOSE(x) \
  if ((x) != ESP_OK) { puts(STR(__LINE__) ": " #x " != ESP_OK"); goto err; }
#define CHECK_OK(x) \
  if ((x) != ESP_OK) { goto err; }

#if VERBOSITY >= 1
#  define CHECK_OK_1 CHECK_OK_VERBOSE
#else
#  define CHECK_OK_1 CHECK_OK
#endif

#if VERBOSITY >= 2
#  define CHECK_OK_2 CHECK_OK_VERBOSE
#else
#  define CHECK_OK_2 CHECK_OK
#endif

#define MIN(a, b) ((a) < (b) ? a : b)
#define MAX(a, b) ((a) > (b) ? a : b)

#include "part_gpio.h"
#include "part_nvs.h"
#include "part_server.h"

void app_main(void) {
  init_gpio();
  init_nvs();
  init_server();
}
