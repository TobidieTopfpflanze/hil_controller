#ifndef TUSB_CONFIG_H
#define TUSB_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#define CFG_TUSB_MCU                 OPT_MCU_RP2040
#define CFG_TUSB_DEBUG               0
#define CFG_TUSB_RHPORT0_MODE        (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)

#define CFG_TUD_ENABLED              1
#define CFG_TUD_MAX_SPEED            OPT_MODE_FULL_SPEED
#define CFG_TUD_ENDPOINT0_SIZE       64

#define CFG_TUD_CDC                  1
#define CFG_TUD_CDC_RX_BUFSIZE       256
#define CFG_TUD_CDC_TX_BUFSIZE       256
#define CFG_TUD_CDC_EP_BUFSIZE       64

#define CFG_TUD_HID                  1
#define CFG_TUD_HID_EP_BUFSIZE       16

#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN           __attribute__((aligned(4)))

#ifdef __cplusplus
}
#endif

#endif
