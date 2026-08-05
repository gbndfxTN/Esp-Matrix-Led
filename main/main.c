#include "gpio.h"
#include "OtaCore.h"
#include "NetworkCore.h"
#include "esp_log.h"
#include "esp_system.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "MAIN";

void app_main(void) {
    gpio_init();

    if (gpio_ota_button_pressed()) {
        ESP_LOGI(TAG, "Bouton OTA detecte, demarrage BLE OTA...");
        while (gpio_ota_button_pressed()) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        ble_ota_init();
        while (ble_ota_is_active()) {
            if (gpio_ota_button_pressed()) {
                ESP_LOGI(TAG, "Bouton presse, sortie OTA...");
                esp_restart();
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    } else {
        ESP_LOGI(TAG, "Demarrage normal...");
        wifi_init();
        wifi_wait_for_connection();
    }
}
