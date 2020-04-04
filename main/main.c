#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"


#include "logger.h"
#include "gatt.h"
#include "gps.h"
#include "state.h"

static const char *TAG = "esk8";


struct CurrentState state;

void app_main(void)
{
  state.voltage.value = 1.0;
  state.current.value = 2.0;
  state.used_energy.value = 3.0;
  state.total_energy.value = 4.0;

  state.latitude.value = 50.081624;
  state.longitude.value = 20.007325;
  state.speed.value = 69.69;

  init_sd();
  ble_init();

  init_gps();

  //createLogger();
}
