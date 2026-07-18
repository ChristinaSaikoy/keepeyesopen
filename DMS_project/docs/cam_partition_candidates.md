# ESP32-CAM Partition Candidates

## Decision gate

**Flash is UNKNOWN. No final partition is selected.** The configured 4 MiB
Flash value in `sdkconfig.defaults` is not a hardware inventory result and
must not be used to infer the installed module capacity.

Before selecting a candidate, record the CAM product/model and markings, chip
marking, camera module/sensor, Flash result, PSRAM result, board photograph,
link, boot log, and flash log. Also record the S3 board/module, Flash/PSRAM,
pinout, and photograph. The exact collection and runtime procedure is in
`cam_board_validation_protocol.md`.

## Baseline P0

ESP-IDF's current generated table is `nvs` (24 KiB), `phy_init` (4 KiB), and
one factory app partition of 1 MiB. C0 uses 948,715 bytes, leaving 10% free.
P0 is a reproducible baseline only; it does not pass the 15% app-space gate.

## Candidates

| Plan | File | Capacity assumption | App arrangement | C0 headroom implication | Intended use |
| --- | --- | --- | --- | --- | --- |
| P1 | `partitions/g6_factory_4mb.csv` | verified Flash >= 4 MiB | one 3 MiB factory app | about 69.8% free | first board integration candidate |
| P2 | `partitions/g6_ota_8mb.csv` | verified Flash >= 8 MiB | two 3 MiB OTA apps | about 69.8% free per slot | future OTA only |
| P3 | `partitions/g6_factory_4mb_diagnostics.csv` | verified Flash >= 4 MiB | one 2.5 MiB factory app plus 256 KiB diagnostic reservation | about 63.8% free | diagnostics experiment only |

The headroom calculations use the C0 image size and do not include a future
feature increase. P3 reserves SPIFFS space but does not enable, mount, or
write SPIFFS in this task. It is not a data-retention feature.

## Priority and activation

1. Verify hardware inventory and Flash capacity using the board's own flash
   output.
2. If the board reports at least 4 MiB and OTA is not required, choose P1 for
   the first integration test. This is the priority candidate because it has
   the smallest product-scope change.
3. Use P2 only after confirming at least 8 MiB and an actual OTA requirement.
4. Use P3 only when diagnostic storage has a defined owner and retention
   policy.

Candidate CSV files are intentionally not referenced by the active CMake or
sdkconfig configuration. Activating one requires a clean build, flash-size
check, boot/PSRAM/camera verification, and the MJPEG runtime record. This
separation prevents a guessed partition table from being flashed to an
unknown board.
