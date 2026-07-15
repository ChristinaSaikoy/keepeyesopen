#ifndef DMS_S3_PIPELINE_H
#define DMS_S3_PIPELINE_H

#include <stdbool.h>
#include <stdint.h>

#include "dms_uart_protocol.h"
#include "perclos.h"

typedef struct {
    perclos_state_t fatigue;
    bool has_model_result;
    bool model_result_timed_out;
    uint32_t last_model_result_ms;
    uint32_t model_result_count;
    uint32_t model_unavailable_count;
    uint32_t camera_status_count;
    uint32_t heartbeat_count;
    uint32_t invalid_model_result_count;
    uint32_t model_result_timeout_count;
} dms_s3_pipeline_t;

void dms_s3_pipeline_init(dms_s3_pipeline_t *pipeline);
void dms_s3_pipeline_handle_frame(const dms_uart_frame_t *frame, void *context);
void dms_s3_pipeline_advance_time(dms_s3_pipeline_t *pipeline, uint32_t now_ms);

#endif /* DMS_S3_PIPELINE_H */
