#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_task_wdt.h"

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

#include "esp_pm.h"

#include "driver/i2c.h"
#include "driver/gpio.h"
#include "ds3231/ds3231.h"

#include <time.h>
#include <sys/time.h>

#include "uploader.h"

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
      delay = 500;
    } else if (state.riding_state == STATE_PARKED) {
      delay = 5000;
    }
    
    gpio_set_level(GPIO_NUM_22, 0);
    vTaskDelay(50/ portTICK_PERIOD_MS);
    gpio_set_level(GPIO_NUM_22, 1);
    vTaskDelay(delay/ portTICK_PERIOD_MS);

    state_update();

    esp_task_wdt_reset();
  }
}

esp_err_t i2c_init() {
    int i2c_master_port = I2C_NUM_0;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = GPIO_NUM_23;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = GPIO_NUM_19;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 400000;

    i2c_param_config(i2c_master_port, &conf);
    return i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0);
}

void app_init_time() {
  struct tm time;

  ds3231_get_time(&time);

  time_t t = mktime(&time);
  struct timeval now = { .tv_sec = t };
  settimeofday(&now, NULL);

  ESP_LOGI("MAIN", "time %ld %d:%d:%d", now.tv_sec, time.tm_hour, time.tm_min, time.tm_sec);
}

void app_main(void) {
  state.riding_state = STATE_PARKED;
  settings.manual_ride_start = MANUAL_START_DISABLED;

  i2c_init();

  app_init_time();

  settings_init();

  ble_init();
  init_gps();
  log_init();
  uploader_init();

  wifi_init();
  power_sensor_init();

  const esp_pm_config_esp32_t pm = {
    .light_sleep_enable = true,
    .max_freq_mhz = 160,
    .min_freq_mhz = 80
  };

  ESP_ERROR_CHECK( esp_pm_configure(&pm) );

  TaskHandle_t handle;

  xTaskCreate(main_led_notification, "main_led_notification", 1024, NULL, configMAX_PRIORITIES, &handle);

  esp_task_wdt_init(3, true);
  esp_task_wdt_add(handle);
}
