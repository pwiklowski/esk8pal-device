#ifndef gatt_h
#define gatt_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"



struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

#define ADV_CONFIG_FLAG             (1 << 0)
#define SCAN_RSP_CONFIG_FLAG        (1 << 1)

#define SAMPLE_DEVICE_NAME "esk8-logger"

#ifdef __cplusplus
extern "C"
{
#endif

    void ble_init();

#ifdef __cplusplus
}
#endif


#endif