#include <string.h>

#include "freertos/FreeRTOS.h"
/* #include "freertos/task.h" */
/* #include "freertos/queue.h" */
#include "freertos/timers.h"
#include "freertos/event_groups.h"

#include "driver/gpio.h"
/* #include "driver/hw_timer.h" */

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_timer.h"
// #include "esp_log.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#define STR1(x) #x
#define STR(x) STR1(x)

// #define FIELD_SIZE(t,f) (sizeof(((t*)0)->f))

/*
#define NOP() asm volatile ("nop")

uint64_t IRAM_ATTR micros() {
  return esp_timer_get_time();
}
void IRAM_ATTR delayMicroseconds(uint64_t us) {
  uint64_t m = micros();
  if (us) {
    uint64_t e = (m + us);
    if (m > e) { // overflow
      while (micros() > e) { NOP(); }
    }
    while (micros() < e) { NOP(); }
  }
}
*/

#define SSID      "thermostat"
#define PASS      "thermostat"
#define MAX_CONN   8

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// AP = access point
// STA = station
// stations connect to an access point

// default IP address: 192.168.4.1

static httpd_handle_t server = NULL;

// embed static files
extern const uint8_t connect_html[] asm("_binary_min_connect_html_start");
extern const uint8_t connect_html_end[] asm("_binary_min_connect_html_end");

extern const uint8_t control_html[] asm("_binary_min_control_html_start");
extern const uint8_t control_html_end[] asm("_binary_min_control_html_end");

// ESP8266_RTOS_SDK/components/esp8266/include/esp_wifi_types.h
#define MAX_SSID_STRLEN 31
#define MAX_PASS_STRLEN 63
char
  wifi_ssid[MAX_SSID_STRLEN+1],
  wifi_pass[MAX_PASS_STRLEN+1];

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

// static_assert( sizeof(SSID) <= FIELD_SIZE(wifi_config_t,ap.ssid) );
// static_assert( sizeof(PASS) <= FIELD_SIZE(wifi_config_t,ap.password) );
// static_assert( sizeof(wifi_ssid) == FIELD_SIZE(wifi_config_t,sta.ssid) );
// static_assert( sizeof(wifi_pass) == FIELD_SIZE(wifi_config_t,sta.password) );

void start_access_point(void);
void start_station(void);

bool connected = false, new_ap = false;

esp_err_t get_handler_thermostat(httpd_req_t* req) {
  httpd_resp_send(
    req,
    (const char*) control_html,
    control_html_end-control_html
  );
  return ESP_OK;
}

esp_err_t get_handler_connect(httpd_req_t* req) {
  httpd_resp_send(
    req,
    (const char*) connect_html,
    connect_html_end-connect_html
  );
  return ESP_OK;
}

esp_err_t post_handler_thermostat(httpd_req_t* req) {
  return ESP_OK;
}

esp_err_t post_handler_connect(httpd_req_t* req) {
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

/* static void wifi_event_handler( */
/*   void* arg, */
/*   esp_event_base_t event_base, */
/*   int32_t event_id, */
/*   void* event_data */
/* ) { */
/*   if (event_id == WIFI_EVENT_AP_STACONNECTED) { */
/*     wifi_event_ap_staconnected_t* event = */
/*       (wifi_event_ap_staconnected_t*) event_data; */
/*     printf( */
/*       "station " MACSTR " join, AID=%d\n", */
/*       MAC2STR(event->mac), */
/*       event->aid */
/*     ); */
/*   } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) { */
/*     wifi_event_ap_stadisconnected_t* event = */
/*       (wifi_event_ap_stadisconnected_t*) event_data; */
/*     printf( */
/*       "station " MACSTR " leave, AID=%d\n", */
/*       MAC2STR(event->mac), */
/*       event->aid */
/*     ); */
/*   } */
/* } */

void start_access_point(void) {
  connected = false;

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

  /* puts( */
  /*   "SSID: " SSID "\n" */
  /*   "PASS: " PASS */
  /* ); */

  /* server = NULL; */
  /* httpd_config_t config = HTTPD_DEFAULT_CONFIG(); */
  /*  */
  /* if (httpd_start(&server, &config) == ESP_OK) { */
  /*   // Set URI handlers */
  /*   httpd_register_uri_handler(server, &connect_get); */
  /*   httpd_register_uri_handler(server, &connect_post); */
  /* } else { */
  /*   server = NULL; */
  /* } */

  tcpip_adapter_ip_info_t ip_info;
  tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info);
  puts(ip4addr_ntoa(&ip_info.ip));
}

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

static void station_event_handler(
  void* arg,
  esp_event_base_t event_base,
  int32_t event_id,
  void* event_data
) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry_num < 5) { // number of retries
      esp_wifi_connect();
      s_retry_num++;
      puts("Retrying AP connection");
    } else {
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
    puts("AP connection failed");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    puts("AP connection succeeded");
    puts(ip4addr_ntoa(&event->ip_info.ip));
  }
}

void start_station(void) {
  connected = true;

  s_wifi_event_group = xEventGroupCreate();

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
  ESP_ERROR_CHECK(esp_wifi_start() );

  // Waiting until either the connection is established (WIFI_CONNECTED_BIT) or
  // connection failed for the maximum number of re-tries (WIFI_FAIL_BIT). The
  // bits are set by station_event_handler() (see above)
  EventBits_t bits = xEventGroupWaitBits(
    s_wifi_event_group,
    WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
    pdFALSE,
    pdFALSE,
    portMAX_DELAY
  );

  // xEventGroupWaitBits() returns the bits before the call returned, hence we
  // can test which event actually happened.
  if (bits & WIFI_CONNECTED_BIT) {
    puts("Connected to AP");
  } else if (bits & WIFI_FAIL_BIT) {
    puts("Failed to connect to AP");
  } else {
    puts("Unexpected WiFi connection event");
  }

  if (new_ap) {
    new_ap = false;
    nvs_set_ssid_pass();
  }

  ESP_ERROR_CHECK(esp_event_handler_unregister(
    IP_EVENT, IP_EVENT_STA_GOT_IP, &station_event_handler
  ));
  ESP_ERROR_CHECK(esp_event_handler_unregister(
    WIFI_EVENT, ESP_EVENT_ANY_ID, &station_event_handler
  ));

  vEventGroupDelete(s_wifi_event_group);

  // Switch back to AP mode if connection failed
  if (bits & WIFI_FAIL_BIT) {
    ESP_ERROR_CHECK(esp_wifi_stop());
    start_access_point();
  }
}

esp_err_t get_handler(httpd_req_t* req) {
  if (connected) {
    return get_handler_thermostat(req);
  } else {
    return get_handler_connect(req);
  }
}
esp_err_t post_handler(httpd_req_t* req) {
  if (connected) {
    return post_handler_thermostat(req);
  } else {
    return post_handler_connect(req);
  }
}
httpd_uri_t get_handler_def = {
  .uri       = "/",
  .method    = HTTP_GET,
  .handler   = get_handler,
  .user_ctx  = NULL
};
httpd_uri_t post_handler_def = {
  .uri       = "/",
  .method    = HTTP_POST,
  .handler   = post_handler,
  .user_ctx  = NULL
};

#define BUTTON_PIN 5
#define LED_PIN 2

/* static xQueueHandle gpio_evt_queue = NULL; */
/* static StaticTimer_t button_timer_buffer; */
static TimerHandle_t
  button_debounce_timer = NULL,
  button_multiclick_timer = NULL;

static volatile bool
  button_isr_enable = true,
  relay_light = false,
  relay_fan   = false;

static volatile uint8_t
  button_click_count = 0;

static void button_isr(void *arg) {
  if (button_isr_enable) {
    BaseType_t xHigherPriorityTaskWoken = pdTRUE;
    xTimerStartFromISR( button_debounce_timer, &xHigherPriorityTaskWoken );
  }
}
static void button_debounce_timer_callback(void *arg) {
  if (gpio_get_level(BUTTON_PIN)) { // if the input is still high
    button_isr_enable = false;
    xTimerStart( button_multiclick_timer, 10 );
    ++button_click_count;
    button_click_count %= 3;
    button_isr_enable = true;
  }
  // TODO: record time
  // TODO: notify clients
}
static void button_multiclick_timer_callback(void *arg) {
  if (button_click_count == 1) {
    gpio_set_level(LED_PIN, !/*GPIO2*/(relay_light = !relay_light));
    printf("Light %s\n",(relay_light ? "ON" : "OFF"));
  } else if (button_click_count == 2) {
    relay_fan = !relay_fan;
    printf("Fan %s\n",(relay_fan ? "ON" : "OFF"));
  }
  button_click_count = 0;
}

void app_main(void) {
  /* // HTTP =========================================================== */
  /* esp_err_t err; */
  /*  */
  /* tcpip_adapter_init(); */
  /*  */
  /* ESP_ERROR_CHECK(esp_netif_init()); */
  /* ESP_ERROR_CHECK(esp_event_loop_create_default()); */
  /*  */
  /* // Initialize NVS ------------------------------------------------- */
  /* // https://github.com/espressif/esp-idf/blob/cf7e743a9b2e5fd2520be4ad047c8584188d54da/examples/storage/nvs_rw_value/main/nvs_value_example_main.c */
  /* err = nvs_flash_init(); */
  /* if ( */
  /*   err == ESP_ERR_NVS_NO_FREE_PAGES || */
  /*   err == ESP_ERR_NVS_NEW_VERSION_FOUND */
  /* ) { // NVS partition was truncated and needs to be erased */
  /*   ESP_ERROR_CHECK(nvs_flash_erase()); */
  /*   err = nvs_flash_init(); */
  /* } */
  /* ESP_ERROR_CHECK(err); */
  /*  */
  /* err = nvs_open("storage", NVS_READWRITE, &nvs); */
  /* if (err != ESP_OK) { */
  /*   puts("Failed to open nvs"); */
  /*   return; */
  /* } */
  /*  */
  /* nvs_get_ssid_pass(); */
  /*  */
  /* // Start HTTP daemon ---------------------------------------------- */
  /* server = NULL; */
  /* httpd_config_t config = HTTPD_DEFAULT_CONFIG(); */
  /*  */
  /* err = httpd_start(&server, &config); */
  /* if (err != ESP_OK) { */
  /*   puts("Failed to start httpd"); */
  /*   return; */
  /* } */
  /*  */
  /* // Set URI handlers */
  /* httpd_register_uri_handler(server, & get_handler_def); */
  /* httpd_register_uri_handler(server, &post_handler_def); */
  /*  */
  /* // Start WiFi ----------------------------------------------------- */
  /* if (wifi_ssid[0]) start_station(); */
  /* else start_access_point(); */

  // GPIO ===========================================================
  { gpio_config_t io_conf = {
      .pin_bit_mask = (1ull << LED_PIN), // GPIO pin
      .intr_type = GPIO_INTR_DISABLE, // no interrupt
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = 0,
      .pull_down_en = 0
    };
    gpio_config(&io_conf);
  }
  gpio_set_level(LED_PIN, !/*GPIO2*/relay_light);

  { gpio_config_t io_conf = {
      .pin_bit_mask = (1ull << BUTTON_PIN), // GPIO pin
      .intr_type = GPIO_INTR_POSEDGE, // interrupt on rising edge
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = 0,
      .pull_down_en = 1
    };
    gpio_config(&io_conf);
  }

  // create event queue and task
  /* gpio_evt_queue = xQueueCreate(8,sizeof(uint32_t)); */
  /* xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL); */

  button_debounce_timer = xTimerCreate/*Static*/(
    "",
    30 / portTICK_PERIOD_MS, // period in ticks
    pdFALSE, // not periodic
    (void*) 0, // timer id
    button_debounce_timer_callback
    /* &button_timer_buffer */
  );
  button_multiclick_timer = xTimerCreate/*Static*/(
    "",
    300 / portTICK_PERIOD_MS, // period in ticks
    pdFALSE, // not periodic
    (void*) 0, // timer id
    button_multiclick_timer_callback
    /* &button_timer_buffer */
  );

  // install gpio isr service
  gpio_install_isr_service(0);
  // hook isr handler for specific gpio pin
  gpio_isr_handler_add(BUTTON_PIN, button_isr, (void*)BUTTON_PIN);
}
