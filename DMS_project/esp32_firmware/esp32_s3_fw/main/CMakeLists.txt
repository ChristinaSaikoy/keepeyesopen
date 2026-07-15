idf_component_register(
    SRCS "main.c" "perclos.c" "dms_s3_pipeline.c"
    INCLUDE_DIRS "."
    REQUIRES common nvs_flash esp_wifi esp_event mqtt driver freertos esp_timer
)
