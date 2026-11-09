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

List connected controllers (`dead:beef`) and their stored setup details:

```bash
sudo python3 controller.py --list
```

```text
USB: 1-2.3
 sn=123456789, description="X242 - Full setup"
```

Select by the serial number stored in setup flash using `--sn`:

```bash
sudo python3 controller.py --sn 123456789 --read-setup
sudo python3 controller.py --sn 123456789 --power-en 1
```

```bash
sudo python3 controller.py --read
sudo python3 controller.py --set now --read
sudo python3 controller.py --set 2026-10-29T12:00:00Z
sudo python3 controller.py --gpio 13 1
```

## Persistent setup details

```bash
# Replace both setup fields and print the stored result.
sudo python3 controller.py --write-setup "X242 - Full setup" "123456789"

```

The description accepts 1–127 UTF-8 bytes, and the serial number accepts 1–31
UTF-8 bytes (including letters or leading zeroes).

### Setup report layout


| Byte    | SET_REPORT                    | GET_REPORT                     |
| ------- | ----------------------------- | ------------------------------ |
| 0       | Report ID `3`                 | Report ID `3`                  |
| 1       | `0`                           | Status of this read            |
| 2       | `0`                           | Status of the last setup write |
| 3       | Format version `1`            | Format version `1`             |
| 4       | `0` (reserved)                | `0`                            |
| 5–132   | Description (128-byte field)  | Description                    |
| 133–164 | Serial number (32-byte field) | Serial number                  |

| Status | Meaning                                                 |
| ------ | ------------------------------------------------------- |
| `0`    | Success                                                 |
| `1`    | Setup storage is empty                                  |
| `2`    | Invalid fields or report payload                        |
| `3`    | Flash operation or readback verification failed         |
| `4`    | No valid record: corrupted or unsupported stored format |
| `255`  | No setup write since firmware boot; only used in byte 2 |

