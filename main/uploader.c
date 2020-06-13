#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "logger.h"
#include "uploader.h"
#include "ff.h"
#include "wifi.h"

char* UPLOADER_TAG = "uploader";

extern struct Settings settings;

void uploader_init() {
  xTaskCreate(uploader_sync, "uploader_sync_files", 1024*6, NULL, configMAX_PRIORITIES, NULL);
}

bool uploader_upload_file(char* fname, FSIZE_t fsize) {
  ESP_LOGI(UPLOADER_TAG, "Upload file %s %d", fname, fsize);
  return false;
}

void uploader_sync() {
  while(1) {
    if (wifi_get_state() == WIFI_CLIENT_CONNECTED) {
      uploader_sync_files();
    } else{
      ESP_LOGI(UPLOADER_TAG, "Skip uploading");
    }
    vTaskDelay(10 * 1000/ portTICK_PERIOD_MS);
  }
}

void uploader_sync_files() {
  FRESULT res;
  FILINFO file;
  FF_DIR dir;

  if (f_opendir(&dir, LOGS_LOCATION) != FR_OK) {
    ESP_LOGE(UPLOADER_TAG, "Failed to create folder %s", LOGS_LOCATION);
    return;
  }

  for(;;) {
    res = f_readdir(&dir, &file);
    if (res != FR_OK || file.fname[0] == 0) break;

    if (uploader_upload_file(file.fname, file.fsize)) {
      //move file
    }
  }

  f_closedir(&dir);
}
