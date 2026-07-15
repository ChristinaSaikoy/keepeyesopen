#ifndef DMS_CAMERA_BOARD_H
#define DMS_CAMERA_BOARD_H

#include <stdbool.h>

#include "esp_camera.h"

typedef enum {
    DMS_CAMERA_BOARD_OK = 0,
    DMS_CAMERA_BOARD_UNCONFIGURED,
    DMS_CAMERA_BOARD_PIN_CONFLICT,
} dms_camera_board_status_t;

dms_camera_board_status_t dms_camera_board_validate(void);
const char *dms_camera_board_status_name(dms_camera_board_status_t status);
void dms_camera_board_fill_config(camera_config_t *config);

#endif /* DMS_CAMERA_BOARD_H */
