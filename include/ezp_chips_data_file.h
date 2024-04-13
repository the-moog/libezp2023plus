#ifndef LIBEZP2023PLUS_EZP_CHIPS_DATA_FILE_H
#define LIBEZP2023PLUS_EZP_CHIPS_DATA_FILE_H

#include <stdint.h>
#include <stddef.h>

/**
 * name - chip type, manufacturer, name      | Split by comma
 * chip_id - 32-bit chip id                  |
 * flash - 32-bit flash size. Power of 2     |
 * flash_page - 16-bit flash size size       |
 *     Power of 2. Range: 1 - 256            |
 * clazz - 8-bit flash type id               | 0 - SPI_FLASH, 1 - 24_EEPROM, 2 - 93_EEPROM,
 *                                           |      3 - 25_EEPROM, 4 - 95_EEPROM
 * algorithm - 8-bit flashing algorithm      | TODO: enum with algorithm ids and names
 * delay - 16-bit some delay value           |
 * extend - 16-bit some extend value         |
 * eeprom - 16-bit some eeprom size.         | In original software you can choose values that do not fit in 16 bits
 * eeprom_page - 8-bit some eeprom page size | In original software you can choose "256" that do not fit in 8 bits
 * voltage - 8-bit supply voltage value      | 0 - 3.3v; 1 - 1.8v; 2 - 5.0v
 *     Range: 0 - 2.                         |
 */
typedef struct {
    char name[48];
    uint32_t chip_id;
    uint32_t flash;
    uint16_t flash_page;
    uint8_t clazz;
    uint8_t algorithm;
    uint16_t delay;
    uint16_t extend;
    uint16_t eeprom;
    uint8_t eeprom_page;
    uint8_t voltage;
} ezp_chip_data;

typedef enum {
    SPI_FLASH = 0,
    EEPROM_24 = 1,
    EEPROM_93 = 2,
    EEPROM_25 = 3, //(not tested)
    EEPROM_95 = 4  //(not tested)
} ezp_flash;

typedef enum {
    VOLTAGE_3V3 = 0,
    VOLTAGE_1V8 = 1,
    VOLTAGE_5V = 2,
} ezp_voltage;

/**
 * Read chips data from file
 * @param data Buffer for data
 * @param file File path
 * @return Entries count if success, or EZP_ERROR_IO or EZP_ERROR_INVALID_FILE when an error occurred
 */
int ezp_chips_data_read(ezp_chip_data **data, const char *file);

/**
 * Write chips data in file
 * @param data Buffer with data
 * @param count Entries count
 * @param file File path
 * @return EZP_OK if success or EZP_ERROR_IO when an error occurred
 */
int ezp_chips_data_write(ezp_chip_data *data, size_t count, const char *file);

#endif //LIBEZP2023PLUS_EZP_CHIPS_DATA_FILE_H
