#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define BASE_LOCATION "/sdcard/"
static const char *TAG = "SD";

std::string generateLogFilename() {
  return "/sdcard/logfile.log";
}



void logger(void *) {
  while (1)
  {
    std::string log_filename = generateLogFilename();
    uint32_t i = 0;

    ESP_LOGI(TAG, "Create file %s", log_filename.c_str());

    while(1) { // TODO add condition when logs neeed to be collected
      FILE *f = fopen(log_filename.c_str(), "a");
      if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
      }
      fprintf(f, "%d, %d, \n", esp_log_timestamp(), i++);
      fclose(f);
      ESP_LOGI(TAG, "File written");
      vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}

void createLogger() {
  TaskHandle_t xHandle = NULL;

  xTaskCreate(logger, "logger_task", 1024 * 4, NULL, configMAX_PRIORITIES, &xHandle);
}