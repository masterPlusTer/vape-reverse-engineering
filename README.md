# Vape Display Replay (Arduino Nano)


![IMG_3186](https://github.com/user-attachments/assets/523ab45a-bc0d-4a10-b0af-01bbbc1e89cf)
![IMG_3185](https://github.com/user-attachments/assets/0c26b9e5-5bbe-4da3-872d-dea00d1ac4c0)
![IMG_3183](https://github.com/user-attachments/assets/50756790-e48d-4c3c-8c08-71427dac80f5)
![IMG_315DF290-99AE-4660-8C72-7250E5E3D94E](https://github.com/user-attachments/assets/7581c8d4-195e-42ac-913e-f7f869f34122)

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
 

![IMG_309E37B4-B62F-4F48-A170-8914BD57300F](https://github.com/user-attachments/assets/d95aeae9-361a-4300-a516-2634ef227caf)

  
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



