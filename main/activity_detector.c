#include "esp_log.h"
#include "state.h"

static const char *TAG = "activity_detector";
extern struct Settings settings;

#define CONFIG_RIDING_CURRENT_LEVEL 0.1
#define CONFIG_CHARGING_CURRENT_LEVEL -0.1
#define CONFIG_IDLE_TIME_TO_GO_PARKED 30

uint32_t idle_start_time = 0;

void detect_activity(double current) {
  if (!settings.manual_ride_start) {
    if (state_get_device_state() == STATE_PARKED) {
      if (current > CONFIG_RIDING_CURRENT_LEVEL) {
        ESP_LOGI(TAG, "riding detected");
        state_set_device_state(STATE_RIDING);
      } else if (current < CONFIG_CHARGING_CURRENT_LEVEL) {
        ESP_LOGI(TAG, "charging detected");
        state_set_device_state(STATE_CHARGING);
      }
    }

    if (state_get_device_state() == STATE_CHARGING) {
      if (current > CONFIG_CHARGING_CURRENT_LEVEL) {
        ESP_LOGI(TAG, "charging finished");
        state_set_device_state(STATE_PARKED);
      }
    }

    if (state_get_device_state() == STATE_RIDING) {
      if (current < CONFIG_RIDING_CURRENT_LEVEL && current > CONFIG_CHARGING_CURRENT_LEVEL) {

        if (idle_start_time == 0) {
          idle_start_time = xTaskGetTickCount();
        } else {
          ESP_LOGI(TAG, "idle detected %d", xTaskGetTickCount() - idle_start_time);
          if ((xTaskGetTickCount() - idle_start_time) > CONFIG_IDLE_TIME_TO_GO_PARKED * 1000) {
            state_set_device_state(STATE_PARKED);
            idle_start_time = 0;
          }
        }
      } else {
        idle_start_time = 0;
      }
    }
  }
}