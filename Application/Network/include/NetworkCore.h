#pragma once

#include <freertos/FreeRTOS.h>

void wifi_init(void);
void wifi_wait_connected(TickType_t timeout);
void wifi_wait_synced(TickType_t wifi_timeout, TickType_t ntp_timeout);
void wifi_wait_for_connection(void);

