#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "logger.h"
#include "uploader.h"
#include "ff.h"
#include "wifi.h"

#include "esp_http_client.h"
#include "esp_tls.h"

#define ESK8PAL_UPLOAD_URL "http://192.168.1.28:8080/upload"

static const char* TAG = "uploader";

extern struct Settings settings;

void uploader_init() {
  xTaskCreate(uploader_sync, "uploader_sync_files", 1024*6, NULL, configMAX_PRIORITIES, NULL);
}

void uploader_sync() {
  while(1) {
    if (wifi_get_state() == WIFI_CLIENT_CONNECTED) {
      uploader_sync_files();
    } else{
      ESP_LOGI(TAG, "Skip uploading");
    }
    vTaskDelay(60 * 1000/ portTICK_PERIOD_MS);
  }
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

  for(;;) {
    res = f_readdir(&dir, &file);
    if (res != FR_OK || file.fname[0] == 0) break;

    sprintf(filename, "%s%s/%s", BASE_LOCATION, LOGS_LOCATION, file.fname);

    if (uploader_upload_file(filename, file.fsize)) {
      ESP_LOGI(TAG, "Synced file %s %d", file.fname, file.fsize);
    }
  }

  f_closedir(&dir);
}

bool uploader_upload_file(char* filename, size_t size) {
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

  const size_t data_len = strlen(post_data_start1) + strlen(post_data_start2) + strlen(filename) + strlen(post_data_end) + size + 2;

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

  esp_http_client_write(client,"\r\n", 2);
  esp_http_client_write(client, post_data_end, strlen(post_data_end));

  esp_http_client_fetch_headers(client);

  const int responseCode = esp_http_client_get_status_code(client);

  ESP_LOGI(TAG, "HTTP POST Status = %d", esp_http_client_get_status_code(client));

  esp_http_client_close(client);
  esp_http_client_cleanup(client);

  fclose(f);

  return responseCode == 200;
}
