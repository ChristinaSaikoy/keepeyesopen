#include <string.h>

#include "dms_uart_protocol.h"

enum {
    PARSER_MAGIC0 = 0,
    PARSER_MAGIC1,
    PARSER_VERSION,
    PARSER_TYPE,
    PARSER_LENGTH_HI,
    PARSER_LENGTH_LO,
    PARSER_SEQUENCE_0,
    PARSER_SEQUENCE_1,
    PARSER_SEQUENCE_2,
    PARSER_SEQUENCE_3,
    PARSER_PAYLOAD,
    PARSER_CRC_HI,
    PARSER_CRC_LO,
};

static void put_u16_be(uint8_t *out, uint16_t value)
{
    out[0] = (uint8_t)(value >> 8U);
    out[1] = (uint8_t)value;
}

static void put_u32_be(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)(value >> 24U);
    out[1] = (uint8_t)(value >> 16U);
    out[2] = (uint8_t)(value >> 8U);
    out[3] = (uint8_t)value;
}

static uint16_t get_u16_be(const uint8_t *in)
{
    return (uint16_t)(((uint16_t)in[0] << 8U) | in[1]);
}

static uint32_t get_u32_be(const uint8_t *in)
{
    return ((uint32_t)in[0] << 24U) | ((uint32_t)in[1] << 16U) |
           ((uint32_t)in[2] << 8U) | in[3];
}

static uint16_t crc_update(uint16_t crc, uint8_t byte)
{
    crc ^= (uint16_t)byte << 8U;
    for (uint8_t bit = 0U; bit < 8U; ++bit) {
        crc = (crc & 0x8000U) != 0U ? (uint16_t)((crc << 1U) ^ 0x1021U) : (uint16_t)(crc << 1U);
    }
    return crc;
}

static void parser_reset_frame(dms_uart_parser_t *parser)
{
    parser->state = PARSER_MAGIC0;
    parser->payload_length = 0U;
    parser->payload_index = 0U;
    parser->sequence = 0U;
    parser->crc = 0xFFFFU;
    parser->received_crc = 0U;
}

static void parser_start_frame(dms_uart_parser_t *parser, uint32_t now_ms)
{
    parser_reset_frame(parser);
    parser->state = PARSER_MAGIC1;
    parser->last_byte_ms = now_ms;
}

uint16_t dms_uart_crc16_ccitt_false(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFFU;
    if (data == NULL && length != 0U) {
        return 0U;
    }
    for (size_t i = 0U; i < length; ++i) {
        crc = crc_update(crc, data[i]);
    }
    return crc;
}

bool dms_uart_message_type_is_valid(uint8_t type)
{
    return type >= (uint8_t)DMS_MSG_CAMERA_STATUS && type <= (uint8_t)DMS_MSG_HEARTBEAT;
}

bool dms_uart_probability_is_valid(uint16_t probability_q)
{
    return probability_q <= DMS_UART_PROBABILITY_SCALE;
}

float dms_uart_probability_to_float(uint16_t probability_q)
{
    return (float)probability_q / (float)DMS_UART_PROBABILITY_SCALE;
}

dms_uart_status_t dms_uart_encode_frame(dms_uart_message_type_t type,
                                        uint32_t sequence,
                                        const uint8_t *payload,
                                        uint16_t payload_length,
                                        uint8_t *out,
                                        size_t out_capacity,
                                        size_t *out_length)
{
    const size_t frame_length = DMS_UART_HEADER_SIZE + payload_length + DMS_UART_CRC_SIZE;
    if (out == NULL || out_length == NULL || (payload == NULL && payload_length != 0U) ||
        !dms_uart_message_type_is_valid((uint8_t)type) || payload_length > DMS_UART_MAX_PAYLOAD) {
        return DMS_UART_INVALID_ARGUMENT;
    }
    if (out_capacity < frame_length) {
        return DMS_UART_BUFFER_TOO_SMALL;
    }

    out[0] = DMS_UART_MAGIC0;
    out[1] = DMS_UART_MAGIC1;
    out[2] = DMS_UART_VERSION;
    out[3] = (uint8_t)type;
    put_u16_be(&out[4], payload_length);
    put_u32_be(&out[6], sequence);
    if (payload_length != 0U) {
        memcpy(&out[DMS_UART_HEADER_SIZE], payload, payload_length);
    }
    put_u16_be(&out[DMS_UART_HEADER_SIZE + payload_length],
               dms_uart_crc16_ccitt_false(&out[2], DMS_UART_HEADER_SIZE - 2U + payload_length));
    *out_length = frame_length;
    return DMS_UART_OK;
}

dms_uart_status_t dms_uart_encode_model_result(uint32_t sequence,
                                               const dms_uart_model_result_t *result,
                                               uint8_t *out,
                                               size_t out_capacity,
                                               size_t *out_length)
{
    uint8_t payload[DMS_UART_MODEL_RESULT_PAYLOAD_SIZE];
    if (result == NULL || !dms_uart_probability_is_valid(result->face_valid_probability_q) ||
        !dms_uart_probability_is_valid(result->eye_closed_probability_q) ||
        !dms_uart_probability_is_valid(result->yawn_probability_q)) {
        return DMS_UART_INVALID_PAYLOAD;
    }
    put_u32_be(&payload[0], result->source_timestamp_ms);
    put_u16_be(&payload[4], result->face_valid_probability_q);
    put_u16_be(&payload[6], result->eye_closed_probability_q);
    put_u16_be(&payload[8], result->yawn_probability_q);
    put_u32_be(&payload[10], result->inference_time_us);
    put_u16_be(&payload[14], result->status_flags);
    return dms_uart_encode_frame(DMS_MSG_MODEL_RESULT, sequence, payload, sizeof(payload), out, out_capacity, out_length);
}

dms_uart_status_t dms_uart_encode_camera_status(uint32_t sequence,
                                                const dms_uart_camera_status_t *status,
                                                uint8_t *out,
                                                size_t out_capacity,
                                                size_t *out_length)
{
    uint8_t payload[DMS_UART_CAMERA_STATUS_PAYLOAD_SIZE];
    if (status == NULL) {
        return DMS_UART_INVALID_ARGUMENT;
    }
    payload[0] = status->status_code;
    put_u16_be(&payload[1], status->sensor_pid);
    put_u16_be(&payload[3], status->width);
    put_u16_be(&payload[5], status->height);
    payload[7] = status->pixel_format;
    put_u32_be(&payload[8], status->capture_error_count);
    return dms_uart_encode_frame(DMS_MSG_CAMERA_STATUS, sequence, payload, sizeof(payload), out, out_capacity, out_length);
}

dms_uart_status_t dms_uart_encode_model_unavailable(uint32_t sequence,
                                                    const dms_uart_model_unavailable_t *status,
                                                    uint8_t *out,
                                                    size_t out_capacity,
                                                    size_t *out_length)
{
    uint8_t payload[DMS_UART_MODEL_UNAVAILABLE_PAYLOAD_SIZE];
    if (status == NULL) {
        return DMS_UART_INVALID_ARGUMENT;
    }
    put_u32_be(&payload[0], status->source_timestamp_ms);
    put_u16_be(&payload[4], status->status_code);
    return dms_uart_encode_frame(DMS_MSG_MODEL_UNAVAILABLE, sequence, payload, sizeof(payload), out, out_capacity, out_length);
}

dms_uart_status_t dms_uart_encode_heartbeat(uint32_t sequence,
                                            const dms_uart_heartbeat_t *heartbeat,
                                            uint8_t *out,
                                            size_t out_capacity,
                                            size_t *out_length)
{
    uint8_t payload[DMS_UART_HEARTBEAT_PAYLOAD_SIZE];
    if (heartbeat == NULL) {
        return DMS_UART_INVALID_ARGUMENT;
    }
    put_u32_be(&payload[0], heartbeat->uptime_ms);
    put_u16_be(&payload[4], heartbeat->status_flags);
    return dms_uart_encode_frame(DMS_MSG_HEARTBEAT, sequence, payload, sizeof(payload), out, out_capacity, out_length);
}

dms_uart_status_t dms_uart_decode_model_result(const dms_uart_frame_t *frame,
                                               dms_uart_model_result_t *result)
{
    if (frame == NULL || result == NULL || frame->type != DMS_MSG_MODEL_RESULT ||
        frame->payload_length != DMS_UART_MODEL_RESULT_PAYLOAD_SIZE) {
        return DMS_UART_INVALID_ARGUMENT;
    }
    result->source_timestamp_ms = get_u32_be(&frame->payload[0]);
    result->face_valid_probability_q = get_u16_be(&frame->payload[4]);
    result->eye_closed_probability_q = get_u16_be(&frame->payload[6]);
    result->yawn_probability_q = get_u16_be(&frame->payload[8]);
    result->inference_time_us = get_u32_be(&frame->payload[10]);
    result->status_flags = get_u16_be(&frame->payload[14]);
    if (!dms_uart_probability_is_valid(result->face_valid_probability_q) ||
        !dms_uart_probability_is_valid(result->eye_closed_probability_q) ||
        !dms_uart_probability_is_valid(result->yawn_probability_q)) {
        return DMS_UART_INVALID_PAYLOAD;
    }
    return DMS_UART_OK;
}

dms_uart_status_t dms_uart_decode_model_unavailable(const dms_uart_frame_t *frame,
                                                    dms_uart_model_unavailable_t *status)
{
    if (frame == NULL || status == NULL || frame->type != DMS_MSG_MODEL_UNAVAILABLE ||
        frame->payload_length != DMS_UART_MODEL_UNAVAILABLE_PAYLOAD_SIZE) {
        return DMS_UART_INVALID_ARGUMENT;
    }
    status->source_timestamp_ms = get_u32_be(&frame->payload[0]);
    status->status_code = get_u16_be(&frame->payload[4]);
    return DMS_UART_OK;
}

void dms_uart_parser_init(dms_uart_parser_t *parser)
{
    if (parser == NULL) {
        return;
    }
    memset(parser, 0, sizeof(*parser));
    parser_reset_frame(parser);
}

static void parser_emit_frame(dms_uart_parser_t *parser, dms_uart_frame_callback_t callback, void *context)
{
    dms_uart_frame_t frame = {
        .type = (dms_uart_message_type_t)parser->type,
        .payload_length = parser->payload_length,
        .sequence = parser->sequence,
    };
    if (frame.payload_length != 0U) {
        memcpy(frame.payload, parser->payload, frame.payload_length);
    }
    if (parser->has_last_sequence && parser->sequence != parser->last_sequence + 1U) {
        ++parser->stats.sequence_gap_count;
    }
    parser->last_sequence = parser->sequence;
    parser->has_last_sequence = true;
    ++parser->stats.frames_ok;
    if (callback != NULL) {
        callback(&frame, context);
    }
}

void dms_uart_parser_feed(dms_uart_parser_t *parser,
                          const uint8_t *data,
                          size_t length,
                          uint32_t now_ms,
                          dms_uart_frame_callback_t callback,
                          void *context)
{
    if (parser == NULL || (data == NULL && length != 0U)) {
        return;
    }
    for (size_t i = 0U; i < length; ++i) {
        const uint8_t byte = data[i];
        switch (parser->state) {
        case PARSER_MAGIC0:
            if (byte == DMS_UART_MAGIC0) {
                parser_start_frame(parser, now_ms);
            } else {
                ++parser->stats.discarded_bytes;
            }
            break;
        case PARSER_MAGIC1:
            if (byte == DMS_UART_MAGIC1) {
                parser->state = PARSER_VERSION;
            } else if (byte == DMS_UART_MAGIC0) {
                parser->last_byte_ms = now_ms;
            } else {
                ++parser->stats.discarded_bytes;
                parser_reset_frame(parser);
            }
            break;
        case PARSER_VERSION:
            if (byte != DMS_UART_VERSION) {
                ++parser->stats.bad_version_errors;
                parser_reset_frame(parser);
                break;
            }
            parser->crc = crc_update(parser->crc, byte);
            parser->state = PARSER_TYPE;
            break;
        case PARSER_TYPE:
            if (!dms_uart_message_type_is_valid(byte)) {
                ++parser->stats.bad_type_errors;
                parser_reset_frame(parser);
                break;
            }
            parser->type = byte;
            parser->crc = crc_update(parser->crc, byte);
            parser->state = PARSER_LENGTH_HI;
            break;
        case PARSER_LENGTH_HI:
            parser->payload_length = (uint16_t)byte << 8U;
            parser->crc = crc_update(parser->crc, byte);
            parser->state = PARSER_LENGTH_LO;
            break;
        case PARSER_LENGTH_LO:
            parser->payload_length |= byte;
            parser->crc = crc_update(parser->crc, byte);
            if (parser->payload_length > DMS_UART_MAX_PAYLOAD) {
                ++parser->stats.bad_length_errors;
                parser_reset_frame(parser);
            } else {
                parser->state = PARSER_SEQUENCE_0;
            }
            break;
        case PARSER_SEQUENCE_0:
        case PARSER_SEQUENCE_1:
        case PARSER_SEQUENCE_2:
        case PARSER_SEQUENCE_3:
            parser->sequence = (parser->sequence << 8U) | byte;
            parser->crc = crc_update(parser->crc, byte);
            if (parser->state == PARSER_SEQUENCE_3) {
                parser->state = parser->payload_length == 0U ? PARSER_CRC_HI : PARSER_PAYLOAD;
            } else {
                ++parser->state;
            }
            break;
        case PARSER_PAYLOAD:
            parser->payload[parser->payload_index++] = byte;
            parser->crc = crc_update(parser->crc, byte);
            if (parser->payload_index == parser->payload_length) {
                parser->state = PARSER_CRC_HI;
            }
            break;
        case PARSER_CRC_HI:
            parser->received_crc = (uint16_t)byte << 8U;
            parser->state = PARSER_CRC_LO;
            break;
        case PARSER_CRC_LO:
            parser->received_crc |= byte;
            if (parser->received_crc == parser->crc) {
                parser_emit_frame(parser, callback, context);
            } else {
                ++parser->stats.crc_errors;
            }
            parser_reset_frame(parser);
            break;
        default:
            parser_reset_frame(parser);
            break;
        }
        if (parser->state != PARSER_MAGIC0) {
            parser->last_byte_ms = now_ms;
        }
    }
}

void dms_uart_parser_advance_time(dms_uart_parser_t *parser, uint32_t now_ms, uint32_t timeout_ms)
{
    if (parser == NULL || parser->state == PARSER_MAGIC0 || timeout_ms == 0U) {
        return;
    }
    if ((uint32_t)(now_ms - parser->last_byte_ms) >= timeout_ms) {
        ++parser->stats.timeout_count;
        parser_reset_frame(parser);
    }
}
