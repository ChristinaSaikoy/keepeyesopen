idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_wifi esp_event esp_http_server freertos esp_timer espressif__esp32-camera
)
