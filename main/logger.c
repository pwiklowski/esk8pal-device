#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"
#include "logger.h"
#include "sdmmc_cmd.h"
#include "state.h"

#include "gps.h"

#include "esp32/pm.h"
#include "esp_pm.h"
#include "math.h"
#include "service_battery.h"
#include "service_location.h"
#include "service_settings.h"

#include <sys/time.h>
#include <time.h>

#define d2r (M_PI / 180.0)

#define LOG_CHARGING_INTERVAL 1000
#define NOT_ACTIVE_TIME_MS 1000 * 10
#define LOG_INTERVAL 1000
static const char *TAG = "SD";

#define PIN_NUM_MISO 2
#define PIN_NUM_MOSI 15
#define PIN_NUM_CLK 14
#define PIN_NUM_CS 13

extern struct Settings settings;

bool is_logger_running = false;
bool is_charging_running = false;

void log_generate_filename(char *name) {

  struct timeval now;
  gettimeofday(&now, NULL);

  time_t t = (time_t)now.tv_sec;
  struct tm *time;
  time = gmtime(&t);

  sprintf(name, "%s/log.%d.%02d.%02d.%02d.%02d.%02d.log", BASE_LOCATION LOGS_LOCATION, (time->tm_year + 1900),
          time->tm_mon, time->tm_mday, time->tm_hour, time->tm_min, time->tm_sec);
}

void log_deinit_sd_card() { esp_vfs_fat_sdmmc_unmount(); }

void log_init_sd_card() {
  ESP_LOGI(TAG, "Using SPI peripheral");
  spi_bus_config_t bus_cfg =
   {
       .mosi_io_num = PIN_NUM_MOSI,
       .miso_io_num = PIN_NUM_MISO,
       .sclk_io_num = PIN_NUM_CLK,
       .quadwp_io_num = -1,
       .quadhd_io_num = -1,
       .max_transfer_sz = 4000
   };

   spi_bus_initialize(HSPI_HOST,&bus_cfg,1);

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();

  slot_config.gpio_cs = PIN_NUM_CS;
  slot_config.host_id = SPI2_HOST;

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false, .max_files = 5, .allocation_unit_size = 16 * 1024};

  sdmmc_card_t *card;
  esp_err_t ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &card);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(TAG, "Failed to mount filesystem. "
                    "If you want the card to be formatted, set format_if_mount_failed = true.");
    } else {
      ESP_LOGE(TAG,
               "Failed to initialize the card (%s). "
               "Make sure SD card lines have pull-up resistors in place.",
               esp_err_to_name(ret));
    }
    return;
  }

  sdmmc_card_print_info(stdout, card);
}

void log_update_free_space() {
  FATFS *fs;
  DWORD fre_clust, fre_sect, tot_sect;

  FRESULT res = f_getfree("/sdcard/", &fre_clust, &fs);
  if (res) {
    return;
  }

  tot_sect = (fs->n_fatent - 2) * fs->csize;
  fre_sect = fre_clust * fs->csize;

  uint64_t tmp_total_bytes = (uint64_t)tot_sect * FF_SS_SDCARD;
  uint64_t tmp_free_bytes = (uint64_t)fre_sect * FF_SS_SDCARD;

  state_get()->free_storage = tmp_free_bytes / (1024 * 1024);
  state_get()->total_storage = tmp_total_bytes / (1024 * 1024);

  settings_set_value(IDX_CHAR_VAL_FREE_STORAGE, 4, (uint8_t *)&state_get()->free_storage);
  settings_set_value(IDX_CHAR_VAL_TOTAL_STORAGE, 4, (uint8_t *)&state_get()->total_storage);

  ESP_LOGI(TAG, "Free space %d/%d", state_get()->free_storage, state_get()->total_storage);
}

void log_add_header(char *name) {
  FILE *f = fopen(name, "a");
  if (f == NULL) {
    ESP_LOGE(TAG, "Failed to open file for writing %s", name);
    log_init_sd_card();
    return;
  }

  fprintf(f, "esp_log_timestamp,timestamp,latitude,longitude,speed,voltage,current,used_energy,total_energy,trip_"
             "distance,altitude\n");

  fclose(f);
}

void log_add_entry(char *name) {
  FILE *f = fopen(name, "a");
  if (f == NULL) {
    ESP_LOGE(TAG, "Failed to open file for writing %s", name);
    log_init_sd_card();
    return;
  }

  struct timeval now;
  gettimeofday(&now, NULL);

  fprintf(f, "%d,%ld,%f,%f,%f,%f,%f,%f,%f,%f,%f\n", state_get()->riding_time, now.tv_sec, state_get()->latitude.value,
          state_get()->longitude.value, state_get()->speed.value, state_get()->voltage.value,
          state_get()->current.value, state_get()->used_energy.value, state_get()->total_energy.value,
          state_get()->trip_distance.value, state_get()->altitude.value);

  ESP_LOGI(TAG, "%d,%ld,%f,%f,%f,%f,%f,%f,%f,%f,%f", state_get()->riding_time, now.tv_sec, state_get()->latitude.value,
           state_get()->longitude.value, state_get()->speed.value, state_get()->voltage.value,
           state_get()->current.value, state_get()->used_energy.value, state_get()->total_energy.value,
           state_get()->trip_distance.value, state_get()->altitude.value);

  fclose(f);
}

double haversine_km(double lat1, double long1, double lat2, double long2) {
  double dlong = (long2 - long1) * d2r;
  double dlat = (lat2 - lat1) * d2r;
  double a = pow(sin(dlat / 2.0), 2) + cos(lat1 * d2r) * cos(lat2 * d2r) * pow(sin(dlong / 2.0), 2);
  double c = 2 * atan2(sqrt(a), sqrt(1 - a));
  double d = 6367 * c;

  return d;
}

double haversine_mi(double lat1, double long1, double lat2, double long2) {
  double dlong = (long2 - long1) * d2r;
  double dlat = (lat2 - lat1) * d2r;
  double a = pow(sin(dlat / 2.0), 2) + cos(lat1 * d2r) * cos(lat2 * d2r) * pow(sin(dlong / 2.0), 2);
  double c = 2 * atan2(sqrt(a), sqrt(1 - a));
  double d = 3956 * c;

  return d;
}

void log_track_task() {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  uint16_t measure_interval = 3000; // TODO add changing it based on current speed ?

  while (state_get()->gps_fix_status != 1) {
    vTaskDelayUntil(&xLastWakeTime, measure_interval / portTICK_PERIOD_MS);
  }

  double pLatitude = state_get()->latitude.value;
  double pLongtitude = state_get()->longitude.value;

  vTaskDelayUntil(&xLastWakeTime, measure_interval / portTICK_PERIOD_MS);

  double chunk;

  while (1) {
    chunk = haversine_km(pLatitude, pLongtitude, state_get()->latitude.value, state_get()->longitude.value);

    pLatitude = state_get()->latitude.value;
    pLongtitude = state_get()->longitude.value;

    location_update_value(state_get()->trip_distance.value + chunk, IDX_CHAR_VAL_TRIP_DISTANCE, false);

    vTaskDelayUntil(&xLastWakeTime, measure_interval / portTICK_PERIOD_MS);
  }
}

time_t log_get_current_time() {
  struct timeval now;
  gettimeofday(&now, NULL);

  return now.tv_sec;
}

bool log_is_logger_running() { return is_logger_running; }

void log_task(void *params) {
  log_update_free_space();
  TaskHandle_t trackTaskHandle;

  esp_err_t ret;

  esp_pm_lock_handle_t pm_lock;
  if ((ret = esp_pm_lock_create(ESP_PM_APB_FREQ_MAX, 1, "CPU_FREQ_MAX", &pm_lock)) != ESP_OK) {
    printf("pm config error %s\n", ret == ESP_ERR_INVALID_ARG
                                       ? "ESP_ERR_INVALID_ARG"
                                       : (ret == ESP_ERR_NOT_SUPPORTED ? "ESP_ERR_NOT_SUPPORTED" : "ESP_ERR_NO_MEM"));
  }

  while (1) {
    esp_pm_lock_release(pm_lock);
    while (!state_is_in_driving_state()) {
      vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    is_logger_running = true;

    esp_pm_lock_acquire(pm_lock);

    char log_filename[60];
    log_generate_filename(log_filename);

    log_add_header(log_filename);

    ESP_LOGI(TAG, "Start log %s", log_filename);
    uint32_t not_active_start_time = 0;
    state_set_device_state(STATE_RIDING);
    log_update_free_space();

    xTaskCreate(log_track_task, "log_track_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, &trackTaskHandle);

    location_update_value(0, IDX_CHAR_VAL_TRIP_DISTANCE, false);

    state_update();

    gps_disable_power_saving_mode();

    time_t start_time = log_get_current_time();

    while (1) {
      state_get()->riding_time = log_get_current_time() - start_time;

      log_add_entry(log_filename);

      if (!state_is_in_driving_state()) {
        if (not_active_start_time == 0 && !settings.manual_ride_start) {
          not_active_start_time = esp_log_timestamp();
          ESP_LOGI(TAG, "detected lack of activity ");
        } else {
          break;
        }
      }

      vTaskDelay(LOG_INTERVAL / portTICK_PERIOD_MS);
    }
    state_set_device_state(STATE_PARKED);
    log_update_free_space();

    gps_enable_power_saving_mode();

    vTaskDelete(trackTaskHandle);

    state_update();

    ESP_LOGI(TAG, "End log");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    is_logger_running = false;
  }
  vTaskDelete(NULL);
}

void log_init() {
  log_init_sd_card();

  FRESULT res = f_mkdir(LOGS_LOCATION);
  if (res != FR_OK && res != FR_EXIST) {
    ESP_LOGE(TAG, "Failed to create folder %s %d", LOGS_LOCATION, res);
  }

  res = f_mkdir(SYNCED_LOGS_LOCATION);
  if (res != FR_OK && res != FR_EXIST) {
    ESP_LOGE(TAG, "Failed to create folder %s %d", SYNCED_LOGS_LOCATION, res);
  }

  xTaskCreate(log_task, "logger_task", 1024 * 6, NULL, configMAX_PRIORITIES, NULL);
}

void log_generate_filename_for_charging_log(char *name) {

  struct timeval now;
  gettimeofday(&now, NULL);

  time_t t = (time_t)now.tv_sec;
  struct tm *time;
  time = gmtime(&t);

  sprintf(name, "%s/charge.%d.%02d.%02d.%02d.%02d.%02d.log", BASE_LOCATION LOGS_LOCATION, (time->tm_year + 1900),
          time->tm_mon, time->tm_mday, time->tm_hour, time->tm_min, time->tm_sec);
}

bool log_is_charging_running() { return is_charging_running; }

void log_charging_task(void *params) {
  log_update_free_space();
  TaskHandle_t trackTaskHandle;

  is_charging_running = true;

  char log_filename[60];
  log_generate_filename(log_filename);
  log_add_header(log_filename);

  ESP_LOGI(TAG, "Start charging log %s", log_filename);

  state_update();

  time_t start_time = log_get_current_time();

  while (1) {
    state_get()->riding_time = log_get_current_time() - start_time;

    log_add_entry(log_filename);

    if (!state_is_in_charging_state()) {
      break;
    }
    state_update();
    vTaskDelay(LOG_CHARGING_INTERVAL / portTICK_PERIOD_MS);
  }
  log_update_free_space();

  state_update();

  ESP_LOGI(TAG, "End charging log");
  is_charging_running = false;
  vTaskDelete(NULL);
}
