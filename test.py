import hid

VID = 0xdead
PID = 0xbeef

GPIO_PIN = 5

with hid.Device(vid=VID, pid=PID) as dev:
    dev.send_feature_report if False else None

    dev.write([0x01, GPIO_PIN, 1])
    print(f"GPIO {GPIO_PIN} set HIGH")

    dev.write([0x01, GPIO_PIN, 0])
    print(f"GPIO {GPIO_PIN} set LOW")
