// main/ppt_timer.h — slide timer state machine. Pure logic, no LVGL/ESP-IDF.
#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    bool running;
    unsigned seconds;
} ppt_timer_t;

void ppt_timer_init(ppt_timer_t *t);
void ppt_timer_start(ppt_timer_t *t);
void ppt_timer_stop(ppt_timer_t *t);
void ppt_timer_tick(ppt_timer_t *t);
bool ppt_timer_format(const ppt_timer_t *t, char *out, size_t out_len);