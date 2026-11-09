#include "setup_storage.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"
#include "pico/flash.h"

// Two independent sectors: erase/program only the older copy. A CRC and
// sequence select the latest complete record after a reset or interrupted write.
#define RECORD_DATA_OFFSET 12
#define RECORD_CRC_OFFSET (FLASH_PAGE_SIZE - 4)
#define RECORD_MAGIC 0x534c4948u // "HILS", little-endian

_Static_assert(SETUP_FLASH_OFFSET % FLASH_SECTOR_SIZE == 0, "Unaligned setup storage");
_Static_assert(SETUP_FLASH_OFFSET + 2 * FLASH_SECTOR_SIZE == PICO_FLASH_SIZE_BYTES,
               "Setup storage must occupy the last two flash sectors");
_Static_assert(RECORD_DATA_OFFSET + SETUP_DATA_SIZE <= RECORD_CRC_OFFSET,
               "Setup record exceeds one flash page");

static uint32_t get_u32(const uint8_t *data) {
    return (uint32_t) data[0] | ((uint32_t) data[1] << 8) |
           ((uint32_t) data[2] << 16) | ((uint32_t) data[3] << 24);
}

static void put_u32(uint8_t *data, uint32_t value) {
    for (unsigned int i = 0; i < 4; i++) {
        data[i] = (uint8_t) (value >> (8 * i));
    }
}

static uint32_t crc32(const uint8_t *data, size_t length) {
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (unsigned int bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0);
        }
    }
    return ~crc;
}

static bool details_valid(const setup_details_t *details) {
    return details != NULL && details->description[0] != '\0' &&
           details->serial_number[0] != '\0' &&
           memchr(details->description, '\0', SETUP_DESCRIPTION_SIZE) != NULL &&
           memchr(details->serial_number, '\0', SETUP_SERIAL_SIZE) != NULL;
}

static const uint8_t *slot_address(unsigned int slot) {
    return (const uint8_t *) (XIP_BASE + SETUP_FLASH_OFFSET + slot * FLASH_SECTOR_SIZE);
}

static bool read_slot(unsigned int slot, setup_details_t *details, uint32_t *sequence) {
    const uint8_t *record = slot_address(slot);
    if (get_u32(record) != RECORD_MAGIC || record[4] != SETUP_FORMAT_VERSION ||
        record[5] != 0 || record[6] != (SETUP_DATA_SIZE & 0xff) ||
        record[7] != (SETUP_DATA_SIZE >> 8) ||
        get_u32(record + RECORD_CRC_OFFSET) != crc32(record, RECORD_CRC_OFFSET)) {
        return false;
    }
    const uint8_t *data = record + RECORD_DATA_OFFSET;
    memcpy(details->description, data, SETUP_DESCRIPTION_SIZE);
    memcpy(details->serial_number, data + SETUP_DESCRIPTION_SIZE, SETUP_SERIAL_SIZE);
    *sequence = get_u32(record + 8);
    return details_valid(details);
}

static int latest_slot(setup_details_t *details, uint32_t *sequence) {
    int latest = -1;
    for (unsigned int slot = 0; slot < 2; slot++) {
        setup_details_t candidate;
        uint32_t candidate_sequence;
        if (read_slot(slot, &candidate, &candidate_sequence) &&
            (latest < 0 || (candidate_sequence != *sequence &&
                           candidate_sequence - *sequence < 0x80000000u))) {
            latest = (int) slot;
            *details = candidate;
            *sequence = candidate_sequence;
        }
    }
    return latest;
}

setup_storage_status_t setup_storage_read(setup_details_t *details) {
    if (details == NULL) {
        return SETUP_STORAGE_ERROR_ARGUMENT;
    }
    setup_details_t candidate;
    uint32_t sequence = 0;
    if (latest_slot(&candidate, &sequence) >= 0) {
        *details = candidate;
        return SETUP_STORAGE_OK;
    }
    for (unsigned int slot = 0; slot < 2; slot++) {
        const uint8_t *record = slot_address(slot);
        for (size_t i = 0; i < FLASH_PAGE_SIZE; i++) {
            if (record[i] != 0xff) {
                return SETUP_STORAGE_ERROR_CORRUPT;
            }
        }
    }
    return SETUP_STORAGE_EMPTY;
}

typedef struct {
    uint32_t offset;
    uint8_t page[FLASH_PAGE_SIZE];
} flash_write_t;

static void __not_in_flash_func(write_record)(void *param) {
    const flash_write_t *write = param;
    flash_range_erase(write->offset, FLASH_SECTOR_SIZE);
    flash_range_program(write->offset, write->page, FLASH_PAGE_SIZE);
}

setup_storage_status_t setup_storage_write(const setup_details_t *details) {
    if (!details_valid(details)) {
        return SETUP_STORAGE_ERROR_ARGUMENT;
    }
    setup_details_t previous;
    uint32_t sequence = 0;
    int latest = latest_slot(&previous, &sequence);
    if (latest >= 0 && strcmp(previous.description, details->description) == 0 &&
        strcmp(previous.serial_number, details->serial_number) == 0) {
        return SETUP_STORAGE_OK; // Avoid wearing flash for unchanged settings.
    }

    unsigned int target = latest == 0 ? 1 : 0;
    flash_write_t write = { .offset = SETUP_FLASH_OFFSET + target * FLASH_SECTOR_SIZE };
    put_u32(write.page, RECORD_MAGIC);
    write.page[4] = SETUP_FORMAT_VERSION;
    write.page[6] = SETUP_DATA_SIZE & 0xff;
    write.page[7] = SETUP_DATA_SIZE >> 8;
    put_u32(write.page + 8, sequence + 1);
    uint8_t *data = write.page + RECORD_DATA_OFFSET;
    memcpy(data, details->description, strlen(details->description));
    memcpy(data + SETUP_DESCRIPTION_SIZE, details->serial_number, strlen(details->serial_number));
    put_u32(write.page + RECORD_CRC_OFFSET, crc32(write.page, RECORD_CRC_OFFSET));

    // The buffer is in RAM; the SDK disables interrupts while XIP is unavailable.
    if (flash_safe_execute(write_record, &write, 1000) != PICO_OK ||
        memcmp(slot_address(target), write.page, FLASH_PAGE_SIZE) != 0) {
        return SETUP_STORAGE_ERROR_FLASH;
    }
    return SETUP_STORAGE_OK;
}
