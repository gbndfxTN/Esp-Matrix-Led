#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void app_core0_init(void);
void app_core0_run(void);       // Core 0: boucle mongoose (MQTT broker + HTTP + WiFi)

#ifdef __cplusplus
}
#endif
