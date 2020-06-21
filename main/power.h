#ifndef power_h
#define power_h

#include <stdbool.h>

#define POWER_MODLE_GPIO GPIO_NUM_27

void power_sensor_init();

void power_up_module();
void power_down_module();

double read_current();
double read_current_short();
double read_voltage();

bool power_is_module_powered();

#endif