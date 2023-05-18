#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_http_server.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#define SSID      "esp-thermostat"
#define PASS      "thermostat"
#define MAX_CONN   8

// default IP address: 192.168.4.1

static httpd_handle_t server = NULL;

// embed static files
extern const uint8_t connect_html[] asm("_binary_connect_html_start");
extern const uint8_t connect_html_end[] asm("_binary_connect_html_end");

char wifi_ssid[65], wifi_pass[65];

esp_err_t connect_get_handler(httpd_req_t* req) {
  const char* resp = (const char*) connect_html;

  httpd_resp_send(req, resp, connect_html_end-connect_html);

  return ESP_OK;
}

esp_err_t connect_post_handler(httpd_req_t* req) {
  char buf[sizeof(wifi_ssid)+sizeof(wifi_pass)];
  int remaining = req->content_len;
  char *p = buf;

  // if (remaining > sizeof(buf))
  //   return ESP_FAIL;

  while (remaining > 0) {
    const int ret = httpd_req_recv(req, p, remaining);
    if (ret <= 0) {
      // Retry receiving if timeout occurred
      if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue;
      return ESP_FAIL;
    }

    p += ret;
    remaining -= ret;
  }

  char *a = buf;
  char *b = memchr(a,'\0',sizeof(wifi_ssid));
  if (!b) return ESP_FAIL;
  ++b;
  const size_t ssid_len = b-a;
  memcpy(wifi_ssid,a,ssid_len);

  a = b;
  b = memchr(a,'\0',sizeof(wifi_pass));
  if (!b) return ESP_FAIL;
  ++b;
  memcpy(wifi_pass,a,b-a);

#define RESPONSE "Connecting to "
  char resp[sizeof(RESPONSE)-1+sizeof(wifi_ssid)] = RESPONSE;
  const size_t resp_len = sizeof(RESPONSE)-1 + ssid_len;
  memcpy(resp+sizeof(RESPONSE)-1,wifi_ssid,ssid_len);
#undef RESPONSE

  httpd_resp_send(req, resp, resp_len);

  return ESP_OK;
}

httpd_uri_t connect_get = {
  .uri       = "/",
  .method    = HTTP_GET,
  .handler   = connect_get_handler,
  .user_ctx  = NULL
};

httpd_uri_t connect_post = {
  .uri       = "/",
  .method    = HTTP_POST,
  .handler   = connect_post_handler,
  .user_ctx  = NULL
};

void start_webserver(void) {
  server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  if (httpd_start(&server, &config) == ESP_OK) {
    // Set URI handlers
    httpd_register_uri_handler(server, &connect_get);
    httpd_register_uri_handler(server, &connect_post);
  } else {
    server = NULL;
  }
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

void app_main() {
  tcpip_adapter_init();

  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

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
      .ssid_len = strlen(SSID),
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

  puts(
    "SSID: " SSID "\n"
    "PASS: " PASS
  );

  // ----------------------------------------------------------------
  start_webserver();
}
