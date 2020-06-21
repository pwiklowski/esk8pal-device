#include "uploader.h"
#include "esp_log.h"
#include "ff.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "logger.h"
#include "wifi.h"

#include "esp_http_client.h"
#include "esp_tls.h"

#define ESK8PAL_UPLOAD_URL "http://192.168.1.28:8080/upload"

static const char *TAG = "uploader";

extern struct Settings settings;

bool is_task_running = false;

bool uploader_is_task_running() { return is_task_running; }

bool uploader_wait_for_wifi() {
  for (uint8_t i = 0; i < 5; i++) {
    if (wifi_get_state() == WIFI_CLIENT_CONNECTED) {
      return true;
    }
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "waiting for wifi");
  }
  return false;
}

void uploader_sync() {
  is_task_running = true;
  uint16_t files_to_be_uploaded = uploader_count_files_to_be_uploaded();
  ESP_LOGI(TAG, "start uploading %d ", files_to_be_uploaded);

  if (files_to_be_uploaded > 0) {
    wifi_set_state(WIFI_CLIENT);

    if (uploader_wait_for_wifi()) {
      uploader_sync_files();

      vTaskDelay(5 * 1000 / portTICK_PERIOD_MS);
      ESP_LOGI(TAG, "uploading finished");
    } else {
      ESP_LOGI(TAG, "uploading failed");
    }
    wifi_set_state(WIFI_DISABLED);
  }

  is_task_running = false;
  vTaskDelete(NULL);
}

uint16_t uploader_count_files_to_be_uploaded() {
  FRESULT res;
  FILINFO file;
  FF_DIR dir;
  uint16_t count = 0;

  if (f_opendir(&dir, LOGS_LOCATION) != FR_OK) {
    ESP_LOGE(TAG, "Failed to create folder %s", LOGS_LOCATION);
    return 0;
  }

  for (;;) {
    res = f_readdir(&dir, &file);
    if (res != FR_OK || file.fname[0] == 0)
      break;
    count++;
  }

  f_closedir(&dir);

  return count;
}

void uploader_sync_files() {
  FRESULT res;
  FILINFO file;
  FF_DIR dir;

  if (f_opendir(&dir, LOGS_LOCATION) != FR_OK) {
    ESP_LOGE(TAG, "Failed to create folder %s", LOGS_LOCATION);
    return;
  }

  char filename[270];
  char synced_filename[280];

  for (;;) {
    res = f_readdir(&dir, &file);
    if (res != FR_OK || file.fname[0] == 0)
      break;

    sprintf(filename, "%s%s/%s", BASE_LOCATION, LOGS_LOCATION, file.fname);

    if (uploader_upload_file(filename, file.fsize)) {
      ESP_LOGI(TAG, "Synced file %s %d", file.fname, file.fsize);
      sprintf(synced_filename, "%s%s/%s", BASE_LOCATION, SYNCED_LOGS_LOCATION, file.fname);

      res = rename(filename, synced_filename);
      ESP_LOGI(TAG, "rename  %s %s %d ", filename, synced_filename, res);
    }
  }

  f_closedir(&dir);
}

bool uploader_upload_file(char *filename, size_t size) {
  FILE *f = fopen(filename, "r");
  if (f == NULL) {
    ESP_LOGE(TAG, "Failed to open file for reading");
    return false;
  }

  char request_url[100];

  sprintf(request_url, "%s?key=%s", ESK8PAL_UPLOAD_URL, settings.device_key);

  ESP_LOGI(TAG, "req url %s", request_url);

  esp_http_client_config_t config = {
      .url = request_url,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);

  const char *post_data_start1 = "--bnd\r\n"
                                 "Content-Disposition: form-data; name=\"logfile\"; filename=\"";

  const char *post_data_start2 = "\"\r\n"
                                 "Content-Type: text/x-log\r\n"
                                 "\r\n";

  const char *post_data_end = "--bnd--\r\n";

  const size_t data_len =
      strlen(post_data_start1) + strlen(post_data_start2) + strlen(filename) + strlen(post_data_end) + size + 2;

  esp_http_client_set_url(client, request_url);
  esp_http_client_set_method(client, HTTP_METHOD_POST);

  esp_http_client_set_header(client, "Content-Type", "multipart/form-data; boundary=bnd");

  esp_err_t err = esp_http_client_open(client, data_len);

  esp_http_client_write(client, post_data_start1, strlen(post_data_start1));
  esp_http_client_write(client, filename, strlen(filename));
  esp_http_client_write(client, post_data_start2, strlen(post_data_start2));

  char chunk[1024];
  while (1) {
    size_t len = fread(chunk, 1, sizeof(chunk), f);
    esp_http_client_write(client, chunk, len);

    if (len < 1024) {
      break;
    }
  }

  esp_http_client_write(client, "\r\n", 2);
  esp_http_client_write(client, post_data_end, strlen(post_data_end));

  esp_http_client_fetch_headers(client);

  const int responseCode = esp_http_client_get_status_code(client);

  ESP_LOGI(TAG, "HTTP POST Status = %d", esp_http_client_get_status_code(client));

  esp_http_client_close(client);
  esp_http_client_cleanup(client);

  fclose(f);

  return responseCode == 200;
}
