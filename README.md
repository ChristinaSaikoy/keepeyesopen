# KeepEyesOpen

**Edge / cloud driver-monitoring system prototype built around ESP32-class hardware.**

This repository contains a team project for the National College Student IoT Design Contest (Espressif track). The system explores a split embedded architecture for driver-fatigue monitoring: camera-side acquisition, device-side fatigue logic, local alerts, MQTT telemetry, and a cloud/dashboard path.

> **Project status:** prototype / in progress. The repository contains working subsystem implementations, but the complete camera-to-landmark-to-S3 closed loop is not yet integrated end to end.

## System Architecture

```text
Camera / driver scene
        |
        v
ESP32 camera node
  - camera capture
  - UART transport
  - face-landmark stage [in progress]
        |
        v
ESP32-S3 compute node
  - EAR / MAR geometry
  - duration-based fatigue state machine
  - I2S local alert
  - IR / status outputs
  - MQTT telemetry
        |
        v
MQTT broker
        |
        v
PC / cloud backend
  - event handling
  - optional DeepSeek response generation
  - local fallback responses
  - TTS
  - WebSocket dashboard
```

## What Is Implemented

| Subsystem | Current state |
| --- | --- |
| Camera initialization and frame capture | Implemented |
| Camera-to-S3 UART transport scaffold | Implemented / placeholder payload |
| 68-point landmark extraction on camera node | In progress |
| EAR / MAR geometry in C | Implemented |
| Eye-closure / yawn duration state machine | Implemented |
| ESP32-S3 UART, I2S, IR LED, Wi-Fi and MQTT subsystem tests | Implemented |
| Fully integrated S3 processing mode | In progress |
| MQTT -> backend -> WebSocket dashboard path | Implemented in prototype code |
| Optional LLM-generated warning text and local TTS | Implemented in prototype code |
| Full hardware end-to-end validation | Pending |

The current firmware source makes these boundaries explicit: the camera node still marks face-landmark extraction as a TODO, while the S3 firmware currently exposes subsystem test modes rather than a completed integrated mode.

## My Contribution

This is a **team project**, not a solo project. My work is concentrated on the embedded fatigue-analysis path:

- C implementation of **EAR (Eye Aspect Ratio)** and **MAR (Mouth Aspect Ratio)** geometry
- Device-side eye-closure / yawn timing logic in `perclos.c`
- Integration design for the camera-landmark -> embedded fatigue-state pipeline
- Algorithm-side work for the ESP32-S3 processing path

Other team members worked on the PC prototype, hardware integration, and cloud/backend components. See [`DMS_project/README.md`](DMS_project/README.md) for the detailed responsibility split.

## Repository Layout

```text
keepeyesopen/
└── DMS_project/
    ├── esp32_firmware/
    │   ├── esp32_cam_fw/      # camera node: capture + UART scaffold
    │   └── esp32_s3_fw/       # compute node: algorithms + peripheral tests
    ├── pc_prototype/           # PC-side vision / logic prototype
    ├── cloud_backend/          # MQTT + WebSocket + optional LLM/TTS
    ├── protocol.json           # telemetry format
    └── README.md               # detailed project notes and team roles
```

## Embedded Algorithm Core

The S3-side algorithm code separates geometry from temporal fatigue logic:

```text
68 facial landmarks
        |
        v
EAR / MAR geometry
        |
        v
threshold + duration state
        |
        +--> micro-sleep / prolonged eye closure
        +--> yawn event
        |
        v
local alert + telemetry
```

`ear_mar.c` computes eye and mouth ratios from 68-point landmarks. `perclos.c` currently implements **duration-based eye-closure and yawn detection**. Despite the historical filename, it is not yet a full sliding-window PERCLOS-percentage implementation; that distinction is kept explicit here for technical accuracy.

## Hardware / Software Stack

`ESP32-S3` · `ESP-IDF` · `C` · `FreeRTOS` · `UART` · `I2S` · `MQTT` · `Python` · `WebSocket` · `MediaPipe prototype`

## Current Limitations

- Camera-side 68-point landmark extraction is not yet integrated in the checked-in firmware.
- The current UART camera payload is a transport placeholder rather than the final landmark packet format.
- The S3 `TEST_MODE=5` path is not yet a complete integrated processing loop.
- Thresholds are prototype parameters and have not been presented here as clinically or statistically validated values.
- End-to-end latency, accuracy, power and long-duration robustness benchmarks are not yet available as reproducible public results.

## Next Engineering Steps

1. Integrate and validate camera-side facial-landmark extraction.
2. Freeze a compact UART landmark/event packet format.
3. Complete the S3 integrated processing loop.
4. Add host-side unit tests for EAR/MAR and fatigue-state transitions.
5. Record reproducible board-level latency and robustness measurements.
6. Add an architecture diagram and hardware validation evidence once the full chain is stable.

## Detailed Notes

See [`DMS_project/README.md`](DMS_project/README.md) for hardware paths, data protocol, team responsibilities, and development notes.
