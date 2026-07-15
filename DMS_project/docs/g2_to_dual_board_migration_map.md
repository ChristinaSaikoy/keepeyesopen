# G2 To Dual-Board Migration Map

## Bases

- Dual-board mainline: `463e54bfb741f6941e8aa1b80b48ccf175cc5de2`.
- Verified algorithm source: `6f8c44b10337b12df763efc0771b41fc9d139e61`.
- This work does not cherry-pick the old single-board directory.

## Migration Decisions

| G2 source | Dual-board destination | Decision | Verification |
| --- | --- | --- | --- |
| `main/perclos.c,h` | `esp32_s3_fw/main/perclos.c,h` | Migrate semantic observation, hysteresis, recovery, fixed-memory PERCLOS, risk priority, cooldown, and timestamp-wrap logic. | Host algorithm and integration CTest. |
| G2 classifier thresholds in `main/config.h` | `esp32_s3_fw/main/config.h` | Migrate with units and confidence semantics. | Host classifier-hysteresis cases. |
| `main/model_adapter.h,c` | CAM model adapter plus `common/dms_uart_protocol.h` | Preserve the unavailable-model behavior but replace local floating output with Q0.10000 UART serialization at the board boundary. | Protocol encode/decode tests and CAM build. |
| G2 `ear_mar.c,h` and `dlib68_adapter.c,h` | Not in active dual-board firmware | Keep only as historical/test reference; official ESP-WHO five-point output cannot satisfy it. | Active firmware search has no `landmarks[68]` or dlib dependency. |
| G2 camera config/test | Not migrated to S3 | Camera ownership moves to `esp32_cam_fw`; its board profile remains unverified until hardware confirmation. | CAM build; no board claim. |

## Replacements Required On The Dual-Board Mainline

- Replace `esp32_s3_fw/main/ear_mar.*` and its 68-point call path with the
  UART `MODEL_RESULT` to semantic-observation pipeline.
- Replace the simplified S3 `perclos.*`, which lacks valid-observation
  accounting, recovery, windowed PERCLOS, and cooldown separation.
- Remove S3 `esp_camera` dependency.
- Replace CAM grayscale-prefix UART output and commented 68-point API with
  framed status, heartbeat, model-unavailable, and future real-model output.
- Replace hard-coded Wi-Fi/MQTT values with placeholders plus ignored local
  secrets; placeholder values fail clearly at startup.

## Dependency And Configuration Conflicts

- CAM targets `esp32` and needs `espressif/esp32-camera`; S3 targets `esp32s3`
  and must not depend on that component.
- Each firmware project has an independent build directory and `sdkconfig`.
- G2's former camera pins were S3 placeholders and cannot be reused for the
  ESP32-CAM board. The CAM default is `USER_DEFINED` until a hardware teammate
  confirms the exact board and wiring.
- The shared protocol has no ESP-IDF dependency, allowing host tests to verify
  serialization and parser behavior.
