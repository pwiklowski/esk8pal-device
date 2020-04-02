#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"


#include "logger.h"
#include "gatt.h"

#include "state.h"

extern "C"
{
  void app_main(void);
}

static const char *TAG = "esk8";



void app_main(void)
{
  CurrentState state;


  init_sd();

  ble_init();

  createLogger(&state);
}
