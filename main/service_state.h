#ifndef service_state_h
#define service_state_h

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
  IDX_SVC_STATE,

  IDX_CHAR_STATE,
  IDX_CHAR_VAL_STATE,
  IDX_CHAR_CFG_STATE,

  STATE_IDX_NB,
};

struct gatts_profile_inst init_state_service();
void state_gatts_service_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
void state_update();

bool is_state_service_connected();

#endif