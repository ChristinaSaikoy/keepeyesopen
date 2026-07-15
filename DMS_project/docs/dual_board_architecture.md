# Dual-Board Architecture

## Responsibility Boundary

```text
ESP32-CAM
  board-profile validation -> controlled camera capture -> future real classifier
  -> shared UART frame serializer

ESP32-S3
  UART bytes -> shared stream parser -> CRC/range validation -> MODEL_RESULT
  -> classifier semantic observation -> G2 fatigue state machine -> alert decision
```

The CAM owns `esp32-camera`; the S3 has no camera component dependency. The
shared protocol component has no ESP-IDF dependency and is exercised by host
tests.

## Current Model State

No model artifact or real classifier is included. The CAM adapter returns
`DMS_MODEL_UNAVAILABLE`; `MODEL_RESULT` is emitted only by a future adapter
that returns `DMS_MODEL_OK` and supplies finite probabilities in range. This
project makes no 68-point ESP-WHO claim. The G1 source review established that
the relevant official detector result is five points, insufficient for EAR/MAR
eyelid/lip geometry.

## S3 Fatigue Rules

Only a valid protocol `MODEL_RESULT` creates a usable observation. Face-valid,
eye-closed, and yawn probabilities use start/recovery hysteresis. The state
machine retains G2's fixed-memory time-window PERCLOS, valid-observation-only
denominator, microsleep/deep-closure thresholds, yawn recovery, deterministic
priority, alert cooldown separation, and `uint32_t` time-wrap arithmetic.

No new model data for the timeout interval clears continuous-event timing and
excludes the gap from PERCLOS while retaining parser/link diagnostics. Alert
cooldown does not downgrade the actual fatigue level.

## Hardware Boundaries

Camera profile, all camera GPIOs, CAM-to-S3 UART wiring, PSRAM behavior, I2S,
IR LED, Wi-Fi, MQTT, capture rate, memory use, and alarm output are all
`USER ACTION REQUIRED` until real board logs exist. The AI-Thinker profile is
a source-level reference guarded by conflict detection, not a board validation.
