#ifndef DXF_PARSER_H
#define DXF_PARSER_H

#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float x0, y0;
    float x1, y1;
} dxf_segment_t;

// Parse DXF ASCII buffer, estrae solo entità LINE sul layer specificato.
// Alloca *out_segs in PSRAM — il chiamante deve free().
esp_err_t dxf_parse(const char *buf, size_t len,
                    const char *layer_filter,
                    dxf_segment_t **out_segs, int *out_count);

#ifdef __cplusplus
}
#endif

#endif // DXF_PARSER_H
