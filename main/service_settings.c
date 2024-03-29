#include "esp_bt.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"
#include "gatt.h"
#include "power.h"
#include "service_settings.h"
#include "settings.h"
#include "state.h"
#include "wifi.h"

#include "ds3231/ds3231.h"
#include <sys/time.h>
#include <time.h>

#define GATTS_TABLE_TAG "SettingsService"

#define SVC_INST_ID 0

#define GATTS_DEMO_CHAR_VAL_LEN_MAX 500
#define PREPARE_BUF_MAX_SIZE 1024
#define CHAR_DECLARATION_SIZE (sizeof(uint8_t))

extern struct CurrentState state;
extern struct Settings settings;

static uint8_t adv_config_done = 0;

uint16_t settings_handle_table[SETTINGS_IDX_NB];

uint16_t settings_notification_table[SETTINGS_IDX_NB];

static uint16_t connection_id;

uint8_t settings_service_uuid[16] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    // first uuid, 16bit, [12],[13] is the value
    0xfd, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00,
};

/* The length of adv data must be less than 31 bytes */
esp_ble_adv_data_t settings_adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006, // slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval = 0x0010, // slave connection max interval, Time = max_interval * 1.25 msec
    .appearance = 0x00,
    .manufacturer_len = 0,       // TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data = NULL, // test_manufacturer,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(settings_service_uuid),
    .p_service_uuid = settings_service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

// scan response data
esp_ble_adv_data_t settings_scan_rsp_data = {
    .set_scan_rsp = true,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006,
    .max_interval = 0x0010,
    .appearance = 0x00,
    .manufacturer_len = 0,       // TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data = NULL, //&test_manufacturer[0],
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(settings_service_uuid),
    .p_service_uuid = settings_service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

extern esp_ble_adv_params_t adv_params;

struct gatts_profile_inst settings_profile_tab = {
    .gatts_cb = settings_service_event_handler,
    .gatts_if = ESP_GATT_IF_NONE, /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
};

/* Service */
static const uint16_t GATTS_SERVICE_UUID_SETTINGS = 0x00FD;

static const uint16_t GATTS_CHAR_UUID_RIDING_STATE = 0xFD01;
static const uint16_t GATTS_CHAR_UUID_MANUAL_RIDE_START = 0xFD02;
static const uint16_t GATTS_CHAR_UUID_WIFI_SSID = 0xFD03;
static const uint16_t GATTS_CHAR_UUID_WIFI_PASS = 0xFD04;
static const uint16_t GATTS_CHAR_UUID_WIFI_ENABLED = 0xFD05;
static const uint16_t GATTS_CHAR_UUID_FREE_STORAGE = 0xFD06;
static const uint16_t GATTS_CHAR_UUID_TOTAL_STORAGE = 0xFD07;
static const uint16_t GATTS_CHAR_UUID_TIME = 0xFD08;

static const uint16_t GATTS_CHAR_UUID_DEVICE_KEY = 0xFD09;
static const uint16_t GATTS_CHAR_UUID_WIFI_SSID_CLIENT = 0xFD0A;
static const uint16_t GATTS_CHAR_UUID_WIFI_PASS_CLIENT = 0xFD0B;
static const uint16_t GATTS_CHAR_UUID_UPLOAD_INTERVAL = 0xFD0C;

static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint8_t char_prop_read_write = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE;
static const uint8_t char_prop_read_write_notify =
    ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY | ESP_GATT_CHAR_PROP_BIT_WRITE;

static const uint8_t char_prop_read = ESP_GATT_CHAR_PROP_BIT_READ;
static const uint8_t char_prop_read_notify = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;

static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t config_descriptor[2] = {0x00, 0x00};

uint8_t time_data[6];

/* Full Database Description - Used to add attributes into the database */
static const esp_gatts_attr_db_t gatt_db[SETTINGS_IDX_NB] = {
    // Service Declaration
    [IDX_SVC_SETTINGS] = {{ESP_GATT_AUTO_RSP},
                          {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ, sizeof(uint16_t),
                           sizeof(GATTS_SERVICE_UUID_SETTINGS), (uint8_t *)&GATTS_SERVICE_UUID_SETTINGS}},

    /* Characteristic Declaration */
    [IDX_CHAR_RIDING_STATE] = {{ESP_GATT_AUTO_RSP},
                               {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
                                CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write_notify}},
    /* Characteristic Value */
    [IDX_CHAR_VAL_RIDING_STATE] = {{ESP_GATT_AUTO_RSP},
                                   {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_RIDING_STATE,
                                    ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(state.riding_state),
                                    sizeof(state.riding_state), (uint8_t *)&state.riding_state}},
    /* Client Characteristic Configuration Descriptor */
    [IDX_CHAR_CFG_RIDING_STATE] = {{ESP_GATT_AUTO_RSP},
                                   {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid,
                                    ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(uint16_t),
                                    sizeof(config_descriptor), (uint8_t *)config_descriptor}},

    /* Characteristic Declaration */
    [IDX_CHAR_MANUAL_RIDE_START] = {{ESP_GATT_AUTO_RSP},
                                    {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
                                     CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write}},
    /* Characteristic Value */
    [IDX_CHAR_VAL_MANUAL_RIDE_START] = {{ESP_GATT_AUTO_RSP},
                                        {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_MANUAL_RIDE_START,
                                         ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(settings.manual_ride_start),
                                         sizeof(settings.manual_ride_start), (uint8_t *)&settings.manual_ride_start}},

    /* Characteristic Declaration */
    [IDX_CHAR_WIFI_SSID] = {{ESP_GATT_AUTO_RSP},
                            {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
                             CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write}},
    /* Characteristic Value */
    [IDX_CHAR_VAL_WIFI_SSID] = {{ESP_GATT_AUTO_RSP},
                                {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_WIFI_SSID,
                                 ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(settings.wifi_ssid),
                                 sizeof(settings.wifi_ssid), (uint8_t *)settings.wifi_ssid}},

    /* Characteristic Declaration */
    [IDX_CHAR_WIFI_PASS] = {{ESP_GATT_AUTO_RSP},
                            {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
                             CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write}},
    /* Characteristic Value */
    [IDX_CHAR_VAL_WIFI_PASS] = {{ESP_GATT_AUTO_RSP},
                                {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_WIFI_PASS,
                                 ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(settings.wifi_pass),
                                 sizeof(settings.wifi_pass), (uint8_t *)settings.wifi_pass}},

    /* Characteristic Declaration */
    [IDX_CHAR_WIFI_ENABLED] = {{ESP_GATT_AUTO_RSP},
                               {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
                                CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write}},
    /* Characteristic Value */
    [IDX_CHAR_VAL_WIFI_ENABLED] = {{ESP_GATT_AUTO_RSP},
                                   {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_WIFI_ENABLED,
                                    ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(settings.wifi_state),
                                    sizeof(settings.wifi_state), (uint8_t *)&settings.wifi_state}},

    /* Characteristic Declaration */
    [IDX_CHAR_FREE_STORAGE] = {{ESP_GATT_AUTO_RSP},
                               {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
                                CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_notify}},
    /* Characteristic Value */
    [IDX_CHAR_VAL_FREE_STORAGE] = {{ESP_GATT_AUTO_RSP},
                                   {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_FREE_STORAGE,
                                    ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(state.free_storage),
                                    sizeof(state.free_storage), (uint8_t *)&state.free_storage}},
    /* Client Characteristic Configuration Descriptor */
    [IDX_CHAR_CFG_FREE_STORAGE] = {{ESP_GATT_AUTO_RSP},
                                   {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid,
                                    ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(uint16_t),
                                    sizeof(config_descriptor), (uint8_t *)config_descriptor}},

    /* Characteristic Declaration */
    [IDX_CHAR_TOTAL_STORAGE] = {{ESP_GATT_AUTO_RSP},
                                {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
                                 CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read}},
    /* Characteristic Value */
    [IDX_CHAR_VAL_TOTAL_STORAGE] = {{ESP_GATT_AUTO_RSP},
                                    {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_TOTAL_STORAGE,
                                     ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(state.total_storage),
                                     sizeof(state.total_storage), (uint8_t *)&state.total_storage}},

    [IDX_CHAR_TIME] = {{ESP_GATT_AUTO_RSP},
                       {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
                        CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write}},
    /* Characteristic Value */
    [IDX_CHAR_VAL_TIME] = {{ESP_GATT_AUTO_RSP},
                           {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_TIME, ESP_GATT_PERM_WRITE, sizeof(time_data),
                            sizeof(time_data), (uint8_t *)&time_data}},

    /* Characteristic Declaration */
    [IDX_CHAR_DEVICE_KEY] = {{ESP_GATT_AUTO_RSP},
                             {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
                              CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write}},
    /* Characteristic Value */
    [IDX_CHAR_VAL_DEVICE_KEY] = {{ESP_GATT_AUTO_RSP},
                                 {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_DEVICE_KEY,
                                  ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(settings.device_key),
                                  sizeof(settings.device_key), (uint8_t *)settings.device_key}},

    /* Characteristic Declaration */
    [IDX_CHAR_WIFI_SSID_CLIENT] = {{ESP_GATT_AUTO_RSP},
                                   {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
                                    CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write}},
    /* Characteristic Value */
    [IDX_CHAR_VAL_WIFI_SSID_CLIENT] = {{ESP_GATT_AUTO_RSP},
                                       {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_WIFI_SSID_CLIENT,
                                        ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(settings.wifi_ssid_client),
                                        sizeof(settings.wifi_ssid_client), (uint8_t *)settings.wifi_ssid_client}},

    /* Characteristic Declaration */
    [IDX_CHAR_WIFI_PASS_CLIENT] = {{ESP_GATT_AUTO_RSP},
                                   {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
                                    CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write}},
    /* Characteristic Value */
    [IDX_CHAR_VAL_WIFI_PASS_CLIENT] = {{ESP_GATT_AUTO_RSP},
                                       {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_WIFI_PASS_CLIENT,
                                        ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(settings.wifi_pass_client),
                                        sizeof(settings.wifi_pass_client), (uint8_t *)settings.wifi_pass_client}},

    /* Characteristic Declaration */
    [IDX_CHAR_WIFI_CLIENT_UPLOAD_INTERVAL] = {{ESP_GATT_AUTO_RSP},
                                              {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid,
                                               ESP_GATT_PERM_READ, CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE,
                                               (uint8_t *)&char_prop_read_write}},
    /* Characteristic Value */
    [IDX_CHAR_VAL_WIFI_CLIENT_UPLOAD_INTERVAL] = {{ESP_GATT_AUTO_RSP},
                                                  {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_UPLOAD_INTERVAL,
                                                   ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                   sizeof(settings.upload_interval), sizeof(settings.upload_interval),
                                                   (uint8_t *)&settings.upload_interval}},

};

struct gatts_profile_inst init_settings_service() {
  return settings_profile_tab;
}

void set_time(int year, int month, int day, int hour, int min, int sec) {
  ESP_LOGI(GATTS_TABLE_TAG, "set_time %d %d %d %d %d %d", year, month, day, hour, min, sec);

  struct tm tm;
  tm.tm_year = year - 1900;

  tm.tm_mon = month;
  tm.tm_mday = day;

  tm.tm_hour = hour;
  tm.tm_min = min;
  tm.tm_sec = sec;
  time_t t = mktime(&tm);
  struct timeval now = {.tv_sec = t};
  settimeofday(&now, NULL);

  bool isPowered = power_is_module_powered();

  if (!isPowered) {
    power_up_module();
  }
  ds3231_set_time(&tm);
  if (!isPowered) {
    power_down_module();
  }
}

void settings_set_state(uint16_t handle, uint8_t *value, uint16_t len) {
  ESP_LOGI(GATTS_TABLE_TAG, "settings_set_state %d %d", handle, len);
  if (handle == settings_handle_table[IDX_CHAR_VAL_RIDING_STATE]) {
    if (settings.manual_ride_start) {
      state_set_device_state(value[0]);
    }
    ESP_LOGI(GATTS_TABLE_TAG, "settings_set_state IDX_CHAR_VAL_RIDING_STATE %d", value[0]);
  } else if (handle == settings_handle_table[IDX_CHAR_VAL_MANUAL_RIDE_START]) {
    settings.manual_ride_start = value[0];
    settings_save();
    ESP_LOGI(GATTS_TABLE_TAG, "settings_set_state IDX_CHAR_VAL_MANUAL_RIDE_START %d", value[0]);
  } else if (handle == settings_handle_table[IDX_CHAR_VAL_WIFI_ENABLED]) {
    settings.wifi_state = value[0];

    wifi_set_state(settings.wifi_state);

    ESP_LOGI(GATTS_TABLE_TAG, "settings_set_state IDX_CHAR_VAL_WIFI_ENABLED %d", value[0]);
  } else if (handle == settings_handle_table[IDX_CHAR_VAL_WIFI_SSID]) {
    memcpy(settings.wifi_ssid, value, len);
    settings.wifi_ssid[len] = 0;
    settings_save();
    ESP_LOGI(GATTS_TABLE_TAG, "settings_set_state IDX_CHAR_VAL_WIFI_SSID %s", settings.wifi_ssid);
  } else if (handle == settings_handle_table[IDX_CHAR_VAL_WIFI_PASS]) {
    memcpy(settings.wifi_pass, value, len);
    settings.wifi_pass[len] = 0;
    settings_save();
    ESP_LOGI(GATTS_TABLE_TAG, "settings_set_state IDX_CHAR_VAL_WIFI_PASS %s", settings.wifi_pass);
  } else if (handle == settings_handle_table[IDX_CHAR_VAL_WIFI_SSID_CLIENT]) {
    memcpy(settings.wifi_ssid_client, value, len);
    settings.wifi_ssid_client[len] = 0;
    settings_save();
    ESP_LOGI(GATTS_TABLE_TAG, "settings_set_state IDX_CHAR_VAL_WIFI_SSID_CLIENT %s", settings.wifi_ssid_client);

  } else if (handle == settings_handle_table[IDX_CHAR_VAL_WIFI_PASS_CLIENT]) {
    memcpy(settings.wifi_pass_client, value, len);
    settings.wifi_pass_client[len] = 0;
    settings_save();
    ESP_LOGI(GATTS_TABLE_TAG, "settings_set_state IDX_CHAR_VAL_WIFI_PASS_CLIENT %s", settings.wifi_pass_client);

  } else if (handle == settings_handle_table[IDX_CHAR_VAL_DEVICE_KEY]) {
    memcpy(settings.device_key, value, len);
    settings.device_key[len] = 0;
    settings_save();
    ESP_LOGI(GATTS_TABLE_TAG, "settings_set_state IDX_CHAR_VAL_DEVICE_KEY %s", settings.device_key);

  } else if (handle == settings_handle_table[IDX_CHAR_VAL_WIFI_CLIENT_UPLOAD_INTERVAL]) {
    memcpy(&settings.upload_interval, value, 2);
    settings_save();
    ESP_LOGI(GATTS_TABLE_TAG, "settings_set_state IDX_CHAR_VAL_UPLOAD_INTERVAL %d", settings.upload_interval);

  } else if (handle == settings_handle_table[IDX_CHAR_VAL_TIME]) {
    if (len != 6)
      return;

    set_time((int)2000 + (int)value[0], value[1], value[2], value[3], value[4], value[5]);

    ESP_LOGI(GATTS_TABLE_TAG, "settings_set_state IDX_CHAR_VAL_TIME %s", settings.wifi_pass);
  }
}

void settings_service_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                    esp_ble_gatts_cb_param_t *param) {
  switch (event) {
  case ESP_GATTS_REG_EVT: {
    esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(SAMPLE_DEVICE_NAME);
    if (set_dev_name_ret) {
      ESP_LOGE(GATTS_TABLE_TAG, "set device name failed, error code = %x", set_dev_name_ret);
    }
    // config adv data
    esp_err_t ret = esp_ble_gap_config_adv_data(&settings_adv_data);
    if (ret) {
      ESP_LOGE(GATTS_TABLE_TAG, "config adv data failed, error code = %x", ret);
    }
    adv_config_done |= ADV_CONFIG_FLAG;

    adv_config_done |= SCAN_RSP_CONFIG_FLAG;
    esp_err_t create_attr_ret = esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, SETTINGS_IDX_NB, SVC_INST_ID);
    if (create_attr_ret) {
      ESP_LOGE(GATTS_TABLE_TAG, "create attr table failed, error code = %x", create_attr_ret);
    }

    settings_profile_tab.gatts_if = gatts_if;

  } break;
  case ESP_GATTS_READ_EVT:
    ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_READ_EVT");
    break;
  case ESP_GATTS_WRITE_EVT:
    ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_WRITE_EVT");

    if (param->write.handle == settings_handle_table[IDX_CHAR_CFG_RIDING_STATE] ||
        param->write.handle == settings_handle_table[IDX_CHAR_CFG_FREE_STORAGE]) {
      uint16_t descr_value = param->write.value[1] << 8 | param->write.value[0];
      uint16_t index = 0;

      for (uint8_t i = 0; i < SETTINGS_IDX_NB; i++) {
        if (settings_handle_table[i] == param->write.handle) {
          index = i;
          break;
        }
      }

      settings_notification_table[index] = descr_value;
      ESP_LOGI(GATTS_TABLE_TAG, "notify enable %d %d %d ", index, param->write.handle, descr_value);

      if (param->write.handle == settings_handle_table[IDX_CHAR_CFG_RIDING_STATE]) {
        settings_set_value(IDX_CHAR_VAL_RIDING_STATE, 1, &state.riding_state);
      } else if (param->write.handle == settings_handle_table[IDX_CHAR_CFG_FREE_STORAGE]) {
        settings_set_value(IDX_CHAR_VAL_FREE_STORAGE, 4, &state.free_storage);
      }
    }

    settings_set_state(param->write.handle, param->write.value, param->write.len);
    break;
  case ESP_GATTS_EXEC_WRITE_EVT:
    ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_EXEC_WRITE_EVT");
    break;
  case ESP_GATTS_MTU_EVT:
    ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
    break;
  case ESP_GATTS_CONF_EVT:
    break;
  case ESP_GATTS_START_EVT:
    ESP_LOGI(GATTS_TABLE_TAG, "SERVICE_START_EVT, status %d, service_handle %d", param->start.status,
             param->start.service_handle);
    break;
  case ESP_GATTS_CONNECT_EVT:
    ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_CONNECT_EVT, conn_id = %d", param->connect.conn_id);
    connection_id = param->connect.conn_id;
    break;
  case ESP_GATTS_DISCONNECT_EVT:
    ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_DISCONNECT_EVT, reason = 0x%x", param->disconnect.reason);
    esp_ble_gap_start_advertising(&adv_params);
    break;
  case ESP_GATTS_CREAT_ATTR_TAB_EVT: {
    if (param->add_attr_tab.status != ESP_GATT_OK) {
      ESP_LOGE(GATTS_TABLE_TAG, "create attribute table failed, error code=0x%x", param->add_attr_tab.status);
    } else if (param->add_attr_tab.num_handle != SETTINGS_IDX_NB) {
      ESP_LOGE(GATTS_TABLE_TAG, "create attribute table abnormally, num_handle (%d) \
                        doesn't equal to HRS_IDX_NB(%d)",
               param->add_attr_tab.num_handle, SETTINGS_IDX_NB);
    } else {
      ESP_LOGI(GATTS_TABLE_TAG, "create attribute table successfully, the number handle = %d\n",
               param->add_attr_tab.num_handle);
      memcpy(settings_handle_table, param->add_attr_tab.handles, sizeof(settings_handle_table));
      esp_ble_gatts_start_service(settings_handle_table[IDX_SVC_SETTINGS]);
    }
    break;
  }
  case ESP_GATTS_STOP_EVT:
  case ESP_GATTS_OPEN_EVT:
  case ESP_GATTS_CANCEL_OPEN_EVT:
  case ESP_GATTS_CLOSE_EVT:
  case ESP_GATTS_LISTEN_EVT:
  case ESP_GATTS_CONGEST_EVT:
  case ESP_GATTS_UNREG_EVT:
  case ESP_GATTS_DELETE_EVT:
  default:
    break;
  }
}

void settings_set_value(uint16_t handle_idx, uint16_t len, const uint8_t *value) {
  esp_ble_gatts_set_attr_value(settings_handle_table[handle_idx], len, value);

  if (settings_notification_table[handle_idx + 1] == 0x0001) {
    esp_ble_gatts_send_indicate(settings_profile_tab.gatts_if, connection_id, settings_handle_table[handle_idx], len,
                                value, false);
  }
}