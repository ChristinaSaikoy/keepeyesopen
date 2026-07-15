# Camera Status Packet Test

This test begins only after the heartbeat transport test has a complete
10-minute record. It does not test classifier accuracy because no classifier
exists in this firmware.

## Preconditions

- Confirmed CAM profile, camera GPIO map, sensor connection, and power.
- CAM runs `CAM_MODE_MODEL_PIPELINE`.
- S3 runs `TEST_MODE_UART_PROTOCOL`.
- Both logs and wiring photographs are being saved.

## Required Observations

From CAM and S3 logs, demonstrate that the S3 distinguishes these types:

| Packet | Required source condition | Required S3 behavior |
| --- | --- | --- |
| `CAMERA_STATUS` | init/capture status from CAM | count/parse only; no fatigue alert |
| `HEARTBEAT` | periodic CAM link liveness | count/parse only; no fatigue alert |
| `MODEL_UNAVAILABLE` | current adapter has no model | increment unavailable diagnostic; no fatigue alert |

The S3 log includes `frames`, `crc`, `gaps`, `timeout`, `model`, and
`unavailable` counters. Save enough unedited log to link observed CAM output to
these counters. A status-only run must have `model=0` and no fatigue alert.

## Synthetic Model Results Are Not Part Of This Test

Do not send a hand-crafted `MODEL_RESULT` through the normal build. The current
project has no `PROTOCOL_TEST_MODE`. If a later protocol-only test is approved,
it must add a build-disabled explicit mode, print `SYNTHETIC TEST DATA` on both
serial streams, and label every result as protocol/state-machine evidence only,
not facial-model or fatigue-detection evidence.
