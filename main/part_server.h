// TODO: Implement API to return saved SSIDs
// TODO: Implement disconnect API

// embedded static files
extern const uint8_t index_page[] asm("_binary_index_html_gz_start");
extern const uint8_t index_page_end[] asm("_binary_index_html_gz_end");

#define UTF8_TEXT "text/html; charset=utf-8"

#define HTTP_PAGES \
  X(GET, ) \
  X(GET, get) \
  X(GET, set) \
  X(GET, ssid) \
  X(POST, connect) \

// =============================================================================

static esp_err_t GET_(httpd_req_t* req) {
  httpd_resp_set_type(req, UTF8_TEXT);
  httpd_resp_set_hdr(req,"Content-Encoding","gzip");
  httpd_resp_send(req, (const char*) index_page, index_page_end - index_page);
  return ESP_OK;
}

static esp_err_t GET_get(httpd_req_t* req) {
  char buf[] = "{\"light\":0,\"fan\":0}";
  char* p = buf;
  FOR_ARRAY(controls, i) {
    Control* const ctrl = controls + i;
    if (ctrl->switch_pin < 0)
      continue;
    p = strchr(p+1, '0');
    *p += gpio_get_level(ctrl->output_pin);
  }
  httpd_resp_set_type(req, HTTPD_TYPE_JSON);
  httpd_resp_send(req, buf, sizeof(buf)-1);
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

      FOR_ARRAY(controls, i) {
        Control* const ctrl = controls + i;

        if (strncmp(ctrl->name, key, key_len))
          continue;

        const char val = *a;
        if (b - a != 1 || !(val == '0' || val == '1')) goto next;
        gpio_set_level(ctrl->output_pin, (val - '0') != ctrl->inverted);

        if (buf_ptr - buf > 1) { *buf_ptr++ = ','; }
        *buf_ptr++ = '\"';
        buf_ptr = mempcpy(buf_ptr, key, key_len);
        *buf_ptr++ = '\"';
        *buf_ptr++ = ':';
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

static esp_err_t GET_ssid(httpd_req_t* req) {
  const char* const ssid = wifi_ssid_pass; // always points to SSID\0PASS\0
  const char* pass = memchr(ssid, '\0', MAX_SSID_LEN+1);
  const uint8_t ssid_len = pass ? pass - ssid : 0;

  httpd_resp_set_type(req, HTTPD_TYPE_OCTET);
  httpd_resp_send(req, ssid, ssid_len);
  return ESP_OK;
}

static esp_err_t start_access_point(void);
static esp_err_t start_station(void);

static TimerHandle_t connect_timer;

static void connect_timer_callback(void* arg) {
  { // save current WiFi mode
    wifi_mode_t mode = WIFI_MODE_AP;
    esp_wifi_get_mode(&mode);
    fallback_wifi_mode_sta = (mode == WIFI_MODE_STA);
  }

  // Stop WiFi and free control block
  esp_wifi_stop();

  // TODO: does this fail synchronously,
  // or do I need to handle this in the event callback?
  if (start_station() != ESP_OK) {
    if (fallback_wifi_mode_sta) {
      read_ssid_pass();
      start_station();
    } else {
      start_access_point();
    }
  }
}

static esp_err_t POST_connect(httpd_req_t* req) {
  const char* response = "";
  char* ssid = wifi_ssid_pass;
  char* pass = NULL;

  size_t len = req->content_len;
  if (len == 0) { // Empty request
    read_ssid_pass();
    if (*ssid) {
      pass = memchr(ssid, '\0', MAX_SSID_LEN+1);
      if (!pass)
        goto server_error;
      ++pass;
      goto connect; // Reconnect using stored credentials
    }
    response = "No saved SSID";
    goto bad_request;
  } else if (len > sizeof(wifi_ssid_pass)) { // Request is too long
    response = "Invalid SSID or PASS";
    goto bad_request;
  }

  // Read SSID and PASS data from request
  for (char *p = ssid; len; ) {
    const int ret = httpd_req_recv(req, p, len);
    if (ret <= 0) {
      // If an error is returned, the URI handler must further return an error.
      // This will ensure that the erroneous socket is closed and cleaned up by
      // the web server.
      if (ret == HTTPD_SOCK_ERR_TIMEOUT)
        continue; // Retry receiving if timeout occurred
      goto server_error;
    }
    p += ret;
    len -= ret;
  }
  len = req->content_len;

  // Validate SSID
  pass = memchr(ssid, '\0', MIN(len, MAX_SSID_LEN+1));
  if (pass < ssid + 1) { // SSID must be at least 1 byte long
    response = "Invalid SSID";
    goto bad_request;
  }
  ++pass; // move past null byte

  // Validate PASS
  len -= pass - ssid;
  if (len == 0) { // no PASS in request
    *pass = '\0';
    len = 1;
  } else if (pass + len - 1 != memchr(pass, '\0', MIN(len, MAX_PASSPHRASE_LEN+1))) {
    response = "Invalid PASS";
    goto bad_request;
  }

connect:
  // TODO: use a timer instead
  connect_timer = xTimerCreate(
    NULL,
    2000 / portTICK_PERIOD_MS, // period in ticks
    pdFALSE, // not periodic
    (void*) 0, // timer id
    station_reconnect_timer_callback
  );
  if (!connect_timer || xTimerStart(connect_timer, 0) != pdPASS)
    goto server_error;

  {
#define PREFIX "Connecting to "
    char response[sizeof(PREFIX) + MAX_SSID_LEN] = PREFIX;
    char* end = mempcpy(response + sizeof(PREFIX) - 1, ssid, pass - ssid - 1);
#undef PREFIX
    httpd_resp_set_type(req, UTF8_TEXT);
    httpd_resp_send(req, response, end - response);
  }

  return ESP_OK;

bad_request:
  httpd_resp_set_status(req, HTTPD_400);
  httpd_resp_set_type(req, UTF8_TEXT);
  return httpd_resp_send(req, response, strlen(response));

server_error:
  httpd_resp_send_500(req);

// err:
  return ESP_FAIL;
}
