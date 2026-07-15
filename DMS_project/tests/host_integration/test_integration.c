#include <stdint.h>
#include <stdio.h>

#include "dms_s3_pipeline.h"

static int failures = 0;

#define CHECK(condition)                                                                            \
    do {                                                                                            \
        if (!(condition)) {                                                                         \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                \
            ++failures;                                                                             \
            return;                                                                                 \
        }                                                                                           \
    } while (0)

static void feed_result(dms_uart_parser_t *parser,
                        dms_s3_pipeline_t *pipeline,
                        uint32_t sequence,
                        uint32_t timestamp_ms,
                        uint16_t eye_probability)
{
    dms_uart_model_result_t result = {.source_timestamp_ms = timestamp_ms,
                                      .face_valid_probability_q = 9000U,
                                      .eye_closed_probability_q = eye_probability,
                                      .yawn_probability_q = 1000U};
    uint8_t raw[DMS_UART_MAX_FRAME_SIZE];
    size_t length = 0U;
    CHECK(dms_uart_encode_model_result(sequence, &result, raw, sizeof(raw), &length) == DMS_UART_OK);
    dms_uart_parser_feed(parser, raw, length, timestamp_ms, dms_s3_pipeline_handle_frame, pipeline);
}

static void test_closed_eye_to_recovery(void)
{
    dms_uart_parser_t parser;
    dms_s3_pipeline_t pipeline;
    dms_uart_parser_init(&parser);
    dms_s3_pipeline_init(&pipeline);
    feed_result(&parser, &pipeline, 1U, 0U, 9000U);
    feed_result(&parser, &pipeline, 2U, BLINK_MICRO_SLEEP_MS, 9000U);
    CHECK(pipeline.fatigue.level == DMS_LEVEL_2_MICROSLEEP);
    feed_result(&parser, &pipeline, 3U, BLINK_DEEP_SLEEP_MS, 9000U);
    CHECK(pipeline.fatigue.level == DMS_LEVEL_3_SLEEP);
    feed_result(&parser, &pipeline, 4U, BLINK_DEEP_SLEEP_MS + 1U, 1000U);
    CHECK(pipeline.fatigue.level == DMS_LEVEL_NORMAL);
}

static void test_unavailable_crc_and_timeout_do_not_alert(void)
{
    dms_uart_parser_t parser;
    dms_s3_pipeline_t pipeline;
    dms_uart_parser_init(&parser);
    dms_s3_pipeline_init(&pipeline);
    feed_result(&parser, &pipeline, 1U, 0U, 9000U);

    dms_uart_model_unavailable_t unavailable = {.source_timestamp_ms = 1U, .status_code = DMS_MODEL_UNAVAILABLE};
    uint8_t raw[DMS_UART_MAX_FRAME_SIZE];
    size_t length = 0U;
    CHECK(dms_uart_encode_model_unavailable(2U, &unavailable, raw, sizeof(raw), &length) == DMS_UART_OK);
    dms_uart_parser_feed(&parser, raw, length, 1U, dms_s3_pipeline_handle_frame, &pipeline);
    CHECK(pipeline.model_unavailable_count == 1U && pipeline.fatigue.level == DMS_LEVEL_NORMAL);

    raw[length - 1U] ^= 0x01U;
    dms_uart_parser_feed(&parser, raw, length, 2U, dms_s3_pipeline_handle_frame, &pipeline);
    CHECK(parser.stats.crc_errors == 1U && pipeline.model_unavailable_count == 1U);

    feed_result(&parser, &pipeline, 3U, 10U, 9000U);
    dms_s3_pipeline_advance_time(&pipeline, 10U + DMS_MODEL_RESULT_TIMEOUT_MS);
    CHECK(pipeline.model_result_timeout_count == 1U && pipeline.fatigue.level == DMS_LEVEL_NORMAL);
}

int main(void)
{
    test_closed_eye_to_recovery();
    test_unavailable_crc_and_timeout_do_not_alert();
    if (failures != 0) {
        fprintf(stderr, "%d integration test group(s) failed\n", failures);
        return 1;
    }
    puts("2 protocol-to-state-machine integration test groups passed");
    return 0;
}
