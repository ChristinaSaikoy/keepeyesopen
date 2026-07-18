# Firmware Memory Experiment Matrix

All S3 rows use ESP-IDF v5.3.5. `IRAM-only` is the `idf.py size` 16 KiB
classification; the actual `iram0_0_seg` linker region is 358,144 bytes and
passes the 4 KiB/20% budget in every successful row.

| ID | Change | IRAM-only | DIRAM used/free | Flash code/data | BIN / build | Candidate | Runtime validation |
| --- | --- | --- | --- | --- | --- | --- | --- |
| A0 | Exact G4 baseline | 16,383/16,384 | 49,395 / 292,365 | 121,370 / 711,252 | 893,504 / pass | chosen | Wi-Fi/MQTT required |
| A1 | Default log WARN | unchanged; generated config initially retained INFO | unchanged | unchanged | pass, invalid comparison | no | n/a |
| A2 | SPI master/slave ISR IRAM off | 16,383/16,384 | 48,571 / 293,189 | 122,138 / 711,060 | 893,520 / pass | no | static review plus board if SPI is added |
| A3 | Heap functions in Flash | 16,383/16,384 | 41,671 / 300,089 | 127,798 / 711,920 | 893,136 / pass | no | required: heap/I2S/cache-disabled runtime |
| B1 | Wi-Fi general IRAM on, RX off | unchanged | unchanged | unchanged | 893,504 / pass | no | comparison only |
| B2 | Wi-Fi general off, RX on | unchanged | unchanged | unchanged | 893,504 / pass | no | comparison only |
| B3 | Wi-Fi general and RX off | same exact configuration as A0 | same as A0 | same as A0 | A0 artifact reused | chosen | required |

The Kconfig help for ESP-IDF 5.3.5 states that disabling Wi-Fi general and RX
IRAM optimizations normally saves IRAM at throughput cost. The present
application's final link set did not select a measurable additional Wi-Fi
section in B1/B2; no network performance inference is made.

CAM A0 baseline: IRAM 121,802/131,072 (9,270 free, 7.07%), DRAM 45,232/180,736,
Flash code/data 651,265/147,608, total image 948,715 bytes, app partition
`0xe7a60/0x100000` (10% free), build passed. It is not a release candidate
against the CAM 10% IRAM and 15% partition-free thresholds.

## G6 CAM experiments

All G6 CAM rows are independent clean ESP-IDF v5.3.5 builds from G5 commit
`f7977e7679c4e40a79983d8c81f3ef83f1d574ec`. Build artifacts and logs are
retained outside Git in `handoff/g6_evidence/`.

| ID | Change | IRAM used/free | Image bytes | 1 MiB app free | Build conclusion |
| --- | --- | --- | ---: | --- | --- |
| C0 | exact baseline | 121,802 / 9,270 (7.07%) | 948,715 | 10% | baseline fails both targets |
| C1 | Wi-Fi RX IRAM off | 108,706 / 22,366 (17.06%) | 948,363 | 10% | build candidate; MJPEG runtime required |
| C2 | Wi-Fi general IRAM off | 109,742 / 21,330 (16.27%) | 948,183 | 10% | build candidate; MJPEG runtime required |
| C3 | both Wi-Fi IRAM options off | 96,566 / 34,506 (26.33%) | 947,703 | 10% | best IRAM result; MJPEG runtime required |
| C4 | SPI master/slave ISR IRAM off | 121,802 / 9,270 (7.07%) | 948,715 | 10% | no measured link benefit |
| C5 | default and maximum log level WARN | 121,514 / 9,558 (7.29%) | 934,907 | 11% | effective image reduction; still fails targets |
| C6 | dependency-removal audit | 121,802 / 9,270 (7.07%) | 948,715 | 10% | no safe direct dependency can be removed |

C1-C3 move Wi-Fi execution from IRAM to Flash. Without a connected board,
they are **BUILD CANDIDATE — MJPEG RUNTIME VALIDATION REQUIRED**, not a
throughput, stability, latency, or camera claim. C6 reviewed each direct
`main/CMakeLists.txt` dependency: `nvs_flash`, `esp_wifi`, `esp_event`,
`esp_http_server`, `freertos`, `esp_timer`, and `espressif__esp32-camera` all
have an application call site. Removing one would remove required startup,
stream, timing, task, or camera behaviour.

Evidence directories under `handoff/g5_evidence/` retain logs and generated
ELF/map/sdkconfig files for A0, A2, A3, B1, B2 and CAM A0. A1 is retained as
an explicitly invalid defaults-precedence trial rather than optimization
evidence.
