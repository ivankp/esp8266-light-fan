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
  if (wifi_cred) free(wifi_cred);
  wifi_cred = malloc(wifi_cred_len);
  CHECK_OK(nvs_get_blob(nvs_storage, "wifi_cred", &wifi_cred, &wifi_cred_len)); // get data

  return;
err:
  // no valid credentials are available
  if (wifi_cred) free(wifi_cred);
  wifi_cred = NULL;
}

/*
static void add_wifi_cred(const uint8_t* new_cred) {
  const uint8_t  new_ssid_len = *new_cred++;
  const uint8_t* new_ssid = new_cred;
  new_cred += new_ssid_len;
  const uint8_t  new_pass_len = *new_cred++;
  const uint8_t* new_pass = new_cred;
  new_cred = new_ssid - 1;

  const uint8_t* a = wifi_cred;
  uint8_t ncreds = a ? *a++ : 0;

  const uint8_t *b = a, *c = a, *d = a, *e = a;
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
    const uint8_t pass_len = *e++;
    e += pass_len; // skip pass
  }

  if (ncreds > 7 && c == a) e = d;

  size_t new_len = 3; // ncreds, new_ssid_len, new_pass_len
  new_len += new_ssid_len;
  new_len += new_pass_len;
  new_len += b - a;
  new_len += e - c;

  if (ncreds < 8 && c == a) ++ncreds;

  uint8_t* p = malloc(new_len);
  *p++ = ncreds;
  p = mempcpy(p, new_cred, new_ssid_len + new_pass_len + 2);
  p = mempcpy(p, a, b - a);
  p = mempcpy(p, c, e - c);

  if (wifi_cred) free(wifi_cred);
  wifi_cred = p - new_len;
  wifi_cred_len = new_len;

  nvs_set_blob(nvs_storage, "wifi_cred", wifi_cred, wifi_cred_len);
}
*/

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

  puts("getting wifi cred from nvs");
  get_wifi_cred();

err: ;
}
