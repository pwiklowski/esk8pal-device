#include "esp_log.h"
#include "state.h"

static const char* TAG = "activity_detector";
extern struct Settings settings;

#define CONFIG_RIDING_CURRENT_LEVEL 0.2
#define CONFIG_CHARGING_CURRENT_LEVEL -0.2

void detect_activity(double current) {
  if (current > CONFIG_RIDING_CURRENT_LEVEL){
    ESP_LOGI(TAG, "riding detected");
    if (settings.manual_ride_start) {

    }
  } else if( current < CONFIG_CHARGING_CURRENT_LEVEL) {
    ESP_LOGI(TAG, "charging detected");
  }
}