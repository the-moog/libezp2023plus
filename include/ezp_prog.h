#ifndef LIBEZP2023PLUS_EZP_PROG_H
#define LIBEZP2023PLUS_EZP_PROG_H

#include "ezp_chips_data_file.h"
#include <libusb-1.0/libusb.h>

typedef struct {
    libusb_device_handle *handle;
} ezp_programmer;

typedef enum {
    SPEED_12MHZ = 0,
    SPEED_6MHZ = 1,
    SPEED_3MHZ = 2,
    SPEED_1_5MHZ = 3,
    SPEED_750KHZ = 4,
    SPEED_375KHZ = 5,
} ezp_speed;

/**
 * EZP_READY - Programmer connected and user has access to it
 * EZP_CONNECTED - Programmer connected but user has no access in
 * EZP_DISCONNECTED - Programmer disconnected
 */
typedef enum {
    EZP_READY,
    EZP_CONNECTED,
    EZP_DISCONNECTED
} ezp_status;

typedef void (*ezp_callback)(uint32_t current, uint32_t max);
typedef void (*ezp_status_callback)(ezp_status status);

/**
 * Init USB communication
 * @return 0 if success, or some libusb error code
 */
int ezp_init();

/**
 * Create new ezp_programmer instance
 * @return new ezp_programmer instance or NULL if programmer is not connected
 */
ezp_programmer *ezp_find_programmer();

/**
 * Read data from flash
 * @param programmer
 * @param data buffer for data
 * @param chip_data information about chip
 * @param speed reading speed
 * @param callback progress callback
 * @return EZP_OK when success. EZP_FLASH_SIZE_OR_PAGE_INVALID or EZP_LIBUSB_ERROR when an error occurred
 */
int ezp_read_flash(ezp_programmer *programmer, uint8_t **data, ezp_chip_data *chip_data, ezp_speed speed, ezp_callback callback);

/**
 * Write data into flash
 * @param programmer
 * @param data buffer with data
 * @param chip_data information about chip
 * @param speed writing speed
 * @param callback progress callback
 * @return EZP_OK when success. EZP_FLASH_SIZE_OR_PAGE_INVALID or EZP_LIBUSB_ERROR when an error occurred
 */
int ezp_write_flash(ezp_programmer *programmer, const uint8_t *data, ezp_chip_data *chip_data, ezp_speed speed, ezp_callback callback);

/**
 * Test chip model
 * @param programmer
 * @param type chip type
 * @param chip_id chip ID. For SPI_FLASH only
 * @return EZP_OK when success. EZP_FLASH_NOT_DETECTED, EZP_INVALID_DATA_FROM_PROGRAMMER or EZP_LIBUSB_ERROR when an error occurred
 */
int ezp_test_flash(ezp_programmer *programmer, ezp_flash *type, uint32_t *chip_id);

/**
 * Blocking function that listens status of programmer and notifies about changes via callback
 * @param callback
 * @return EZP_HOTPLUG_UNSUPPORTED, EZP_LIBUSB_ERROR. should never return if everything is ok
 */
int ezp_listen_programmer_status(ezp_status_callback callback);

/**
 * Stop connection with programmer and free resources
 * @param programmer
 */
void ezp_free_programmer(ezp_programmer *programmer);

/**
 * Stop USB communication
 */
void ezp_free();

#endif //LIBEZP2023PLUS_EZP_PROG_H
