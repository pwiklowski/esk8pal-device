#ifndef state_h
#define state_h

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stdint.h"
#include <stdbool.h>
#include <stdio.h>

typedef union {
  double value;
  char bytes[sizeof(double)];
} DoubleCharacteristic;

typedef enum {
  STATE_PARKED,
  STATE_RIDING,
  STATE_CHARGING,
} device_state_t;

typedef enum { WIFI_DISABLED, WIFI_AP, WIFI_CLIENT, WIFI_CLIENT_CONNECTED } wifi_state_t;

typedef enum { MANUAL_START_DISABLED, MANUAL_START_ENABLED } manual_start_t;

struct CurrentState {
  DoubleCharacteristic current;
  DoubleCharacteristic voltage;
  DoubleCharacteristic used_energy;
  DoubleCharacteristic total_energy;

  DoubleCharacteristic speed;
  DoubleCharacteristic latitude;
  DoubleCharacteristic longitude;

  DoubleCharacteristic trip_distance;

  DoubleCharacteristic altitude;

  uint32_t riding_time;

  uint8_t gps_fix_status;
  uint8_t gps_satelites_count;

  uint8_t riding_state;

  uint32_t free_storage;
  uint32_t total_storage;
};

struct Settings {
  uint8_t manual_ride_start;
  wifi_state_t wifi_state;
  uint8_t wifi_ssid[21];
  uint8_t wifi_pass[21];

  uint8_t device_key[41];
  uint8_t wifi_ssid_client[21];
  uint8_t wifi_pass_client[21];
  uint16_t upload_interval;
};

struct CurrentState *state_get();
bool state_is_in_driving_state();
bool state_is_in_charging_state();
void state_set_device_state(device_state_t new_state);
device_state_t state_get_device_state();

#endif