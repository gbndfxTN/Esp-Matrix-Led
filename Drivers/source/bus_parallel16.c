#include "bus_parallel16.h"

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_rom_gpio.h"
#include "esp_rom_sys.h"
#include "driver/gpio.h"
#include "esp_private/periph_ctrl.h"
#include "soc/gpio_sig_map.h"

static const char *TAG = "Bus_Parallel16";

void bus_parallel16_config(bus_parallel16_t *bus, const bus_parallel16_config_t *cfg) {
    if (!bus || !cfg) return;
    bus->cfg = *cfg;
    bus->dev = &LCD_CAM;
}

bool bus_parallel16_init(bus_parallel16_t *bus) {
    if (!bus) return false;

    periph_module_enable(PERIPH_LCD_CAM_MODULE);
    periph_module_reset(PERIPH_LCD_CAM_MODULE);

    LCD_CAM.lcd_user.lcd_reset = 1;
    esp_rom_delay_us(1000);

    LCD_CAM.lcd_clock.lcd_clk_sel = 3;
    LCD_CAM.lcd_clock.lcd_ck_out_edge = 0;
    LCD_CAM.lcd_clock.lcd_ck_idle_edge = 0;
    LCD_CAM.lcd_clock.lcd_clkcnt_n = 1;
    LCD_CAM.lcd_clock.lcd_clk_equ_sysclk = 1;

    uint32_t div_num = 16;
    if (bus->cfg.bus_freq <= 10000000L) {
        div_num = 16;
    } else if (bus->cfg.bus_freq < 20000000L) {
        div_num = 10;
    } else {
        div_num = 7;
    }

    LCD_CAM.lcd_clock.lcd_clkm_div_num = div_num;
    LCD_CAM.lcd_clock.lcd_clkm_div_a = 1;
    LCD_CAM.lcd_clock.lcd_clkm_div_b = 0;

    ESP_LOGI(TAG, "Clock divider: %d | Freq Output: %ld MHz", 
             (int)div_num, 160000000L / div_num / 1000000L);

    // Configuration du format de trame LCD
    LCD_CAM.lcd_ctrl.lcd_rgb_mode_en = 0;      // Mode i8080
    LCD_CAM.lcd_rgb_yuv.lcd_conv_bypass = 0;
    LCD_CAM.lcd_misc.lcd_next_frame_en = 0;
    LCD_CAM.lcd_misc.lcd_bk_en = 1;
    LCD_CAM.lcd_data_dout_mode.val = 0;
    LCD_CAM.lcd_user.lcd_always_out_en = 1;
    LCD_CAM.lcd_user.lcd_8bits_order = 0;
    LCD_CAM.lcd_user.lcd_bit_order = 0;
    LCD_CAM.lcd_user.lcd_2byte_en = 1;
    LCD_CAM.lcd_user.lcd_dummy = 1;
    LCD_CAM.lcd_user.lcd_dummy_cyclelen = 1;
    LCD_CAM.lcd_user.lcd_cmd = 0;

    for (int i = 0; i < 16; i++) {
        if (bus->cfg.pin_data[i] >= 0) {
            esp_rom_gpio_connect_out_signal(bus->cfg.pin_data[i], LCD_DATA_OUT0_IDX + i, false, false);
            gpio_func_sel((gpio_num_t)bus->cfg.pin_data[i], PIN_FUNC_GPIO);
            gpio_set_drive_capability((gpio_num_t)bus->cfg.pin_data[i], GPIO_DRIVE_CAP_3);
        }
    }

    if (bus->cfg.pin_wr >= 0) {
        esp_rom_gpio_connect_out_signal(bus->cfg.pin_wr, LCD_PCLK_IDX, bus->cfg.invert_pclk, false);
        gpio_func_sel((gpio_num_t)bus->cfg.pin_wr, PIN_FUNC_GPIO);
        gpio_set_drive_capability((gpio_num_t)bus->cfg.pin_wr, GPIO_DRIVE_CAP_3);
    }

    gdma_channel_alloc_config_t dma_chan_config = {
        .sibling_chan = NULL,
        .direction = GDMA_CHANNEL_DIRECTION_TX,
        .flags = { .reserve_sibling = 0 }
    };

    esp_err_t err = gdma_new_ahb_channel(&dma_chan_config, &bus->dma_chan);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erreur allocation canal GDMA : %s", esp_err_to_name(err));
        return false;
    }

    gdma_connect(bus->dma_chan, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_LCD, 0));

    gdma_strategy_config_t strategy_config = {
        .owner_check = false,
        .auto_update_desc = false
    };
    gdma_apply_strategy(bus->dma_chan, &strategy_config);

    gdma_transfer_config_t transfer_config = {
        .max_data_burst_size = 32,
        .access_ext_mem = false
    };
    gdma_config_transfer(bus->dma_chan, &transfer_config);

    while (LCD_CAM.lcd_user.lcd_start);

    gdma_reset(bus->dma_chan);
    esp_rom_delay_us(1000);

    LCD_CAM.lcd_user.lcd_dout = 1;
    LCD_CAM.lcd_user.lcd_update = 1;
    LCD_CAM.lcd_misc.lcd_afifo_reset = 1;

    return true;
}

void bus_parallel16_release(bus_parallel16_t *bus) {
    if (!bus) return;

    if (bus->dmadesc_a) {
        heap_caps_free(bus->dmadesc_a);
        bus->dmadesc_a = NULL;
    }
    if (bus->dmadesc_b) {
        heap_caps_free(bus->dmadesc_b);
        bus->dmadesc_b = NULL;
    }
    bus->dmadesc_count = 0;

    if (bus->dma_chan) {
        gdma_del_channel(bus->dma_chan);
        bus->dma_chan = NULL;
    }
}

void bus_parallel16_enable_double_dma_desc(bus_parallel16_t *bus) {
    if (!bus) return;
    ESP_LOGI(TAG, "Support du second buffer DMA active.");
    bus->double_dma_buffer = true;
}

bool bus_parallel16_allocate_dma_desc_memory(bus_parallel16_t *bus, size_t len) {
    if (!bus) return false;

    if (bus->dmadesc_a) heap_caps_free(bus->dmadesc_a);
    if (bus->dmadesc_b) heap_caps_free(bus->dmadesc_b);

    bus->dmadesc_count = len;

    bus->dmadesc_a = (dma_descriptor_t *)heap_caps_malloc(sizeof(dma_descriptor_t) * len, MALLOC_CAP_DMA);
    if (!bus->dmadesc_a) {
        ESP_LOGE(TAG, "Erreur : Malloc echoue pour dmadesc_a.");
        return false;
    }

    if (bus->double_dma_buffer) {
        bus->dmadesc_b = (dma_descriptor_t *)heap_caps_malloc(sizeof(dma_descriptor_t) * len, MALLOC_CAP_DMA);
        if (!bus->dmadesc_b) {
            ESP_LOGE(TAG, "Erreur : Malloc echoue pour dmadesc_b.");
            bus->double_dma_buffer = false;
        }
    }

    bus->dmadesc_a_idx = 0;
    bus->dmadesc_b_idx = 0;

    return true;
}

void bus_parallel16_create_dma_desc_link(bus_parallel16_t *bus, void *data, size_t size, bool use_buffer_b) {
    if (!bus) return;

    const size_t MAX_DMA_LEN = (4096 - 4);
    if (size > MAX_DMA_LEN) {
        size = MAX_DMA_LEN;
        ESP_LOGW(TAG, "Taille du payload superieure a MAX_DMA_LEN, tronquee.");
    }

    if (use_buffer_b) {
        if (bus->dmadesc_b_idx >= bus->dmadesc_count) return;

        bus->dmadesc_b[bus->dmadesc_b_idx].dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
        bus->dmadesc_b[bus->dmadesc_b_idx].dw0.suc_eof = (bus->dmadesc_b_idx == (bus->dmadesc_count - 1));
        bus->dmadesc_b[bus->dmadesc_b_idx].dw0.size = size;
        bus->dmadesc_b[bus->dmadesc_b_idx].dw0.length = size;
        bus->dmadesc_b[bus->dmadesc_b_idx].buffer = data;

        if (bus->dmadesc_b_idx == bus->dmadesc_count - 1) {
            bus->dmadesc_b[bus->dmadesc_b_idx].next = &bus->dmadesc_b[0];
        } else {
            bus->dmadesc_b[bus->dmadesc_b_idx].next = &bus->dmadesc_b[bus->dmadesc_b_idx + 1];
        }

        bus->dmadesc_b_idx++;
    } else {
        if (bus->dmadesc_a_idx >= bus->dmadesc_count) return;

        bus->dmadesc_a[bus->dmadesc_a_idx].dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
        bus->dmadesc_a[bus->dmadesc_a_idx].dw0.suc_eof = (bus->dmadesc_a_idx == (bus->dmadesc_count - 1));
        bus->dmadesc_a[bus->dmadesc_a_idx].dw0.size = size;
        bus->dmadesc_a[bus->dmadesc_a_idx].dw0.length = size;
        bus->dmadesc_a[bus->dmadesc_a_idx].buffer = data;

        if (bus->dmadesc_a_idx == bus->dmadesc_count - 1) {
            bus->dmadesc_a[bus->dmadesc_a_idx].next = &bus->dmadesc_a[0];
        } else {
            bus->dmadesc_a[bus->dmadesc_a_idx].next = &bus->dmadesc_a[bus->dmadesc_a_idx + 1];
        }

        bus->dmadesc_a_idx++;
    }
}

void bus_parallel16_dma_transfer_start(bus_parallel16_t *bus) {
    if (!bus || !bus->dmadesc_a) return;

    gdma_start(bus->dma_chan, (intptr_t)&bus->dmadesc_a[0]);
    esp_rom_delay_us(100);
    LCD_CAM.lcd_user.lcd_start = 1;
}

void bus_parallel16_dma_transfer_stop(bus_parallel16_t *bus) {
    if (!bus) return;

    LCD_CAM.lcd_user.lcd_reset = 1;
    LCD_CAM.lcd_user.lcd_update = 1;
    gdma_stop(bus->dma_chan);
}

void bus_parallel16_flip_dma_output_buffer(bus_parallel16_t *bus, int back_buffer_id) {
    if (!bus || bus->dmadesc_count == 0) return;

    if (back_buffer_id == 1) {
        bus->dmadesc_b[bus->dmadesc_count - 1].next = &bus->dmadesc_b[0];
        bus->dmadesc_a[bus->dmadesc_count - 1].next = &bus->dmadesc_b[0];
    } else {
        bus->dmadesc_a[bus->dmadesc_count - 1].next = &bus->dmadesc_a[0];
        bus->dmadesc_b[bus->dmadesc_count - 1].next = &bus->dmadesc_a[0];
    }
}