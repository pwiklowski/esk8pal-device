#include "wifi.h"
#include "state.h"

extern struct CurrentState state;
static const char *TAG = "WiFi";

bool enabled = false;

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d", MAC2STR(event->mac), event->aid);
    }
}

void wifi_init_softap() {
    wifi_config_t wifi_config = {
        .ap = {
            .max_connection = 1, 
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    memcpy(wifi_config.ap.ssid, state.wifi_ssid, strlen((char* )state.wifi_ssid));
    wifi_config.ap.ssid_len = strlen((char* )state.wifi_ssid),
    memcpy(wifi_config.ap.password, state.wifi_pass, strlen((char* )state.wifi_pass));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void wifi_init() {
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
}

void wifi_set_state(bool enable) {
    if (enable) {
        if (!enabled) {
            wifi_init_softap();
            enabled = true;
        }
    } else {
        if (enabled) {
            esp_wifi_disconnect();
            esp_wifi_stop();
            enabled = false;
        }
    }
}
