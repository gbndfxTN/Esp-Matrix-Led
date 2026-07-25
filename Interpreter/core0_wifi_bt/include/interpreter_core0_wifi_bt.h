#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void interpreter_core0_handle_mqtt(const char *topic, const char *payload, int len);

#ifdef __cplusplus
}
#endif
