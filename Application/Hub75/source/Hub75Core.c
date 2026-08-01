#include "Hub75Core.h"
#include "esp_log.h"

void app_core1_init(void)
{
}

void app_core1_run(void)
{
    while (1)
    {
        char user[32], arg[256];
        uint8_t *gif_data;
        int gif_len;
        int cmd = shared_poll_cmd(user, arg, &gif_data, &gif_len);

        if (cmd == SHARED_CMD_PLAY_GIF)
        {
            // TODO: start playback from SPIFFS
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_core1_display_gif(const char *mode_code)
{

}

void app_core1_set_brightness(uint8_t pct)
{

}

void app_core1_set_duration(uint16_t minutes)
{

}

void app_core1_set_background(const char *code)
{

}

void app_core1_playlist_set(const char *data)
{

}

void app_core1_playlist_control(const char *cmd)
{

}

void app_core1_playlist_publish_state(void)
{

}
