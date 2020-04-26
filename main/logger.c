#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "state.h"

#include "service_settings.h"
#include "service_location.h"
#include "math.h"

#include <time.h>
#include <sys/time.h>

#define d2r (M_PI / 180.0)

#define BASE_LOCATION "/sdcard/"
#define NOT_ACTIVE_TIME_MS 1000*10
#define LOG_INTERVAL 1000
static const char *TAG = "SD";

#define PIN_NUM_MISO 2
#define PIN_NUM_MOSI 15
#define PIN_NUM_CLK  14
#define PIN_NUM_CS   13

extern struct CurrentState state;

extern bool is_in_driving_state();
extern void set_device_state(device_state_t state);

void log_generate_filename(char* name) {

  struct timeval now;
  gettimeofday(&now, NULL);

  time_t t =  (time_t) now.tv_sec;
  struct tm* time;
  time = gmtime(&t);

  sprintf(name, "/sdcard/log.%d.%02d.%02d.%02d.%02d.%02d.log", (time->tm_year + 1900), time->tm_mon, time->tm_mday, time->tm_hour, time->tm_min, time->tm_sec);
}

void log_init_sd_card() {
  ESP_LOGI(TAG, "Using SPI peripheral");

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
  slot_config.gpio_miso = PIN_NUM_MISO;
  slot_config.gpio_mosi = PIN_NUM_MOSI;
  slot_config.gpio_sck  = PIN_NUM_CLK;
  slot_config.gpio_cs   = PIN_NUM_CS;

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

  state.free_storage = tmp_free_bytes / (1024*1024);
  state.total_storage =  tmp_total_bytes / (1024*1024);

  settings_set_value(IDX_CHAR_VAL_FREE_STORAGE, 4, (uint8_t*) &state.free_storage);
  settings_set_value(IDX_CHAR_VAL_TOTAL_STORAGE, 4, (uint8_t*) &state.total_storage);

	ESP_LOGI(TAG, "Free space %d/%d", state.free_storage, state.total_storage);
}


void log_add_header(char* name) {
  FILE *f = fopen(name, "a");
  if (f == NULL) {
    ESP_LOGE(TAG, "Failed to open file for writing");
    log_init_sd_card();
    return;
  }

  fprintf(f, "esp_log_timestamp, timestamp, latitude, longitude, speed, voltage, current, used_energy, total_energy, trip_distance, altitude\n");

  fclose(f);
}

void log_add_entry(char* name) {
  FILE *f = fopen(name, "a");
  if (f == NULL) {
    ESP_LOGE(TAG, "Failed to open file for writing");
    log_init_sd_card();
    return;
  }

  struct timeval now;
  gettimeofday(&now, NULL);

  fprintf(f, "%d, %ld, %f, %f, %f, %f, %f, %f, %f, %f, %f\n", 
    esp_log_timestamp(), 
    now.tv_sec,
    state.latitude.value,
    state.longitude.value,
    state.speed.value,
    state.voltage.value,
    state.current.value,
    state.used_energy.value,
    state.total_energy.value,
    state.trip_distance.value,
    state.altitude
  );


  ESP_LOGI(TAG, "%d, %ld, %f, %f, %f, %f, %f, %f, %f, %f, %f", 
    esp_log_timestamp(), 
    now.tv_sec,
    state.latitude.value,
    state.longitude.value,
    state.speed.value,
    state.voltage.value,
    state.current.value,
    state.used_energy.value,
    state.total_energy.value,
    state.trip_distance.value,
    state.altitude
  );

  fclose(f);
}

double haversine_km(double lat1, double long1, double lat2, double long2) {
    double dlong = (long2 - long1) * d2r;
    double dlat = (lat2 - lat1) * d2r;
    double a = pow(sin(dlat/2.0), 2) + cos(lat1*d2r) * cos(lat2*d2r) * pow(sin(dlong/2.0), 2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    double d = 6367 * c;

    return d;
}

double haversine_mi(double lat1, double long1, double lat2, double long2) {
    double dlong = (long2 - long1) * d2r;
    double dlat = (lat2 - lat1) * d2r;
    double a = pow(sin(dlat/2.0), 2) + cos(lat1*d2r) * cos(lat2*d2r) * pow(sin(dlong/2.0), 2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    double d = 3956 * c; 

    return d;
}

void log_track_task() {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    uint16_t measure_interval = 3000; //TODO add changing it based on current speed ?

    double pLatitude = state.latitude.value;
    double pLongtitude = state.longitude.value;

    vTaskDelayUntil(&xLastWakeTime, measure_interval / portTICK_PERIOD_MS);

    double chunk;

    while (1) {
        chunk = haversine_km(pLatitude, pLongtitude, state.latitude.value, state.longitude.value);

        pLatitude = state.latitude.value;
        pLongtitude = state.longitude.value;

        location_update_value(state.trip_distance.value + chunk, IDX_CHAR_VAL_TRIP_DISTANCE, false);
        
        ESP_LOGI("Distance", "chunk %f %f",chunk, state.trip_distance.value);
        vTaskDelayUntil(&xLastWakeTime, measure_interval / portTICK_PERIOD_MS);
    }
}

void log_task(void* params) {
  ESP_LOGI(TAG, "Wait for time value to be initiated");
  log_update_free_space();
  ESP_LOGI(TAG, "Time value initiated");

  TaskHandle_t trackTaskHandle;

  while (1) {
    while(!is_in_driving_state()) { 
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    char log_filename[40];
    log_generate_filename(log_filename);

    log_add_header(log_filename);

    ESP_LOGI(TAG, "Start log %s", log_filename);
    uint32_t not_active_start_time = 0;
    set_device_state(STATE_RIDING);
    log_update_free_space();

    xTaskCreate(log_track_task, "log_track_task", 1024*2, NULL, configMAX_PRIORITIES-1, &trackTaskHandle);

    location_update_value(0, IDX_CHAR_VAL_TRIP_DISTANCE, false);

    while(1) { 
      log_add_entry(log_filename);

      if (!is_in_driving_state()) {
        if (not_active_start_time == 0 && !state.manual_ride_start ) {
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
    set_device_state(STATE_PARKED);
    log_update_free_space();

    vTaskDelete(trackTaskHandle); 

    ESP_LOGI(TAG, "End log");
  }
  vTaskDelete(NULL);
}

void log_init() {
  xTaskCreate(log_task, "logger_task", 1024 * 4, NULL, configMAX_PRIORITIES, NULL);
}