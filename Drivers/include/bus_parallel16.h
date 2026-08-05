#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"
#include "esp_private/gdma.h"
#include "soc/lcd_cam_struct.h"
#include "hal/dma_types.h"

typedef struct {
    uint32_t bus_freq;
    int8_t pin_wr;
    int8_t pin_rd;
    int8_t pin_rs;
    bool invert_pclk;
    int8_t pin_data[16];
} bus_parallel16_config_t;

typedef struct {
    bus_parallel16_config_t cfg;
    volatile lcd_cam_dev_t *dev;
    gdma_channel_handle_t dma_chan;
    
    uint32_t dmadesc_count;
    uint32_t dmadesc_a_idx;
    uint32_t dmadesc_b_idx;

    dma_descriptor_t *dmadesc_a;
    dma_descriptor_t *dmadesc_b;

    bool double_dma_buffer;
} bus_parallel16_t;

void bus_parallel16_config(bus_parallel16_t *bus, const bus_parallel16_config_t *cfg);
bool bus_parallel16_init(bus_parallel16_t *bus);
void bus_parallel16_release(bus_parallel16_t *bus);

void bus_parallel16_enable_double_dma_desc(bus_parallel16_t *bus);
bool bus_parallel16_allocate_dma_desc_memory(bus_parallel16_t *bus, size_t len);
void bus_parallel16_create_dma_desc_link(bus_parallel16_t *bus, void *data, size_t size, bool use_buffer_b);

void bus_parallel16_dma_transfer_start(bus_parallel16_t *bus);
void bus_parallel16_dma_transfer_stop(bus_parallel16_t *bus);
void bus_parallel16_flip_dma_output_buffer(bus_parallel16_t *bus, int back_buffer_id);