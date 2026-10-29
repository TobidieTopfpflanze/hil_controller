#ifndef RTC_PCF85063_H
#define RTC_PCF85063_H

#include <stdint.h>

#include "hardware/i2c.h"

typedef struct {
    uint16_t year;    // 2000..2099
    uint8_t month;    // 1..12
    uint8_t day;      // 1..31, subject to month and leap year
    uint8_t weekday;  // 0 = Sunday, 6 = Saturday
    uint8_t hour;     // 0..23
    uint8_t minute;   // 0..59
    uint8_t second;   // 0..59
} rtc_pcf85063_time_t;

// Values are also returned in the RTC HID feature report.
typedef enum {
    RTC_PCF85063_OK = 0,
    RTC_PCF85063_ERROR_I2C = 1,
    RTC_PCF85063_ERROR_ARGUMENT = 2,
    RTC_PCF85063_ERROR_TIME_INVALID = 3
} rtc_pcf85063_status_t;

rtc_pcf85063_status_t rtc_pcf85063_set_time(i2c_inst_t *i2c,
                                          const rtc_pcf85063_time_t *time);

rtc_pcf85063_status_t rtc_pcf85063_read_time(i2c_inst_t *i2c,
                                           rtc_pcf85063_time_t *time);

#endif
