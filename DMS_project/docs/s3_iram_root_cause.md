# ESP32-S3 IRAM Root Cause

Superseded by G5: 16383/16384 is an ESP-IDF size category, not the actual iram0_0_seg capacity.

## Scope and evidence

This analysis uses ESP-IDF v5.3.5 and a clean build of G4 commit
`d154f5bd864cd6c05bc59f08346d6f73f429eac7`. Full build logs, ELF, map,
generated sdkconfig, linker scripts, component/file reports, and CSV symbol
tables are retained outside Git in `handoff/g5_evidence/s3_a0_baseline/`.

## Meaning of `16383 / 16384`

`idf.py size` reports the first 16 KiB of executable-only IRAM, from
`0x40374000` to `_diram_i_start` at `0x40378000`. It is not the capacity of
the actual `iram0_0_seg` linker region.

The generated `memory.ld` defines `iram0_0_seg` at `0x40374000` with length
`0x57700` (358,144 bytes). The map places `.iram0.vectors` at `0x40374000`
(1,027 bytes) and `.iram0.text` at `0x40374404` (48,147 bytes), followed by
233 bytes of end padding/alignment. Code after `0x40378000` uses shared
D/IRAM; the size tool accounts that part as DIRAM text (32,791 bytes), not
as the 16 KiB IRAM-only subtotal.

Therefore the one-byte `size` subtotal is not a linker-overflow boundary.
The actual linker assertion is `_iram_end - ORIGIN(iram0_0_seg) <=
LENGTH(iram0_0_seg)`. The baseline uses about 49.4 KiB of 358 KiB, leaving
about 308 KiB. There is no padding/reservation that turns 16 KiB into an
independent linker region: the 233-byte `.iram0.text_end` padding is included
in the normal section end.

Consequences:

- A normal Flash function does not consume `iram0_0_seg` and will not fail the
  IRAM link assertion merely because the 16 KiB subtotal is full.
- A small `IRAM_ATTR` function would extend into shared D/IRAM if needed; it
  still consumes shared DRAM capacity, so additions must be measured.
- The previous release blocker was a misinterpretation of `idf.py size`, not
  evidence of a 16 KiB physical or linker limit.

## Map and symbol analysis

The IRAM-only subtotal is 1,027 bytes of vectors plus 15,356 bytes of code.
No application source under `esp32_s3_fw/main` uses `IRAM_ATTR`,
`DRAM_ATTR`, `RTC_IRAM_ATTR`, `ESP_INTR_FLAG_IRAM`, `noflash`, linker
fragments, or an `iram0` placement rule. The state machine, MQTT contract,
audio queue and command parser are not the direct cause.

Top map-attributed components in the IRAM-only range are:

| Component | Bytes | Placement reason |
| --- | ---: | --- |
| esp_system | 3,361 | startup, critical system/cache paths |
| spi_flash | 1,737 | cache-disabled Flash operations |
| xtensa | 1,532 | vectors and architecture support |
| heap | 1,146 | allocator cache-disabled paths |
| esp_hw_support | 1,034 | interrupt/clock/cache support |
| newlib | 891 | libc support selected by linker rules |
| bootloader_support | 730 | Flash guard/reset paths |
| esp_mm | 503 | MMU/cache mapping |
| esp_driver_i2c | 464 | I2C ISR path used by OLED support |
| esp_driver_i2s | 279 | audio DMA/driver ISR support |

Largest symbols include `tlsf_memalign_offs` (1,223 bytes,
`heap/tlsf.c.obj`), `tlsf_realloc` (1,007,
`heap/tlsf.c.obj`), `call_start_cpu0` (935,
`esp_system/cpu_start.c.obj`), `spi_flash_hal_configure_host_io_mode` (799,
`hal/spi_flash_hal_iram.c.obj`), `tlsf_free` (792,
`heap/tlsf.c.obj`), `i2c_master_isr_handler_default` (760,
`esp_driver_i2c/i2c_master.c.obj`), `tlsf_malloc` (755,
`heap/tlsf.c.obj`), `xTaskIncrementTick` (628,
`freertos/tasks.c.obj`), and `spi_flash_hal_gpspi_configure_host_io_mode`
(619, `hal/spi_flash_hal_iram.c.obj`).

The complete top-50 table, including address, size, section, archive, object
and inferred reason, is `handoff/g5_evidence/s3_a0_baseline/iram_symbols_top50.csv`.
The associated component/object CSV tables are retained beside it. Their map
entries are emitted by ESP-IDF linker rules (`.iram1` inputs), vectors, or
cache-disabled driver paths, not an application-wide noflash rule.

## Configuration baseline

| Option | ESP-IDF 5.3.5 status | Baseline value / note |
| --- | --- | --- |
| `CONFIG_COMPILER_OPTIMIZATION_SIZE` | valid | enabled |
| Wi-Fi IRAM, RX IRAM, EXTRA IRAM, SLP IRAM | valid | all disabled |
| `CONFIG_LWIP_IRAM_OPTIMIZATION` | valid | disabled |
| `CONFIG_HEAP_PLACE_FUNCTION_INTO_FLASH` | valid | disabled |
| `CONFIG_HEAP_TLSF_USE_ROM_IMPL` | valid but unavailable on this target | absent due to `ESP_ROM_HAS_HEAP_TLSF` dependency |
| `CONFIG_HAL_WDT_USE_ROM_IMPL` | valid | enabled |
| `CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH` | valid | enabled |
| `CONFIG_LIBC_LOCKS_PLACE_IN_IRAM` | not present in this IDF configuration | absent |
| SPI master/slave ISR in IRAM | valid | enabled by default |
| assertion level | valid | level 2 |
| log default/max level | valid | INFO / INFO |
| dynamic log control | not present in this configuration | absent |

## Safety review

`CONFIG_HEAP_PLACE_FUNCTION_INTO_FLASH=y` was built as A3. It reduced shared
DIRAM use by 7,724 bytes but did not change the 16 KiB classification. The
S3 application source has no IRAM ISR and does not call heap APIs from an ISR;
the SPI-master Kconfig dependency automatically prevents its IRAM ISR mode
when heap functions move to Flash. This is **STATICALLY REVIEWED** only:
board runtime, I2S DMA, Wi-Fi/MQTT reconnect, and cache-disabled behavior are
still pending. It is not the chosen candidate because the real linker region
already meets the budget without this runtime tradeoff.

The chosen S3 candidate is the original G4 configuration: size optimization,
non-ISR FreeRTOS Flash placement, and Wi-Fi/PHY IRAM optimizations disabled.
It preserves MQTT, Wi-Fi STA, I2S, the fatigue state machine, audio queue and
whitelisted command path. Network runtime validation remains required because
the chosen G4 configuration already trades network IRAM for Flash execution.

## CAM secondary risk

The clean ESP32-CAM build is still a secondary release risk: IRAM is
121,802/131,072 bytes (9,270 free, 7.07%) and the 1 MiB application partition
has only 10% free. The target thresholds are 10% IRAM and 15% app-partition
free, so CAM does not pass them. It retains QVGA MJPEG and has no MQTT JPEG
path. Wi-Fi IRAM/RX optimizations are enabled on CAM; disabling them is a
build candidate with expected throughput cost and requires board validation.
Changing the partition plan is a separate Flash-layout decision and was not
made here.

## Runtime boundary

All conclusions above are source, map, and build evidence. No board-side
audio, I2S DMA, camera, PSRAM, Wi-Fi/MQTT throughput/reconnect, latency, FPS,
accuracy, power, or LLM behavior has been measured.
