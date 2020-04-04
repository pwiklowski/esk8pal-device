#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"
#include "state.h"

static const int RX_BUF_SIZE = 1024;

#define TXD_PIN (GPIO_NUM_17)
#define RXD_PIN (GPIO_NUM_16)

static const char *RX_TASK_TAG = "RX_TASK";

extern struct CurrentState state;


void init() {
    const uart_config_t uart_config = {
        .baud_rate = 9600,
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


void get_time_date(uint8_t* data) {
    //$GNZDA,142713.000,04,04,2020,00,00*4A
    char substr[10];

    char* start = strchr((char*)data, ',') + 1;
    strncpy(substr, start, 6);
    substr[6] = 0;
    state.time = atoi(substr);

    start = strchr((char*)start, ',') + 1;
    strncpy(substr, start, 2);
    substr[2] = 0;
    state.day = atoi(substr);

    start = strchr((char*)start, ',') + 1;
    strncpy(substr, start, 2);
    substr[2] = 0;
    state.month = atoi(substr);

    start = strchr((char*)start, ',') + 1;
    strncpy(substr, start, 4);
    substr[4] = 0;
    state.year = atoi(substr);
}

void get_location(uint8_t* data) {
    char substr[12];

    double lat, lon;
    char lat_ind, lon_ind;

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

    if (lon_ind) {
        lon = -lon;
    }

    state.latitude.value = lat;
    state.longitude.value = lon;
}

void get_speed(uint8_t* data) {
    if (data == NULL) {
        return;
    }
    char substr[12];

    char* start = strstr((char*) data, "N,");

    if (start != NULL) {
        start = start + 2;
        char* end = strchr((char*)start, ',');

        mempcpy(substr, start, end - start);
        substr[end-start] = 0;
        state.speed.value = atof(substr);
    }
}

void handleNmeaData(uint8_t* data, uint16_t len){
    get_time_date((uint8_t*)strstr((char*)data, "$GNZDA"));
    get_location((uint8_t*)strstr((char*)data, "$GNGLL"));
    get_speed((uint8_t*)strstr((char*)data, "$GNVTG"));
}

void rx_task() {
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE+1);
    while (1) {
        const int rxBytes = uart_read_bytes(UART_NUM_2, data, RX_BUF_SIZE, 500 / portTICK_RATE_MS);
        if (rxBytes > 0) {
            data[rxBytes] = 0;
            handleNmeaData(data, rxBytes);
        }
    }
    free(data);
}


void init_gps() {
    init();
    xTaskCreate(rx_task, "uart_rx_task", 1024*2, NULL, configMAX_PRIORITIES, NULL);
}
