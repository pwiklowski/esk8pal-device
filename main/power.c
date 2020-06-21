#include <stdio.h>
#include <stdlib.h>
#include "power.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "service_battery.h"
#include "activity_detector.h"

#include "driver/i2c.h"
#include "ads1115/ads1115.h"

#define VOLTAGE_DIVIDER (1+12)/1
#define CURRENT_SENSOR_SENSIVITY 0.026666666667
#define AMPERE_PER_MS 1/(60*60*1000)
#define CURRENT_NUM_SAMPLES 8

ads1115_t ads;

double zero = 1.65;


double read_current() {
    ads1115_set_mux(&ads, ADS1115_MUX_1_GND);
    double current = 0;
    for (uint8_t i=0; i<CURRENT_NUM_SAMPLES; i++) { 
        current += ads1115_get_voltage(&ads);
    }

    return -((current/CURRENT_NUM_SAMPLES - zero) / CURRENT_SENSOR_SENSIVITY);
}

double read_current_short() {
    ads1115_set_mux(&ads, ADS1115_MUX_1_GND);
    double current = ads1115_get_voltage(&ads);
    return -((current- zero) / CURRENT_SENSOR_SENSIVITY);
}

void calibrate_current_sensor() {
    ads1115_set_mux(&ads, ADS1115_MUX_1_GND);
    double current = 0;
    for (uint8_t i=0; i<64; i++) { 
        current += ads1115_get_voltage(&ads);
    }
    ESP_LOGI("ADC", "curr %f", current/64);
    zero = current/64;
}

double read_voltage() {
    ads1115_set_mux(&ads, ADS1115_MUX_0_GND);
    double voltage = ads1115_get_voltage(&ads);
    return voltage * VOLTAGE_DIVIDER;
}

void read_adc_data() {
    TickType_t xLastWakeTime = xTaskGetTickCount();

    double mah = 0;
    double voltage = 0;
    double current = 0;

    uint16_t measure_interval = 50;

    uint16_t ticks_per_second = 1000 / measure_interval;
    uint16_t iterator = 0;

    while (1) {
        if (state_is_in_driving_state()) { 
            power_up_module();
            current = read_current();
            mah += current * AMPERE_PER_MS * measure_interval;

            if (iterator >= ticks_per_second/2) {
                iterator = 0;
                voltage = read_voltage();
                battery_update_value(current, IDX_CHAR_VAL_CURRENT, false);
                battery_update_value(voltage, IDX_CHAR_VAL_VOLTAGE, false);
                battery_update_value(mah, IDX_CHAR_VAL_USED_ENERGY, false);

                detect_activity(current);
            }
            iterator++;
            vTaskDelayUntil(&xLastWakeTime, measure_interval / portTICK_PERIOD_MS);
        } else if (state_is_in_charging_state()) {
            power_up_module();
            iterator = 0;

            current = read_current();
            mah += current * AMPERE_PER_MS * measure_interval;
            voltage = read_voltage();

            battery_update_value(current, IDX_CHAR_VAL_CURRENT, false);
            battery_update_value(voltage, IDX_CHAR_VAL_VOLTAGE, false);
            battery_update_value(mah, IDX_CHAR_VAL_USED_ENERGY, false);

            detect_activity(current);

            vTaskDelayUntil(&xLastWakeTime, 1000 / portTICK_PERIOD_MS);
        } else {
            iterator = 0;
            vTaskDelayUntil(&xLastWakeTime, 1000 / portTICK_PERIOD_MS);
        }
    }
}

void power_up_module() { gpio_set_level(POWER_MODLE_GPIO, 0); }
void power_down_module() { gpio_set_level(POWER_MODLE_GPIO, 1); }
bool power_is_module_powered() { return gpio_get_level(POWER_MODLE_GPIO); }

void power_sensor_init() {
    ads = ads1115_config(I2C_NUM_0, 0x48);

    ads1115_set_pga(&ads, ADS1115_FSR_2_048);
    ads1115_set_sps(&ads, ADS1115_SPS_860);

    power_up_module();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    calibrate_current_sensor();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    power_down_module();

    xTaskCreate(read_adc_data, "read_adc_data", 1024 * 4, NULL, configMAX_PRIORITIES, NULL);
} 
