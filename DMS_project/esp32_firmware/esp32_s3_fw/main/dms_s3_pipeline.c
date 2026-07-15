#include <stddef.h>
#include <string.h>

#include "config.h"
#include "dms_s3_pipeline.h"

void dms_s3_pipeline_init(dms_s3_pipeline_t *pipeline)
{
    if (pipeline != NULL) {
        memset(pipeline, 0, sizeof(*pipeline));
        perclos_reset(&pipeline->fatigue);
    }
}

void dms_s3_pipeline_handle_frame(const dms_uart_frame_t *frame, void *context)
{
    dms_s3_pipeline_t *pipeline = context;
    if (pipeline == NULL || frame == NULL) {
        return;
    }
    if (frame->type == DMS_MSG_MODEL_RESULT) {
        dms_uart_model_result_t result;
        if (dms_uart_decode_model_result(frame, &result) != DMS_UART_OK) {
            ++pipeline->invalid_model_result_count;
            perclos_mark_observation_invalid(frame->sequence, &pipeline->fatigue);
            return;
        }
        ++pipeline->model_result_count;
        pipeline->has_model_result = true;
        pipeline->model_result_timed_out = false;
        pipeline->last_model_result_ms = result.source_timestamp_ms;
        perclos_update_classifier(dms_uart_probability_to_float(result.face_valid_probability_q),
                                  dms_uart_probability_to_float(result.eye_closed_probability_q),
                                  dms_uart_probability_to_float(result.yawn_probability_q),
                                  result.source_timestamp_ms,
                                  &pipeline->fatigue);
        return;
    }
    if (frame->type == DMS_MSG_MODEL_UNAVAILABLE) {
        dms_uart_model_unavailable_t unavailable;
        if (dms_uart_decode_model_unavailable(frame, &unavailable) == DMS_UART_OK) {
            ++pipeline->model_unavailable_count;
        }
        pipeline->has_model_result = false;
        pipeline->model_result_timed_out = false;
        perclos_mark_observation_invalid(frame->sequence, &pipeline->fatigue);
        return;
    }
    if (frame->type == DMS_MSG_CAMERA_STATUS) {
        ++pipeline->camera_status_count;
    } else if (frame->type == DMS_MSG_HEARTBEAT) {
        ++pipeline->heartbeat_count;
    }
}

void dms_s3_pipeline_advance_time(dms_s3_pipeline_t *pipeline, uint32_t now_ms)
{
    if (pipeline == NULL || !pipeline->has_model_result || pipeline->model_result_timed_out) {
        return;
    }
    if ((uint32_t)(now_ms - pipeline->last_model_result_ms) >= DMS_MODEL_RESULT_TIMEOUT_MS) {
        pipeline->model_result_timed_out = true;
        ++pipeline->model_result_timeout_count;
        perclos_mark_observation_invalid(now_ms, &pipeline->fatigue);
    }
}
