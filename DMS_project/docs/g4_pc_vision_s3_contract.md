# G4 PC vision to S3 contract

## Baseline and scope

This integration starts from `origin/architecture-dev` commit `46203c0483b461761af0556e5ae10f34bc336c34`. The G2/G3 reference branch was read at `32204543fe1e3af76cd983314a0c4832ae6d6053`; its state-machine behaviour was adapted instead of merging the UART architecture.

Before G4, the CAM published complete JPEG frames to MQTT, the PC could publish a final fatigue level, and the S3 looked for a `fatigue_level` string. That arrangement was replaced by the explicit contracts below.

## Vision observation

Topic: `dms/{device_id}/vision/observation`.

The JSON object has exactly nine fields:

```json
{
  "schema_version": 1,
  "device_id": "pc_dms_001",
  "sequence": 42,
  "source_timestamp_ms": 1716888888,
  "face_valid": true,
  "ear": 0.28,
  "mar": 0.15,
  "head_pitch": -2.5,
  "processing_time_ms": 31.4
}
```

`shared/dms_contract.py` rejects missing/extra fields, booleans in numeric slots, non-finite values and ranges outside the contract. The S3 counterpart is `esp32_firmware/esp32_s3_fw/main/dms_mqtt_contract.c`, function `dms_parse_vision_observation`. It parses via cJSON and validates both the exact topic and the exact object field count; no `strstr` parsing is used.

`sequence` is monotonic modulo uint32. The S3 drops duplicate and out-of-order observations. `source_timestamp_ms` is retained for observability; freshness uses S3 receive time because unsynchronised wall clocks must not create a false stale result. If no valid observation arrives before `DMS_OBSERVATION_TIMEOUT_MS`, link state becomes `stale` and the fatigue state machine receives an invalid observation.

## S3 state and alerts

State topic: `dms/{s3_id}/fatigue/state`.

Alert topic: `dms/{s3_id}/fatigue/alert`.

Both use the same exact fields: `schema_version`, `device_id`, `sequence`, `source_timestamp_ms`, `current_fatigue_level`, `alert_due`, `perclos`, `observation_valid`, `link_status`, `last_sequence`, `cause`.

`current_fatigue_level` is the actual decision. `alert_due` is separately rate-limited. Invalid input, MQTT disconnect and stale input produce no alert. The source is `dms_decision.c` plus `perclos.c`; both use unsigned subtraction for uint32 millisecond wraparound.

## Fixed local warning and cloud advice

The S3 MQTT callback only queues a fixed local clip. `audio_task` owns I2S writing so audio does not block the MQTT callback or state decision. Five generated clip headers remain only in the active S3 component. Duplicate legacy and tool copies were removed. The large arrays remain compiled into the firmware image; placement and resource headroom require review from the final linker map.

The cloud process consumes structured S3 alerts. `generate_advice` runs the request off the event loop with a 2.5 second default timeout; all timeout, request and configuration failures publish the fixed fallback advice. Advice is published to `dms/{s3_id}/ai/advice`. The PC may read this topic and use a separate optional TTS queue, but the S3 does not download dynamic audio and the LLM is never the first safety warning.

## Command restriction

Cloud commands use `dms/{s3_id}/command` and exactly: `schema_version`, `device_id`, `command_id`, `action`, `enabled`. The only allowed action is `set_status_led`; S3 acknowledges to `dms/{s3_id}/command/ack`. This is deliberately a non-safety-critical whitelist, not an arbitrary GPIO or LLM control interface.

## Latency instrumentation

The CAM includes capture timestamps in MJPEG part headers. The PC uses a queue of maximum one frame, replaces old frames and tracks capture-to-receive, MediaPipe and receive-to-publish stages. It reports count, average, P50, P95, maximum and drop rate while running. No runtime sample was collected in this source-only gate, so there are no claimed latency values.

## Unverified items

No board execution was performed. Board type/pins, camera and PSRAM operation, HTTP/MJPEG image compatibility, Wi-Fi/MQTT operation, audio, measured latency/FPS, model accuracy, power, IMU fusion and real LLM service behaviour remain unverified.
