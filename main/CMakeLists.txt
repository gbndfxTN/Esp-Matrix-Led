idf_component_register(
    SRCS main.c
    INCLUDE_DIRS .
    REQUIRES
        drivers
        interpreter_core0_wifi_bt
        interpreter_core1_led_matrix
        app_core0_wifi_bt
        app_core1_led_matrix
        freertos
)
