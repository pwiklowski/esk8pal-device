#include "state.h"

#include "logger.h"
#include "service_battery.h"
#include "service_settings.h"

struct CurrentState state;

struct CurrentStateAdvertisment adv_state;

struct CurrentState *state_get() {
  return &state;
}

bool state_is_in_driving_state() { return state.riding_state == STATE_RIDING; }
bool state_is_in_charging_state() { return state.riding_state == STATE_CHARGING; }
device_state_t state_get_device_state() { return state.riding_state; }

void state_set_device_state(device_state_t new_state) {
  state.riding_state = new_state;
  settings_set_value(IDX_CHAR_VAL_RIDING_STATE, 1, &new_state);
  state_update();

  if (new_state == STATE_CHARGING) {
    xTaskCreate(log_charging_task, "log_charging_task", 1024 * 6, NULL, configMAX_PRIORITIES, NULL);
  }
}
