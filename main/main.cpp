#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"

#include "logger.h"

#include "gatt.h"

extern "C"
{
  void app_main(void);
}

static const char *TAG = "esk8";

void init_sd()
{
  ESP_LOGI(TAG, "Initializing SD card");

  ESP_LOGI(TAG, "Using SDMMC peripheral");
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();

  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

  gpio_set_pull_mode(GPIO_NUM_15, GPIO_PULLUP_ONLY); // CMD, needed in 4- and 1- line modes
  gpio_set_pull_mode(GPIO_NUM_2, GPIO_PULLUP_ONLY); // D0, needed in 4- and 1-line modes
  gpio_set_pull_mode(GPIO_NUM_4, GPIO_PULLUP_ONLY); // D1, needed in 4-line mode only
  gpio_set_pull_mode(GPIO_NUM_12, GPIO_PULLUP_ONLY); // D2, needed in 4-line mode only
  gpio_set_pull_mode(GPIO_NUM_13, GPIO_PULLUP_ONLY); // D3, needed in 4- and 1-line modes

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024
      };

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

void app_main(void)
{
  init_sd();

  ble_init();

  createLogger();
}
