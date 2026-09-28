// main/ppt_timer.c — pure logic, decoupled from LVGL/ESP-IDF so it can be
// unit tested on the host.
#include "ppt_timer.h"

#include <limits.h>
#include <stdio.h>

void ppt_timer_init(ppt_timer_t *t) {
    if (!t) return;
    t->running = false;
    t->seconds = 0;
}

void ppt_timer_start(ppt_timer_t *t) {
    if (!t) return;
    t->running = true;
    t->seconds = 0;
}

void ppt_timer_stop(ppt_timer_t *t) {
    if (!t) return;
    t->running = false;
    t->seconds = 0;
}

void ppt_timer_tick(ppt_timer_t *t) {
    if (!t) return;
    if (t->seconds < UINT_MAX) {
        t->seconds++;
    }
}

bool ppt_timer_format(const ppt_timer_t *t, char *out, size_t out_len) {
    if (!t || !out || out_len < 9) return false; /* "HH:MM:SS" + NUL */
    unsigned h = t->seconds / 3600u;
    unsigned m = (t->seconds % 3600u) / 60u;
    unsigned s = t->seconds % 60u;
    if (h > 0u) {
        snprintf(out, out_len, "%02u:%02u:%02u", h, m, s);
    } else {
        snprintf(out, out_len, "%02u:%02u", m, s);
    }
    return true;
}