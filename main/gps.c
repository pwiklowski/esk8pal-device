#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"
#include "state.h"
#include "service_location.h"

static const int RX_BUF_SIZE = 1024;

#define TXD_PIN (GPIO_NUM_17)
#define RXD_PIN (GPIO_NUM_16)

static const char *RX_TASK_TAG = "RX_TASK";

extern struct CurrentState state;


void gps_init_uart() {
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_2, &uart_config);
    uart_set_pin(UART_NUM_2, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    // We won't use a buffer for sending data.
    uart_driver_install(UART_NUM_2, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
}

bool gps_is_location_valid(uint8_t* data) {
    char* end_line_index = strchr((char*) data, 0x0D);
    char* is_data_correct_marker = strstr((char*) data, ",A,");
    return is_data_correct_marker != NULL && is_data_correct_marker < end_line_index;
}

void gps_get_location(uint8_t* data) {
    if (data == NULL) {
        return;
    }
    char substr[12];

    double lat = 0;
    double lon = 0;
    char lat_ind, lon_ind;

    if (gps_is_location_valid(data)) {
        char* start = strchr((char*)data, ',') + 1;
        strncpy(substr, start, 2);
        substr[2] = 0;
        lat = atoi(substr);

        strncpy(substr, start + 2, 7);
        substr[7] = 0;
        lat = lat + atof(substr)/60;

        start = strchr((char*)start, ',') + 1;
        strncpy(substr, start, 1);
        substr[1] = 0;
        lat_ind = substr[0];

        start = strchr((char*)start, ',') + 1;
        strncpy(substr, start, 3);
        substr[3] = 0;
        lon = atoi(substr);

        strncpy(substr, start + 3, 7);
        substr[7] = 0;
        lon = lon + atof(substr)/60;

        start = strchr((char*)start, ',') + 1;
        strncpy(substr, start, 1);
        substr[1] = 0;
        lon_ind = substr[0];

        if (lat_ind == 'S') {
            lat = -lat;
        }

        if (lon_ind == 'W') {
            lon = -lon;
        }
        location_update_value(lat, IDX_CHAR_VAL_LATITUDE, false);
        location_update_value(lon, IDX_CHAR_VAL_LONGITUDE, false);
    }
}

void gps_get_speed(uint8_t* data) {
    if (data == NULL) {
        return;
    }
    char substr[12];

    char* start = strstr((char*) data, "N,");
    double speed = 0;

    if (start != NULL) {
        start = start + 2;
        char* end = strchr((char*)start, ',');

        mempcpy(substr, start, end - start);
        substr[end-start] = 0;
        speed = atof(substr);
        location_update_value(speed, IDX_CHAR_VAL_SPEED, false);
    }
}

void gps_satelite_info(uint8_t* data) {
    if (data == NULL) {
        return;
    }

    char substr[12];
    char* start = strchr((char*) data, ',') + 1;

    if (start != NULL) {
        for (uint8_t i=0; i<5; i++) {
            start = strchr((char*)start, ',') +1;
        }

        char* end = strchr((char*)start, ',');

        mempcpy(substr, start, end - start);
        substr[end-start] = 0;

        uint8_t fix = atoi(substr);

        start = end +1;
        end = strchr((char*)start, ',');

        mempcpy(substr, start, end - start);
        substr[end-start] = 0;

        uint8_t satelite_number = atoi(substr);

        start = end +1;
        end = strchr((char*)start, ',');

        start = end +1;
        end = strchr((char*)start, ',');
        mempcpy(substr, start, end - start);
        substr[end-start] = 0;

        state.altitude = atof(substr);;

        location_update_u8_value(satelite_number, IDX_CHAR_VAL_GPS_SATELITE_COUNT, false);
        location_update_u8_value(fix, IDX_CHAR_VAL_GPS_FIX, false);
    }
}

void gps_handle_nmea_data(uint8_t* data, uint16_t len){
    gps_get_location((uint8_t*)strstr((char*)data, "$GNGLL"));
    gps_get_speed((uint8_t*)strstr((char*)data, "$GNVTG"));
    gps_satelite_info((uint8_t*)strstr((char*)data, "$GNGGA"));
}

void gps_rx_task() {
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE+1);
    while (1) {
        const int rxBytes = uart_read_bytes(UART_NUM_2, data, RX_BUF_SIZE, 500 / portTICK_RATE_MS);
        if (rxBytes > 0) {
            data[rxBytes] = 0;
            //ESP_LOGI("As", "%s", data);
            gps_handle_nmea_data(data, rxBytes);
        }
    }
    free(data);
}

void gps_enable_power_saving_mode() {
    const char* msg = "$PMTK161,0*28\r\n";
    uart_write_bytes(UART_NUM_2, msg, strlen(msg));
}

void gps_disable_power_saving_mode() {
    const char* msg = "$PMTK101*32\r\n";
    uart_write_bytes(UART_NUM_2, msg, strlen(msg));
}

void init_gps() {
    gps_init_uart();

    gps_enable_power_saving_mode();
    xTaskCreate(gps_rx_task, "uart_rx_task", 1024*2, NULL, configMAX_PRIORITIES, NULL);
}
