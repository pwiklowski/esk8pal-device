#ifndef DS3231_H
#define DS3231_H

#include <time.h>
#include "esp_err.h"

#define DS3231_ADDR 0x68 //!< I2C address

#define DS3231_STAT_OSCILLATOR 0x80
#define DS3231_STAT_32KHZ 0x08
#define DS3231_STAT_BUSY 0x04
#define DS3231_STAT_ALARM_2 0x02
#define DS3231_STAT_ALARM_1 0x01

#define DS3231_CTRL_OSCILLATOR 0x80
#define DS3231_CTRL_SQUAREWAVE_BB 0x40
#define DS3231_CTRL_TEMPCONV 0x20
#define DS3231_CTRL_ALARM_INTS 0x04
#define DS3231_CTRL_ALARM2_INT 0x02
#define DS3231_CTRL_ALARM1_INT 0x01

#define DS3231_ALARM_WDAY 0x40
#define DS3231_ALARM_NOTSET 0x80

#define DS3231_ADDR_TIME 0x00
#define DS3231_ADDR_ALARM1 0x07
#define DS3231_ADDR_ALARM2 0x0b
#define DS3231_ADDR_CONTROL 0x0e
#define DS3231_ADDR_STATUS 0x0f
#define DS3231_ADDR_AGING 0x10
#define DS3231_ADDR_TEMP 0x11

#define DS3231_12HOUR_FLAG 0x40
#define DS3231_12HOUR_MASK 0x1f
#define DS3231_PM_FLAG 0x20
#define DS3231_MONTH_MASK 0x1f

esp_err_t ds3231_set_time(struct tm *time);
esp_err_t ds3231_get_time(struct tm *time);

#endif