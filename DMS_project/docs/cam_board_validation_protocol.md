# ESP32-CAM First Board Validation Protocol

## Inventory before flashing

Do not select P1/P2/P3 until this table is complete. `AI Thinker` is not an
acceptable default assumption.

| Item | CAM evidence required | S3 evidence required |
| --- | --- | --- |
| Board identity | product/model, front/back markings, board photo and link | product/model, module marking, photo and link |
| Processor and memory | chip marking, `flash_id` output, Flash size, PSRAM boot result | chip/module marking, Flash size, PSRAM result |
| Camera and pins | sensor/module, installed GPIO map, camera PID boot log | actual peripheral pinout |
| Programming | full flash command and serial boot log | full flash command and serial boot log |

Record `UNKNOWN` rather than guessing a value. Do not include Wi-Fi passwords,
API keys, raw face images, or private network identifiers in the evidence.

## Candidate activation and smoke test

1. Preserve the successful C0 binary and use the selected partition candidate
   only after the Flash result satisfies its stated capacity.
2. Build the candidate with ESP-IDF v5.3.5, capture `idf.py size`, component
   and file reports, ELF/BIN/map, generated sdkconfig, and partition table.
3. Flash CAM and capture reset/boot logs. Verify no boot loop, Flash mismatch,
   PSRAM error, or camera-init error; record the camera PID.
4. Configure secrets only in ignored `dms_secrets.h`. Verify `/health`, then
   consume `/stream` with the PC latest-frame client. Record connection time,
   disconnect/reconnect count, resets, frame drops, and PC latency summaries.
5. Run the CAM stream for at least 15 minutes before calling it stable. This
   task defines the record format only; it contains no such measurement.

## C3 runtime gate

C3 disables `CONFIG_ESP_WIFI_IRAM_OPT` and
`CONFIG_ESP_WIFI_RX_IRAM_OPT`. It clears the source-level IRAM gate but must
be reported exactly as **BUILD CANDIDATE — MJPEG RUNTIME VALIDATION REQUIRED**
until the smoke test above demonstrates the intended camera, Wi-Fi, and stream
behaviour on the actual board.

## Completion record

Return the inventory table, board and wiring photographs, flash and serial
logs, `flash_id` output, resource reports, stream/PC statistics, failure
count, and an explicit pass/fail decision. "Test passed" alone is not
evidence.
