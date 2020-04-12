#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "state.h"

#include "service_settings.h"

#define BASE_LOCATION "/sdcard/"
#define NOT_ACTIVE_TIME_MS 1000*10
#define LOG_INTERVAL 1000*5
static const char *TAG = "SD";

extern struct CurrentState state;

extern bool is_in_driving_state();

void log_generate_filename(char* name) {
  sprintf(name, "/sdcard/log.%d.%d.%d.%d.log", state.year, state.month, state.day, state.time);
}

void log_init_sd_card() {
  ESP_LOGI(TAG, "Initializing SD card");

  ESP_LOGI(TAG, "Using SDMMC peripheral");
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();

  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

  gpio_set_pull_mode(GPIO_NUM_15, GPIO_PULLUP_ONLY); // CMD, needed in 4- and 1- line modes
  gpio_set_pull_mode(GPIO_NUM_2, GPIO_PULLUP_ONLY);  // D0, needed in 4- and 1-line modes
  gpio_set_pull_mode(GPIO_NUM_4, GPIO_PULLUP_ONLY);  // D1, needed in 4-line mode only
  gpio_set_pull_mode(GPIO_NUM_12, GPIO_PULLUP_ONLY); // D2, needed in 4-line mode only
  gpio_set_pull_mode(GPIO_NUM_13, GPIO_PULLUP_ONLY); // D3, needed in 4- and 1-line modes

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024};

  sdmmc_card_t *card;
  esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

  if (ret != ESP_OK)
  {
    if (ret == ESP_FAIL)
    {
      ESP_LOGE(TAG, "Failed to mount filesystem. "
                    "If you want the card to be formatted, set format_if_mount_failed = true.");
    }
    else
    {
      ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                    "Make sure SD card lines have pull-up resistors in place.",
               esp_err_to_name(ret));
    }
    return;
  }

  sdmmc_card_print_info(stdout, card);
}

void log_wait_for_time_to_be_initiated() {
  while (1) {
    if (state.time != 0) {
      break;
    }
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

void log_add_entry(char* name) {
  FILE *f = fopen(name, "a");
  if (f == NULL) {
    ESP_LOGE(TAG, "Failed to open file for writing");
    log_init_sd_card();
    return;
  }

  fprintf(f, "%d, %d, %f, %f, %f, %f, %f, %f, %f\n", 
    esp_log_timestamp(), 
    state.time,
    state.latitude.value,
    state.longitude.value,
    state.speed.value,
    state.voltage.value,
    state.current.value,
    state.used_energy.value,
    state.total_energy.value
  );
  ESP_LOGI(TAG, "%d, %d, %f, %f, %f, %f, %f, %f, %f", 
    esp_log_timestamp(), 
    state.time,
    state.latitude.value,
    state.longitude.value,
    state.speed.value,
    state.voltage.value,
    state.current.value,
    state.used_energy.value,
    state.total_energy.value
  );

  fclose(f);
}

void log_task(void* params) {
  ESP_LOGI(TAG, "Wait for time value to be initiated");
  log_wait_for_time_to_be_initiated();
  ESP_LOGI(TAG, "Time value initiated");

  while (1) {
    while(!is_in_driving_state()) { 
      ESP_LOGI(TAG, "Waiting for drive mode to be started");
      vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    char log_filename[40];
    log_generate_filename(log_filename);

    ESP_LOGI(TAG, "Start log %s", log_filename);
    uint32_t not_active_start_time = 0;
    while(1) { 
      log_add_entry(log_filename);

      if (!is_in_driving_state()) {
        if (not_active_start_time == 0) {
          not_active_start_time = esp_log_timestamp();
          ESP_LOGI(TAG, "detected lack of activity ");
        } else {
          if (esp_log_timestamp() - not_active_start_time > NOT_ACTIVE_TIME_MS) {
            break;
          }
        }
      }

      vTaskDelay(LOG_INTERVAL / portTICK_PERIOD_MS);
    }
    ESP_LOGI(TAG, "End log");
  }
  vTaskDelete(NULL);
}

void log_init() {
  xTaskCreate(log_task, "logger_task", 1024 * 4, NULL, configMAX_PRIORITIES, NULL);
}