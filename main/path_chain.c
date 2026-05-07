#include "path_chain.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "PATH";

#define SNAP_TOLERANCE 0.01f   // mm: distanza sotto cui due punti si considerano coincidenti

static inline float dist2(float ax, float ay, float bx, float by)
{
    float dx = ax - bx, dy = ay - by;
    return dx*dx + dy*dy;
}

esp_err_t path_chain(dxf_segment_t *segs, int seg_count,
                     path_point_t **out_path, int *out_count)
{
    if (!segs || seg_count <= 0 || !out_path || !out_count)
        return ESP_ERR_INVALID_ARG;

    bool *used = calloc(seg_count, sizeof(bool));
    if (!used) return ESP_ERR_NO_MEM;

    // n+1 punti per n segmenti
    path_point_t *path = heap_caps_malloc((seg_count + 1) * sizeof(path_point_t), MALLOC_CAP_SPIRAM);
    if (!path) { free(used); return ESP_ERR_NO_MEM; }

    int pts = 0;
    float cx = segs[0].x0, cy = segs[0].y0;
    path[pts++] = (path_point_t){cx, cy};
    used[0] = true;

    // Aggiungi il punto finale del segmento 0
    cx = segs[0].x1; cy = segs[0].y1;
    path[pts++] = (path_point_t){cx, cy};

    for (int iter = 1; iter < seg_count; iter++) {
        int best = -1;
        float best_d = 1e30f;
        bool best_flip = false;

        for (int i = 0; i < seg_count; i++) {
            if (used[i]) continue;
            float d0 = dist2(cx, cy, segs[i].x0, segs[i].y0);
            float d1 = dist2(cx, cy, segs[i].x1, segs[i].y1);
            if (d0 < best_d) { best_d = d0; best = i; best_flip = false; }
            if (d1 < best_d) { best_d = d1; best = i; best_flip = true;  }
        }

        if (best < 0) break;
        used[best] = true;

        if (best_flip) {
            cx = segs[best].x0; cy = segs[best].y0;
        } else {
            cx = segs[best].x1; cy = segs[best].y1;
        }
        path[pts++] = (path_point_t){cx, cy};
    }

    free(used);

    ESP_LOGI(TAG, "Percorso: %d punti da %d segmenti", pts, seg_count);

    *out_path  = path;
    *out_count = pts;
    return ESP_OK;
}
