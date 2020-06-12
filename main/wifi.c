#include "wifi.h"
#include "state.h"

extern struct CurrentState state;
extern struct Settings settings;

static const char *TAG = "WiFi";

wifi_state_t current_state = WIFI_DISABLED;

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

    memcpy(wifi_config.ap.ssid, settings.wifi_ssid, strlen((char*) settings.wifi_ssid));
    wifi_config.ap.ssid_len = strlen((char*) settings.wifi_ssid),
    memcpy(wifi_config.ap.password, settings.wifi_pass, strlen((char*) settings.wifi_pass));

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

wifi_state_t wifi_get_state() {
    return current_state;
}

void wifi_set_state(wifi_state_t state) {
    if (state == WIFI_AP) {
        if (current_state != WIFI_AP) {
            wifi_init_softap();
            current_state = WIFI_AP;
        }
    } else if (state == WIFI_CLIENT) {
        if (current_state != WIFI_CLIENT) {
            wifi_init_softap();
            current_state = WIFI_CLIENT;
        }
    } else {
        if (current_state != WIFI_DISABLED) {
            esp_wifi_disconnect();
            esp_wifi_stop();
            current_state = WIFI_DISABLED;
        }
    }
}
