#pragma once

#include <stddef.h>

#define MATRIX_WIDTH  128
#define MATRIX_HEIGHT 64
#define SCAN_RATE     (MATRIX_HEIGHT / 2) // 32 lignes

#define BIT_R1  (1 << 0)
#define BIT_G1  (1 << 1)
#define BIT_B1  (1 << 2)
#define BIT_R2  (1 << 3)
#define BIT_G2  (1 << 4)
#define BIT_B2  (1 << 5)

#define BIT_A   (1 << 6)
#define BIT_B   (1 << 7)
#define BIT_C   (1 << 8)
#define BIT_D   (1 << 9)
#define BIT_E   (1 << 10)

#define BIT_LAT (1 << 11)
#define BIT_OE  (1 << 12)


void download_gif(const char *url, const char *filepath, size_t *out_size);
