#pragma once
#include <stdlib.h>
#include <stdint.h>
#include "esp_err.h"

uint8_t* getGifPsram();
esp_err_t mallocGifPsram(size_t size);
esp_err_t reallocGifPsram(size_t new_size);
void freeGifPsram();
