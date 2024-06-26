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

#define STR1(x) #x
#define STR(x) STR1(x)

// #define FIELD_SIZE(t,f) (sizeof(((t*)0)->f))

#define LED_PIN 2
#define LIGHT_PIN 5
#define FAN_PIN 4
#define LIGHT_SWITCH_PIN 13
#define FAN_SWITCH_PIN 14

#define SSID      "thermostat"
#define PASS      "thermostat"
#define MAX_CONN   8

// AP = access point
// STA = station
// stations connect to an access point

// default AP IP address: 192.168.4.1

static httpd_handle_t server = NULL;

// embed static files
extern const uint8_t connect_html[] asm("_binary_min_connect_html_gz_start");
extern const uint8_t connect_html_end[] asm("_binary_min_connect_html_gz_end");

extern const uint8_t control_html[] asm("_binary_min_control_html_gz_start");
extern const uint8_t control_html_end[] asm("_binary_min_control_html_gz_end");

// ESP8266_RTOS_SDK/components/esp8266/include/esp_wifi_types.h
#define MAX_SSID_STRLEN 31
#define MAX_PASS_STRLEN 63
static char
  wifi_ssid[MAX_SSID_STRLEN+1] = { '\0' },
  wifi_pass[MAX_PASS_STRLEN+1] = { '\0' };

nvs_handle_t nvs;

void nvs_get_ssid_pass(void) {
  esp_err_t err;
  size_t len;
  /* nvs_get_str(nvs, "wifi_ssid", NULL, &required_size); */
  len = sizeof(wifi_ssid);
  err = nvs_get_str(nvs, "ssid", wifi_ssid, &len);
  if (err != ESP_OK) goto err;
  len = sizeof(wifi_pass);
  err = nvs_get_str(nvs, "pass", wifi_pass, &len);
  if (err != ESP_OK) goto err;

  return;

err:
  wifi_ssid[0] = '\0';
  wifi_pass[0] = '\0';
}
void nvs_set_ssid_pass(void) {
  esp_err_t err;
  err = nvs_set_str(nvs, "ssid", wifi_ssid);
  if (err != ESP_OK) goto err;
  err = nvs_set_str(nvs, "pass", wifi_pass);
  if (err != ESP_OK) goto err;

  return;

err:
  ;
}

static bool new_ap = false;
static int connected = 0;

void start_access_point(void);
void start_station(void);

esp_err_t get_root(httpd_req_t* req) {
  httpd_resp_set_hdr(req,"Content-Encoding","gzip");
  const char* buf;
  size_t len;
  if (connected) {
    buf = (const char*) control_html;
    len = control_html_end - control_html;
  } else {
    buf = (const char*) connect_html;
    len = connect_html_end - connect_html;
  }
  httpd_resp_send(req,buf,len);
  return ESP_OK;
}
httpd_uri_t get_root_def = {
  .uri       = "/",
  .method    = HTTP_GET,
  .handler   = get_root,
  .user_ctx  = NULL
};

esp_err_t get_get(httpd_req_t* req) {
  if (connected) {
    char buf[] = "{\"light\":L,\"fan\":F}";
    *strchr(buf,'L') = '0' + (char)(gpio_get_level(LIGHT_PIN));
    *strchr(buf,'F') = '0' + (char)(gpio_get_level(FAN_PIN));
    httpd_resp_set_type(req,HTTPD_TYPE_JSON);
    httpd_resp_send(req,buf,strlen(buf));
    return ESP_OK;
  } else {
    return httpd_resp_send_404(req);
  }
}
httpd_uri_t get_get_def = {
  .uri       = "/get",
  .method    = HTTP_GET,
  .handler   = get_get,
  .user_ctx  = NULL
};

esp_err_t get_set(httpd_req_t* req) {
  if (connected) {
    char buf[64] = "{";
    size_t buf_len = 1;
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

#define KEYCMP(KEY) \
        (key_len==(sizeof(KEY)-1) && !strncmp(key,KEY,key_len))
#define KEYCPY(KEY) \
        if (buf_len > 1) { buf[buf_len++] = ','; } \
        strcpy(buf+buf_len,"\"" KEY "\":"); \
        buf_len += sizeof("\"" KEY "\":")-1;


        if (key) {
          if (KEYCMP("light")) {
            const int light = !!atoi(a);
            gpio_set_level(LIGHT_PIN, light);
            KEYCPY("light")
            buf[buf_len++] = '0' + (char)light;
          } else
          if (KEYCMP("fan")) {
            const int fan = !!atoi(a);
            gpio_set_level(FAN_PIN, fan);
            KEYCPY("fan")
            buf[buf_len++] = '0' + (char)fan;
          } else
          if (KEYCMP("led")) {
            const int led = !!atoi(a);
            gpio_set_level(LED_PIN, !/*inverted*/led);
            KEYCPY("led")
            buf[buf_len++] = '0' + (char)led;
          }
        }

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
    buf[buf_len] = '}';
    buf[++buf_len] = '\0';

    httpd_resp_set_type(req,HTTPD_TYPE_JSON);
    httpd_resp_send(req,buf,buf_len);
    return ESP_OK;
  } else {
    return httpd_resp_send_404(req);
  }
}
httpd_uri_t get_set_def = {
  .uri       = "/set",
  .method    = HTTP_GET,
  .handler   = get_set,
  .user_ctx  = NULL
};

esp_err_t post_root(httpd_req_t* req) {
  if (connected) {
    httpd_resp_set_status(req,"405 Method Not Allowed");
    httpd_resp_send(req,"",0);
    return ESP_OK;
  } else {
    char buf[sizeof(wifi_ssid)+sizeof(wifi_pass)];
    int remaining = req->content_len;

    if (remaining > sizeof(buf))
      return ESP_FAIL;

    for (char *p = buf; remaining > 0;) {
      const int ret = httpd_req_recv(req, p, remaining);
      if (ret <= 0) { // Retry receiving if timeout occurred
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue;
        return ESP_FAIL;
      }
      p += ret;
      remaining -= ret;
    }

    char *a = buf;
    char *b = memchr(a,'\0',sizeof(wifi_ssid));
    if (!b) goto bad_ssid;
    ++b;
    const size_t ssid_len = b-a;
    if (ssid_len < 3 || sizeof(wifi_ssid) < ssid_len) {
bad_ssid:
#define RESPONSE "SSID must be 2 to " STR(MAX_SSID_STRLEN) " bytes long"
      httpd_resp_set_status(req, "400 Bad Request");
      httpd_resp_send(req, RESPONSE, sizeof(RESPONSE));
#undef RESPONSE
      return ESP_OK;
    }
    memset(wifi_ssid,0,MAX_SSID_STRLEN+1); // zero out
    memcpy(wifi_ssid,a,ssid_len);

    a = b;
    b = memchr(a,'\0',sizeof(wifi_pass));
    if (!b) goto bad_pass;
    ++b;
    const size_t pass_len = b-a;
    if (sizeof(wifi_pass) < pass_len) {
bad_pass:
#define RESPONSE "Password must be at most " STR(MAX_PASS_STRLEN) " bytes long"
      httpd_resp_set_status(req, "400 Bad Request");
      httpd_resp_send(req, RESPONSE, sizeof(RESPONSE));
#undef RESPONSE
      return ESP_OK;
    }
    memset(wifi_pass,0,MAX_PASS_STRLEN+1); // zero out
    memcpy(wifi_pass,a,pass_len);
    new_ap = true;

#define RESPONSE "Connecting to "
    char resp[sizeof(RESPONSE)-1+sizeof(wifi_ssid)] = RESPONSE;
    const size_t resp_len = sizeof(RESPONSE)-1 + ssid_len;
    memcpy(resp+sizeof(RESPONSE)-1,wifi_ssid,ssid_len);
#undef RESPONSE

    httpd_resp_send(req, resp, resp_len);

    esp_wifi_deauth_sta(0);
    ESP_ERROR_CHECK(esp_wifi_stop());

    start_station();

    return ESP_OK;
  }
}
httpd_uri_t post_root_def = {
  .uri       = "/",
  .method    = HTTP_POST,
  .handler   = post_root,
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
      .ssid = SSID,
      .ssid_len = sizeof(SSID),
      .password = PASS,
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
        nvs_set_ssid_pass();
      }
    }
  }
}

void start_station(void) {
  connected = 1;

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_event_handler_register(
    WIFI_EVENT, ESP_EVENT_ANY_ID, &station_event_handler, NULL
  ));
  ESP_ERROR_CHECK(esp_event_handler_register(
    IP_EVENT, IP_EVENT_STA_GOT_IP, &station_event_handler, NULL
  ));

  wifi_config_t wifi_config = {
    .sta = { }
  };
  memcpy(wifi_config.sta.ssid    , wifi_ssid, MAX_SSID_STRLEN+1);
  memcpy(wifi_config.sta.password, wifi_pass, MAX_PASS_STRLEN+1);
  if (wifi_pass[0]) {
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  }

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
  ESP_ERROR_CHECK(esp_wifi_start());
}

// static const int switch_pin[] = { LIGHT_SWITCH_PIN, FAN_SWITCH_PIN };
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

void app_main(void) {
  // HTTP ===========================================================
  esp_err_t err;

  tcpip_adapter_init();

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  // Initialize NVS -------------------------------------------------
  // https://github.com/espressif/esp-idf/blob/cf7e743a9b2e5fd2520be4ad047c8584188d54da/examples/storage/nvs_rw_value/main/nvs_value_example_main.c
  err = nvs_flash_init();
  if (
    err == ESP_ERR_NVS_NO_FREE_PAGES ||
    err == ESP_ERR_NVS_NEW_VERSION_FOUND
  ) { // NVS partition was truncated and needs to be erased
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  err = nvs_open("storage", NVS_READWRITE, &nvs);
  if (err != ESP_OK) {
    puts("Failed to open nvs");
    goto skip_wifi;
  }

  nvs_get_ssid_pass();

  // Start HTTP daemon ----------------------------------------------
  server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  err = httpd_start(&server, &config);
  if (err != ESP_OK) {
    puts("Failed to start httpd");
    goto skip_wifi;
  }

  // Set URI handlers
  httpd_register_uri_handler(server, & get_root_def);
  httpd_register_uri_handler(server, &post_root_def);
  httpd_register_uri_handler(server, & get_get_def);
  httpd_register_uri_handler(server, & get_set_def);

  // Start WiFi -----------------------------------------------------
  if (wifi_ssid[0]) start_station();
  else start_access_point();

skip_wifi: ;

  // GPIO ===========================================================
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

  gpio_set_intr_type(LIGHT_SWITCH_PIN,GPIO_INTR_ANYEDGE);
  gpio_set_intr_type(  FAN_SWITCH_PIN,GPIO_INTR_ANYEDGE);
}
