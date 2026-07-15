# Wiring Checklist

Do not connect the boards until both inventory rows and the single-board power
checks are complete.

## Required Connections

| Connection | Current software value | Physical confirmation | Status |
| --- | --- | --- | --- |
| CAM TX -> S3 RX | CAM GPIO 14 -> S3 GPIO 17 | `UNKNOWN` | `PENDING` |
| CAM GND -> S3 GND | common ground required | `UNKNOWN` | `PENDING` |
| CAM power | board-specific | `UNKNOWN` | `PENDING` |
| S3 power | board-specific | `UNKNOWN` | `PENDING` |
| S3 TX | GPIO 18, currently debug/reserved only | leave unconnected unless a recorded test needs it | `PENDING` |

The GPIO values above are software configuration values from `config.h`; they
are not validated against the actual boards. Record every confirmed or changed
pin in the evidence template.

## Mandatory Checks

- [ ] Both boards were powered and observed independently before UART wiring.
- [ ] Both UART interfaces use 3.3 V logic.
- [ ] No 5 V UART signal is directly connected to an ESP GPIO.
- [ ] CAM GND and S3 GND are connected.
- [ ] CAM TX is connected to S3 RX. Do not connect TX-to-TX.
- [ ] Initial integration connects only UART signal and ground; do not add I2S,
  IR LED, or other peripherals.
- [ ] The selected GPIOs do not collide with camera, flash, USB serial, or
  board-reserved functions according to the confirmed pinout.
- [ ] Any GPIO change is made in configuration, rebuilt, and recorded.

## Prohibited Shortcuts

- Do not assume an AI Thinker camera map from board appearance.
- Do not add an undocumented fly wire after a conflict.
- Do not power one board through an unknown GPIO from the other board.
- Do not claim UART validation from a CAM log alone; S3 receive statistics are
  also required.
