# ESP32-S3 Flash And Single-Board Test

Disconnect the CAM for this phase. The default S3 mode is
`TEST_MODE_UART_PROTOCOL`, which runs the parser/state-machine loop and emits
diagnostic counters. With no CAM connected, UART timeout is expected and must
not generate a fatigue alert.

## Build, Flash, And Monitor

```powershell
. D:\Espressif\tools\Microsoft.v5.3.5.PowerShell_profile.ps1
cd DMS_project\esp32_firmware\esp32_s3_fw
idf.py set-target esp32s3
idf.py build
idf.py -p <S3_PORT> flash monitor
```

Save complete build, flash, and monitor logs. Record the exact board, port,
USB serial method, ESP-IDF version, and commit.

## Required Checks

- [ ] Boot log identifies `ESP32-S3 mode=2` or the intentionally changed mode.
- [ ] UART initialization succeeds.
- [ ] After no incoming frame for the configured timeout, diagnostics increment
  timeout without a `fatigue alert decision` line.
- [ ] No model-unavailable packet is treated as an alert when later received.
- [ ] Status LED behavior is recorded, including whether `STATUS_LED_PIN=48`
  matches the actual board.
- [ ] I2S, IR LED, and Wi-Fi/MQTT test modes remain disabled for this gate.

The S3 binary has only one byte of reported IRAM headroom
(`16383 / 16384`, 99.99%). A successful flash does not remove this release risk.

## Do Not Test Yet

Do not claim I2S audio, IR optical output, Wi-Fi, MQTT, fatigue alert hardware,
or a real classifier result. Those require separately confirmed board GPIOs,
credentials where applicable, and evidence.
