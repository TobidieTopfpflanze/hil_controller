#include "rtc_pcf85063.h"

// NXP PCF85063A, sections 7.2.1, 7.3 and 7.4:
// https://www.nxp.com/docs/en/data-sheet/PCF85063A.pdf
#define RTC_I2C_ADDRESS       0x51
#define RTC_I2C_TIMEOUT_US    10000
#define RTC_REG_CONTROL_1     0x00
#define RTC_REG_SECONDS       0x04
#define RTC_CONTROL_EXT_TEST  0x80
#define RTC_CONTROL_STOP      0x20
#define RTC_CONTROL_SR        0x10
#define RTC_CONTROL_12_HOUR   0x02
#define RTC_SECONDS_OS        0x80
#define RTC_HOURS_PM          0x20

static bool read_registers(i2c_inst_t *i2c, uint8_t reg,
                           uint8_t *data, size_t length) {
    if (i2c_write_timeout_us(i2c, RTC_I2C_ADDRESS, &reg, 1, true,
                             RTC_I2C_TIMEOUT_US) != 1) {
        return false;
    }
    return i2c_read_timeout_us(i2c, RTC_I2C_ADDRESS, data, length, false,
                               RTC_I2C_TIMEOUT_US) == (int) length;
}

static bool write_control(i2c_inst_t *i2c, uint8_t value) {
    const uint8_t data[] = {RTC_REG_CONTROL_1, value};
    return i2c_write_timeout_us(i2c, RTC_I2C_ADDRESS, data, sizeof(data), false,
                                RTC_I2C_TIMEOUT_US) == (int) sizeof(data);
}

static uint8_t to_bcd(uint8_t value) {
    return (uint8_t) (((value / 10) << 4) | (value % 10));
}

static bool from_bcd(uint8_t value, uint8_t *result) {
    if ((value & 0x0f) > 9 || (value >> 4) > 9) {
        return false;
    }
    *result = (uint8_t) ((value >> 4) * 10 + (value & 0x0f));
    return true;
}

static bool time_is_valid(const rtc_pcf85063_time_t *time) {
    static const uint8_t days_in_month[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    if (time == NULL || time->year < 2000 || time->year > 2099 ||
        time->month < 1 || time->month > 12 || time->weekday > 6 ||
        time->hour > 23 || time->minute > 59 || time->second > 59) {
        return false;
    }

    uint8_t max_day = days_in_month[time->month - 1];
    if (time->month == 2 && time->year % 4 == 0) {
        max_day = 29;
    }
    return time->day >= 1 && time->day <= max_day;
}

rtc_pcf85063_status_t rtc_pcf85063_set_time(i2c_inst_t *i2c,
                                          const rtc_pcf85063_time_t *time) {
    if (i2c == NULL || !time_is_valid(time)) {
        return RTC_PCF85063_ERROR_ARGUMENT;
    }

    uint8_t control;
    if (!read_registers(i2c, RTC_REG_CONTROL_1, &control, 1)) {
        return RTC_PCF85063_ERROR_I2C;
    }

    // Preserve capacitor selection and correction interrupt settings. Stop the
    // divider while updating the whole calendar, and use normal 24-hour mode.
    control &= (uint8_t) ~(RTC_CONTROL_EXT_TEST | RTC_CONTROL_STOP |
                           RTC_CONTROL_SR | RTC_CONTROL_12_HOUR);
    if (!write_control(i2c, control | RTC_CONTROL_STOP)) {
        return RTC_PCF85063_ERROR_I2C;
    }

    const uint8_t data[] = {
        RTC_REG_SECONDS,
        to_bcd(time->second), // Writing bit 7 as zero clears the OS flag.
        to_bcd(time->minute),
        to_bcd(time->hour),
        to_bcd(time->day),
        time->weekday,
        to_bcd(time->month),
        to_bcd((uint8_t) (time->year - 2000))
    };
    if (i2c_write_timeout_us(i2c, RTC_I2C_ADDRESS, data, sizeof(data), false,
                             RTC_I2C_TIMEOUT_US) != (int) sizeof(data)) {
        // A partial calendar must not be exposed as a valid running clock.
        // Leave STOP set until a later successful set_time() call.
        return RTC_PCF85063_ERROR_I2C;
    }
    if (!write_control(i2c, control)) {
        return RTC_PCF85063_ERROR_I2C;
    }

    // OS cannot be cleared if the oscillator is not yet running.
    uint8_t seconds;
    if (!read_registers(i2c, RTC_REG_SECONDS, &seconds, 1)) {
        return RTC_PCF85063_ERROR_I2C;
    }
    return (seconds & RTC_SECONDS_OS) ? RTC_PCF85063_ERROR_TIME_INVALID
                                      : RTC_PCF85063_OK;
}

rtc_pcf85063_status_t rtc_pcf85063_read_time(i2c_inst_t *i2c,
                                           rtc_pcf85063_time_t *time) {
    if (i2c == NULL || time == NULL) {
        return RTC_PCF85063_ERROR_ARGUMENT;
    }

    uint8_t control;
    if (!read_registers(i2c, RTC_REG_CONTROL_1, &control, 1)) {
        return RTC_PCF85063_ERROR_I2C;
    }
    if (control & (RTC_CONTROL_STOP | RTC_CONTROL_EXT_TEST)) {
        return RTC_PCF85063_ERROR_TIME_INVALID;
    }

    // Read seconds through years in one burst to prevent rollover tearing.
    uint8_t data[7];
    if (!read_registers(i2c, RTC_REG_SECONDS, data, sizeof(data))) {
        return RTC_PCF85063_ERROR_I2C;
    }
    if (data[0] & RTC_SECONDS_OS) {
        return RTC_PCF85063_ERROR_TIME_INVALID;
    }

    rtc_pcf85063_time_t result = {0};
    uint8_t year;
    uint8_t hour_mask = (control & RTC_CONTROL_12_HOUR) ? 0x1f : 0x3f;
    if (!from_bcd(data[0] & 0x7f, &result.second) ||
        !from_bcd(data[1] & 0x7f, &result.minute) ||
        !from_bcd(data[2] & hour_mask, &result.hour) ||
        !from_bcd(data[3] & 0x3f, &result.day) ||
        !from_bcd(data[5] & 0x1f, &result.month) ||
        !from_bcd(data[6], &year)) {
        return RTC_PCF85063_ERROR_TIME_INVALID;
    }
    if (control & RTC_CONTROL_12_HOUR) {
        if (result.hour < 1 || result.hour > 12) {
            return RTC_PCF85063_ERROR_TIME_INVALID;
        }
        result.hour = (uint8_t) ((result.hour % 12) +
                                ((data[2] & RTC_HOURS_PM) ? 12 : 0));
    }
    result.weekday = data[4] & 0x07;
    result.year = 2000 + year;
    if (!time_is_valid(&result)) {
        return RTC_PCF85063_ERROR_TIME_INVALID;
    }

    *time = result;
    return RTC_PCF85063_OK;
}
