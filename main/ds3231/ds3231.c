#include "ds3231.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <string.h>


#define I2CDEV_TIMEOUT 1000

static const char *TAG = "DS3231";

uint8_t bcd2dec(uint8_t val) { return (val >> 4) * 10 + (val & 0x0f); }

uint8_t dec2bcd(uint8_t val) { return ((val / 10) << 4) + (val % 10); }

esp_err_t i2c_dev_read(const void *out_data, size_t out_size, void *in_data, size_t in_size) {
  if (!in_data || !in_size)
    return ESP_ERR_INVALID_ARG;

  i2c_cmd_handle_t cmd = i2c_cmd_link_create();

  if (out_data && out_size) {
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, (void *)out_data, out_size, true);
  }
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | 1, true);
  i2c_master_read(cmd, in_data, in_size, I2C_MASTER_LAST_NACK);
  i2c_master_stop(cmd);

  esp_err_t res = i2c_master_cmd_begin(I2C_NUM_0, cmd, I2CDEV_TIMEOUT / portTICK_RATE_MS);
  if (res != ESP_OK)
    ESP_LOGE(TAG, "Could not read from device");
  i2c_cmd_link_delete(cmd);

  return res;
}

esp_err_t i2c_dev_write(const void *out_reg, size_t out_reg_size, const void *out_data, size_t out_size) {
  if (!out_data || !out_size)
    return ESP_ERR_INVALID_ARG;

  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_WRITE, true);
  if (out_reg && out_reg_size)
    i2c_master_write(cmd, (void *)out_reg, out_reg_size, true);
  i2c_master_write(cmd, (void *)out_data, out_size, true);
  i2c_master_stop(cmd);
  esp_err_t res = i2c_master_cmd_begin(I2C_NUM_0, cmd, I2CDEV_TIMEOUT / portTICK_RATE_MS);
  if (res != ESP_OK)
    ESP_LOGE(TAG, "Could not write to device %d", res);
  i2c_cmd_link_delete(cmd);

  return res;
}

inline esp_err_t i2c_dev_read_reg(uint8_t reg, void *in_data, size_t in_size) {
  return i2c_dev_read(&reg, 1, in_data, in_size);
}

inline esp_err_t i2c_dev_write_reg(uint8_t reg, const void *out_data, size_t out_size) {
  return i2c_dev_write(&reg, 1, out_data, out_size);
}

esp_err_t ds3231_set_time(struct tm *time) {
  uint8_t data[7];

  /* time/date data */
  data[0] = dec2bcd(time->tm_sec);
  data[1] = dec2bcd(time->tm_min);
  data[2] = dec2bcd(time->tm_hour);
  /* The week data must be in the range 1 to 7, and to keep the start on the
   * same day as for tm_wday have it start at 1 on Sunday. */
  data[3] = dec2bcd(time->tm_wday + 1);
  data[4] = dec2bcd(time->tm_mday);
  data[5] = dec2bcd(time->tm_mon + 1);
  data[6] = dec2bcd(time->tm_year - 2000);

  return i2c_dev_write_reg(DS3231_ADDR_TIME, data, 7);
}

esp_err_t ds3231_get_time(struct tm *time) {
  uint8_t data[7];

  /* read time */
  esp_err_t res = i2c_dev_read_reg(DS3231_ADDR_TIME, data, 7);
  if (res != ESP_OK)
    return res;

  /* convert to unix time structure */
  time->tm_sec = bcd2dec(data[0]);
  time->tm_min = bcd2dec(data[1]);
  if (data[2] & DS3231_12HOUR_FLAG) {
    /* 12H */
    time->tm_hour = bcd2dec(data[2] & DS3231_12HOUR_MASK) - 1;
    /* AM/PM? */
    if (data[2] & DS3231_PM_FLAG)
      time->tm_hour += 12;
  } else
    time->tm_hour = bcd2dec(data[2]); /* 24H */

  time->tm_wday = bcd2dec(data[3]) - 1;
  time->tm_mday = bcd2dec(data[4]);
  time->tm_mon = bcd2dec(data[5] & DS3231_MONTH_MASK) - 1;
  time->tm_year = bcd2dec(data[6]) + 2000;
  time->tm_isdst = 0;

  // apply a time zone (if you are not using localtime on the rtc or you want to check/apply DST)
  // applyTZ(time);

  return ESP_OK;
}