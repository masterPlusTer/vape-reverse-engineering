# Vape Display Replay (Arduino Nano)

Arduino Nano library to control a custom segmented display via **I2C replay**, using a **serial command interface** and a **mapped logical layout** (digits, indicators, icons, bars, RGB ring).

This project abstracts a captured I2C display protocol into high-level commands such as `line`, `digit`, `rgbfill`, and `monofill`, making the display easy to drive, script, and extend.

---

## Features

- Software (bit-banged) I2C on A4/A5
- Replay of captured I2C frames (preamble + payload)
- Payload size: **145 bytes**
- Robust Serial command parser (no `String`, low SRAM usage)
- Logical grouping of display areas
- Support for:
  - 7-segment digits
  - 2-segment indicator digits
  - RGB outer ring
  - Bars, icons, UI blocks
- `off` command that reliably turns **everything off**
- Debug access via raw payload index control

---

## Hardware

- **MCU:** Arduino Nano (ATmega328P)
- **I2C (software):**
  - SDA → A4
  - SCL → A5
- **I2C device address:** `0x3C`
- External pull-ups on SDA/SCL recommended

---

## Display Layout Model

Each display line (top / bottom):

[ 2-segment indicator ] [ 7-segment digit ] [ 7-segment digit ]

Example:
- `188` = indicator ON + full 8 + full 8
- `075` = indicator OFF + 7 + 5

The indicator is **not** a numeric digit.

---

## Serial Commands

### Turn everything off
```
off
```

### Show current frame
```
show
```

### Raw payload access (debug)
```
raw <index> <value>
```

### 7-segment digit
```
digit <slot> <number> [brightness] [dp]
```

Slots:
- `t0` → top, middle digit
- `t1` → top, right digit
- `b0` → bottom, middle digit
- `b1` → bottom, right digit

### Full line render
```
line <top|bottom> <abc> [brightness] [dpMask]
```

### RGB outer ring
```
rgbfill outer <r> <g> <b>
```

### MONO groups
```
monofill <group> <value>
mono <group> <index> <value>
```

---

## License

MIT / Apache-2.0 / GPL-3.0



