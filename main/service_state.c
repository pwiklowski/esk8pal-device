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
#include "service_state.h"
#include "state.h"

#define GATTS_TABLE_TAG "StateService"

#define SVC_INST_ID 0

/* The max length of characteristic value. When the GATT client performs a write
 * or prepare write operation, the data length must be less than
 * GATTS_DEMO_CHAR_VAL_LEN_MAX.
 */
#define GATTS_DEMO_CHAR_VAL_LEN_MAX 500
#define PREPARE_BUF_MAX_SIZE 1024
#define CHAR_DECLARATION_SIZE (sizeof(uint8_t))

extern struct CurrentState state;

static uint8_t adv_config_done = 0;

uint16_t state_handle_table[STATE_IDX_NB];

uint16_t state_notification_table[STATE_IDX_NB];

static uint16_t connection_id;

bool is_state_connected = false;

uint8_t state_service_uuid[16] = {
    0xfd, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00,
};

extern struct CurrentStateAdvertisment adv_state;

/* The length of adv data must be less than 31 bytes */
esp_ble_adv_data_t state_adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006, // slave connection min interval, Time =
                            // min_interval * 1.25 msec
    .max_interval = 0x0010, // slave connection max interval, Time =
                            // max_interval * 1.25 msec
    .appearance = 0x00,
    .manufacturer_len = 0,    // TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data = 0, // test_manufacturer,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(state_service_uuid),
    .p_service_uuid = state_service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};
static uint8_t adv_data[12];

// scan response data
esp_ble_adv_data_t state_scan_rsp_data = {
    .set_scan_rsp = true,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006,
    .max_interval = 0x0010,
    .appearance = 0x00,
    .manufacturer_len = 12,
    .p_manufacturer_data = adv_data,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(state_service_uuid),
    .p_service_uuid = state_service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

extern esp_ble_adv_params_t adv_params;

struct gatts_profile_inst state_profile_tab = {
    .gatts_cb = state_gatts_service_event_handler, .gatts_if = ESP_GATT_IF_NONE, /* Not get the gatt_if, so initial is
                                                                              ESP_GATT_IF_NONE */
};
static const uint16_t GATTS_SERVICE_UUID = 0x00FF;
static const uint16_t GATTS_CHAR_UUID_STATE = 0xFFFF;

static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint8_t char_prop_read = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;

static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t config_descriptor[2] = {0x00, 0x00};

/* Full Database Description - Used to add attributes into the database */
static const esp_gatts_attr_db_t gatt_db[STATE_IDX_NB] = {
    // Service Declaration
    [IDX_SVC_STATE] = {{ESP_GATT_AUTO_RSP},
                       {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ, sizeof(uint16_t),
                        sizeof(GATTS_SERVICE_UUID), (uint8_t *)&GATTS_SERVICE_UUID}},

    /* Characteristic Declaration */
    [IDX_CHAR_STATE] = {{ESP_GATT_AUTO_RSP},
                        {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
                         CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read}},
    /* Characteristic Value */
    [IDX_CHAR_VAL_STATE] = {{ESP_GATT_AUTO_RSP},
                            {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_STATE,
                             ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(state), sizeof(state),
                             (uint8_t *)&state}},
    /* Client Characteristic Configuration Descriptor */
    [IDX_CHAR_CFG_STATE] = {{ESP_GATT_AUTO_RSP},
                            {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid,
                             ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(uint16_t), sizeof(config_descriptor),
                             (uint8_t *)config_descriptor}},

};

struct gatts_profile_inst init_state_service() {
  return state_profile_tab;
}

void state_set_adv_voltage(float voltage) { *((float *)&adv_data[2]) = voltage; }

void state_set_adv_current(float current) { *((float *)&adv_data[6]) = current; }

void state_set_adv_state(device_state_t state) { adv_data[10] = state; }

void state_adv_data_update() { esp_ble_gap_config_adv_data(&state_scan_rsp_data); }

void state_gatts_service_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                       esp_ble_gatts_cb_param_t *param) {
  switch (event) {
  case ESP_GATTS_REG_EVT: {
    esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(SAMPLE_DEVICE_NAME);
    if (set_dev_name_ret) {
      ESP_LOGE(GATTS_TABLE_TAG, "set device name failed, error code = %x", set_dev_name_ret);
    }
    // config adv data
    esp_err_t ret = esp_ble_gap_config_adv_data(&state_adv_data);
    if (ret) {
      ESP_LOGE(GATTS_TABLE_TAG, "config adv data failed, error code = %x", ret);
    }
    adv_config_done |= ADV_CONFIG_FLAG;
    // config scan response data
    ret = esp_ble_gap_config_adv_data(&state_scan_rsp_data);
    if (ret) {
      ESP_LOGE(GATTS_TABLE_TAG, "config scan response data failed, error code = %x", ret);
    }
    adv_config_done |= SCAN_RSP_CONFIG_FLAG;
    esp_err_t create_attr_ret = esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, STATE_IDX_NB, SVC_INST_ID);
    if (create_attr_ret) {
      ESP_LOGE(GATTS_TABLE_TAG, "create attr table failed, error code = %x", create_attr_ret);
    }

    state_profile_tab.gatts_if = gatts_if;

  } break;
  case ESP_GATTS_READ_EVT:
    ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_READ_EVT");
    break;
  case ESP_GATTS_WRITE_EVT:
    ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_WRITE_EVT");
    ESP_LOGI(GATTS_TABLE_TAG, "GATT_WRITE_EVT, handle = %d, value len = %d, value :", param->write.handle,
             param->write.len);
    esp_log_buffer_hex(GATTS_TABLE_TAG, state_handle_table, 20);

    if (!param->write.is_prep) {
      uint16_t descr_value = param->write.value[1] << 8 | param->write.value[0];

      uint16_t index = 0;

      for (uint8_t i = 0; i < STATE_IDX_NB; i++) {
        if (state_handle_table[i] == param->write.handle) {
          index = i;
          break;
        }
      }

      state_notification_table[index] = descr_value;
      ESP_LOGI(GATTS_TABLE_TAG, "notify enable %d %d %d ", index, param->write.handle, descr_value);

      if (index == IDX_CHAR_CFG_STATE) {
        state_update();
      }
    }
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
    is_state_connected = true;
    break;
  case ESP_GATTS_DISCONNECT_EVT:
    ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_DISCONNECT_EVT, reason = 0x%x", param->disconnect.reason);
    esp_ble_gap_start_advertising(&adv_params);
    is_state_connected = false;
    break;
  case ESP_GATTS_CREAT_ATTR_TAB_EVT: {
    if (param->add_attr_tab.status != ESP_GATT_OK) {
      ESP_LOGE(GATTS_TABLE_TAG, "create attribute table failed, error code=0x%x", param->add_attr_tab.status);
    } else if (param->add_attr_tab.num_handle != STATE_IDX_NB) {
      ESP_LOGE(GATTS_TABLE_TAG, "create attribute table abnormally, num_handle (%d) \
                        doesn't equal to HRS_IDX_NB(%d)",
               param->add_attr_tab.num_handle, STATE_IDX_NB);
    } else {
      ESP_LOGI(GATTS_TABLE_TAG, "create attribute table successfully, the number handle = %d\n",
               param->add_attr_tab.num_handle);
      memcpy(state_handle_table, param->add_attr_tab.handles, sizeof(state_handle_table));
      esp_ble_gatts_start_service(state_handle_table[IDX_SVC_STATE]);
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

void state_update() {
  esp_ble_gatts_set_attr_value(state_handle_table[IDX_CHAR_VAL_STATE], sizeof(state), (uint8_t*) &state);

  if (state_notification_table[IDX_CHAR_CFG_STATE] == 0x0001) {
    esp_ble_gatts_send_indicate(state_profile_tab.gatts_if, connection_id, state_handle_table[IDX_CHAR_VAL_STATE],
        sizeof(state), (uint8_t*) &state, false);
  }
}

bool is_state_service_connected() {
  return is_state_connected;
}
