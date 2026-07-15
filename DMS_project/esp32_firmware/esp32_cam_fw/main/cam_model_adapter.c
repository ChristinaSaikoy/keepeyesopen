#include <stddef.h>
#include <string.h>

#include "cam_model_adapter.h"

dms_model_status_t cam_model_run(const camera_fb_t *frame, dms_model_output_t *output)
{
    if (output != NULL) {
        memset(output, 0, sizeof(*output));
    }
    if (frame == NULL || frame->buf == NULL || frame->len == 0U) {
        return DMS_MODEL_INVALID_INPUT;
    }
    return DMS_MODEL_UNAVAILABLE;
}
