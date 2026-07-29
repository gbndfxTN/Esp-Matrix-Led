#pragma once

#include <stdio.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"

uint8_t* download_gif(const char *url, size_t *out_size);