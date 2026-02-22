// https://github.com/espressif/ESP8266_RTOS_SDK
// https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/

#include <string.h>
#include <stdint.h>

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

// Default Access Point IP address: 192.168.4.1
#define AP_SSID "light-and-fan"
#define AP_PASS "automation"
#define MAX_CONN 4

#define MAX_STATION_ATTEMPTS 2
#define MAX_STATION_ATTEMPTS_DELAYED 8

// Helpers ==========================================================

#define STR1(x) #x
#define STR(x) STR1(x)

// #define FIELD_SIZE(t,f) (sizeof(((t*)0)->f))

#define ENABLE_TEST
#ifndef ENABLE_TEST
#define TEST(...)
#else
#define TEST(str, ...) printf( \
  "\033[33m" STR(__LINE__) "\033[0m: " \
  STR((__VA_ARGS__)) ": " \
  str "\n", ##__VA_ARGS__);
#endif

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

#define ARRAY_SIZE(ARRAY) ( sizeof(ARRAY) / sizeof(*ARRAY) )
#define FOR_ARRAY(ARRAY, INDEX) \
  for (int INDEX = 0; INDEX < ARRAY_SIZE(ARRAY); ++INDEX)

#define MIN(a, b) ((a) < (b) ? a : b)
#define MAX(a, b) ((a) > (b) ? a : b)

// ==================================================================

// ESP8266_RTOS_SDK/components/esp8266/include/esp_wifi_types.h
// WiFi standard allows arbitrary SSID and PASS bytes
// But esp firmware library relies on them being null terminated
#ifndef MAX_SSID_LEN
#  error "MAX_SSID_LEN is not defined"
#endif
#if MAX_SSID_LEN >= UINT8_MAX
#  error "MAX_SSID_LEN >= " STR(UINT8_MAX)
#endif
#ifndef MAX_PASSPHRASE_LEN
#  error "MAX_PASSPHRASE_LEN is not defined"
#endif
#if MAX_PASSPHRASE_LEN >= UINT8_MAX
#  error "MAX_PASSPHRASE_LEN >= " STR(UINT8_MAX)
#endif

char wifi_ssid_pass[MAX_SSID_LEN+1+MAX_PASSPHRASE_LEN+1];
bool fallback_wifi_mode_sta = false;
bool manual_disconnect = false;

// ==================================================================

#include "part_gpio.h"
#include "part_nvs.h"
#include "part_server.h"
#include "part_wifi.h"

void app_main(void) {
  init_gpio();
  // init_nvs();
  init_server();
}
