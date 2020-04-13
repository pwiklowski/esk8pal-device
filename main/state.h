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

  uint8_t day;
  uint8_t month;
  uint16_t year;
  uint32_t time;

  uint8_t riding_state;
  uint8_t manual_ride_start;
  uint8_t wifi_ssid[20];
  uint8_t wifi_pass[20];

  uint8_t wifi_enabled;

  uint32_t free_storage;
  uint32_t total_storage;
};

#endif