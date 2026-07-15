# Dual-Board Hardware Validation Package

## Status

```text
SOURCE/BUILD READY
HARDWARE VALIDATION PENDING
```

This package is for a hardware teammate to collect reproducible evidence for
the ESP32-CAM to ESP32-S3 path. It is not evidence that either board has been
flashed, captured a frame, exchanged UART data, or raised an alert.

Use this package in order:

1. Complete `01_board_inventory.md` with photographs and mark every unknown
   field as `UNKNOWN`.
2. Complete `02_wiring_checklist.md` before connecting the two boards.
3. Flash and record each board independently with `03_...` and `04_...`.
4. Run the UART heartbeat link test for at least ten minutes using `05_...`.
5. Run the semantic status-packet test using `06_...`.
6. Submit logs, photos, statistics, and failures using `07_...`.

Read `08_troubleshooting.md` before changing wiring or GPIO definitions.

## Repository And Firmware Boundary

Use the exact commit written in the evidence record. The intended direction is:

```text
ESP32-CAM CAM_UART_TX_PIN -> ESP32-S3 DMS_UART_RX_PIN
ESP32-CAM GND             -> ESP32-S3 GND
```

The CAM sends framed camera status, heartbeat, and model-unavailable messages.
It has no real classifier, so `MODEL_RESULT` must not appear in normal testing.
The S3 accepts only CRC-valid frames and must not alarm on status, heartbeat,
model-unavailable, CRC errors, or UART timeout.

`PROTOCOL_TEST_MODE` is not included in the current shipped firmware. Do not
inject synthetic `MODEL_RESULT` packets into a normal demo build. Any future
protocol-only test mode must be disabled by default, visibly print
`SYNTHETIC TEST DATA` on UART, and have separately labelled evidence.

## Build Environment

The latest local build evidence uses ESP-IDF v5.3.5. Build CAM and S3 in
separate directories; do not share `sdkconfig` or `build` folders. ESP-IDF
v5.5.2 has not yet been validated for this repository.

For standard Windows ESP-IDF shells, avoid a path containing Chinese characters
if CMake, Ninja, or Kconfig fails before compilation. Copy or create an
ASCII-path worktree for the build and record that path in the evidence template.

## Safety Rules

- Confirm both UART sides are 3.3 V logic. Never connect a 5 V UART signal
  directly to an ESP GPIO.
- Power and test each board alone before connecting UART.
- Two boards must have a common ground before UART traffic is expected.
- Current GPIO values are software defaults, not hardware facts. Resolve a
  GPIO conflict in configuration and record it; do not rely on an undocumented
  temporary jumper change.
- Leave I2S, IR LED, Wi-Fi, and MQTT modes disabled for this gate.
