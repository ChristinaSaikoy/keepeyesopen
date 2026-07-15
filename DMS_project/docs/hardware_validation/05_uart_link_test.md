# UART Physical Link Test

Complete both single-board tests and the wiring checklist first.

## Level 1: Heartbeat Transport

1. Configure CAM as `CAM_MODE_UART_LINK_TEST`. This mode sends only framed
   heartbeat messages at the configured interval and does not require a camera.
2. Keep S3 as `TEST_MODE_UART_PROTOCOL`.
3. Wire CAM TX to S3 RX and connect common ground. Confirm 3.3 V logic.
4. Flash both boards, collect both serial logs, and run continuously for at
   least 10 minutes.
5. From the S3 one-second diagnostics, record `frames`, `crc`, `gaps`, and
   `timeout`. Sequence values should increase modulo `uint32_t`; a gap is a
   diagnostic finding, not a result to hide.

## Pass Evidence, Not Pass Claims

The evidence record must include:

| Metric | Start | End | Difference / observation |
| --- | --- | --- | --- |
| Total received frames |  |  |  |
| Valid frames |  |  |  |
| CRC errors |  |  |  |
| Sequence gaps |  |  |  |
| Parser timeouts |  |  |  |
| Resynchronization events |  |  |  |
| Duration (minutes) |  |  | at least 10 |

No `fatigue alert decision` line is expected during heartbeat-only traffic.
If it appears, stop and attach the full log; do not continue as a pass.

## Failure Boundaries

- No S3 frames: check ground, CAM TX/S3 RX direction, baud rate, selected GPIO,
  USB/UART GPIO conflict, and CAM UART initialization log.
- High CRC or gaps: stop after recording counts; inspect voltage level, ground,
  baud mismatch, cable length, and electrical noise before retrying.
- Timeout after initially valid frames: record the time, reset behavior, and
  both-board logs. Do not silently reset counters.
