# Hardware Evidence Template

Copy this template for each test run. Attach complete logs and original photos;
do not replace them with a statement such as "test passed".

## Identity

| Field | Value |
| --- | --- |
| Date/time and timezone | `UNKNOWN` |
| Test person | `UNKNOWN` |
| Repository / branch | `UNKNOWN` |
| Git commit | `UNKNOWN` |
| ESP-IDF version | `UNKNOWN` |
| Build path | `UNKNOWN` |
| CAM target / port | `UNKNOWN` |
| S3 target / port | `UNKNOWN` |
| Test duration | `UNKNOWN` |

## Hardware And Wiring

| Item | File / value |
| --- | --- |
| CAM board front/back photo | `UNKNOWN` |
| S3 board front/back photo | `UNKNOWN` |
| Camera / PSRAM marking photo | `UNKNOWN` |
| Pinout or schematic source | `UNKNOWN` |
| Wiring photo | `UNKNOWN` |
| CAM TX GPIO | `UNKNOWN` |
| S3 RX GPIO | `UNKNOWN` |
| Common-ground confirmation | `UNKNOWN` |
| Changed GPIOs and reason | `NONE / UNKNOWN` |
| Rebuilt after configuration change | `YES / NO / UNKNOWN` |

## Artifacts

| Artifact | Filename / link |
| --- | --- |
| CAM build/flash log | `UNKNOWN` |
| CAM complete serial log | `UNKNOWN` |
| S3 build/flash log | `UNKNOWN` |
| S3 complete serial log | `UNKNOWN` |
| `CAMERA_STATUS` evidence | `UNKNOWN` |
| `HEARTBEAT` evidence | `UNKNOWN` |
| `MODEL_UNAVAILABLE` evidence | `UNKNOWN` |
| Confirmation of no normal-build `MODEL_RESULT` | `UNKNOWN` |

## UART Statistics

| Metric | Value |
| --- | --- |
| Total frames | `UNKNOWN` |
| Valid frames | `UNKNOWN` |
| CRC errors | `UNKNOWN` |
| Sequence gaps / lost frames | `UNKNOWN` |
| Parser timeouts | `UNKNOWN` |
| Resynchronization events | `UNKNOWN` |
| Fatigue alert occurred | `YES / NO / UNKNOWN` |

## Result And Problems

- Test level: `CAM single board / S3 single board / UART level 1 / status level 2`.
- Exact observed result: `UNKNOWN`.
- Failures, resets, Brownout, PID mismatch, PSRAM result, or unexpected alert:
  `UNKNOWN`.
- Changes made during the run: `NONE / UNKNOWN`.
- Reviewer decision: `PENDING`.
