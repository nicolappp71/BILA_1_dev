#include <stddef.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_memory_utils.h"

static const char *TAG = "FF_MEM";

// FatFS alloca win[] e buffer LFN tramite ff_memalloc.
// Con SPIRAM_USE_CAPS_ALLOC il malloc() va in PSRAM → SDMMC DMA non può accedere
// direttamente → bounce buffer → "not enough mem" (0x101).
// Fix: forza allocazione in RAM interna (DMA-accessible su ESP32-P4),
// senza consumare il pool MALLOC_CAP_DMA che BLE/SDIO usano.

__attribute__((used)) void *__wrap_ff_memalloc(size_t msize)
{
    void *ptr = heap_caps_malloc(msize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_LOGD(TAG, "ff_memalloc(%u) -> %p dma=%d", (unsigned)msize, ptr,
             ptr ? esp_ptr_dma_capable(ptr) : 0);
    return ptr;
}

__attribute__((used)) void __wrap_ff_memfree(void *mblock)
{
    free(mblock);
}
