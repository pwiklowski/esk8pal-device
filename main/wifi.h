#ifndef wifi_h
#define wifi_h

#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <string.h>

#include "lwip/err.h"
#include "lwip/sys.h"
#include "state.h"

void wifi_init();
void wifi_set_state(wifi_state_t state);
wifi_state_t wifi_get_state();

#endif