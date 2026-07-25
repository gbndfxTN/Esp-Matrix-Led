idf_component_register(
    SRCS app_core0_wifi_bt.c
    INCLUDE_DIRS include
    REQUIRES nvs_flash esp_wifi esp_netif esp_http_client spiffs mqtt esp_bt esp_bt_main esp_gap_bt lwip mongoose
)
