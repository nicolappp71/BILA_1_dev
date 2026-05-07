#ifndef PATH_CHAIN_H
#define PATH_CHAIN_H

#include "dxf_parser.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float x, y;
} path_point_t;

// Ordina i segmenti in un percorso continuo (nearest-neighbor greedy).
// Alloca *out_path in PSRAM — il chiamante deve free().
// out_count = numero punti = numero segmenti + 1
esp_err_t path_chain(dxf_segment_t *segs, int seg_count,
                     path_point_t **out_path, int *out_count);

#ifdef __cplusplus
}
#endif

#endif // PATH_CHAIN_H
