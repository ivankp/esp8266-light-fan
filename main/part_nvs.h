// handle to the NVS namespace
// static nvs_handle_t nvs_storage;

// =============================================================================

// static void init_nvs(void) {
//   // https://github.com/espressif/esp-idf/blob/cf7e743a9b2e5fd2520be4ad047c8584188d54da/examples/storage/nvs_rw_value/main/nvs_value_example_main.c
//   // default partition name is "nvs"
//
//   switch (nvs_flash_init()) {
//     case ESP_OK: break;
//     case ESP_ERR_NVS_NO_FREE_PAGES:
//     case ESP_ERR_NVS_NEW_VERSION_FOUND:
//       CHECK_OK(nvs_flash_erase()); // Erase NVS partition
//       CHECK_OK(nvs_flash_init()); // Re-initialize
//     default:
//       goto err;
//   }
//
//   CHECK_OK(nvs_open("storage", NVS_READWRITE, &nvs_storage));
//
//   // read_wifi_cred();
//
// err: ;
// }

// TODO: return fail code
static void read_ssid_pass(void) {
  // zero-out the buffer
  memset(wifi_ssid_pass, 0, sizeof(wifi_ssid_pass));
}

// TODO: return fail code
static void write_ssid_pass(void) {
}
