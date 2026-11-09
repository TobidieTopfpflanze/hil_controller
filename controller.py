"""Control GPIO, RTC time, and persistent setup details over HID."""

import argparse
from datetime import datetime, timezone
import json

import hid

VID = 0xDEAD
PID = 0xBEEF
GPIO_REPORT_ID = 1
RTC_REPORT_ID = 2
RTC_REPORT_LENGTH = 10  # Report ID plus nine payload bytes.
SETUP_REPORT_ID = 3
SETUP_REPORT_LENGTH = 165
SETUP_FORMAT_VERSION = 1
SETUP_DESCRIPTION_SIZE = 128
SETUP_SERIAL_SIZE = 32
SETUP_STATUS_MESSAGES = {
    1: "no setup details stored",
    2: "invalid setup details or report payload",
    3: "flash write or verification failed",
    4: "no valid setup record (corrupt or unsupported stored format)",
    0xFF: "no setup write processed since boot",
}
STATUS_MESSAGES = {
    1: "I2C transfer failed",
    2: "invalid time or report payload",
    3: "RTC time is invalid or the clock/oscillator is stopped; set the time",
    0xFF: "no set-time request processed since boot",
}


def _get_report(dev):
    report = dev.get_feature_report(RTC_REPORT_ID, RTC_REPORT_LENGTH)
    if len(report) != RTC_REPORT_LENGTH or report[0] != RTC_REPORT_ID:
        raise RuntimeError("Unexpected RTC feature report; check the firmware version")
    return report


def _check_status(operation, status):
    if status != 0:
        message = STATUS_MESSAGES.get(status, f"unknown status {status}")
        raise RuntimeError(f"{operation}: {message}")


def _decode_time(report):
    _check_status("Read time", report[1])
    year, month, day, weekday, hour, minute, second = report[3:]
    if weekday > 6:
        raise RuntimeError("Invalid RTC weekday")
    return datetime(2000 + year, month, day, hour, minute, second,
                    tzinfo=timezone.utc)


def read_time(dev):
    return _decode_time(_get_report(dev))


def set_time(dev, value):
    """Store UTC (naive datetimes are treated as UTC); return the readback."""
    if value.tzinfo is None:
        value = value.replace(tzinfo=timezone.utc)
    value = value.astimezone(timezone.utc)
    if not 2000 <= value.year <= 2099:
        raise ValueError("RTC supports years 2000 through 2099")
    report = [RTC_REPORT_ID, 0, 0, value.year - 2000, value.month, value.day,
              (value.weekday() + 1) % 7, value.hour, value.minute, value.second]
    if dev.send_feature_report(report) != len(report):
        raise RuntimeError("Incomplete RTC feature report write")
    result = _get_report(dev)
    # USB SET_REPORT acknowledges transport, not the result of the I2C write.
    _check_status("Set time", result[2])
    return _decode_time(result)


def set_gpio(dev, pin, value):
    dev.write([GPIO_REPORT_ID, pin, value])


def _check_setup_status(operation, status):
    if status != 0:
        message = SETUP_STATUS_MESSAGES.get(status, f"unknown status {status}")
        raise RuntimeError(f"{operation}: {message}")


def _get_setup_report(dev):
    report = bytes(dev.get_feature_report(SETUP_REPORT_ID, SETUP_REPORT_LENGTH))
    if (len(report) != SETUP_REPORT_LENGTH or report[0] != SETUP_REPORT_ID or
            report[3] != SETUP_FORMAT_VERSION or report[4] != 0):
        raise RuntimeError("Unexpected setup feature report; check the firmware version")
    return report


def _encode_setup_text(value, size, name):
    encoded = value.encode("utf-8")
    if not encoded or b"\0" in encoded or len(encoded) >= size:
        raise ValueError(f"{name} must contain 1–{size - 1} UTF-8 bytes without NUL characters")
    return encoded.ljust(size, b"\0")


def _decode_setup_text(value):
    text, terminator, padding = value.partition(b"\0")
    if not text or not terminator or any(padding):
        raise RuntimeError("Invalid setup string in feature report")
    return text.decode("utf-8")


def _decode_setup(report):
    _check_setup_status("Read setup", report[1])
    serial_offset = 5 + SETUP_DESCRIPTION_SIZE
    return {
        "description": _decode_setup_text(report[5:serial_offset]),
        "serial_number": _decode_setup_text(report[serial_offset:serial_offset + SETUP_SERIAL_SIZE]),
    }


def read_setup(dev):
    return _decode_setup(_get_setup_report(dev))


def write_setup(dev, description, serial_number):
    """Replace all setup fields and verify the persisted readback."""
    description_data = _encode_setup_text(description, SETUP_DESCRIPTION_SIZE, "Description")
    serial_data = _encode_setup_text(serial_number, SETUP_SERIAL_SIZE, "Serial number")
    report = (bytes([SETUP_REPORT_ID, 0, 0, SETUP_FORMAT_VERSION, 0]) +
              description_data + serial_data)
    if dev.send_feature_report(report) != len(report):
        raise RuntimeError("Incomplete setup feature report write")
    response = _get_setup_report(dev)
    _check_setup_status("Write setup", response[2])
    result = _decode_setup(response)
    expected = {"description": description, "serial_number": serial_number}
    if result != expected:
        raise RuntimeError("Setup readback does not match the requested details")
    return result


def handle_set_command(dev, args):
    if args == "now":
        value = datetime.now(timezone.utc)
    else:
        value = datetime.fromisoformat(args)

    set_time(dev, value)


def handle_read_command(dev, args):
    if args:
        print(read_time(dev))


def handle_gpio_command(dev, args):
    set_gpio(dev, args[0], args[1])


def handle_write_setup_command(dev, args):
    write_setup(dev, *args)


def handle_read_setup_command(dev, args):
    if args:
        print(json.dumps(read_setup(dev), indent=2, ensure_ascii=False))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--set", metavar="TIME",
                        help="'now' or an ISO datetime; defaults to UTC without an offset")
    parser.add_argument("--read", action="store_true", help="read time")
    parser.add_argument("--gpio", nargs=2, type=int, metavar=("PIN", "VALUE"),
                        help="set GPIO pin to 0 (low) or 1 (high)")
    parser.add_argument("--write-setup", nargs=2,
                        metavar=("DESCRIPTION", "SERIAL"),
                        help="store setup description and serial number in flash")
    parser.add_argument("--read-setup", action="store_true", help="read setup details from flash")

    args = parser.parse_args()
    if (args.set is None and not args.read and args.gpio is None and
            args.write_setup is None and not args.read_setup):
        parser.print_help()
        return

    dev = hid.device()
    try:
        dev.open(VID, PID)
        if args.set is not None:
            handle_set_command(dev, args.set)
        if args.read:
            handle_read_command(dev, args.read)
        if args.gpio is not None:
            handle_gpio_command(dev, args.gpio)
        if args.write_setup is not None:
            handle_write_setup_command(dev, args.write_setup)
        if args.read_setup:
            handle_read_setup_command(dev, args.read_setup)
    except (OSError, RuntimeError, ValueError) as error:
        parser.exit(1, f"HID error: {error}\n")
    finally:
        dev.close()


if __name__ == "__main__":
    main()
