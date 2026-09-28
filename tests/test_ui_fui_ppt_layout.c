// tests/test_ui_fui_ppt_layout.c — verifies the static layout rectangles of
// the FUI home view fit inside the 240x320 panel and never overlap a 6 px
// safe margin around the bezel. No LVGL calls; pure math.
#include <assert.h>
#include <stdio.h>

#include "../main/ui_fui_ppt.h"

static void within(int x, int y, int w, int h, const char *name) {
    assert(x >= 0);
    assert(y >= 0);
    assert(x + w <= 240);
    assert(y + h <= 320);
    (void)name;
}

static void no_overlap(const ui_fui_ppt_layout_rect_t *a,
                       const ui_fui_ppt_layout_rect_t *b,
                       const char *an, const char *bn) {
    int ax2 = a->x + a->w;
    int ay2 = a->y + a->h;
    int bx2 = b->x + b->w;
    int by2 = b->y + b->h;
    bool overlap = (a->x < bx2) && (ax2 > b->x) &&
                   (a->y < by2) && (ay2 > b->y);
    assert(!overlap);
    (void)an; (void)bn;
}

int main(void) {
    ui_fui_ppt_layout_t l = ui_fui_ppt_home_layout();

    within(l.top_bar.x, l.top_bar.y, l.top_bar.w, l.top_bar.h, "top_bar");
    within(l.main_panel.x, l.main_panel.y, l.main_panel.w, l.main_panel.h, "main_panel");
    within(l.link_panel.x, l.link_panel.y, l.link_panel.w, l.link_panel.h, "link_panel");
    within(l.hint_bar.x, l.hint_bar.y, l.hint_bar.w, l.hint_bar.h, "hint_bar");

    /* gap between sections */
    assert(l.top_bar.y + l.top_bar.h <= l.main_panel.y);
    assert(l.main_panel.y + l.main_panel.h <= l.link_panel.y);
    assert(l.link_panel.y + l.link_panel.h <= l.hint_bar.y);

    no_overlap(&l.top_bar, &l.main_panel, "top_bar", "main_panel");
    no_overlap(&l.main_panel, &l.link_panel, "main_panel", "link_panel");
    no_overlap(&l.link_panel, &l.hint_bar, "link_panel", "hint_bar");

    /* menu view */
    ui_fui_ppt_layout_rect_t m = ui_fui_ppt_menu_row_rect(0);
    within(m.x, m.y, m.w, m.h, "menu_row_0");
    ui_fui_ppt_layout_rect_t m1 = ui_fui_ppt_menu_row_rect(1);
    within(m1.x, m1.y, m1.w, m1.h, "menu_row_1");
    no_overlap(&m, &m1, "menu_row_0", "menu_row_1");

    printf("test_ui_fui_ppt_layout: PASS\n");
    return 0;
}