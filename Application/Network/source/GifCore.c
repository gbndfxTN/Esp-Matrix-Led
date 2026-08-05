#include "GifCore.h"
#include "LittlefsCore.h"
#include "HttpsCore.h"
#include "gifdec.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>

static const char *TAG = "GIFCORE";

static void scale_rgb24_nearest(const uint8_t *src, int src_w, int src_h, uint8_t *dst, int dst_w, int dst_h) {
    for (int y = 0; y < dst_h; y++) {
        int src_y = (y * src_h) / dst_h;
        for (int x = 0; x < dst_w; x++) {
            int src_x = (x * src_w) / dst_w;
            
            int src_idx = (src_y * src_w + src_x) * 3;
            int dst_idx = (y * dst_w + x) * 3;
            
            dst[dst_idx + 0] = src[src_idx + 0];
            dst[dst_idx + 1] = src[src_idx + 1];
            dst[dst_idx + 2] = src[src_idx + 2];
        }
    }
}

#pragma pack(push, 1)
typedef struct {
    char magic[4];
    uint8_t width;
    uint8_t height;
    uint16_t frame_count;
    uint16_t loop_count;
} BinHeader;
#pragma pack(pop)

void download_gif(const char *url, const char *filepath, size_t *out_size) {
    size_t downloaded_size = 0;

    uint8_t *gif_data = download_gif_to_psram(url, &downloaded_size);
    if (!gif_data) return;

    gd_GIF *gif = gd_open_gif(gif_data, downloaded_size);
    if (!gif) {
        heap_caps_free(gif_data);
        return;
    }

    int num_pixels = MATRIX_WIDTH * MATRIX_HEIGHT;

    size_t hub75_words_per_frame = SCAN_RATE * 8 * (MATRIX_WIDTH + 2);
    size_t frame_bytes = sizeof(uint16_t) + (hub75_words_per_frame * sizeof(uint16_t));

    size_t bin_size = sizeof(BinHeader);
    uint8_t *bin_buffer = (uint8_t *)malloc(bin_size);
    if (!bin_buffer) {
        gd_close_gif(gif);
        heap_caps_free(gif_data);
        return;
    }

    BinHeader *hdr = (BinHeader *)bin_buffer;
    hdr->magic[0] = 'B'; hdr->magic[1] = 'P'; hdr->magic[2] = 'W'; hdr->magic[3] = 'M';
    hdr->width = (uint8_t)MATRIX_WIDTH;
    hdr->height = (uint8_t)MATRIX_HEIGHT;
    hdr->frame_count = 0;
    hdr->loop_count = 1;

    uint8_t *rgb_src = (uint8_t *)malloc(gif->width * gif->height * 3);
    uint8_t *rgb_scaled = (uint8_t *)malloc(num_pixels * 3);

    if (!rgb_src || !rgb_scaled) {
        free(bin_buffer);
        free(rgb_src);
        free(rgb_scaled);
        gd_close_gif(gif);
        heap_caps_free(gif_data);
        return;
    }

    uint16_t frame_counter = 0;

    while (gd_get_frame(gif) > 0) {
        gd_render_frame(gif, rgb_src);

        size_t write_offset = bin_size;
        bin_size += frame_bytes;

        uint8_t *new_bin_ptr = (uint8_t *)realloc(bin_buffer, bin_size);
        if (!new_bin_ptr) {
            free(bin_buffer);
            free(rgb_src);
            free(rgb_scaled);
            gd_close_gif(gif);
            heap_caps_free(gif_data);
            return;
        }
        bin_buffer = new_bin_ptr;

        scale_rgb24_nearest(rgb_src, gif->width, gif->height, rgb_scaled, MATRIX_WIDTH, MATRIX_HEIGHT);

        uint16_t delay_ms = gif->gce.delay ? (gif->gce.delay * 10) : 100;
        memcpy(&bin_buffer[write_offset], &delay_ms, sizeof(delay_ms));
        write_offset += sizeof(delay_ms);

        uint16_t *dma_words = (uint16_t *)&bin_buffer[write_offset];
        uint32_t w_idx = 0;

        for (uint8_t row = 0; row < SCAN_RATE; row++) {
            uint16_t addr_mask = 0;
            if (row & 0x01) addr_mask |= BIT_A;
            if (row & 0x02) addr_mask |= BIT_B;
            if (row & 0x04) addr_mask |= BIT_C;
            if (row & 0x08) addr_mask |= BIT_D;
            if (row & 0x10) addr_mask |= BIT_E;

            for (uint8_t bit = 0; bit < 8; bit++) {
                for (int x = 0; x < MATRIX_WIDTH; x++) {
                    int idx_top = (row * MATRIX_WIDTH + x) * 3;
                    int idx_bot = ((row + SCAN_RATE) * MATRIX_WIDTH + x) * 3;

                    uint8_t r1 = (rgb_scaled[idx_top + 0] >> bit) & 0x01;
                    uint8_t g1 = (rgb_scaled[idx_top + 1] >> bit) & 0x01;
                    uint8_t b1 = (rgb_scaled[idx_top + 2] >> bit) & 0x01;

                    uint8_t r2 = (rgb_scaled[idx_bot + 0] >> bit) & 0x01;
                    uint8_t g2 = (rgb_scaled[idx_bot + 1] >> bit) & 0x01;
                    uint8_t b2 = (rgb_scaled[idx_bot + 2] >> bit) & 0x01;

                    uint16_t word = addr_mask | BIT_OE;

                    if (r1) word |= BIT_R1;
                    if (g1) word |= BIT_G1;
                    if (b1) word |= BIT_B1;
                    if (r2) word |= BIT_R2;
                    if (g2) word |= BIT_G2;
                    if (b2) word |= BIT_B2;

                    dma_words[w_idx++] = word;
                }

                dma_words[w_idx++] = addr_mask | BIT_LAT | BIT_OE;
                dma_words[w_idx++] = addr_mask;
            }
        }

        frame_counter++;
    }

    hdr = (BinHeader *)bin_buffer;
    hdr->frame_count = frame_counter;

    write_file(filepath, bin_buffer, bin_size);

    if (out_size) {
        *out_size = bin_size;
    }

    free(bin_buffer);
    free(rgb_src);
    free(rgb_scaled);
    gd_close_gif(gif);
    heap_caps_free(gif_data);
}