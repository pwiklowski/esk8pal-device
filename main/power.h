#ifndef power_h
#define power_h

void power_sensor_init();

void power_up_module();
void power_down_module();

double read_current();
double read_voltage();

#endif