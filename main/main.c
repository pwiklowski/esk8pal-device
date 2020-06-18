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
#include "power.h"
#include "activity_detector.h"

#include "esp_sleep.h"


static const char* TAG = "main";


struct Settings settings;



void main_led_notification() {
  gpio_pad_select_gpio(GPIO_NUM_22);
  gpio_set_direction(GPIO_NUM_22, GPIO_MODE_OUTPUT);

  while (1){
    uint16_t delay = 100;
    
    if (state_get()->riding_state == STATE_RIDING) {
      delay = 500;
    } else if (state_get()->riding_state == STATE_PARKED) {
      delay = 1000;
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

bool can_go_to_sleep() {
  return !uploader_is_task_running() && !state_is_in_driving_state() && 
    !is_battery_service_connected() && wifi_get_state() == WIFI_DISABLED;
}

bool should_start_uploader_task(const int64_t* last_upload_attempt) {
  uint16_t files_to_be_uploaded = uploader_count_files_to_be_uploaded();
  bool is_attempt_allowed = (esp_timer_get_time() - *last_upload_attempt) > (settings.upload_interval * 60 * 1000 * 1000);

  return files_to_be_uploaded > 0 && !uploader_is_task_running() && is_attempt_allowed;
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

  while (1) {
    if (!state_is_in_driving_state()) { 
     
      if (should_start_uploader_task(&last_upload_attempt)){
        last_upload_attempt = esp_timer_get_time();

        xTaskCreate(uploader_sync, "uploader_sync", 1024*6, NULL, configMAX_PRIORITIES, NULL);
        vTaskDelayUntil(&xLastWakeTime, 1000 / portTICK_PERIOD_MS);
      }

      vTaskDelayUntil(&xLastWakeTime, 300 / portTICK_PERIOD_MS);

      if (can_go_to_sleep()) {
          ESP_LOGI(TAG, "go to sleep");

          for (uint8_t i=0; i<10; i++) {
            update_battery_details();
            esp_sleep_enable_timer_wakeup(1 * 1000 * 1000);
            esp_light_sleep_start();
          }
      }
    } else {
      vTaskDelayUntil(&xLastWakeTime, 1000 / portTICK_PERIOD_MS);
    }
  }
}

void app_main(void) {
  state_set_device_state(STATE_PARKED);
  settings.manual_ride_start = MANUAL_START_DISABLED;

  i2c_init();

  app_init_time();
  settings_init();

  ble_init();
  init_gps();
  log_init();

  wifi_init();

  power_sensor_init();

  xTaskCreate(main_led_notification, "main_led_notification", 1024, NULL, configMAX_PRIORITIES, NULL);
  xTaskCreate(main_task, "main_task", 1024*4, NULL, configMAX_PRIORITIES, NULL);
}
