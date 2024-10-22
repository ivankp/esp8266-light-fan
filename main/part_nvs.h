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
static char* wifi_cred = NULL;
static size_t wifi_cred_len = 0;

// https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/nvs_flash.html
static nvs_handle_t nvs_storage;

static void get_wifi_cred(void) {
  CHECK_OK(nvs_get_blob(nvs_storage, "wifi_cred", NULL, &wifi_cred_len)); // get length
  if (wifi_cred_len == 0) goto err;
  if (wifi_cred) free(wifi_cred);
  wifi_cred = malloc(wifi_cred_len);
  CHECK_OK(nvs_get_blob(nvs_storage, "wifi_cred", &wifi_cred, &wifi_cred_len)); // get data
  if (wifi_cred[wifi_cred_len-1] != '\0') {
    // corrupted wifi_cred
    nvs_erase_key(nvs_storage, "wifi_cred");
    goto err;
  }

  return;
err:
  // no valid credentials are available
  if (wifi_cred) free(wifi_cred);
  wifi_cred = NULL;
  wifi_cred_len = 0;
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

static void add_wifi_cred(const char* cred) {
  uint8_t cred_len = strlen(cred);
  const char* pass = cred + cred_len + 1;
  cred_len += strlen(pass) + 1;

  const char* a = wifi_cred;
  uint8_t ncreds = a ? *(uint8_t*)(a++) : 0;

  const char *b = a, *c = a, *d = a, *e = a;
  for (uint8_t i = 0; i < ncreds; ++i) {
    d = e;
    const uint8_t ssid_len = strlen(e);
    e += ssid_len + 1;
    const uint8_t pass_len = strlen(e);
    if (c == a && !strcmp(d, cred)) {
      if (d == a && !strcmp(e, pass))
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

  wifi_cred_len = 1; // ncreds
  wifi_cred_len += cred_len;
  wifi_cred_len += b - a;
  wifi_cred_len += e - c;

  char* p = malloc(wifi_cred_len);
  *(uint8_t*)(p++) = ncreds;
  p = mempcpy(p, cred, cred_len);
  p = mempcpy(p, a, b - a);
  p = mempcpy(p, c, e - c);

  if (wifi_cred) free(wifi_cred);
  wifi_cred = p - wifi_cred_len;

  nvs_set_blob(nvs_storage, "wifi_cred", wifi_cred, wifi_cred_len);
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

  get_wifi_cred();

err: ;
}
