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

// embedded static files
extern const uint8_t index_page[] asm("_binary_index_html_gz_start");
extern const uint8_t index_page_end[] asm("_binary_index_html_gz_end");

/* static esp_err_t start_access_point(void); */
/* static esp_err_t start_station(const char* cred); */

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
  httpd_resp_send(req, buf, strlen(buf)); // TODO: sizeof
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

/*
static TimerHandle_t station_connect_timer = NULL;

static void station_connect_timer_callback(void* arg) {
  xTimerDelete(station_connect_timer, 0);
  station_connect_timer = NULL;

  esp_wifi_deauth_sta(0);
  esp_wifi_stop();

  server_busy = false;

  // TODO: why is this called twice sometimes?
  TEST("start_station()")
  if (start_station(ssid) != ESP_OK)
    start_access_point();
}
*/

// WiFi standard allows arbitrary SSID and PASS bytes
// But esp firmware library relies on them being null terminated

// static esp_err_t POST_connect(httpd_req_t* req) {
//   const char* response = "";
//   char buf[MAX_SSID_LEN+1+MAX_PASS_LEN+1];
//   const char* ssid = buf;
//
//   size_t len = req->content_len;
//   if (len == 0) { // no SSID in request
//     if (!wifi_cred) {
//       response = "No known SSIDs";
//       goto bad_request;
//     }
//     ssid = wifi_cred + 1;
//     goto connect;
//   }
//   if (len > sizeof(buf)) {
//     response = "SSID or PASS is too long";
//     goto bad_request;
//   }
//
//   for (char *p = buf; len;) {
//     const int ret = httpd_req_recv(req, p, len);
//     if (ret <= 0) { // Retry receiving if timeout occurred
//       if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue;
//       goto server_error;
//     }
//     p += ret;
//     len -= ret;
//   }
//   len = req->content_len;
//
//   const char* pass = memchr(ssid, '\0', MIN(len, MAX_SSID_LEN+1));
//   if (!pass) {
//     response = "SSID not terminated or longer than " STR(MAX_SSID_LEN) " bytes";
//     goto bad_request;
//   }
//   ++pass; // move past null byte
//
//   len -= pass - ssid;
//   if (len == 0) { // no PASS in request
//     ssid = find_wifi_cred(ssid);
//     if (!ssid) {
//       response = "Not a known SSID";
//       goto bad_request;
//     }
//   }
//   if (!memchr(pass, '\0', MIN(len, MAX_PASS_LEN+1))) {
//     response = "PASS not terminated or longer than " STR(MAX_PASS_LEN) " bytes";
//     goto bad_request;
//   }
//
// connect:
//   {
// #define PREFIX "Connecting to "
//     char response[sizeof(PREFIX) + MAX_SSID_LEN] = PREFIX;
//     char* end = mempcpy(response + sizeof(PREFIX) - 1, ssid, strlen(ssid));
// #undef PREFIX
//     httpd_resp_send(req, response, end - response);
//   }
//   // TODO: httpd_resp_send() returns too fast
//   // client appears to not receive before esp_wifi_stop()
//
//   { // save current WiFi mode
//     wifi_mode_t mode = WIFI_MODE_AP;
//     esp_wifi_get_mode(&mode);
//     global_flags.prev_mode_ap = (mode != WIFI_MODE_STA);
//   }
//
//   global_flags.manual_disconnect = !global_flags.prev_mode_ap;
//
//   // if (!station_connect_timer) {
//   //   station_connect_timer = xTimerCreate/*Static*/(
//   //     "",
//   //     2000 / portTICK_PERIOD_MS, // period in ticks
//   //     pdFALSE, // not periodic
//   //     (void*) 0, // timer id
//   //     station_connect_timer_callback
//   //   );
//   // }
//   // xTimerStart(station_connect_timer, 0);
//
//   sleep(2); // delay to allow current requests to finish
//
//   esp_wifi_deauth_sta(0);
//   esp_wifi_stop();
//
//   // TODO: why is this called twice sometimes?
//   TEST("start_station()")
//   if (start_station(ssid) != ESP_OK)
//     start_access_point();
//
//   return ESP_OK;
//
// bad_request:
//   httpd_resp_set_status(req, HTTPD_400);
//   return httpd_resp_send(req, response, strlen(response));
//
// server_error:
//   httpd_resp_send_500(req);
//
// // err:
//   return ESP_FAIL;
// }
