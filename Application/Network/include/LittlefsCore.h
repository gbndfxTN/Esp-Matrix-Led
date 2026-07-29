#pragma once

#include "esp_err.h"
#include "esp_log.h"
#include "esp_littlefs.h"
#include "stdio.h"
#include "stdlib.h"
#include "esp_heap_caps.h"


// initializes the LittleFS file system
esp_err_t init_littlefs(void);
// loads a file from the LittleFS file system into PSRAM
uint8_t* load_file(const char *filepath, size_t *out_size);
// writes data to a file in the LittleFS file system
esp_err_t write_file(const char *filepath, const void *data, size_t size)
// deletes a file from the LittleFS file system
esp_err_t delete_file(const char *filepath)

