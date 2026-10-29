"""Set/read the controller's RTC and control GPIO using the hidapi Python package."""

import argparse
from datetime import datetime, timezone

import hid

VID = 0xDEAD
PID = 0xBEEF
GPIO_REPORT_ID = 1
RTC_REPORT_ID = 2
RTC_REPORT_LENGTH = 10  # Report ID plus nine payload bytes.
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--set", metavar="TIME",
                        help="'now' or an ISO datetime; defaults to UTC without an offset")
    parser.add_argument("--read", action="store_true", help="read time")
    parser.add_argument("--gpio", nargs=2, type=int, metavar=("PIN", "VALUE"),
                        help="set GPIO pin to 0 (low) or 1 (high)")

    args = parser.parse_args()
    if args.set is None and not args.read and args.gpio is None:
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
    except (OSError, RuntimeError, ValueError) as error:
        parser.exit(1, f"HID error: {error}\n")
    finally:
        dev.close()


if __name__ == "__main__":
    main()
