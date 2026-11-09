#ifndef SETUP_STORAGE_H
#define SETUP_STORAGE_H

#include <stdint.h>

#define SETUP_DESCRIPTION_SIZE 128
#define SETUP_SERIAL_SIZE 32
#define SETUP_DATA_SIZE (SETUP_DESCRIPTION_SIZE + SETUP_SERIAL_SIZE)
#define SETUP_FORMAT_VERSION 1

typedef struct {
    char description[SETUP_DESCRIPTION_SIZE];
    char serial_number[SETUP_SERIAL_SIZE];
} setup_details_t;

typedef enum {
    SETUP_STORAGE_OK = 0,
    SETUP_STORAGE_EMPTY = 1,
    SETUP_STORAGE_ERROR_ARGUMENT = 2,
    SETUP_STORAGE_ERROR_FLASH = 3,
    SETUP_STORAGE_ERROR_CORRUPT = 4
} setup_storage_status_t;

setup_storage_status_t setup_storage_read(setup_details_t *details);
setup_storage_status_t setup_storage_write(const setup_details_t *details);

#endif
