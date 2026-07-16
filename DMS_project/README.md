# Driver Monitoring System

This branch uses four separated responsibilities:

| Component | Responsibility | Safety boundary |
| --- | --- | --- |
| ESP32-CAM | Capture JPEG and expose a LAN MJPEG stream | No MQTT JPEG payload and no fatigue decision |
| PC | MediaPipe vision observation: face validity, EAR, MAR and head-pitch proxy | Publishes observations only; it does not publish a final fatigue level |
| ESP32-S3 | Validate observations, run the fatigue state machine, play a fixed local warning and publish structured state | The real risk level is independent of alert cooldown |
| Cloud and dashboard | Async LLM advice, dashboard delivery and a whitelisted configuration command path | LLM output never controls GPIO or vehicle actions |

## Data flow

```
ESP32-CAM /stream (MJPEG) -> PC MediaPipe -> dms/{pc_id}/vision/observation
                                            -> ESP32-S3 fatigue decision
                                            -> dms/{s3_id}/fatigue/state and /alert
                                            -> Cloud async advice -> dms/{s3_id}/ai/advice
```

The observation schema is versioned and strictly validated by both the Python producer/consumer and the S3 firmware. See `protocol.json` and `docs/g4_pc_vision_s3_contract.md`.

## Local setup

1. Copy `.env.example` to `.env` and fill local broker, stream and LLM values. Do not commit it.
2. For each firmware, copy `main/dms_secrets.h.example` to ignored `main/dms_secrets.h` and fill Wi-Fi settings.
3. Run `pc_prototype/cam_bridge.py` with `CAM_STREAM_URL` pointing at the CAM `/stream` endpoint. It uses a latest-frame queue of one and reports frame drops plus PC-side timing summaries.
4. Run `cloud_backend/web_backend.py` only after MQTT and its Python dependencies are available.

`CAM_FRAME_PROFILE=qvga` is the default low-latency profile. VGA is configurable, but no end-to-end latency, FPS, accuracy, power, or RAM claim is made until hardware measurements are recorded.

## S3 decision semantics

The S3 state machine keeps separate values for continuous eye closure, continuous yawn, fixed-memory time-window PERCLOS, alert cooldown and the current fatigue level. Invalid, stale, duplicate or out-of-order observations cannot trigger an alert. Priority is deep eye closure, micro-sleep, PERCLOS, then yawn. Recovery requires open-eye and closed-mouth observations; cooldown only suppresses a repeated alert, not the actual risk level.

The only cloud command currently accepted is `set_status_led` on `dms/{s3_id}/command`, with a schema-validated command ID and a structured acknowledgement. It is intentionally not a general GPIO API.

## Validation scope

Host tests cover the decision-state transitions, PERCLOS window, observation ordering, stale input, cooldown separation, schema validation, LLM timeout fallback and command whitelist. Firmware builds are source-level checks only. They do not verify a board, camera pin map, PSRAM, UART, audio hardware, Wi-Fi/MQTT link, image quality or measured performance.

## ESP-WHO status

ESP-WHO is not part of this architecture. The prior source review found no supported ESP-WHO dlib-68 landmark contract for the intended ESP32-S3 route. PC MediaPipe supplies the visual observation while the S3 remains the fatigue decision authority.

## Security

Only placeholder values are tracked. Rotate any Wi-Fi password that was previously committed or shared outside the intended secret store.
