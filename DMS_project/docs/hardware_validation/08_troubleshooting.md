# Troubleshooting

Preserve the full logs and current configuration before changing a cable, GPIO,
or build option. Every retry must describe what changed.

| Symptom | Check first | Safe next action |
| --- | --- | --- |
| Camera initialization fails | Board profile, every camera GPIO, supply, ribbon seating | Return to `USER_DEFINED`, complete inventory, then set only confirmed pins and rebuild. |
| Sensor PID mismatch | Camera model/ribbon and full init log | Stop relying on the assumed map; photograph markings and compare with documented board schematic. |
| PSRAM not detected | Module marking, board support, boot log, supply | Record the log and board marking; do not enable larger frame assumptions. |
| Brownout/reset | Supply voltage/current, cable, boot log | Use a known adequate supply and shorter power path; record measured voltage. |
| CAM has no UART output | CAM UART init log, selected mode, TX pin, baud | First use UART link-test mode and measure/trace CAM TX only after confirming 3.3 V. |
| S3 receives no frames | Common ground, CAM TX -> S3 RX, baud, board pinout | Check direction and USB/UART pin conflicts; do not connect 5 V serial. |
| Garbled text / baud mismatch | Monitor baud and firmware baud are both 115200 | Reopen monitor at 115200 and confirm neither board UART config changed. |
| No common ground | Physical wiring/photo | Add a shared GND before interpreting UART traffic. |
| TX/RX reversed | Wiring table | Use CAM TX -> S3 RX; do not use TX -> TX. |
| GPIO conflicts with USB serial | S3/CAM pinout and USB bridge function | Choose a documented free GPIO, change config, rebuild, and update evidence. |
| Many CRC errors | Ground, voltage level, baud, cable/noise | Stop test, record counters, improve physical link, then start a new timed run. |
| Repeated sequence gaps | CAM reset/capture logs and S3 statistics | Correlate timestamps, power/reset events, UART failures, and cable behavior. |
| Unexpected S3 alarm | Full S3 log and packet counters | Stop the test. Status, heartbeat, unavailable, CRC failure, and timeout must not alarm; file as a defect. |
| `MODEL_UNAVAILABLE` appears as model/alert | S3 log and firmware commit | Stop; it violates the message contract. Do not mask it by changing thresholds. |
| Windows serial port busy | Device Manager and open terminal programs | Close other monitor/serial applications, reconnect once, and record the selected COM port. |
| ESP-IDF fails under Chinese path | CMake/Ninja/Kconfig error text | Use an ASCII-path worktree/build directory and record the path; do not change source just to rename a folder. |

## Escalation Record

When escalating, provide board photos, confirmed pinout, exact commit,
`sdkconfig`, full build/flash/serial logs, wiring photo, test duration, and
UART counters. A cropped screenshot or a verbal pass statement is insufficient.
