idf_component_register(
    SRCS "main.c" "camera_board.c" "cam_model_adapter.c"
    INCLUDE_DIRS "."
    REQUIRES common driver freertos esp_timer espressif__esp32-camera
)
