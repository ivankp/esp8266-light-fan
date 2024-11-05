static void read_wifi_cred(void) {
  size_t wifi_cred_len = 0;
  CHECK_OK(nvs_get_blob(nvs_storage, "wifi_cred", NULL, &wifi_cred_len)); // get length
  if (wifi_cred_len == 0) goto err;
  if (wifi_cred) free(wifi_cred);
  wifi_cred = malloc(wifi_cred_len);
  CHECK_OK(nvs_get_blob(nvs_storage, "wifi_cred", &wifi_cred, &wifi_cred_len)); // get data
  if (*(uint8_t*)wifi_cred == 0 || // empty
      wifi_cred[wifi_cred_len-1] != '\0' // not null terminated
  ) {
    nvs_erase_key(nvs_storage, "wifi_cred");
    goto err;
  }

  return;
err:
  // no valid credentials are available
  if (wifi_cred) free(wifi_cred);
  wifi_cred = NULL;
}

static const char* find_wifi_cred(const char* ssid) {
  const char* a = wifi_cred;
  const uint8_t ncreds = a ? *(uint8_t*)(a++) : 0;

  for (uint8_t i = 0; i < ncreds; ++i) {
    if (!strcmp(a, ssid)) return a;
    a += strlen(a) + 1;
    a += strlen(a) + 1;
  }
  return NULL;
}

// TODO: test this function
static void add_wifi_cred() {
  wifi_config_t wifi_config = { };
  CHECK_OK(esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_config));

  const char* ssid = (const char*) wifi_config.sta.ssid;
  const char* pass = (const char*) wifi_config.sta.password;

  const char* end = memchr(ssid, '\0', MAX_SSID_LEN);
  const uint8_t ssid_len = end ? end - ssid : MAX_SSID_LEN;
  end = memchr(pass, '\0', MAX_PASS_LEN);
  const uint8_t pass_len = end ? end - pass : MAX_PASS_LEN;

  // uint8_t ssid_len = strlen(cred);
  // const char* pass = cred + cred_len + 1;
  // cred_len += strlen(pass) + 1;

  const char* a = wifi_cred;
  uint8_t ncreds = a ? *(uint8_t*)(a++) : 0;

  const char *b = a, *c = a, *d = a, *e = a;
  for (uint8_t i = 0; i < ncreds; ++i) {
    d = e;
    const uint8_t ssid_len = strlen(e);
    e += ssid_len + 1;
    const uint8_t pass_len = strlen(e);
    if (c == a && !strncmp(d, ssid, MAX_SSID_LEN)) {
      if (d == a && !strncmp(e, pass, MAX_PASS_LEN))
        return; // same ssid and pass in first credential
      b = d;
      e += pass_len + 1;
      c = e;
    } else {
      e += pass_len + 1;
    }
  }

  if (c == a) { // new ssid
    if (ncreds < MAX_CRED) ++ncreds; // add new credentials
    else e = d; // also remove oldest unused credentials
  }
  // if ssid is already on file, ncreds stays the same
  // and records only need to be moved, not removed

  size_t len = 1; // ncreds
  // len += cred_len;
  len += ssid_len;
  len += 1;
  len += pass_len;
  len += 1;
  len += b - a;
  len += e - c;

  char* p = malloc(len);
  *(uint8_t*)(p++) = ncreds;
  // p = mempcpy(p, cred, cred_len);
  p = mempcpy(p, ssid, ssid_len);
  *p++ = '\0';
  p = mempcpy(p, pass, pass_len);
  *p++ = '\0';
  p = mempcpy(p, a, b - a);
  p = mempcpy(p, c, e - c);

  if (wifi_cred) free(wifi_cred);
  wifi_cred = p - len;

  nvs_set_blob(nvs_storage, "wifi_cred", wifi_cred, len);

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

  CHECK_OK_1(nvs_open("storage", NVS_READWRITE, &nvs_storage));

  read_wifi_cred();

err: ;
}
