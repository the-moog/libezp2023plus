#include "ezp_prog.h"
#include "ezp_errors.h"
#include <libusb-1.0/libusb.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>

#define VID 0x1fc8
#define PID 0x310b
//#define PRINT_HEX

#ifdef PRINT_HEX

static void hexDump(const char *title, const void *addr, const int len) {
    fprintf(stderr, "%s:\n", title);

    if (len <= 0) {
        fprintf(stderr, "  Invalid length: %d\n", len);
        return;
    }

    const uint8_t *ptr = addr;
    for (int i = 0; i < len; ++i) {
        if (i != 0 && (i % 16) == 0) printf("\n");
        fprintf(stderr, " %02x", ptr[i]);
    }
    fprintf(stderr, "\n");
}

#else //PRINT_HEX
#define hexDump(...) while(0)
#endif //PRINT_HEX

#define CHECK_RESULT(x, lambda) if (x != LIBUSB_SUCCESS) { \
                            fprintf(stderr, "LIBUSB ERROR: %s\n", libusb_strerror((enum libusb_error) x)); \
                            lambda \
                        }

#define COMMAND_RESET 0x0108
#define COMMAND_START_TRANSACTION 0x5
#define COMMAND_CHECK_CHIP 0x9
#define COMMAND_SET_CHIP_DATA 0x7
#define COMMAND_START_ERASING 0x0102
#define COMMAND_ERASE 0x0a

typedef struct {
    uint16_t command;
    uint8_t clazz;
    uint8_t algorithm;
    uint16_t flash_page_size; //1,2,4...256
    uint16_t delay;
    uint32_t flash_size;
    uint32_t chip_id;
    uint8_t speed;
    uint8_t dummy2[11];
    uint8_t voltage;
    uint8_t dummy4[35];
} __attribute__((packed)) usb_packet;

typedef struct {
    ezp_status_callback callback;
    void *user_data;
} internal_user_data;

static libusb_hotplug_callback_handle hotplug_cb_handle;
static volatile int status_listener_running = 0;
static volatile int status_listener_finished = 0;

static void usb_packet_flip(usb_packet *packet) {
    packet->command = htons(packet->command);
    packet->flash_page_size = htons(packet->flash_page_size);
    packet->delay = htons(packet->delay);
    packet->flash_size = htonl(packet->flash_size);
    packet->chip_id = htonl(packet->chip_id);
}

int ezp_init() {
    return libusb_init_context(NULL, NULL, 0);
}

ezp_programmer *ezp_find_programmer() {
    libusb_device_handle *handle = libusb_open_device_with_vid_pid(NULL, VID, PID);
    if (!handle) return NULL;

    ezp_programmer *ezp_prog = (ezp_programmer *) malloc(sizeof(ezp_programmer));
    ezp_prog->handle = handle;

    return ezp_prog;
}

static int send_to_programmer(libusb_device_handle *handle, const uint8_t *data, int size, uint8_t isData) {
    hexDump("send_to_programmer", data, size);
    int actual_size;
    int r = libusb_bulk_transfer(handle, LIBUSB_ENDPOINT_OUT | (isData ? 1 : 2),
                                 data, size, &actual_size, 1000);
    if (actual_size != size) fprintf(stderr, "Warning! actual_size != size");
    return r;
}

static int recv_from_programmer(libusb_device_handle *handle, uint8_t *data, int size) {
    int actual_size;
    int r = libusb_bulk_transfer(handle, LIBUSB_ENDPOINT_IN | 2,
                                 data, size, &actual_size, 1000);
    hexDump("recv_from_programmer", data, size);
    if (actual_size != size) fprintf(stderr, "Warning! actual_size != size");
    return r;
}

int
ezp_read_flash(ezp_programmer *programmer, uint8_t **data, ezp_chip_data *chip_data, ezp_speed speed,
               ezp_callback callback) {
    if (chip_data->flash % chip_data->flash_page != 0)
        return EZP_FLASH_SIZE_OR_PAGE_INVALID;

    *data = (uint8_t *) malloc(chip_data->flash);

    //send second packet with chip data 00 07
    usb_packet packet = {
            .command = COMMAND_SET_CHIP_DATA,
            .clazz = chip_data->clazz,
            .algorithm = chip_data->algorithm,
            .flash_page_size = chip_data->flash_page,
            .delay = chip_data->delay,
            .flash_size = chip_data->flash,
            .chip_id = chip_data->chip_id,
            .speed = speed, //in original software: if chip is eeprom_24, then this field equals voltage.
                            //maybe speed value is valid only for spi_flash
            .voltage = chip_data->voltage
    };
    usb_packet_flip(&packet);
    int ret = send_to_programmer(programmer->handle, (uint8_t *) &packet, sizeof(usb_packet), 0);
    CHECK_RESULT(ret, {
        free(*data);
        *data = NULL;
        return EZP_LIBUSB_ERROR;
    })
    //read response
    ret = recv_from_programmer(programmer->handle, (uint8_t *) &packet, sizeof(usb_packet));
    CHECK_RESULT(ret, {
        free(*data);
        *data = NULL;
        return EZP_LIBUSB_ERROR;
    })

    //send begin data transaction packet 00 05
    memset(&packet, 0, sizeof(usb_packet));
    packet.command = COMMAND_START_TRANSACTION;
    usb_packet_flip(&packet);
    ret = send_to_programmer(programmer->handle, (uint8_t *) &packet, sizeof(usb_packet), 0);
    CHECK_RESULT(ret, {
        free(*data);
        *data = NULL;
        return EZP_LIBUSB_ERROR;
    })
    //read response
    ret = recv_from_programmer(programmer->handle, (uint8_t *) &packet, sizeof(usb_packet));
    CHECK_RESULT(ret, {
        free(*data);
        *data = NULL;
        return EZP_LIBUSB_ERROR;
    })

    //loop
    uint16_t right_page_size = chip_data->flash_page > 64 ? chip_data->flash_page : 64;
    size_t blocks_count = chip_data->flash / right_page_size;
    uint8_t *ptr = *data;
    for (size_t i = 0; i < blocks_count; ++i) {
        if (callback) callback(i * right_page_size, chip_data->flash);
        ret = recv_from_programmer(programmer->handle, ptr, right_page_size);
        CHECK_RESULT(ret, {
            free(*data);
            *data = NULL;
            return EZP_LIBUSB_ERROR;
        })
        ptr += right_page_size;
    }

    //send reset packet 01 08
    memset(&packet, 0, sizeof(usb_packet));
    packet.command = COMMAND_RESET;
    usb_packet_flip(&packet);
    ret = send_to_programmer(programmer->handle, (uint8_t *) &packet, sizeof(usb_packet), 0);
    CHECK_RESULT(ret, {//error after read, so data may be valid
        return EZP_LIBUSB_ERROR;
    })
    //read response
    ret = recv_from_programmer(programmer->handle, (uint8_t *) &packet, sizeof(usb_packet));
    CHECK_RESULT(ret, {//error after read, so data may be valid
        return EZP_LIBUSB_ERROR;
    })

    return EZP_OK;
}

int ezp_write_flash(ezp_programmer *programmer, const uint8_t *data, ezp_chip_data *chip_data, ezp_speed speed,
                    ezp_callback callback) {
    if (chip_data->flash % chip_data->flash_page != 0)
        return EZP_FLASH_SIZE_OR_PAGE_INVALID;

    usb_packet packet = {
            .command = COMMAND_SET_CHIP_DATA,
            .clazz = chip_data->clazz,
            .algorithm = chip_data->algorithm,
            .flash_page_size = chip_data->flash_page,
            .delay = chip_data->delay,
            .flash_size = chip_data->flash,
            .chip_id = chip_data->chip_id,
            .speed = speed, //in original software: if chip is eeprom_24, then this field equals voltage
                            //maybe speed value is valid only for spi_flash
            .voltage = chip_data->voltage
    };
    usb_packet_flip(&packet);
    int ret = send_to_programmer(programmer->handle, (uint8_t *) &packet, sizeof(usb_packet), 0);
    CHECK_RESULT(ret, {
        return EZP_LIBUSB_ERROR;
    })
    //read response
    ret = recv_from_programmer(programmer->handle, (uint8_t *) &packet, sizeof(usb_packet));
    CHECK_RESULT(ret, {
        return EZP_LIBUSB_ERROR;
    })

    //send begin data transaction packet 00 05
    memset(&packet, 0, sizeof(usb_packet));
    packet.command = COMMAND_START_TRANSACTION;
    usb_packet_flip(&packet);
    ret = send_to_programmer(programmer->handle, (uint8_t *) &packet, sizeof(usb_packet), 0);
    CHECK_RESULT(ret, {
        return EZP_LIBUSB_ERROR;
    })

    //loop
    uint16_t right_page_size = chip_data->flash_page > 64 ? chip_data->flash_page : 64;
    size_t blocks_count = chip_data->flash / right_page_size;
    const uint8_t *ptr = data;
    for (size_t i = 0; i < blocks_count; ++i) {
        ret = send_to_programmer(programmer->handle, ptr, right_page_size, 1);
        CHECK_RESULT(ret, {
            return EZP_LIBUSB_ERROR;
        })
        ptr += right_page_size;
        if (callback) callback(i * right_page_size, chip_data->flash);
    }

    //send reset packet 01 08
    memset(&packet, 0, sizeof(usb_packet));
    packet.command = COMMAND_RESET;
    usb_packet_flip(&packet);
    ret = send_to_programmer(programmer->handle, (uint8_t *) &packet, sizeof(usb_packet), 0);
    CHECK_RESULT(ret, {
        return EZP_LIBUSB_ERROR;
    })

    return EZP_OK;
}

int ezp_test_flash(ezp_programmer *programmer, ezp_flash *type, uint32_t *chip_id) {
    //send first packet with chip data 00 09
    usb_packet packet = {
            .command = COMMAND_CHECK_CHIP
    };
    usb_packet_flip(&packet);
    int ret = send_to_programmer(programmer->handle, (uint8_t *) &packet, sizeof(usb_packet), 0);
    CHECK_RESULT(ret, {
        return EZP_LIBUSB_ERROR;
    })
    //read response
    uint8_t buffer[64];
    ret = recv_from_programmer(programmer->handle, buffer, sizeof(usb_packet));
    CHECK_RESULT(ret, {
        return EZP_LIBUSB_ERROR;
    })

    //send reset packet 01 08
    memset(&packet, 0, sizeof(usb_packet));
    packet.command = COMMAND_RESET;
    usb_packet_flip(&packet);
    ret = send_to_programmer(programmer->handle, (uint8_t *) &packet, sizeof(usb_packet), 0);
    CHECK_RESULT(ret, {
        return EZP_LIBUSB_ERROR;
    })

    *type = buffer[0];
    *chip_id = htonl(*(uint32_t *) (buffer)) & ~(0xff << 24);
    uint32_t programmer_code = htonl(*(uint32_t *) (buffer + 60));


    if (programmer_code == 0x9a7336bd) {
        if (*type != 0) {
            *type -= 1;
            return EZP_OK;
        } else {
            return EZP_FLASH_NOT_DETECTED;
        }
    } else {
        return EZP_INVALID_DATA_FROM_PROGRAMMER;
    }
}

static int LIBUSB_CALL
hotplug_callback(libusb_context *ctx, libusb_device *dev, libusb_hotplug_event event, void *iud) {
    (void) ctx;
    struct libusb_device_descriptor desc;
    libusb_device_handle *handle = NULL;

    ezp_status_callback callback = ((internal_user_data*) iud)->callback;
    void *user_data = ((internal_user_data*) iud)->user_data;

    int res = libusb_get_device_descriptor(dev, &desc);
    if (LIBUSB_SUCCESS == res) {
        switch (event) {
            case LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED:
                res = libusb_open(dev, &handle);
                if (handle) libusb_close(handle);
                if (LIBUSB_SUCCESS != res) {
                    callback(EZP_CONNECTED, user_data); //callback connected
                } else {
                    callback(EZP_READY, user_data); //callback ready
                }
                break;
            case LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT:
                callback(EZP_DISCONNECTED, user_data); //callback disconnected
                break;
        }
    } else {
        switch (event) {
            case LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED:
                fprintf(stderr, "Device attached, but...\n");
                break;
            case LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT:
                fprintf(stderr, "Device detached, but...\n");
                break;
        }
        fprintf(stderr, "Error getting device descriptor: %s\n", libusb_strerror((enum libusb_error) res));
        callback(EZP_DISCONNECTED, user_data); //callback disconnected
    }

    return 0;
}

int ezp_listen_programmer_status(ezp_status_callback callback, void *user_data) {
    if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
        return EZP_HOTPLUG_UNSUPPORTED;
    }

    internal_user_data iud;
    iud.callback = callback;
    iud.user_data = user_data;

    int res = libusb_hotplug_register_callback(NULL,
                                               LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
                                               0, VID, PID, LIBUSB_HOTPLUG_MATCH_ANY, hotplug_callback, &iud,
                                               &hotplug_cb_handle);
    CHECK_RESULT(res, {
        fprintf(stderr, "Error: libusb_hotplug_register_callback\n");
        return EZP_LIBUSB_ERROR;
    })

    status_listener_running = 1;
    while (status_listener_running) {
        res = libusb_handle_events(NULL);
        CHECK_RESULT(res, {
            fprintf(stderr, "Error: libusb_handle_events\n");
            return EZP_LIBUSB_ERROR;
        })
    }
    status_listener_finished = 1;
    return 0;
}

void ezp_free_programmer(ezp_programmer *programmer) {
    libusb_close(programmer->handle);
    free(programmer);
}

void ezp_free() {
    if (status_listener_running) {
        status_listener_running = 0;
        libusb_hotplug_deregister_callback(NULL, hotplug_cb_handle);
        while (!status_listener_finished) __asm__("nop");
        status_listener_finished = 0;
    }
    libusb_exit(NULL);
}