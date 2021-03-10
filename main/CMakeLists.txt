idf_component_register(SRCS "service_state.c" "http_server.c" "wifi.c" "settings.c" "service_settings.c" "service_location.c" "service_battery.c" "power.c" "gps.c" "logger.c" "main.c" "gatt.c" "ads1115/ads1115.c" "ds3231/ds3231.c" "uploader.c" "activity_detector.c" "state.c" 
INCLUDE_DIRS "." 
EMBED_TXTFILES root_cert.pem
REQUIRES driver bt esp_http_server fatfs spiffs esp_http_client esp-tls
)