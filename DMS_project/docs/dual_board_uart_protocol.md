# Dual-Board UART Protocol

## Scope

The ESP32-CAM is the producer and ESP32-S3 is the consumer. Both compile
`esp32_firmware/common/dms_uart_protocol.c`; neither endpoint writes an
in-memory C structure directly to UART.

## Frame Format

All multi-byte values are unsigned big-endian.

| Field | Size | Value |
| --- | ---: | --- |
| magic | 2 bytes | `0xD5 0x4D` |
| version | 1 byte | `1` |
| message type | 1 byte | one of the values below |
| payload length | 2 bytes | `0..64` |
| sequence | 4 bytes | monotonically incrementing modulo `uint32_t` |
| payload | 0..64 bytes | type-specific bytes |
| CRC16 | 2 bytes | CRC over version through payload |

CRC is CRC-16/CCITT-FALSE: polynomial `0x1021`, initial `0xFFFF`, no input or
output reflection, final XOR `0x0000`. The CRC field is big-endian.

## Message Types

| Type | Value | Meaning |
| --- | ---: | --- |
| `DMS_MSG_CAMERA_STATUS` | 1 | Camera init/capture state and sensor metadata. |
| `DMS_MSG_MODEL_RESULT` | 2 | A real classifier completed valid inference. |
| `DMS_MSG_MODEL_UNAVAILABLE` | 3 | No classifier exists, initialization failed, or inference cannot run. |
| `DMS_MSG_HEARTBEAT` | 4 | Link liveness only; not an algorithm result. |

`MODEL_RESULT` has a 16-byte payload:

| Field | Size | Encoding |
| --- | ---: | --- |
| source timestamp | 4 | milliseconds, `uint32_t` wrap allowed |
| face valid probability | 2 | Q0.10000, inclusive `0..10000` |
| eye closed probability | 2 | Q0.10000, inclusive `0..10000` |
| yawn probability | 2 | Q0.10000, inclusive `0..10000` |
| inference time | 4 | microseconds |
| status flags | 2 | classifier-defined documented flags |

The frame header supplies the required sequence. The S3 maps valid Q0.10000
probabilities to `[0,1]`, then applies its configured face-valid and
start/recovery thresholds. Floating-point structs never cross the UART link.

## Parser Rules

`dms_uart_parser_feed` accepts arbitrary read chunks. It handles fragmented
frames, multiple frames per read, leading garbage, invalid magic, wrong
version, unknown type, overlength payload, CRC failure, sequence gaps, and
timeout. The caller invokes `dms_uart_parser_advance_time`; unsigned timestamp
subtraction handles `uint32_t` wrap.

CRC-invalid frames never invoke the callback and therefore cannot update the
S3 state machine. Sequence jumps are retained as diagnostics, not silently
rewritten. `MODEL_UNAVAILABLE`, heartbeat, camera status, parser timeout, and
expired model-result data all mark the fatigue observation invalid and cannot
trigger an alarm.

## Physical Link

`USER ACTION REQUIRED`: confirm CAM TX, S3 RX, voltage levels, and common
ground on the actual boards. The code contains no evidence that this link has
been wired or tested on hardware.
