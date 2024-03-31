#include "ezp_chips_data_file.h"
#include <stdio.h>
#include <malloc.h>
#include "ezp_errors.h"

static size_t get_file_size(FILE *f) {
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    return size;
}

int ezp_chips_data_read(ezp_chip_data **data, const char *file) {
    FILE *data_file = fopen(file, "r");
    if (!data_file) return EZP_ERROR_IO;

    size_t file_size = get_file_size(data_file);
    if (file_size == 0 || file_size % sizeof(ezp_chip_data) != 0) return EZP_ERROR_INVALID_FILE;
    size_t entries_count = file_size / sizeof(ezp_chip_data) - 1; // last entry is empty

    *data = malloc(entries_count * sizeof(ezp_chip_data));
    ezp_chip_data *ptr = *data;
    for (size_t i = 0; i < entries_count; ++i) {
        size_t size = fread(ptr, sizeof(ezp_chip_data), 1, data_file);
        if (size == 0) {
            free(*data);
            *data = NULL;
            return EZP_ERROR_IO;
        }
        ptr++;
    }
    if (fclose(data_file) != 0) return EZP_ERROR_IO;
    return (int) entries_count;
}

int ezp_chips_data_write(ezp_chip_data *data, size_t count, const char *file) {
    FILE *data_file = fopen(file, "w");
    if (!data_file) return EZP_ERROR_IO;

    for (size_t i = 0; i < count; ++i) {
        fwrite(data++, sizeof(ezp_chip_data), 1, data_file);
    }
    ezp_chip_data empty = {};
    fwrite(&empty, sizeof(ezp_chip_data), 1, data_file);

    if (fclose(data_file) != 0) return EZP_ERROR_IO;
    return EZP_OK;
}