#include "GifPsram.h"
#include "esp_heap_caps.h"

static uint8_t* GifPsram = NULL;

uint8_t* getGifPsram(void) {
    return GifPsram;
}

esp_err_t mallocGifPsram(size_t size) {
    if (GifPsram) {
        freeGifPsram();
    }
    
    GifPsram = (uint8_t*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    
    if (GifPsram == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}

esp_err_t reallocGifPsram(size_t new_size) {
    if (!GifPsram) {
        return mallocGifPsram(new_size);
    }
    
    uint8_t* new_ptr = (uint8_t*)heap_caps_realloc(GifPsram, new_size, MALLOC_CAP_SPIRAM);
    if (new_ptr == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    GifPsram = new_ptr;
    return ESP_OK;
}

void freeGifPsram(void) {
    if (GifPsram) {
        heap_caps_free(GifPsram);
        GifPsram = NULL;
    }
}