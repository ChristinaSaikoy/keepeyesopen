# Dual-Board Driver Monitoring System

This repository uses a two-board boundary. It contains source and host-test
evidence only; it does not claim a completed camera, UART, alarm, MQTT, or
model run on physical hardware.

```text
ESP32-CAM                         ESP32-S3
camera capture                    UART stream parser
future classifier                 confidence gates and fatigue state machine
status / model result UART  --->  fixed-memory PERCLOS and local alert decision
                                 optional I2S, IR LED, Wi-Fi/MQTT test modes
```

## Layout

```text
esp32_firmware/
  common/                 Shared framed UART protocol and CRC parser
  esp32_cam_fw/           ESP32 camera capture and future model adapter
  esp32_s3_fw/            ESP32-S3 receiver and G2 fatigue logic
tests/
  host_algorithm/         State-machine regression tests
  host_uart_protocol/     Serialization and stream-parser tests
  host_integration/       UART-to-state-machine simulation
docs/
  dual_board_architecture.md
  dual_board_uart_protocol.md
  g2_to_dual_board_migration_map.md
```

## Model Boundary

The CAM firmware currently has no real classifier. In model-pipeline mode it
sends `CAMERA_STATUS`, `HEARTBEAT`, and `MODEL_UNAVAILABLE`; it does not send
fixed probabilities, random values, image prefixes, fake landmarks, or
`MODEL_RESULT`.

Official ESP-WHO human-face detection's five-point result cannot directly
calculate eyelid distance or mouth opening for 68-point EAR/MAR. The active
dual-board path therefore accepts a documented classifier probability contract,
not a fictional landmark API. Legacy EAR/MAR/dlib mapping is not part of either
firmware's default build.

## UART Wiring

`USER ACTION REQUIRED`: confirm the real board profiles and GPIOs before
connecting hardware. The intended one-way link is CAM `CAM_UART_TX_PIN` to S3
`DMS_UART_RX_PIN`, with a shared ground. S3 TX is only a reserved debug pin;
the current protocol needs no return channel. See
`docs/dual_board_uart_protocol.md` for framing and error behavior.

The CAM default profile is `CAMERA_BOARD_PROFILE_USER_DEFINED`, which rejects
camera initialization until a hardware teammate confirms every camera GPIO.
The AI-Thinker profile is a reference map only and is not board-verified here.

## Build

Build each firmware independently. Do not share `sdkconfig` or build folders.

```powershell
cd DMS_project/esp32_firmware/esp32_cam_fw
idf.py set-target esp32
idf.py fullclean
idf.py reconfigure
idf.py build
idf.py size
```

```powershell
cd DMS_project/esp32_firmware/esp32_s3_fw
idf.py set-target esp32s3
idf.py fullclean
idf.py reconfigure
idf.py build
idf.py size
idf.py size-components
idf.py size-files
```

CAM resolves `espressif/esp32-camera ==2.1.4` through the ESP-IDF Component
Manager. S3 intentionally has no `esp_camera` dependency.

## Configuration And Tests

The repository keeps only placeholders. Cloud `.env` values live in a local
ignored `.env`; S3 firmware can use an ignored local `dms_secrets.h` copied
from `esp32_s3_fw/main/dms_secrets.example.h`. Placeholder credentials cause a
clear Wi-Fi/MQTT test-mode error rather than a connection attempt.

S3 `TEST_MODE` values are raw UART link, protocol parser, I2S, IR LED,
Wi-Fi/MQTT, and full protocol/state-machine mode. Mode 6 parses framed UART
data and feeds the G2 state machine; it is not a raw-receive loop.

Host tests cover G2 microsleep, deep closure, recovery, yawn, priority,
PERCLOS, invalid observations, cooldown, timestamp wrap, classifier
hysteresis, UART fragmentation, concatenation, garbage, CRC, timeout, and an
end-to-end simulated CAM-to-S3 path. They do not substitute for board evidence.
