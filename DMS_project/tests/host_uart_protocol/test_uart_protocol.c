#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dms_uart_protocol.h"

static int failures = 0;

#define CHECK(condition)                                                                            \
    do {                                                                                            \
        if (!(condition)) {                                                                         \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                \
            ++failures;                                                                             \
            return;                                                                                 \
        }                                                                                           \
    } while (0)

typedef struct {
    uint32_t count;
    dms_uart_frame_t last;
} receiver_t;

static void receive_frame(const dms_uart_frame_t *frame, void *context)
{
    receiver_t *receiver = context;
    ++receiver->count;
    receiver->last = *frame;
}

static size_t encode_result(uint32_t sequence, uint16_t face, uint16_t eye, uint16_t yawn, uint8_t *frame)
{
    const dms_uart_model_result_t result = {
        .source_timestamp_ms = 1234U,
        .face_valid_probability_q = face,
        .eye_closed_probability_q = eye,
        .yawn_probability_q = yawn,
        .inference_time_us = 4321U,
        .status_flags = 0x55AAU,
    };
    size_t length = 0U;
    if (dms_uart_encode_model_result(sequence, &result, frame, DMS_UART_MAX_FRAME_SIZE, &length) != DMS_UART_OK) {
        fprintf(stderr, "FAIL %s:%d: model result encoding\n", __FILE__, __LINE__);
        ++failures;
        return 0U;
    }
    return length;
}

static void test_encode_decode_probability_bounds(void)
{
    uint8_t raw[DMS_UART_MAX_FRAME_SIZE];
    const size_t length = encode_result(7U, 0U, DMS_UART_PROBABILITY_SCALE, 5000U, raw);
    CHECK(length != 0U);
    dms_uart_parser_t parser;
    receiver_t receiver = {0};
    dms_uart_parser_init(&parser);
    dms_uart_parser_feed(&parser, raw, length, 10U, receive_frame, &receiver);
    CHECK(receiver.count == 1U);
    dms_uart_model_result_t result;
    CHECK(dms_uart_decode_model_result(&receiver.last, &result) == DMS_UART_OK);
    CHECK(result.face_valid_probability_q == 0U);
    CHECK(result.eye_closed_probability_q == DMS_UART_PROBABILITY_SCALE);
    CHECK(result.yawn_probability_q == 5000U);

    dms_uart_model_result_t invalid = result;
    invalid.face_valid_probability_q = DMS_UART_PROBABILITY_SCALE + 1U;
    size_t out_length = 0U;
    CHECK(dms_uart_encode_model_result(8U, &invalid, raw, sizeof(raw), &out_length) == DMS_UART_INVALID_PAYLOAD);
}

static void test_fragmented_and_concatenated_frames(void)
{
    uint8_t first[DMS_UART_MAX_FRAME_SIZE];
    uint8_t second[DMS_UART_MAX_FRAME_SIZE];
    const size_t first_length = encode_result(1U, 9000U, 7000U, 1000U, first);
    const size_t second_length = encode_result(2U, 9000U, 1000U, 7000U, second);
    CHECK(first_length != 0U && second_length != 0U);
    dms_uart_parser_t parser;
    receiver_t receiver = {0};
    dms_uart_parser_init(&parser);
    dms_uart_parser_feed(&parser, first, 3U, 1U, receive_frame, &receiver);
    dms_uart_parser_feed(&parser, &first[3], first_length - 3U, 2U, receive_frame, &receiver);
    CHECK(receiver.count == 1U);
    uint8_t joined[DMS_UART_MAX_FRAME_SIZE * 2U];
    memcpy(joined, first, first_length);
    memcpy(&joined[first_length], second, second_length);
    dms_uart_parser_feed(&parser, joined, first_length + second_length, 3U, receive_frame, &receiver);
    CHECK(receiver.count == 3U);
    CHECK(receiver.last.sequence == 2U);
}

static void test_garbage_crc_version_length_and_timeout(void)
{
    uint8_t raw[DMS_UART_MAX_FRAME_SIZE];
    const size_t length = encode_result(4U, 9000U, 9000U, 1000U, raw);
    CHECK(length != 0U);
    dms_uart_parser_t parser;
    receiver_t receiver = {0};
    dms_uart_parser_init(&parser);
    const uint8_t garbage[] = {0x00U, 0xD5U, 0x00U, 0xFFU};
    dms_uart_parser_feed(&parser, garbage, sizeof(garbage), 1U, receive_frame, &receiver);
    dms_uart_parser_feed(&parser, raw, length, 2U, receive_frame, &receiver);
    CHECK(receiver.count == 1U);
    CHECK(parser.stats.discarded_bytes > 0U);

    raw[length - 1U] ^= 0x01U;
    dms_uart_parser_feed(&parser, raw, length, 3U, receive_frame, &receiver);
    CHECK(receiver.count == 1U);
    CHECK(parser.stats.crc_errors == 1U);
    raw[length - 1U] ^= 0x01U;

    raw[2] = DMS_UART_VERSION + 1U;
    dms_uart_parser_feed(&parser, raw, length, 4U, receive_frame, &receiver);
    CHECK(parser.stats.bad_version_errors == 1U);
    raw[2] = DMS_UART_VERSION;

    const uint8_t bad_length[] = {DMS_UART_MAGIC0, DMS_UART_MAGIC1, DMS_UART_VERSION, DMS_MSG_HEARTBEAT, 0x00U, 0x41U};
    dms_uart_parser_feed(&parser, bad_length, sizeof(bad_length), 5U, receive_frame, &receiver);
    CHECK(parser.stats.bad_length_errors == 1U);

    dms_uart_parser_feed(&parser, raw, 4U, UINT32_MAX - 2U, receive_frame, &receiver);
    dms_uart_parser_advance_time(&parser, 20U, 10U);
    CHECK(parser.stats.timeout_count == 1U);
}

static void test_sequence_jump_wrap_and_model_unavailable(void)
{
    uint8_t raw[DMS_UART_MAX_FRAME_SIZE];
    dms_uart_parser_t parser;
    receiver_t receiver = {0};
    dms_uart_parser_init(&parser);
    size_t length = encode_result(UINT32_MAX, 9000U, 1000U, 1000U, raw);
    CHECK(length != 0U);
    dms_uart_parser_feed(&parser, raw, length, 1U, receive_frame, &receiver);
    length = encode_result(0U, 9000U, 1000U, 1000U, raw);
    dms_uart_parser_feed(&parser, raw, length, 2U, receive_frame, &receiver);
    CHECK(parser.stats.sequence_gap_count == 0U);
    length = encode_result(2U, 9000U, 1000U, 1000U, raw);
    dms_uart_parser_feed(&parser, raw, length, 3U, receive_frame, &receiver);
    CHECK(parser.stats.sequence_gap_count == 1U);

    const dms_uart_model_unavailable_t unavailable = {.source_timestamp_ms = 3U, .status_code = DMS_MODEL_UNAVAILABLE};
    size_t unavailable_length = 0U;
    CHECK(dms_uart_encode_model_unavailable(3U, &unavailable, raw, sizeof(raw), &unavailable_length) == DMS_UART_OK);
    dms_uart_parser_feed(&parser, raw, unavailable_length, 4U, receive_frame, &receiver);
    dms_uart_model_unavailable_t decoded;
    CHECK(dms_uart_decode_model_unavailable(&receiver.last, &decoded) == DMS_UART_OK);
    CHECK(decoded.status_code == DMS_MODEL_UNAVAILABLE);
}

int main(void)
{
    test_encode_decode_probability_bounds();
    test_fragmented_and_concatenated_frames();
    test_garbage_crc_version_length_and_timeout();
    test_sequence_jump_wrap_and_model_unavailable();
    if (failures != 0) {
        fprintf(stderr, "%d UART protocol test group(s) failed\n", failures);
        return 1;
    }
    puts("4 UART protocol test groups passed");
    return 0;
}
