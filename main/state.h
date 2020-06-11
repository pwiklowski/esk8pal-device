#ifndef state_h
#define state_h


typedef union {
  double value;
  char bytes[sizeof(double)];
} DoubleCharacteristic;

typedef enum {
  STATE_PARKED,
  STATE_RIDING
} device_state_t;

typedef enum {
  MANUAL_START_DISABLED,
  MANUAL_START_ENABLED
} manual_start_t;

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
  uint8_t wifi_enabled;
  uint8_t wifi_ssid[21];
  uint8_t wifi_pass[21];

  uint8_t device_key[41];
  uint8_t wifi_ssid_client[21];
  uint8_t wifi_pass_client[21];
  uint16_t upload_interval;
};

#endif