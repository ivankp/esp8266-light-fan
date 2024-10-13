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
#define MAX_CONN 8

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

#if VERBOSITY > 0
#  define CHECK_OK_1 CHECK_OK_VERBOSE
#else
#  define CHECK_OK_1 CHECK_OK
#endif

#if VERBOSITY > 1
#  define CHECK_OK_2 CHECK_OK_VERBOSE
#else
#  define CHECK_OK_2 CHECK_OK
#endif

// GPIO =============================================================

// TODO: transpose
static const int output_pin[] = { LIGHT_PIN, FAN_PIN };
static volatile int switch_state[] = { 0, 0 };
static volatile bool switch_enable[] = { true, true };
static TimerHandle_t switch_timer[] = { NULL, NULL };

static void switch_isr(void *arg) {
  const int i = (int)arg;
  if (switch_enable[i]) {
    switch_enable[i] = false;
    // set output pin level
    gpio_set_level(output_pin[i], (switch_state[i] = !switch_state[i]));
  }
  // start timer
  BaseType_t xHigherPriorityTaskWoken = pdTRUE;
  xTimerStartFromISR( switch_timer[i], &xHigherPriorityTaskWoken );
}

// TODO: combine timer callbacks
static void light_switch_timer_callback(void *arg) {
  // set output pin level
  gpio_set_level(LIGHT_PIN,
    (switch_state[0] = gpio_get_level(LIGHT_SWITCH_PIN)));
  // enable switch
  switch_enable[0] = true;
  // TODO: notify clients
}

static void fan_switch_timer_callback(void *arg) {
  // set output pin level
  gpio_set_level(FAN_PIN,
    (switch_state[1] = gpio_get_level(FAN_SWITCH_PIN)));
  // enable switch
  switch_enable[1] = true;
  // TODO: notify clients
}

static void init_gpio(void) {
  { gpio_config_t io_conf = {
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE /* no interrupt */
    };

#define OUTPUT_PIN(PIN,VAL) \
    io_conf.pin_bit_mask = (1ull << PIN); /* GPIO pin */ \
    gpio_config(&io_conf); \
    gpio_set_level(PIN, VAL);

    OUTPUT_PIN(  LED_PIN, 1/*inverted*/)
    OUTPUT_PIN(LIGHT_PIN, 0)
    OUTPUT_PIN(  FAN_PIN, 0)

    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.intr_type = GPIO_INTR_DISABLE; /* interrupt edge */

#define INPUT_PIN(PIN) \
    io_conf.pin_bit_mask = (1ull << PIN); /* GPIO pin */ \
    gpio_config(&io_conf);

    INPUT_PIN(LIGHT_SWITCH_PIN)
    INPUT_PIN(  FAN_SWITCH_PIN)
  }

  switch_timer[0] = xTimerCreate/*Static*/(
    "",
    50 / portTICK_PERIOD_MS, // period in ticks
    pdFALSE, // not periodic
    (void*) 0, // timer id
    light_switch_timer_callback
  );
  switch_timer[1] = xTimerCreate/*Static*/(
    "",
    50 / portTICK_PERIOD_MS, // period in ticks
    pdFALSE, // not periodic
    (void*) 0, // timer id
    fan_switch_timer_callback
  );

  // install gpio isr service
  gpio_install_isr_service(0);

  // hook isr handlers for specific gpio pins
  gpio_isr_handler_add(LIGHT_SWITCH_PIN, switch_isr, (void*)0);
  gpio_isr_handler_add(  FAN_SWITCH_PIN, switch_isr, (void*)1);

  gpio_set_intr_type(LIGHT_SWITCH_PIN, GPIO_INTR_ANYEDGE);
  gpio_set_intr_type(  FAN_SWITCH_PIN, GPIO_INTR_ANYEDGE);
}

// NVS ==============================================================

// ESP8266_RTOS_SDK/components/esp8266/include/esp_wifi_types.h
// store last 8 successfully used access point credentials
// #define MAX_SSID_LEN 32
// #define MAX_PASS_LEN 64
#define MAX_PASS_LEN MAX_PASSPHRASE_LEN

// 1 : number of saved credentials
// n : 1 : ssid length
//     n : ssid
//     1 : password length
//     n : password
static uint8_t* wifi_cred = NULL;

// https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/nvs_flash.html
nvs_handle_t nvs;

void nvs_get_wifi_cred(void) {
  size_t wifi_cred_len = 0;
  CHECK_OK(nvs_get_blob(nvs, "wifi_cred", NULL, &wifi_cred_len)); // get length
  if (wifi_cred) free(wifi_cred);
  wifi_cred = malloc(wifi_cred_len);
  CHECK_OK(nvs_get_blob(nvs, "wifi_cred", &wifi_cred, &wifi_cred_len)); // get data

  return;
err:
  // no valid credentials are available
  if (wifi_cred) free(wifi_cred);
  wifi_cred = NULL;
}

void nvs_add_wifi_cred(const uint8_t* new_cred) {
  const uint8_t  new_ssid_len = *new_cred++;
  const uint8_t* new_ssid = new_cred;
  new_cred += new_ssid_len;
  const uint8_t  new_pass_len = *new_cred++;
  const uint8_t* new_pass = new_cred;
  new_cred = new_ssid - 1;

  const uint8_t* a = wifi_cred;
  uint8_t ncreds = a ? *a++ : 0;

  uint8_t *b = a, *c = a, *d = a, *e = a;
  for (uint8_t i = 0; i < ncreds; ++i) {
    d = e;
    const uint8_t ssid_len = *e++;
    if (b == a && ssid_len == new_ssid_len && !memcmp(e, new_ssid, ssid_len)) {
      c = e + ssid_len;
      const uint8_t pass_len = *c++;
      if (d == a && pass_len == new_pass_len && !memcmp(c, new_pass, pass_len)) {
        return; // same ssid and pass in first credential
      }
      b = d;
      c += pass_len;
    }
    e += ssid_len; // skip ssid
    e += *e++; // skip pass
  }

  if (ncreds > 7) e = d;

  size_t new_len = 3; // ncreds, new_ssid_len, new_pass_len
  new_len += new_ssid_len;
  new_len += new_pass_len;
  new_len += b - a;
  new_len += e - c;

  if (ncreds < 8) ++ncreds;

  uint8_t* p = malloc(new_len);
  *p++ = ncreds;
  p = mempcpy(p, new_cred, new_ssid_len + new_pass_len + 2);
  p = mempcpy(p, a, b - a);
  p = mempcpy(p, c, e - c);

  if (wifi_cred) free(wifi_cred);
  wifi_cred = p - new_len;

  CHECK_OK(nvs_set_blob(nvs, "wifi_cred", wifi_cred, new_len));

err: ;
}

static void init_nvs(void) {
  // https://github.com/espressif/esp-idf/blob/cf7e743a9b2e5fd2520be4ad047c8584188d54da/examples/storage/nvs_rw_value/main/nvs_value_example_main.c

  esp_err_t err = nvs_flash_init();
  if (
    err == ESP_ERR_NVS_NO_FREE_PAGES ||
    err == ESP_ERR_NVS_NEW_VERSION_FOUND
  ) { // NVS partition was truncated and needs to be erased
    CHECK_OK_1(nvs_flash_erase());
    CHECK_OK_1(nvs_flash_init());
  }

  CHECK_OK_1(nvs_open("storage", NVS_READWRITE, &nvs));

  nvs_get_wifi_cred();

err: ;
}

// HTTP =============================================================

// AP = access point
// STA = station
// stations connect to access points

static bool new_ap = false;
static int connected = 0;

void start_access_point(void);
void start_station(void);

esp_err_t get_root(httpd_req_t* req) {
  httpd_resp_set_hdr(req,"Content-Encoding","gzip");
  httpd_resp_send(req, (const char*) index_page, index_page_end - index_page);
  return ESP_OK;
}
httpd_uri_t get_root_def = {
  .uri       = "/",
  .method    = HTTP_GET,
  .handler   = get_root,
  .user_ctx  = NULL
};

esp_err_t get_get(httpd_req_t* req) {
  char buf[] = "{\"light\":0,\"fan\":0}";
  char* p = strchr(buf, '0');
  *p += gpio_get_level(LIGHT_PIN);
  p = strchr(p+1, '0');
  *p += gpio_get_level(FAN_PIN);
  httpd_resp_set_type(req, HTTPD_TYPE_JSON);
  httpd_resp_send(req, buf, strlen(buf));
  return ESP_OK;
}
httpd_uri_t get_get_def = {
  .uri       = "/get",
  .method    = HTTP_GET,
  .handler   = get_get,
  .user_ctx  = NULL
};

esp_err_t get_set(httpd_req_t* req) {
  char buf[64] = "{";
  char* buf_ptr = buf + 1;
  const char* a = strchr(req->uri,'?');
  if (!a || !*++a) goto send;
  while (*a == '&') ++a;

  const char *b=a, *key = NULL;
  size_t key_len = 0;
  for (;;) {
    const char c = *b;
    if (c == '=' && !key) {
      key = a;
      key_len = b-a;
      a = ++b;
    } else if (c == '&' || c == '\0') {
      if (!key) goto next;

#define KEYCMP(KEY) \
      (key_len==(sizeof(KEY)-1) && !memcmp(key, KEY, key_len))

#define KEYCPY \
      if (buf_ptr - buf > 1) { *buf_ptr++ = ','; } \
      *buf_ptr++ = '\"'; \
      buf_ptr = mempcpy(buf_ptr, key, key_len); \
      *buf_ptr++ = '\"'; \
      *buf_ptr++ = ':';

      if (KEYCMP("light") || KEYCMP("fan")) {
        const char val = *a;
        if (b - a != 1 || !(val == '0' || val == '1')) goto next;
        gpio_set_level(LIGHT_PIN, val - '0');
        KEYCPY
        *buf_ptr++ = val;
      } else
      if (KEYCMP("led")) {
        const char val = *a;
        if (b - a != 1 || !(val == '0' || val == '1')) goto next;
        gpio_set_level(LED_PIN, !(val - '0')); // inverted
        KEYCPY
        *buf_ptr++ = val;
      }

next:
      if (c == '\0') break;
      while (*++b == '&');
      if (*b == '\0') break;
      key = NULL;
      a = b;
    } else {
      ++b;
    }
  }

send:
  *buf_ptr++ = '}';
  *buf_ptr = '\0';

  httpd_resp_set_type(req, HTTPD_TYPE_JSON);
  httpd_resp_send(req, buf, buf_ptr - buf);

  return ESP_OK;
}
httpd_uri_t get_set_def = {
  .uri       = "/set",
  .method    = HTTP_GET,
  .handler   = get_set,
  .user_ctx  = NULL
};

esp_err_t post_wifi(httpd_req_t* req) {
  ssid_pass_t cred;
  int remaining = req->content_len;

  if (remaining > sizeof(cred))
    return ESP_FAIL;

  for (char *p = &cred; remaining > 0;) {
    const int ret = httpd_req_recv(req, p, remaining);
    if (ret <= 0) { // Retry receiving if timeout occurred
      if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue;
      return ESP_FAIL;
    }
    p += ret;
    remaining -= ret;
  }

  const char* response = NULL;

  if (cred.ssid_len > MAX_SSID_LEN) {
    response = "SSID must contain at most " STR(MAX_SSID_LEN) " bytes";
    goto bad_request;
  }



bad_request:
  httpd_resp_set_status(req, "400 Bad Request");
  httpd_resp_send(req, response, strlen(response));

  // ________________________________________________________________


  char *a = buf;
  char *b = memchr(a, '\0', MAX_SSID_LEN);
  if (!b) goto bad_ssid;
  ++b;
  const size_t ssid_len = b-a;
  if (ssid_len < 3 || MAX_SSID_LEN < ssid_len) { // TODO: no need for the second check
bad_ssid:
#define RESPONSE "SSID size must be [2," STR(MAX_SSID_LEN) ") bytes"
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, RESPONSE, sizeof(RESPONSE));
#undef RESPONSE
    return ESP_OK;
  }
  memset(wifi_ssid,0,MAX_SSID_LEN); // zero out
  memcpy(wifi_ssid,a,ssid_len);

  a = b;
  b = memchr(a,'\0',MAX_PASS_LEN); // TODO: n <= remaining
  if (!b) goto bad_pass;
  ++b;
  const size_t pass_len = b-a;
  if (MAX_PASS_LEN < pass_len) { // TODO: check above
bad_pass:
#define RESPONSE "Password size must be less than " STR(MAX_PASS_LEN) " bytes"
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, RESPONSE, sizeof(RESPONSE));
#undef RESPONSE
    return ESP_OK;
  }
  memset(wifi_pass,0,MAX_PASS_LEN); // zero out
  memcpy(wifi_pass,a,pass_len);
  new_ap = true;

#define RESPONSE "Connecting to "
  char resp[sizeof(RESPONSE)-1+MAX_SSID_LEN] = RESPONSE;
  const size_t resp_len = sizeof(RESPONSE)-1 + ssid_len;
  memcpy(resp+sizeof(RESPONSE)-1,wifi_ssid,ssid_len);
#undef RESPONSE

  httpd_resp_send(req, resp, resp_len);

  esp_wifi_deauth_sta(0);
  ESP_ERROR_CHECK(esp_wifi_stop());

  start_station();

  return ESP_OK;
}
httpd_uri_t post_wifi_def = {
  .uri       = "/wifi",
  .method    = HTTP_POST,
  .handler   = post_wifi,
  .user_ctx  = NULL
};

void start_access_point(void) {
  connected = 0;

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  /* ESP_ERROR_CHECK(esp_event_handler_register( */
  /*   WIFI_EVENT, */
  /*   ESP_EVENT_ANY_ID, */
  /*   &wifi_event_handler, */
  /*   NULL */
  /* )); */

  wifi_config_t wifi_config = {
    .ap = {
      .ssid = AP_SSID,
      .ssid_len = sizeof(AP_SSID),
      .password = AP_PASS,
      .max_connection = MAX_CONN,
      .authmode = WIFI_AUTH_WPA_WPA2_PSK
    }
  };
  /* if (strlen(PASS) == 0) { */
  /*   wifi_config.ap.authmode = WIFI_AUTH_OPEN; */
  /* } */

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  tcpip_adapter_ip_info_t ip_info;
  tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info);

  puts("Access point IP address");
  puts(ip4addr_ntoa(&ip_info.ip));
}

// https://docs.espressif.com/projects/esp-idf/en/v4.0.3/api-reference/network/esp_wifi.html
// https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/wifi.html

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static TimerHandle_t station_reconnect_timer = NULL;

static void station_reconnect_timer_callback(void *arg) {
  esp_wifi_connect();
}

static void station_event_handler(
  void* arg,
  esp_event_base_t event_base,
  int32_t event_id,
  void* event_data
) {
  static int attempt = 0;
  if (event_base == WIFI_EVENT) {
    if (event_id == WIFI_EVENT_STA_START) {
      esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
      if (attempt < 5) { // max consecutive attempts
        ++attempt;
        puts("Retrying AP connection");
        esp_wifi_connect();
        // WIFI_EVENT_STA_DISCONNECTED is triggered by esp_wifi_connect()
        // if it fails
      } else {
        attempt = 0;
        puts("AP connection failed");
        if (--connected <= 0) {
          if (station_reconnect_timer)
            xTimerDelete(station_reconnect_timer, 0);

          ESP_ERROR_CHECK(esp_event_handler_unregister(
            IP_EVENT, IP_EVENT_STA_GOT_IP, &station_event_handler
          ));
          ESP_ERROR_CHECK(esp_event_handler_unregister(
            WIFI_EVENT, ESP_EVENT_ANY_ID, &station_event_handler
          ));

          // Switch back to AP mode if connection failed
          ESP_ERROR_CHECK(esp_wifi_stop());
          start_access_point();
        } else {
          puts("Attempting to reconnect in 1 minute");
          if (!station_reconnect_timer) {
            station_reconnect_timer = xTimerCreate/*Static*/(
              "",
              60000 / portTICK_PERIOD_MS, // period in ticks
              pdFALSE, // not periodic
              (void*) 0, // timer id
              station_reconnect_timer_callback
            );
          }
          xTimerStart(station_reconnect_timer, 0);
        }
      }
    }
  } else if (event_base == IP_EVENT) {
    if (event_id == IP_EVENT_STA_GOT_IP) {
      attempt = 0;
      connected = 8;

      ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
      puts("Obtained IP address");
      puts(ip4addr_ntoa(&event->ip_info.ip));

      if (new_ap) {
        new_ap = false;
        nvs_set_wifi_cred();
      }
    }
  }
}

esp_err_t start_station(void) {
  // TODO: try all credentials
  connected = 1;

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  CHECK_OK(esp_wifi_init(&cfg));

  CHECK_OK(esp_event_handler_register(
    WIFI_EVENT, ESP_EVENT_ANY_ID, &station_event_handler, NULL
  ));
  CHECK_OK(esp_event_handler_register(
    IP_EVENT, IP_EVENT_STA_GOT_IP, &station_event_handler, NULL
  ));

  wifi_config_t wifi_config = {
    .sta = { }
  };
  memcpy(wifi_config.sta.ssid    , wifi_ssid, MAX_SSID_LEN);
  memcpy(wifi_config.sta.password, wifi_pass, MAX_PASS_LEN);
  if (wifi_pass[0]) {
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  }

  CHECK_OK(esp_wifi_set_mode(WIFI_MODE_STA));
  CHECK_OK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
  CHECK_OK(esp_wifi_start());

  return ESP_OK;
err:
  esp_wifi_stop();
  return err;
}

static void init_http(void) {
  tcpip_adapter_init();

  CHECK_OK_1(esp_netif_init());
  CHECK_OK_1(esp_event_loop_create_default());

  // Start HTTP daemon
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  CHECK_OK_1(httpd_start(&server, &config));

  // Set URI handlers
  // TODO: do defs need to persist?
  httpd_register_uri_handler(server, & get_root_def);
  httpd_register_uri_handler(server, &post_wifi_def);
  httpd_register_uri_handler(server, & get_get_def);
  httpd_register_uri_handler(server, & get_set_def);

  // Start WiFi
  if (start_station() != ESP_OK) {
    start_access_point();
  }

err: ;
}

// MAIN =============================================================

void app_main(void) {
  init_gpio();
  init_nvs();
  init_http();
}
