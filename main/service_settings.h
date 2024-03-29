#ifndef service_settings_h
#define service_settings_h

#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#include "gatt.h"
#include "state.h"
#include <string.h>

enum {
  IDX_SVC_SETTINGS,

  IDX_CHAR_RIDING_STATE,
  IDX_CHAR_VAL_RIDING_STATE,
  IDX_CHAR_CFG_RIDING_STATE,

  IDX_CHAR_MANUAL_RIDE_START,
  IDX_CHAR_VAL_MANUAL_RIDE_START,

  IDX_CHAR_WIFI_SSID,
  IDX_CHAR_VAL_WIFI_SSID,

  IDX_CHAR_WIFI_PASS,
  IDX_CHAR_VAL_WIFI_PASS,

  IDX_CHAR_WIFI_ENABLED,
  IDX_CHAR_VAL_WIFI_ENABLED,

  IDX_CHAR_FREE_STORAGE,
  IDX_CHAR_VAL_FREE_STORAGE,
  IDX_CHAR_CFG_FREE_STORAGE,

  IDX_CHAR_TOTAL_STORAGE,
  IDX_CHAR_VAL_TOTAL_STORAGE,

  IDX_CHAR_TIME,
  IDX_CHAR_VAL_TIME,

  IDX_CHAR_DEVICE_KEY,
  IDX_CHAR_VAL_DEVICE_KEY,

  IDX_CHAR_WIFI_SSID_CLIENT,
  IDX_CHAR_VAL_WIFI_SSID_CLIENT,

  IDX_CHAR_WIFI_PASS_CLIENT,
  IDX_CHAR_VAL_WIFI_PASS_CLIENT,

  IDX_CHAR_WIFI_CLIENT_UPLOAD_INTERVAL,
  IDX_CHAR_VAL_WIFI_CLIENT_UPLOAD_INTERVAL,

  SETTINGS_IDX_NB,
};

struct gatts_profile_inst init_settings_service();
void settings_service_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                    esp_ble_gatts_cb_param_t *param);
void settings_update_value(void *value, uint16_t characteristic_index);
void settings_set_value(uint16_t handle_idx, uint16_t len, const uint8_t *value);

#endif