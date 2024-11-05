// AP = access point
// STA = station
// stations connect to access points

// WiFi connection behavior
// ============================================================================
// Cause     | N saved | Effect                       | Option | Handler
// ----------------------------------------------------------------------------
// Boot up   | N =  0  | AP                           | AP     | init_server
// Boot up   | N >= 1  | Try first saved, else AP     | ONCE   | start_station
// Blank req | N =  0  | Error                        | Error  | POST_connect
// Blank req | N >= 1  | Try each saved once, else AP | ALL    | start_station
// Req SSID  | N =  0  | Error                        | Error  | POST_connect
// Req SSID  | N >= 1  | Try once, else prev state    | ONCE   | start_station
// Req S & P |         | Try once, else prev state    | ONCE   | start_station
// Disconnect|         | Try first saved a few times  | FIRST  | event handler
// ----------------------------------------------------------------------------

// TODO: Implement API to return saved SSIDs
// TODO: Implement disconnect API

static esp_err_t start_access_point(void);
static esp_err_t start_station(const char* cred);

static esp_err_t GET_(httpd_req_t* req) {
  httpd_resp_set_hdr(req,"Content-Encoding","gzip");
  httpd_resp_send(req, (const char*) index_page, index_page_end - index_page);
  return ESP_OK;
}

static esp_err_t GET_get(httpd_req_t* req) {
  char buf[] = "{\"light\":0,\"fan\":0}";
  char* p = strchr(buf, '0');
  *p += gpio_get_level(LIGHT_PIN);
  p = strchr(p+1, '0');
  *p += gpio_get_level(FAN_PIN);
  httpd_resp_set_type(req, HTTPD_TYPE_JSON);
  httpd_resp_send(req, buf, strlen(buf));
  return ESP_OK;
}

static esp_err_t GET_set(httpd_req_t* req) {
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

      if (KEYCMP("light")) {
        const char val = *a;
        if (b - a != 1 || !(val == '0' || val == '1')) goto next;
        gpio_set_level(LIGHT_PIN, val - '0');
        KEYCPY
        *buf_ptr++ = val;
      } else
      if (KEYCMP("fan")) {
        const char val = *a;
        if (b - a != 1 || !(val == '0' || val == '1')) goto next;
        gpio_set_level(FAN_PIN, val - '0');
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

#undef KEYCMP
#undef KEYCPY

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

// WiFi standard allows arbitrary SSID and PASS bytes
// But esp firmware library relies on them being null terminated

static esp_err_t POST_connect(httpd_req_t* req) {
  const char* response = "";
  char buf[MAX_SSID_LEN+1+MAX_PASS_LEN+1];
  const char* ssid = buf;

  size_t len = req->content_len;
  if (len == 0) { // no SSID in request
    if (!wifi_cred) {
      response = "No known SSIDs";
      goto bad_request;
    }
    ssid = wifi_cred + 1;
    goto connect;
  }
  if (len > sizeof(buf)) {
    response = "SSID or PASS is too long";
    goto bad_request;
  }

  for (char *p = buf; len;) {
    const int ret = httpd_req_recv(req, p, len);
    if (ret <= 0) { // Retry receiving if timeout occurred
      if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue;
      goto server_error;
    }
    p += ret;
    len -= ret;
  }
  len = req->content_len;

  const char* pass = memchr(ssid, '\0', MIN(len, MAX_SSID_LEN+1));
  if (!pass) {
    response = "SSID not terminated or longer than " STR(MAX_SSID_LEN) " bytes";
    goto bad_request;
  }
  ++pass; // move past null byte

  len -= pass - ssid;
  if (len == 0) { // no PASS in request
    ssid = find_wifi_cred(ssid);
    if (!ssid) {
      response = "Not a known SSID";
      goto bad_request;
    }
  }
  if (!memchr(pass, '\0', MIN(len, MAX_PASS_LEN+1))) {
    response = "PASS not terminated or longer than " STR(MAX_PASS_LEN) " bytes";
    goto bad_request;
  }

connect:
  {
#define PREFIX "Connecting to "
    char response[sizeof(PREFIX) + MAX_SSID_LEN] = PREFIX;
    char* end = mempcpy(response + sizeof(PREFIX) - 1, ssid, strlen(ssid));
#undef PREFIX
    httpd_resp_send(req, response, end - response);
  }

  { // save current WiFi mode
    wifi_mode_t mode = WIFI_MODE_AP;
    esp_wifi_get_mode(&mode);
    wifi_flags.prev_mode_ap = (mode != WIFI_MODE_STA);
  }

  esp_wifi_deauth_sta(0);
  esp_wifi_stop();

  if (start_station(ssid) != ESP_OK)
    start_access_point();

  return ESP_OK;

bad_request:
  httpd_resp_set_status(req, HTTPD_400);
  return httpd_resp_send(req, response, strlen(response));

server_error:
  httpd_resp_send_500(req);

// err:
  return ESP_FAIL;
}

// https://docs.espressif.com/projects/esp-idf/en/v4.0.3/api-reference/network/esp_wifi.html
// https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/wifi.html

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static TimerHandle_t station_reconnect_timer = NULL;

static void station_reconnect_timer_callback(void* arg) {
  esp_wifi_connect();
}

// TODO: find info on esp_wifi_set_config + NVS

static void station_event_handler(
  void* arg,
  esp_event_base_t event_base,
  int32_t event_id,
  void* event_data
) {
  static uint8_t attempt = 0;
  static uint8_t delayed_attempt = 0;
  if (event_base == WIFI_EVENT) {
    if (event_id == WIFI_EVENT_STA_START) {
      attempt = 0;
      delayed_attempt = 0;
      wifi_flags.connected = false;
      esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
      if (attempt < MAX_STATION_ATTEMPTS) {
        ++attempt;
        puts("Retrying AP connection");
        esp_wifi_connect();
        // WIFI_EVENT_STA_DISCONNECTED is triggered by esp_wifi_connect()
        // if it fails
      } else { // try to connect after a delay
        attempt = 0;
        puts("AP connection failed");
        if (wifi_flags.connected &&
            delayed_attempt < MAX_STATION_DELAYED_ATTEMPTS
        ) {
          ++delayed_attempt;
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
        } else {
          delayed_attempt = 0;
          if (station_reconnect_timer)
            xTimerDelete(station_reconnect_timer, 0);

          // Switch back to the previous mode if connection failed
          esp_wifi_stop();
          if (wifi_flags.prev_mode_ap ||
              !wifi_cred ||
              (wifi_flags.prev_mode_ap = true,
               start_station(wifi_cred + 1) != ESP_OK)
          ) start_access_point();
        }
      }
    }
  } else if (event_base == IP_EVENT) {
    if (event_id == IP_EVENT_STA_GOT_IP) {
      attempt = 0;
      delayed_attempt = 0;
      wifi_flags.connected = true;

      ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
      puts("Station IP address:");
      puts(ip4addr_ntoa(&event->ip_info.ip));

      add_wifi_cred();
    }
  }
}

static esp_err_t start_station(const char* ssid) {
  // the function is always called with ssid\0pass\0
  const uint8_t ssid_len = strlen(ssid);
  const char* pass = ssid + ssid_len + 1;
  const uint8_t pass_len = strlen(pass);

  // TODO: can this be done without an intermediate struct?
  // This can (maybe) in principle be done on the event handler,
  // but then credentials would need to be dynamically allocated
  wifi_config_t wifi_config = { .sta = { } };
  memcpy(wifi_config.sta.ssid, ssid, ssid_len + (ssid_len < MAX_SSID_LEN));
  if (pass_len) {
    memcpy(wifi_config.sta.password, pass, pass_len + (pass_len < MAX_PASS_LEN));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  }

  CHECK_OK(esp_wifi_set_mode(WIFI_MODE_STA));
  CHECK_OK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
  CHECK_OK_1(esp_wifi_start());

  return ESP_OK;
err:
  esp_wifi_stop();
  return ESP_FAIL;
}

static esp_err_t start_access_point(void) {
  wifi_flags.connected = false;

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
  CHECK_OK_1(esp_wifi_start());

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

  CHECK_OK_1(esp_netif_init());
  CHECK_OK_1(esp_event_loop_create_default());

  // Start HTTP daemon
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  CHECK_OK_1(httpd_start(&server, &config));

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

  ADD_PAGE(POST, connect)

#undef ADD_PAGE

  // Init WiFi
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  CHECK_OK(esp_wifi_init(&cfg));

  CHECK_OK(esp_event_handler_register(
    WIFI_EVENT, WIFI_EVENT_STA_START, &station_event_handler, NULL
  ));
  CHECK_OK(esp_event_handler_register(
    IP_EVENT, IP_EVENT_STA_GOT_IP, &station_event_handler, NULL
  ));
  CHECK_OK(esp_event_handler_register(
    IP_EVENT, WIFI_EVENT_STA_DISCONNECTED, &station_event_handler, NULL
  ));

  // Start Access Point or Station
  if (!wifi_cred || start_station(wifi_cred) != ESP_OK)
    start_access_point();

err: ;
}
