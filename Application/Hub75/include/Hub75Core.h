#pragma once

#include <stdint.h>

void app_core1_init(void);
void app_core1_run(void);

void app_core1_display_gif(const char *mode_code);
void app_core1_set_brightness(uint8_t pct);
void app_core1_set_duration(uint16_t minutes);
void app_core1_set_background(const char *code);
void app_core1_playlist_set(const char *data);
void app_core1_playlist_control(const char *cmd);
void app_core1_playlist_publish_state(void);
