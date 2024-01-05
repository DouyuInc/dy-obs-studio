#pragma once
#ifdef __DRAWVIDEOINFOTOOLS_H

extern "C" {
#endif

#include <graphics/graphics.h>

    gs_texture_t *drawProgress(gs_texture_t *texture, int cx, int cy, int ballRadius, int pts, int duration);
    gs_texture_t *DrawVideoInfo(gs_texture_t *texture, char* filePath, int sourceCx, int sourceCy, int tempTime, int tortalTime, uint32_t color, uint32_t fontSize);
    gs_texture_t *getTextTexture(gs_texture_t *texture, int cx, int cy, char* text);
#ifdef __DRAWVIDEOINFOTOOLS_H
}
#endif
