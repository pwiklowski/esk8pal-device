#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"


#include "logger.h"
#include "gatt.h"

#include "state.h"

static const char *TAG = "esk8";


struct CurrentState state;

void app_main(void)
{
  state.voltage = 1.0;
  state.current = 2.0;
  state.used_energy = 3.0;
  state.total_energy = 4.0;

  init_sd();
  ble_init();

  //createLogger();
}
