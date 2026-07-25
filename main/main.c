#include "app_core0_wifi_bt.h"
#include "app_core1_led_matrix.h"
#include "drivers.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static void core1_task(void *arg)
{
    app_core1_run();
}

void app_main(void)
{
    drivers_init();
    app_core0_init();
    app_core1_init();

    xTaskCreatePinnedToCore(core1_task, "core1_matrix", 8192, NULL, 2, NULL, 1);
    app_core0_run();
}
