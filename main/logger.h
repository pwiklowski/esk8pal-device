#ifndef logger_h
#define logger_h

#include "state.h"

#define BASE_LOCATION "/sdcard"

#define LOGS_LOCATION  "/logs"
#define SYNCED_LOGS_LOCATION  "/logs-synced"

void log_init();
void log_init_sd_card();
bool log_is_logger_running();

#endif