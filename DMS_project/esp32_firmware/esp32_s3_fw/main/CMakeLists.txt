idf_component_register(
    SRCS "main.c" "perclos.c" "dms_decision.c" "dms_mqtt_contract.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_wifi esp_event mqtt driver freertos esp_timer json
)
