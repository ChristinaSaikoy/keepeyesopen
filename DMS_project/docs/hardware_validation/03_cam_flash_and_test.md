# ESP32-CAM Flash And Single-Board Test

Disconnect the S3 for this phase. This procedure verifies source configuration
and local logs only; it does not prove a UART receiver exists.

## Before Build

1. Complete the CAM inventory.
2. Keep `CAMERA_BOARD_PROFILE_USER_DEFINED` until the camera GPIO map is
   confirmed. With the default unset pins, camera initialization is expected to
   reject configuration safely.
3. After physical confirmation, update the profile or user-defined pins,
   record every change, and rebuild. Do not select the AI Thinker reference
   profile without evidence that the board matches it.
4. Choose the intended mode in `esp32_cam_fw/main/config.h`:
   `CAM_MODE_CAPTURE_TEST`, `CAM_MODE_UART_LINK_TEST`, or
   `CAM_MODE_MODEL_PIPELINE`. Capture and model modes need a confirmed camera.

## Build, Flash, And Monitor

```powershell
. D:\Espressif\tools\Microsoft.v5.3.5.PowerShell_profile.ps1
cd DMS_project\esp32_firmware\esp32_cam_fw
idf.py set-target esp32
idf.py build
idf.py -p <CAM_PORT> flash monitor
```

Save the complete build, flash, and monitor logs. Stop monitor with
`Ctrl+]`; do not submit only selected log lines.

## Expected Evidence

For a confirmed camera configuration, record:

- boot succeeds without repeated reset or Brownout;
- `camera initialized ... sensor PID=...`;
- PSRAM-related boot information from the full serial log;
- frame capture messages represented by `CAMERA_STATUS` traffic;
- periodic `HEARTBEAT` traffic;
- `MODEL_UNAVAILABLE` in model-pipeline mode because no real model is linked.

There must be **no `MODEL_RESULT`** in this gate. It would be a defect unless a
future real classifier and separately approved test plan are present.

For the default `USER_DEFINED` profile, record the configuration-rejection log
and status/model-unavailable behavior as a safe pre-hardware result, then stop
and resolve the board inventory before expecting frames.

## Record Failures

Record sensor PID mismatch, frame acquisition failure, repeated resets,
Brownout text, camera initialization failures, and the exact GPIO/profile used.
Do not repeatedly change wiring without recording the before/after state.
