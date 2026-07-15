#ifndef DMS_UART_PROTOCOL_H
#define DMS_UART_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DMS_UART_MAGIC0 0xD5U
#define DMS_UART_MAGIC1 0x4DU
#define DMS_UART_VERSION 1U
#define DMS_UART_MAX_PAYLOAD 64U
#define DMS_UART_HEADER_SIZE 10U
#define DMS_UART_CRC_SIZE 2U
#define DMS_UART_MAX_FRAME_SIZE (DMS_UART_HEADER_SIZE + DMS_UART_MAX_PAYLOAD + DMS_UART_CRC_SIZE)

#define DMS_UART_PROBABILITY_SCALE 10000U
#define DMS_UART_MODEL_RESULT_PAYLOAD_SIZE 16U
#define DMS_UART_CAMERA_STATUS_PAYLOAD_SIZE 12U
#define DMS_UART_MODEL_UNAVAILABLE_PAYLOAD_SIZE 6U
#define DMS_UART_HEARTBEAT_PAYLOAD_SIZE 6U

typedef enum {
    DMS_MSG_CAMERA_STATUS = 1,
    DMS_MSG_MODEL_RESULT = 2,
    DMS_MSG_MODEL_UNAVAILABLE = 3,
    DMS_MSG_HEARTBEAT = 4,
} dms_uart_message_type_t;

typedef enum {
    DMS_UART_OK = 0,
    DMS_UART_INVALID_ARGUMENT,
    DMS_UART_BUFFER_TOO_SMALL,
    DMS_UART_INVALID_PAYLOAD,
} dms_uart_status_t;

typedef enum {
    DMS_CAMERA_STATUS_INIT = 1,
    DMS_CAMERA_STATUS_CAPTURE_OK = 2,
    DMS_CAMERA_STATUS_CAPTURE_ERROR = 3,
} dms_camera_status_code_t;

typedef enum {
    DMS_MODEL_OK = 0,
    DMS_MODEL_UNAVAILABLE = 1,
    DMS_MODEL_INVALID_INPUT = 2,
    DMS_MODEL_INFERENCE_FAILED = 3,
} dms_model_status_t;

typedef struct {
    uint32_t source_timestamp_ms;
    uint16_t face_valid_probability_q;
    uint16_t eye_closed_probability_q;
    uint16_t yawn_probability_q;
    uint32_t inference_time_us;
    uint16_t status_flags;
} dms_uart_model_result_t;

typedef struct {
    uint8_t status_code;
    uint16_t sensor_pid;
    uint16_t width;
    uint16_t height;
    uint8_t pixel_format;
    uint32_t capture_error_count;
} dms_uart_camera_status_t;

typedef struct {
    uint32_t source_timestamp_ms;
    uint16_t status_code;
} dms_uart_model_unavailable_t;

typedef struct {
    uint32_t uptime_ms;
    uint16_t status_flags;
} dms_uart_heartbeat_t;

typedef struct {
    dms_uart_message_type_t type;
    uint16_t payload_length;
    uint32_t sequence;
    uint8_t payload[DMS_UART_MAX_PAYLOAD];
} dms_uart_frame_t;

typedef struct {
    uint32_t frames_ok;
    uint32_t crc_errors;
    uint32_t bad_length_errors;
    uint32_t bad_version_errors;
    uint32_t bad_type_errors;
    uint32_t discarded_bytes;
    uint32_t sequence_gap_count;
    uint32_t timeout_count;
} dms_uart_parser_stats_t;

typedef struct {
    uint8_t state;
    uint8_t type;
    uint16_t payload_length;
    uint16_t payload_index;
    uint32_t sequence;
    uint16_t crc;
    uint16_t received_crc;
    uint8_t payload[DMS_UART_MAX_PAYLOAD];
    bool has_last_sequence;
    uint32_t last_sequence;
    uint32_t last_byte_ms;
    dms_uart_parser_stats_t stats;
} dms_uart_parser_t;

typedef void (*dms_uart_frame_callback_t)(const dms_uart_frame_t *frame, void *context);

uint16_t dms_uart_crc16_ccitt_false(const uint8_t *data, size_t length);
bool dms_uart_message_type_is_valid(uint8_t type);
bool dms_uart_probability_is_valid(uint16_t probability_q);
float dms_uart_probability_to_float(uint16_t probability_q);

dms_uart_status_t dms_uart_encode_frame(dms_uart_message_type_t type,
                                        uint32_t sequence,
                                        const uint8_t *payload,
                                        uint16_t payload_length,
                                        uint8_t *out,
                                        size_t out_capacity,
                                        size_t *out_length);
dms_uart_status_t dms_uart_encode_model_result(uint32_t sequence,
                                               const dms_uart_model_result_t *result,
                                               uint8_t *out,
                                               size_t out_capacity,
                                               size_t *out_length);
dms_uart_status_t dms_uart_encode_camera_status(uint32_t sequence,
                                                const dms_uart_camera_status_t *status,
                                                uint8_t *out,
                                                size_t out_capacity,
                                                size_t *out_length);
dms_uart_status_t dms_uart_encode_model_unavailable(uint32_t sequence,
                                                    const dms_uart_model_unavailable_t *status,
                                                    uint8_t *out,
                                                    size_t out_capacity,
                                                    size_t *out_length);
dms_uart_status_t dms_uart_encode_heartbeat(uint32_t sequence,
                                            const dms_uart_heartbeat_t *heartbeat,
                                            uint8_t *out,
                                            size_t out_capacity,
                                            size_t *out_length);
dms_uart_status_t dms_uart_decode_model_result(const dms_uart_frame_t *frame,
                                               dms_uart_model_result_t *result);
dms_uart_status_t dms_uart_decode_model_unavailable(const dms_uart_frame_t *frame,
                                                    dms_uart_model_unavailable_t *status);

void dms_uart_parser_init(dms_uart_parser_t *parser);
void dms_uart_parser_feed(dms_uart_parser_t *parser,
                          const uint8_t *data,
                          size_t length,
                          uint32_t now_ms,
                          dms_uart_frame_callback_t callback,
                          void *context);
void dms_uart_parser_advance_time(dms_uart_parser_t *parser, uint32_t now_ms, uint32_t timeout_ms);

#endif /* DMS_UART_PROTOCOL_H */
