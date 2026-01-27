// https://docs.espressif.com/projects/esp-idf/en/v4.0.3/api-reference/network/esp_wifi.html
// https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/wifi.html

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// static TimerHandle_t station_reconnect_timer = NULL;
//
// static void station_reconnect_timer_callback(void* arg) {
//   esp_wifi_connect();
// }
//
// // TODO: find info on esp_wifi_set_config + NVS
//
// static void station_event_handler(
//   void* arg,
//   esp_event_base_t event_base,
//   int32_t event_id,
//   void* event_data
// ) {
//   TEST("%s %" PRId32 "\n", event_base, event_id);
//   static uint8_t attempt = 0;
//   static uint8_t delayed_attempt = 0;
//   if (event_base == WIFI_EVENT) {
//     if (event_id == WIFI_EVENT_STA_START) {
//       attempt = 0;
//       delayed_attempt = 0;
//       global_flags.connected = false;
//       CHECK_OK(esp_wifi_connect());
//       // TODO: nothing happens after esp_wifi_connect() call
//       // with non-existent SSID
//     } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
//       if (global_flags.manual_disconnect) {
//         global_flags.manual_disconnect = false;
//         return;
//       }
//       if (attempt < MAX_STATION_ATTEMPTS) {
//         ++attempt;
//         puts("Retrying AP connection");
//         CHECK_OK(esp_wifi_connect());
//         // WIFI_EVENT_STA_DISCONNECTED is triggered by esp_wifi_connect()
//         // if it fails
//       } else { // try to connect after a delay
//         attempt = 0;
//         puts("AP connection failed");
//         if (global_flags.connected &&
//             delayed_attempt < MAX_STATION_DELAYED_ATTEMPTS
//         ) {
//           ++delayed_attempt;
//           puts("Attempting to reconnect in 1 minute");
//           if (!station_reconnect_timer) {
//             station_reconnect_timer = xTimerCreate/*Static*/(
//               "",
//               60000 / portTICK_PERIOD_MS, // period in ticks
//               pdFALSE, // not periodic
//               (void*) 0, // timer id
//               station_reconnect_timer_callback
//             );
//           }
//           xTimerStart(station_reconnect_timer, 0);
//         } else {
//           delayed_attempt = 0;
//           if (station_reconnect_timer)
//             xTimerDelete(station_reconnect_timer, 0);
//
//           // Switch back to the previous mode if connection failed
//           esp_wifi_stop();
//           TEST("start_station()")
//           if (global_flags.prev_mode_ap ||
//               !wifi_cred ||
//               (global_flags.prev_mode_ap = true,
//                start_station(wifi_cred + 1) != ESP_OK)
//           ) start_access_point();
//         }
//       }
//     }
//   } else if (event_base == IP_EVENT) {
//     if (event_id == IP_EVENT_STA_GOT_IP) {
//       attempt = 0;
//       delayed_attempt = 0;
//       global_flags.connected = true;
//
//       ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
//       puts("Station IP address:");
//       puts(ip4addr_ntoa(&event->ip_info.ip));
//
//       add_wifi_cred();
//     }
//   }
// err: ;
// }
//
// static esp_err_t start_station(const char* ssid) {
//   // the function is always called with ssid\0pass\0
//   const uint8_t ssid_len = strlen(ssid);
//   const char* pass = ssid + ssid_len + 1;
//   const uint8_t pass_len = strlen(pass);
//
//   // TODO: can this be done without an intermediate struct?
//   // This can (maybe) in principle be done on the event handler,
//   // but then credentials would need to be dynamically allocated
//   wifi_config_t wifi_config = { .sta = { } };
//   memcpy(wifi_config.sta.ssid, ssid, ssid_len + (ssid_len < MAX_SSID_LEN));
//   if (pass_len) {
//     memcpy(wifi_config.sta.password, pass, pass_len + (pass_len < MAX_PASS_LEN));
//     wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
//   }
//
//   CHECK_OK(esp_wifi_set_mode(WIFI_MODE_STA));
//   CHECK_OK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
//   CHECK_OK(esp_wifi_start());
//
//   return ESP_OK;
// err:
//   esp_wifi_stop();
//   return ESP_FAIL;
// }

static esp_err_t start_access_point(void) {
  // global_flags.connected = false;

  // TODO: can this be set once on boot, or is AP config overwritten by STA?
  wifi_config_t wifi_config = {
    .ap = {
      .ssid = AP_SSID,
      .ssid_len = sizeof(AP_SSID) - 1,
      .password = AP_PASS,
      .max_connection = MAX_CONN,
      .authmode = sizeof(AP_PASS) > 1 ? WIFI_AUTH_WPA_WPA2_PSK : WIFI_AUTH_OPEN
    }
  };

  // TODO: can this be done without an intermediate struct?
  CHECK_OK(esp_wifi_set_mode(WIFI_MODE_AP));
  CHECK_OK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
  CHECK_OK(esp_wifi_start());

  tcpip_adapter_ip_info_t ip_info;
  tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info);

  puts("Access point IP address:");
  puts(ip4addr_ntoa(&ip_info.ip));

  return ESP_OK;
err:
  esp_wifi_stop();
  return ESP_FAIL;
}

static void init_server(void) {
  tcpip_adapter_init();

  CHECK_OK(esp_netif_init());
  CHECK_OK(esp_event_loop_create_default());

  // Start HTTP daemon
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  CHECK_OK(httpd_start(&server, &config));

#define ADD_PAGE(METHOD, PAGE) \
  { httpd_uri_t handler = { \
      .uri       = "/" STR(PAGE), \
      .method    = HTTP_##METHOD, \
      .handler   = METHOD##_##PAGE, \
      .user_ctx  = NULL \
    }; \
    httpd_register_uri_handler(server, &handler); \
  }

  ADD_PAGE(GET, )
  ADD_PAGE(GET, get)
  ADD_PAGE(GET, set)

  // ADD_PAGE(POST, connect)

  // ADD_PAGE(POST, nvs_list)

#undef ADD_PAGE

  // Init WiFi
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  CHECK_OK(esp_wifi_init(&cfg));

  // CHECK_OK(esp_event_handler_register(
  //   WIFI_EVENT, WIFI_EVENT_STA_START, &station_event_handler, NULL
  // ));
  // CHECK_OK(esp_event_handler_register(
  //   IP_EVENT, IP_EVENT_STA_GOT_IP, &station_event_handler, NULL
  // ));
  // CHECK_OK(esp_event_handler_register(
  //   IP_EVENT, WIFI_EVENT_STA_DISCONNECTED, &station_event_handler, NULL
  // ));

  // Start Access Point or Station
  puts("start_access_point()");
  /* if (!wifi_cred || start_station(wifi_cred+1) != ESP_OK) */
    start_access_point();

err: ;
}
