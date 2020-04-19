#ifndef service_battery_h
#define service_battery_h

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"
#include <string.h>
#include "gatt.h"
#include "state.h"

enum {
    IDX_SVC_BATTERY,
    IDX_CHAR_CURRENT,
    IDX_CHAR_VAL_CURRENT,
    IDX_CHAR_CFG_CURRENT,

    IDX_CHAR_VOLTAGE,
    IDX_CHAR_VAL_VOLTAGE,
    IDX_CHAR_CFG_VOLTAGE,

    IDX_CHAR_USED_ENERGY,
    IDX_CHAR_VAL_USED_ENERGY,
    IDX_CHAR_CFG_USED_ENERGY,

    IDX_CHAR_TOTAL_ENERGY,
    IDX_CHAR_VAL_TOTAL_ENERGY,
    IDX_CHAR_CFG_TOTAL_ENERGY,

    BATTERY_IDX_NB,
};

struct gatts_profile_inst init_battery_service();
void gatts_service_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
void battery_update_value(double value, uint16_t characteristic_index, bool force_notify);

#endif