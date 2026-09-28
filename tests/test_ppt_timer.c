// tests/test_ppt_timer.c — host test for ppt_timer_format.
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../main/ppt_timer.h"

static void check(ppt_timer_t t, unsigned seconds, const char *expected) {
    ppt_timer_init(&t);
    for (unsigned i = 0; i < seconds; i++) {
        ppt_timer_tick(&t);
    }
    char buf[16];
    bool ok = ppt_timer_format(&t, buf, sizeof(buf));
    assert(ok);
    assert(strcmp(buf, expected) == 0);
}

int main(void) {
    ppt_timer_t t;
    ppt_timer_init(&t);
    char buf[16];
    assert(ppt_timer_format(&t, buf, sizeof(buf)));
    assert(strcmp(buf, "00:00") == 0);

    check(t, 0,    "00:00");
    check(t, 59,   "00:59");
    check(t, 60,   "01:00");
    check(t, 3599, "59:59");
    check(t, 3600, "01:00:00");
    check(t, 86399,"23:59:59");

    /* start / stop transitions */
    ppt_timer_init(&t);
    ppt_timer_start(&t);
    assert(t.running);
    for (unsigned i = 0; i < 5; i++) ppt_timer_tick(&t);
    ppt_timer_stop(&t);
    assert(!t.running);
    assert(ppt_timer_format(&t, buf, sizeof(buf)));
    assert(strcmp(buf, "00:00") == 0);

    /* buffer too small must fail cleanly */
    ppt_timer_init(&t);
    for (unsigned i = 0; i < 3600; i++) ppt_timer_tick(&t);
    char small[4];
    assert(!ppt_timer_format(&t, small, sizeof(small)));

    printf("test_ppt_timer: PASS\n");
    return 0;
}