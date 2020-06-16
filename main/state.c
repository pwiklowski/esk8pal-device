#include "state.h"

#include "service_settings.h"

struct CurrentState state;

struct CurrentState* state_get() {
    return &state;
}

bool state_is_in_driving_state() {
  return state.riding_state == STATE_RIDING;
}

void state_set_device_state(device_state_t new_state) {
  state.riding_state = new_state;
  settings_set_value(IDX_CHAR_VAL_RIDING_STATE, 1, &new_state);
}

device_state_t state_get_device_state() {
  return state.riding_state;
}