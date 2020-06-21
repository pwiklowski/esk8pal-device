#include "esp_bt.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "gatt.h"
#include "gps.h"
#include "logger.h"
#include "power.h"
#include "settings.h"
#include "state.h"
#include "wifi.h"

#include "service_battery.h"
#include "service_location.h"
#include "service_settings.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"

#include "esp_pm.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "ds3231/ds3231.h"

#include <sys/time.h>
#include <time.h>

#include "activity_detector.h"
#include "esp_sleep.h"
#include "power.h"
#include "uploader.h"

static const char *TAG = "main";
struct Settings settings;

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

  bool isPowered = power_is_module_powered();

  if (!isPowered) {
    power_up_module();
  }
  ds3231_get_time(&time);
  if (!isPowered) {
    power_down_module();
  }

  time_t t = mktime(&time);
  struct timeval now = {.tv_sec = t};
  settimeofday(&now, NULL);

  ESP_LOGI("MAIN", "time %ld %d:%d:%d", now.tv_sec, time.tm_hour, time.tm_min,
           time.tm_sec);
}

bool can_go_to_sleep() {
  return !uploader_is_task_running() && !state_is_in_driving_state() &&
         !log_is_logger_running() && !is_battery_service_connected() &&
         !log_is_charging_running() &&
         wifi_get_state() == WIFI_DISABLED;
}

bool should_start_uploader_task(const int64_t *last_upload_attempt) {
  uint16_t files_to_be_uploaded = uploader_count_files_to_be_uploaded();
  bool is_attempt_allowed = (esp_timer_get_time() - *last_upload_attempt) >
                            (settings.upload_interval * 60 * 1000 * 1000);

  return files_to_be_uploaded > 0 && !uploader_is_task_running() &&
         is_attempt_allowed;
}

void update_battery_details() {
  power_up_module();

  double voltage = read_voltage();
  double current = read_current_short();
  battery_update_value(voltage, IDX_CHAR_VAL_VOLTAGE, false);
  battery_update_value(current, IDX_CHAR_VAL_CURRENT, false);

  //ESP_LOGI(TAG, "voltage %f, current %f", voltage, current);
  detect_activity(current);

  power_down_module();
}

void main_task() {
  TickType_t xLastWakeTime = xTaskGetTickCount();

  int64_t last_upload_attempt = 0;
  gpio_pad_select_gpio(GPIO_NUM_22);
  gpio_set_direction(GPIO_NUM_22, GPIO_MODE_OUTPUT);

  while (1) {
    if (!state_is_in_driving_state() && !state_is_in_charging_state()) {

      if (should_start_uploader_task(&last_upload_attempt)) {
        last_upload_attempt = esp_timer_get_time();

        xTaskCreate(uploader_sync, "uploader_sync", 1024 * 6, NULL,
                    configMAX_PRIORITIES, NULL);
        vTaskDelayUntil(&xLastWakeTime, 1000 / portTICK_PERIOD_MS);
      }

      gpio_set_level(GPIO_NUM_22, 0);
      vTaskDelay(20 / portTICK_PERIOD_MS);
      gpio_set_level(GPIO_NUM_22, 1);
      vTaskDelayUntil(&xLastWakeTime, 300 / portTICK_PERIOD_MS);

      if (can_go_to_sleep()) {
        ESP_LOGI(TAG, "go to sleep");

        for (uint8_t i = 0; i < 10; i++) {
          update_battery_details();
          esp_sleep_enable_timer_wakeup(1 * 1000 * 1000);
          esp_light_sleep_start();
        }
      }
      update_battery_details();
      state_update();
    } else {
      state_update();
      vTaskDelayUntil(&xLastWakeTime, 900 / portTICK_PERIOD_MS);
      gpio_set_level(GPIO_NUM_22, 0);
      vTaskDelayUntil(&xLastWakeTime, 100 / portTICK_PERIOD_MS);
      gpio_set_level(GPIO_NUM_22, 1);
    }
  }
}

void app_init_power_module_control_pin() {
  gpio_pad_select_gpio(POWER_MODLE_GPIO);
  gpio_set_direction(POWER_MODLE_GPIO, GPIO_MODE_OUTPUT);
  gpio_set_pull_mode(POWER_MODLE_GPIO, GPIO_PULLUP_PULLDOWN);
}

void app_main(void) {
  state_set_device_state(STATE_PARKED);

  app_init_power_module_control_pin();

  i2c_init();

  app_init_time();
  settings_init();

  ble_init();
  init_gps();
  log_init();

  wifi_init();

  power_sensor_init();

  xTaskCreate(main_task, "main_task", 1024 * 4, NULL, configMAX_PRIORITIES,
              NULL);
}
