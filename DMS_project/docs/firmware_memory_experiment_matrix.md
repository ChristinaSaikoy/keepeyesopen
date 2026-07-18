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

Evidence directories under `handoff/g5_evidence/` retain logs and generated
ELF/map/sdkconfig files for A0, A2, A3, B1, B2 and CAM A0. A1 is retained as
an explicitly invalid defaults-precedence trial rather than optimization
evidence.
