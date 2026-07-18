# ESP32-CAM Resource Root Cause

## Current status

The G6 baseline is a clean ESP-IDF v5.3.5 build of
`esp32_firmware/esp32_cam_fw` at G5 commit
`f7977e7679c4e40a79983d8c81f3ef83f1d574ec`. It links successfully, but it
does not meet this release gate's resource targets: at least 10% free IRAM and
at least 15% free application-partition space.

| Resource | C0 baseline | Gate | Status |
| --- | ---: | ---: | --- |
| IRAM | 121,802 / 131,072 used; 9,270 free (7.07%) | >= 10% free | fail |
| DRAM | 45,232 / 180,736 used; 135,504 free | recorded | pass |
| Image | 948,715 bytes | recorded | build pass |
| Default factory app | `0xe7a60 / 0x100000`; 10% free | >= 15% free | fail |

The generated ESP-IDF default table is `nvs`, `phy_init`, and one 1 MiB
factory app. `sdkconfig.defaults` requests 4 MiB Flash, but that is a build
setting, not proof of the board's physical Flash. The board inventory is
therefore `UNKNOWN`, so no partition candidate is activated.

## Link and component findings

The direct application dependencies are all necessary:

| Dependency | Application evidence |
| --- | --- |
| `nvs_flash` | `nvs_flash_init` before STA setup |
| `esp_wifi` | STA init, config, connect, start |
| `esp_event` | Wi-Fi/IP event loop and handlers |
| `esp_http_server` | `/health` and multipart `/stream` handlers |
| `freertos` | event group and stream retry delay |
| `esp_timer` | per-frame capture timestamp |
| `espressif__esp32-camera` | camera init, latest frame fetch/return |

The source uses an OV2640 parallel camera configuration, not the ESP-IDF SPI
master/slave driver API. C4 nevertheless disabled both SPI ISR-in-IRAM options
and linked to the identical resource result, so it is not a useful candidate.
The full ESP-IDF component inventory contains many framework components; it is
not evidence that they can be removed from this application.

## G6 experiments

The exact C0-C6 figures are in `firmware_memory_experiment_matrix.md`; logs,
ELF, map, generated sdkconfig, binary, and size JSON are outside Git under
`handoff/g6_evidence/`.

- C1/C2/C3 each cross the IRAM free-space threshold. C3 is the strongest
  result: 34,506 bytes free (26.33%).
- C5 confirms that configuring both default and maximum log level as WARN
  reduces the image to 934,907 bytes. It still leaves only 11% free in the
  default 1 MiB app partition.
- None of the C0-C6 configurations changes the current partition capacity.

The selected source-level direction is C3 plus a capacity-confirmed P1
factory partition. C3 is **BUILD CANDIDATE — MJPEG RUNTIME VALIDATION
REQUIRED** because Wi-Fi code moves from IRAM to Flash. P1 is only a
conditional candidate, not a final partition selection.

## Runtime boundary

No ESP32-CAM board, USB serial adapter, Flash-ID output, boot log, camera PID,
PSRAM result, `/stream` client, Wi-Fi link, reset count, FPS, latency, power,
or image-quality measurement was available in this workspace. G6 therefore
does not claim a board-side result.
