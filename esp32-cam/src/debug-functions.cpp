#include "esp_heap_caps.h"
#include "esp_log.h"
#include "debug.hpp"

void print_free_psram() {
    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t largest_free_block = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);

    DEBUG("Free PSRAM: ", free_psram, " bytes");
    DEBUG("Largest free block in PSRAM: ", largest_free_block, " bytes");
}
