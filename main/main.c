// https://github.com/espressif/ESP8266_RTOS_SDK
// https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"

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

// Default Access Point IP address: 192.168.4.1
#define AP_SSID "light-and-fan"
#define AP_PASS "automation"
#define MAX_CONN 4

// #define MAX_CRED 8
#define MAX_STATION_ATTEMPTS 2
#define MAX_STATION_DELAYED_ATTEMPTS 8

// Helpers ==========================================================

#define STR1(x) #x
#define STR(x) STR1(x)

// #define FIELD_SIZE(t,f) (sizeof(((t*)0)->f))

// #define TEST(str, ...)
//   printf(STR(__LINE__) ": " str "\n", ##__VA_ARGS__);

#define CHECK_OK_DEBUG

#ifndef CHECK_OK_DEBUG
#define CHECK_OK(x) \
  if ((x) != ESP_OK) { goto err; }
#else
#define CHECK_OK(x) \
  if ((x) != ESP_OK) { puts(STR(__LINE__) ": " #x " != ESP_OK"); goto err; }
#endif

#define CHECK_OK_VERBOSE(x) { \
  const int ret = (x); \
  printf(STR(__LINE__) ": " #x " == %d\n", ret); \
  if (ret != ESP_OK) { goto err; } \
}

// #define MIN(a, b) ((a) < (b) ? a : b)
// #define MAX(a, b) ((a) > (b) ? a : b)

// Globals ==========================================================
// ESP8266_RTOS_SDK/components/esp8266/include/esp_wifi_types.h
// store last 8 successfully used access point credentials
// #define MAX_SSID_LEN 32
// #define MAX_PASS_LEN 64
// #define MAX_PASS_LEN MAX_PASSPHRASE_LEN

// ==================================================================

#include "part_gpio.h"
/* #include "part_nvs.h" */
#include "part_server.h"
#include "part_wifi.h"

void app_main(void) {
  init_gpio();
  /* init_nvs(); */
  init_server();
}
