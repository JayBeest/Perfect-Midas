/*
 * The frame/tick statistics of the in-game block.
 *
 * Drives the shipped functions in port/src/tick_stats.c against windows
 * computed by hand, so the numbers a player reads in the corner are checked
 * without a window, a GPU or a ROM.
 *
 * The cases are chosen to pin the three things that were wrong or unverifiable
 * before: that fields/s means 60.0 whenever simulated time is keeping up, that
 * it falls when the sim loses time, and that a stage-load reset cannot publish
 * a negative rate.
 */

#include <stdio.h>
#include <stdlib.h>
#include "tick_stats.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); abort(); } } while (0)

/* A 500 ms window of a healthy 60 Hz sim: 30 fields, 30 ticks.
 * fields/s = 30 * 10000 / 500 = 600 (= 60.0), fields/tick = 100 (= 1.00). */
static void healthy_60hz(void)
{
    const uint32_t fields = 30, ticks = 30, dt = 500;

    CHECK(portStatsRatePerSec(fields, dt, 10) == 600);
    CHECK(portStatsFieldsPerTickX100(fields, ticks) == 100);
}

/* A 500 ms window at 120 Hz: one field per two ticks, so the tick rate is 120
 * while fields/s is still 60.0. This is the case the old TPS row got wrong: it
 * counted 60 clock-advancing ticks and reported "60 tps" for a 120 tick/s sim. */
static void healthy_120hz(void)
{
    const uint32_t fields = 30, ticks = 60, dt = 500;

    CHECK(portStatsRatePerSec(fields, dt, 10) == 600);
    CHECK(portStatsRatePerSec(ticks, dt, 1) == 120); /* the honest tick rate */
    CHECK(portStatsFieldsPerTickX100(fields, ticks) == 50); /* 0.50 f/t */
}

/* The live macOS capture: software rasterisation made frames cost ~466 ms, so a
 * window held one 60 Hz tick that was credited the GE_TICKMAXSTEP cap of 8
 * fields. Observed in audio_pump.log as `f/s 16.7 f/t 8.00` at dt=479 ms. This
 * is the reading that identified the sim losing time, so pin it. */
static void stalled_window(void)
{
    const uint32_t fields = 8, ticks = 1, dt = 479;

    CHECK(portStatsRatePerSec(fields, dt, 10) == 167);  /* 16.7 fields/s */
    CHECK(portStatsFieldsPerTickX100(fields, ticks) == 800); /* 8.00 f/t */
    /* And it is unmistakably short of the invariant. */
    CHECK(portStatsRatePerSec(fields, dt, 10) < 600);
}

/* A window with no admitted tick at all (the loop blocked loading): no ratio,
 * and no division by zero. */
static void no_ticks(void)
{
    CHECK(portStatsFieldsPerTickX100(0, 0) == 0);
    CHECK(portStatsFieldsPerTickX100(8, 0) == 0);
}

/* A zero-length window cannot happen on the wall clock, but the function must
 * not divide by it if it ever does. */
static void zero_window(void)
{
    CHECK(portStatsRatePerSec(30, 0, 10) == 0);
}

/* Rounding, not truncation: 22 fields over a 367 ms window is 59.945 fields/s,
 * which must read 599 (59.9) rather than 599.4's floor or a drifted 600. */
static void rounding(void)
{
    CHECK(portStatsRatePerSec(22, 367, 10) == 599);

    /* And a genuinely on-target window that is not exactly 500 ms: 61 fields in
     * 1017 ms is 59.98/s, which reads 600 rather than 599. */
    CHECK(portStatsRatePerSec(61, 1017, 10) == 600);
}

/* D4: lvlStageLoad zeroes g_GlobalTickCount. The baseline must follow the reset
 * so the window reads 0 instead of -2x the pre-load count. */
static void stage_load_reset(void)
{
    /* 20 s of play, then the counter is zeroed by a stage load. */
    CHECK(portStatsReseatTickBaseline(1200, 0) == 0);
    /* The delta that follows is now 0, not -1200: no negative can be published. */
    CHECK((int32_t) (0 - portStatsReseatTickBaseline(1200, 0)) == 0);

    /* 5 s of play: the -600 case observed in the field. */
    CHECK(portStatsReseatTickBaseline(300, 0) == 0);

    /* A normal forward move keeps the baseline. */
    CHECK(portStatsReseatTickBaseline(1200, 1260) == 1200);
    /* Equal counts likewise (a window with no ticks). */
    CHECK(portStatsReseatTickBaseline(1200, 1200) == 1200);
}

/* Wrapping numbers must not produce a negative published rate. */
static void negative_never_published(void)
{
    int32_t baseline = 1200;

    baseline = portStatsReseatTickBaseline(baseline, 0);
    CHECK(baseline >= 0);
    CHECK((0 - baseline) == 0);
}

int main(void)
{
    healthy_60hz();
    healthy_120hz();
    stalled_window();
    no_ticks();
    zero_window();
    rounding();
    stage_load_reset();
    negative_never_published();

    printf("tick_stats: all checks passed\n");
    return 0;
}
