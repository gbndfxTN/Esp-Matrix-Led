#include "GifCore.h"
#include "LittlefsCore.h"
#include "HttpsCore.h"
#include "gifdec.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>


static const char *TAG = "GIFCORE";

#define MATRIX_WIDTH  128
#define MATRIX_HEIGHT 64


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
    size_t frame_bytes = sizeof(uint16_t) + (num_pixels * 8);

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

        uint8_t *bitplanes_ptr = &bin_buffer[write_offset];
        for (int p = 0; p < num_pixels; p++) {
            uint8_t r = rgb_scaled[p * 3 + 0];
            uint8_t g = rgb_scaled[p * 3 + 1];
            uint8_t b = rgb_scaled[p * 3 + 2];

            for (int bit = 0; bit < 8; bit++) {
                uint8_t r_bit = (r >> bit) & 0x01;
                uint8_t g_bit = (g >> bit) & 0x01;
                uint8_t b_bit = (b >> bit) & 0x01;
                bitplanes_ptr[(bit * num_pixels) + p] = (r_bit << 0) | (g_bit << 1) | (b_bit << 2);
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

