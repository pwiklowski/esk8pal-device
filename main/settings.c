#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "state.h"

extern struct CurrentState state;
extern struct Settings settings;
const char *TAG = "Settings";

#define KEY_WIFI_SSID "wifi_ssid"
#define KEY_WIFI_PASS "wifi_pass"
#define KEY_MANUAL_RIDE_START "ride_start"

#define KEY_DEVICE_KEY "device_key"
#define KEY_WIFI_SSID_CLIENT "wifi_ssid_client"
#define KEY_WIFI_PASS_CLIENT "wifi_pass_client"

#define KEY_WIFI_CLIENT_UPLOAD_INTERVAL "wifi_client_upload_interval"

void settings_load() {
  esp_err_t err;
  nvs_handle_t my_handle;
  err = nvs_open("storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
  } else {
    nvs_get_u8(my_handle, KEY_MANUAL_RIDE_START, &settings.manual_ride_start);

    size_t len = 20;
    nvs_get_str(my_handle, KEY_WIFI_SSID, (char *)settings.wifi_ssid, &len);

    len = 20;
    nvs_get_str(my_handle, KEY_WIFI_PASS, (char *)settings.wifi_pass, &len);

    len = 20;
    nvs_get_str(my_handle, KEY_WIFI_SSID_CLIENT, (char *)settings.wifi_ssid_client, &len);

    len = 20;
    nvs_get_str(my_handle, KEY_WIFI_PASS_CLIENT, (char *)settings.wifi_pass_client, &len);

    len = 40;
    nvs_get_str(my_handle, KEY_DEVICE_KEY, (char *)settings.device_key, &len);

    nvs_get_u16(my_handle, KEY_WIFI_CLIENT_UPLOAD_INTERVAL, &settings.upload_interval);

    nvs_close(my_handle);
  }
}

void settings_save() {
  esp_err_t err;
  nvs_handle_t my_handle;
  err = nvs_open("storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
  } else {
    nvs_set_u8(my_handle, KEY_MANUAL_RIDE_START, settings.manual_ride_start);
    nvs_set_str(my_handle, KEY_WIFI_SSID, (char *)settings.wifi_ssid);
    nvs_set_str(my_handle, KEY_WIFI_PASS, (char *)settings.wifi_pass);

    nvs_set_str(my_handle, KEY_WIFI_SSID_CLIENT, (char *)settings.wifi_ssid_client);
    nvs_set_str(my_handle, KEY_WIFI_PASS_CLIENT, (char *)settings.wifi_pass_client);
    nvs_set_str(my_handle, KEY_DEVICE_KEY, (char *)settings.device_key);

    nvs_set_u16(my_handle, KEY_WIFI_CLIENT_UPLOAD_INTERVAL, (char *)settings.upload_interval);

    err = nvs_commit(my_handle);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to save settings");
    }
    nvs_close(my_handle);
  }
}

void settings_init() {
  esp_err_t ret;

  ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  settings_load();
}