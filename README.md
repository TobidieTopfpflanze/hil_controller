# RP2040 USB UART bridge + GPIO HID control

## Interfaces

**UART**

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

## TODO

https://www.nxp.com/products/PCF85063A
