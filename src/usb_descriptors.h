#ifndef USB_DESCRIPTORS_H
#define USB_DESCRIPTORS_H

#include "tusb.h"
#include "setup_storage.h"

#define HID_REPORT_ID_GPIO  1
#define HID_REPORT_ID_RTC   2
#define HID_REPORT_ID_SETUP 3
#define HID_REPORT_GPIO_SIZE 2
#define HID_REPORT_RTC_SIZE  9
#define HID_REPORT_SETUP_SIZE (4 + SETUP_DATA_SIZE)
#define HID_RTC_SET_NOT_ATTEMPTED 0xff
#define HID_SETUP_WRITE_NOT_ATTEMPTED 0xff

enum {
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_HID,
    ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_HID_DESC_LEN)

extern uint8_t const hid_report_descriptor[];

#endif
