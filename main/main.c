#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"


#include "logger.h"
#include "gatt.h"
#include "gps.h"
#include "state.h"
#include "power.h"
#include "settings.h"
#include "wifi.h"

#include "service_location.h"
#include "service_battery.h"
#include "service_settings.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "esk8";

struct CurrentState state;

bool is_in_driving_state() {
  if (state.manual_ride_start == MANUAL_START_ENABLED) {
    return state.riding_state == STATE_RIDING;
  } else {
    return state.current.value > 0.5 || state.speed.value > 1.0; // TODO add condition when logs neeed to be collected
  }
}

void set_device_state(device_state_t new_state) {
  state.riding_state = new_state;
  settings_set_value(IDX_CHAR_VAL_RIDING_STATE, 1, &new_state);
}

void app_main(void) {
  state.riding_state = STATE_PARKED;
  state.manual_ride_start = MANUAL_START_DISABLED;

  settings_init();

  log_init_sd_card();
  ble_init();

  init_gps();

  power_sensor_init();

  log_init();

  wifi_init();
}
