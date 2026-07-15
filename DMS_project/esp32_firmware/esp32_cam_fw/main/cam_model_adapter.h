#ifndef DMS_CAM_MODEL_ADAPTER_H
#define DMS_CAM_MODEL_ADAPTER_H

#include <stdbool.h>
#include <stdint.h>

#include "dms_uart_protocol.h"
#include "esp_camera.h"

typedef struct {
    bool face_valid;
    float face_valid_probability;
    float eye_closed_probability;
    float yawn_probability;
    uint32_t inference_time_us;
    uint16_t status_flags;
} dms_model_output_t;

dms_model_status_t cam_model_run(const camera_fb_t *frame, dms_model_output_t *output);

#endif /* DMS_CAM_MODEL_ADAPTER_H */
