#pragma once

#include <stdlib.h>
#include <stdint.h>

uint8_t* download_gif_to_psram(const char *url, size_t *out_size);