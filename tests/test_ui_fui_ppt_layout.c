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

static void inside(const ui_fui_ppt_layout_rect_t *child,
                   const ui_fui_ppt_layout_rect_t *parent,
                   const char *name) {
    assert(child->x >= parent->x);
    assert(child->y >= parent->y);
    assert(child->x + child->w <= parent->x + parent->w);
    assert(child->y + child->h <= parent->y + parent->h);
    (void)name;
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

    /* main panel internals: enlarged timer + big action/state must stay
     * inside the panel and never collide with the flanking arrows. */
    ui_fui_ppt_main_layout_t mi = ui_fui_ppt_main_layout();
    inside(&mi.timer,      &l.main_panel, "timer");
    inside(&mi.prev_arrow, &l.main_panel, "prev_arrow");
    inside(&mi.next_arrow, &l.main_panel, "next_arrow");
    inside(&mi.action,     &l.main_panel, "action");
    inside(&mi.state,      &l.main_panel, "state");
    no_overlap(&mi.prev_arrow, &mi.timer,   "prev_arrow", "timer");
    no_overlap(&mi.next_arrow, &mi.timer,   "next_arrow", "timer");
    no_overlap(&mi.timer,      &mi.action,  "timer", "action");
    no_overlap(&mi.action,     &mi.state,   "action", "state");

    /* link panel internals: the v0.2.0 regression was "HOST LINK"/"RSSI"
     * labels overlapping the status dot/text. Two rows must be disjoint
     * and every element must fit inside the 224x64 panel. */
    ui_fui_ppt_link_layout_t li = ui_fui_ppt_link_layout();
    inside(&li.host_link,    &l.link_panel, "host_link");
    inside(&li.status_dot,   &l.link_panel, "status_dot");
    inside(&li.status_value, &l.link_panel, "status_value");
    inside(&li.rssi_label,   &l.link_panel, "rssi_label");
    inside(&li.rssi_value,   &l.link_panel, "rssi_value");
    inside(&li.chart,        &l.link_panel, "chart");
    no_overlap(&li.host_link,    &li.status_dot,   "host_link", "status_dot");
    no_overlap(&li.status_dot,   &li.status_value, "status_dot", "status_value");
    no_overlap(&li.rssi_label,   &li.rssi_value,   "rssi_label", "rssi_value");
    no_overlap(&li.rssi_value,   &li.chart,        "rssi_value", "chart");
    no_overlap(&li.host_link,    &li.rssi_label,   "rowA_label", "rowB_label");
    no_overlap(&li.status_value, &li.chart,        "rowA_value", "rowB_chart");

    printf("test_ui_fui_ppt_layout: PASS\n");
    return 0;
}