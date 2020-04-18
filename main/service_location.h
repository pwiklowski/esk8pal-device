#ifndef service_location_h
#define service_location_h

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"
#include <string.h>
#include "gatt.h"


enum {
    IDX_SVC_LOCATION,
    IDX_CHAR_LATITUDE,
    IDX_CHAR_VAL_LATITUDE,
    IDX_CHAR_CFG_LATITUDE,

    IDX_CHAR_LONGITUDE,
    IDX_CHAR_VAL_LONGITUDE,
    IDX_CHAR_CFG_LONGITUDE,

    IDX_CHAR_SPEED,
    IDX_CHAR_VAL_SPEED,
    IDX_CHAR_CFG_SPEED,

    IDX_CHAR_TRIP_DISTANCE,
    IDX_CHAR_VAL_TRIP_DISTANCE,
    IDX_CHAR_CFG_TRIP_DISTANCE,

    LOCATION_IDX_NB,
};

struct gatts_profile_inst init_location_service();
void location_service_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
void location_update_value(double value, uint16_t characteristic_index);

#endif