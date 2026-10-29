# RP2040 USB UAR bridge and GPIO,RTC HID control

## Interfaces

**UART**

-> Currently fixed setting

| Name   | value  |
| ------ | ------ |
| ID     | uart0  |
| TX_PIN | GP0    |
| RX_PIN | GP1    |
| BAUD   | 115200 |

**I2C**

| Name    | value  |
| ------- | ------ |
| PORT    | i2c1   |
| SDA_PIN | GP2    |
| SCL_PIN | GP3    |
| SPEED   | 400Khz |

**USB**

| Name       | value  |
| ---------- | ------ |
| VENDOR_ID  | 0xdead |
| PRODUCT_ID | 0xbeef |

-> configured to expose a HID and CDC ACM device, when connected over USB

## Hardware

```
 ┌─────────────────────┐
 │         J1          │
 └──────────┬──────────┘
           USB
 ┌──────────┴──────────┐
 │        RP2040       │
 └─────┬────────┬─────┬┘
       │        │     │
      GPIO     UART  I2C
       │        │     ├──────────────┐
       │        │     │              │
 ┌─────┴────┐  ┌┴─────┴────┐  ┌──────┴───────────┐
 │    J5    │  │    J3     │  │ PCF85063A (RTC)  │
 └──────────┘  └───────────┘  └──────────────────┘
```

## PCF85063A RTC

| Byte | SET_REPORT                | GET_REPORT                     |
| ---- | ------------------------- | ------------------------------ |
| 0    | Report ID `2`             | Report ID `2`                  |
| 1    | `0` (reserved)            | Status of this read            |
| 2    | `0` (reserved)            | Status of the last set request |
| 3    | Year minus 2000 (0–99)    | Year minus 2000                |
| 4    | Month (1–12)              | Month                          |
| 5    | Day (1–31)                | Day                            |
| 6    | Weekday (0–6, Sunday = 0) | Weekday                        |
| 7    | Hour (0–23)               | Hour                           |
| 8    | Minute (0–59)             | Minute                         |
| 9    | Second (0–59)             | Second                         |

| Status | Meaning                                                            |
| ------ | ------------------------------------------------------------------ |
| `0`    | Success                                                            |
| `1`    | I²C transfer failed or timed out                                   |
| `2`    | Invalid argument, date, or set-report payload                      |
| `3`    | Clock/time invalid (including oscillator-stop, STOP, or test mode) |
| `255`  | No set request since firmware boot; only used in byte 2            |

### Host usage

```bash
sudo python3 controller.py --read
sudo python3 controller.py --set now --read
sudo python3 controller.py --set 2026-10-29T12:00:00Z
sudo python3 controller.py --gpio 13 1
```
