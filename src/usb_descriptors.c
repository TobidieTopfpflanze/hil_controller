#include "usb_descriptors.h"

#define USB_VID 0xdead
#define USB_PID 0xbeef
#define USB_DEVICE_VERSION 0x0100

// HID output report: Report ID 1, then two payload bytes:
// byte 0 = GPIO number, byte 1 = requested state (0=LOW, 1=HIGH).
uint8_t const hid_report_descriptor[] = {
    0x06, 0x00, 0xFF,       // Usage Page (Vendor-defined 0xFF00)
    0x09, 0x01,             // Usage (1)
    0xA1, 0x01,             // Collection (Application)
    0x85, 0x01,             //   Report ID (1)
    0x15, 0x00,             //   Logical Minimum (0)
    0x26, 0xFF, 0x00,       //   Logical Maximum (255)
    0x75, 0x08,             //   Report Size (8 bits)
    0x95, 0x02,             //   Report Count (2 bytes)
    0x09, 0x02,             //   Usage (2)
    0x91, 0x02,             //   Output (Data, Variable, Absolute)
    0xC0                    // End Collection
};

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    (void) instance;
    return hid_report_descriptor;
}

uint8_t const *tud_descriptor_device_cb(void) {
    static tusb_desc_device_t const desc_device = {
        .bLength            = sizeof(tusb_desc_device_t),
        .bDescriptorType    = TUSB_DESC_DEVICE,
        .bcdUSB              = 0x0200,
        .bDeviceClass        = TUSB_CLASS_MISC,
        .bDeviceSubClass     = MISC_SUBCLASS_COMMON,
        .bDeviceProtocol     = MISC_PROTOCOL_IAD,
        .bMaxPacketSize0     = CFG_TUD_ENDPOINT0_SIZE,
        .idVendor            = USB_VID,
        .idProduct           = USB_PID,
        .bcdDevice           = USB_DEVICE_VERSION,
        .iManufacturer       = 1,
        .iProduct            = 2,
        .iSerialNumber       = 3,
        .bNumConfigurations  = 1
    };
    return (uint8_t const *) &desc_device;
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void) index;
    static uint8_t const desc_configuration[] = {
        TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN,
                              TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
        // CDC ACM: notification IN 0x81, data OUT 0x02, data IN 0x82.
        TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, 0x81, 8, 0x02, 0x82, 64),
        // HID has an interrupt IN endpoint. Host output reports use HID
        // SET_REPORT over endpoint 0 (hidapi write() works without OUT EP).
        TUD_HID_DESCRIPTOR(ITF_NUM_HID, 5, HID_ITF_PROTOCOL_NONE,
                           sizeof(hid_report_descriptor), 0x83, 16, 10)
    };
    return desc_configuration;
}

char const *string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 }, // 0: English (United States)
    "custom",                      // 1: Manufacturer
    "custom controller",           // 2: Product
    "123456789",                   // 3: Serial number (change per device)
    "UART CDC",                    // 4: CDC interface
    "GPIO HID"                     // 5: HID interface
};

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;
    static uint16_t desc_str[32];
    uint8_t chr_count;

    if (index == 0) {
        desc_str[1] = 0x0409;
        chr_count = 1;
    } else {
        if (index >= (sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) {
            return NULL;
        }

        char const *str = string_desc_arr[index];
        chr_count = 0;
        while (str[chr_count] != '\0' && chr_count < 31) {
            desc_str[1 + chr_count] = (uint8_t) str[chr_count];
            chr_count++;
        }
    }

    desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);
    return desc_str;
}
