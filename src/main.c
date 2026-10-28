#include <stdbool.h>
#include <stdint.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"
#include "tusb.h"
#include "usb_descriptors.h"

// UART
#define UART_ID      uart0
#define UART_TX_PIN  0
#define UART_RX_PIN  1
#define UART_BAUD    115200

// I2C
#define I2C_PORT     i2c1
#define I2C_SDA      2
#define I2C_SCL      3
#define I2C_SPEED    400 // in Khz

// GPIO
#define GPIO_LED_R   11
#define GPIO_LED_G   12
#define GPIO_LED_B   14
#define GPIO_LED_W   13
#define GPIO_MISC_22 22
#define GPIO_MISC_20 20
#define GPIO_MISC_19 19
#define GPIO_MISC_18 18

static uint8_t allowed_gpios[] = {
    GPIO_LED_R,
    GPIO_LED_G,
    GPIO_LED_B,
    GPIO_LED_W,
    GPIO_MISC_22,
    GPIO_MISC_20,
    GPIO_MISC_19,
    GPIO_MISC_18
};

static bool gpio_is_allowed(uint8_t pin) {
    for (size_t i = 0; i < sizeof(allowed_gpios); i++)
    {
        if(allowed_gpios[i] == pin)
            return true;
    }
    
    return false;
}

static void set_control_gpio(uint8_t pin, uint8_t state) {
    if (!gpio_is_allowed(pin) || state > 1) {
        return;
    }
    gpio_put(pin, state != 0);
}

uint16_t tud_hid_get_report_cb(uint8_t instance,
                               uint8_t report_id,
                               hid_report_type_t report_type,
                               uint8_t *buffer,
                               uint16_t reqlen) {
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance,
                           uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const *buffer,
                           uint16_t bufsize) {
    (void) instance;

    if (report_type != HID_REPORT_TYPE_OUTPUT || report_id != 1 ||
        buffer == NULL || bufsize < 2) {
        return;
    }

    set_control_gpio(buffer[0], buffer[1]);
}

static void bridge_usb_to_uart(void) {
    uint8_t buffer[64];
    while (tud_cdc_available() > 0) {
        uint32_t count = tud_cdc_read(buffer, sizeof(buffer));
        for (uint32_t i = 0; i < count; ++i) {
            uart_putc_raw(UART_ID, buffer[i]);
        }
    }
}

static void bridge_uart_to_usb(void) {
    uint8_t buffer[64];
    uint32_t count = 0;

    while (count < sizeof(buffer) && uart_is_readable(UART_ID)) {
        buffer[count++] = uart_getc(UART_ID);
    }

    if (count > 0 && tud_cdc_connected()) {
        uint32_t offset = 0;
        while (offset < count) {
            uint32_t written = tud_cdc_write(buffer + offset, count - offset);
            offset += written;
            if (written == 0) {
                break;
            }
        }
        tud_cdc_write_flush();
    }
}

int main(void) {
    // I2c
    i2c_init(I2C_PORT, I2C_SPEED*1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // UART
    uart_init(UART_ID, UART_BAUD);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(UART_ID, true);

    // GPIO
    for (size_t i = 0; i < sizeof(allowed_gpios); i++)
    {
        gpio_init(allowed_gpios[i]);
        gpio_put(allowed_gpios[i], false);
        gpio_set_dir(allowed_gpios[i], GPIO_OUT);
    }

    tusb_init();

    while (true) {
        tud_task();
        if (tud_cdc_connected()) {
            bridge_usb_to_uart();
            bridge_uart_to_usb();
        } else {
            // Keep draining UART when USB is disconnected so the UART RX FIFO
            // does not fill.
            while (uart_is_readable(UART_ID)) {
                (void) uart_getc(UART_ID);
            }
        }
        tight_loop_contents();
    }
}
