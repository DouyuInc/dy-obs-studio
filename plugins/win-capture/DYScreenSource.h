#pragma once

#include <obs-module.h>
#include "cursor-capture.h"

struct DYScreenSourceData {
    obs_source_t *source;
    int monitor;
    bool capture_cursor;
    bool showing;

    long x;
    long y;
    int rot;
    uint32_t width;
    uint32_t height;
    gs_duplicator_t *duplicator;
    float reset_timeout;
    struct cursor_data cursor_data;
};

