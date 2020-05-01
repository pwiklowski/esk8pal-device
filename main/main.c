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

#include "driver/gpio.h"

static const char *TAG = "esk8";

struct CurrentState state;
struct Settings settings;

bool is_in_driving_state() {
  if (settings.manual_ride_start == MANUAL_START_ENABLED) {
    return state.riding_state == STATE_RIDING;
  } else {
    return state.current.value > 0.5 || state.speed.value > 1.0; // TODO add condition when logs neeed to be collected
  }
}

void set_device_state(device_state_t new_state) {
  state.riding_state = new_state;
  settings_set_value(IDX_CHAR_VAL_RIDING_STATE, 1, &new_state);
}

void main_led_notification() {
  gpio_pad_select_gpio(GPIO_NUM_22);
  gpio_set_direction(GPIO_NUM_22, GPIO_MODE_OUTPUT);

  while (1){
    uint16_t delay = 100;
    
    if (state.riding_state == STATE_RIDING) {
      delay = 300;
    } else if (state.riding_state == STATE_PARKED) {
      delay = 1000;
    }
    
    gpio_set_level(GPIO_NUM_22, 0);
    vTaskDelay(delay/ portTICK_PERIOD_MS);
    gpio_set_level(GPIO_NUM_22, 1);
    vTaskDelay(delay/ portTICK_PERIOD_MS);
  }
}

void app_main(void) {
  state.riding_state = STATE_PARKED;
  settings.manual_ride_start = MANUAL_START_DISABLED;

  settings_init();

  log_init_sd_card();
  ble_init();

  init_gps();

  power_sensor_init();

  log_init();

  wifi_init();

  xTaskCreate(main_led_notification, "main_led_notification", 1024, NULL, configMAX_PRIORITIES, NULL);
}
