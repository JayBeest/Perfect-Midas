/*
 * ROM-free, device-free, log-free tests for the pace-config environment parser.
 *
 * Drives the shipped parser (port/src/audio_pace_env.c), not a model of it.
 * Two layers are exercised, because they fail differently:
 *
 *   portAudioPaceEnvApply  the decisions, from synthetic text. No process
 *                          environment involved, so every case is exact.
 *   portAudioPaceCfgFromEnv the getenv wiring, where a wrong variable NAME
 *                          would otherwise look exactly like a working override.
 *
 * The single most important case is the first one: an environment with no
 * GE_AUDIO_* variables must leave the config byte-identical to the compiled-in
 * defaults. That is the promise that lets this ship without changing anyone's
 * audio.
 *
 * Build standalone:
 *   cc -std=c99 -Iport/include port/tests/audio_pace_env.c \
 *      port/src/audio_pace_env.c -o t && ./t
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio_pace_env.h"
/* The pump's own defaults, not a copy of them: the "unset changes nothing" case
 * below must be a statement about the game, not about this file. */
#include "audio_pace_defaults.h"

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); abort(); } } while (0)

/* The pump's compiled-in configuration, built from the same macros the pump
 * initialises s_PaceDefaults with (port/include/audio_pace_defaults.h). */
static PortAudioPaceCfg shipped(void)
{
    PortAudioPaceCfg c;

    c.rate = PORT_AUDIO_RATE;
    c.frame_align = PORT_AUDIO_ALIGN;
    c.max_frame = PORT_MAX_FRAME_SAMPLES;
    c.target = PORT_AUDIO_TARGET;
    c.target_max = PORT_AUDIO_TARGET_MAX;
    c.target_step = PORT_AUDIO_TARGET_STEP;
    c.target_ease = PORT_AUDIO_TARGET_EASE;
    c.catchup_ms = PORT_AUDIO_CATCHUP_MS;
    c.drain_ms = PORT_AUDIO_DRAIN_MS;
    c.device_period_ms = PORT_AUDIO_DEV_PERIOD_MS;
    c.queue_max = PORT_AUDIO_QUEUE_MAX;
    c.max_interval_ms = PORT_AUDIO_MAX_INTERVAL_MS;
    return c;
}

static int same(const PortAudioPaceCfg *a, const PortAudioPaceCfg *b)
{
    return memcmp(a, b, sizeof(*a)) == 0;
}

/* How much room the ratchet has above the configured target. It climbs by
 * target_step and clamps at the ceiling, so any non-zero value means it can
 * learn; zero means it cannot move at all. */
static unsigned headroomBytes(const PortAudioPaceCfg *cfg)
{
    return cfg->target_max - cfg->target;
}

static void clearEnv(void)
{
    int i;

    for (i = 0; i < PORT_AUDIO_PACE_ENV_COUNT; i++) {
        unsetenv(portAudioPaceEnvName(i));
    }
}

/* ---- the promise: unset changes nothing --------------------------------- */

static void test_unset_is_a_no_op(void)
{
    PortAudioPaceEnvIn in;
    PortAudioPaceEnvReport rep;
    PortAudioPaceCfg cfg = shipped();
    PortAudioPaceCfg want = shipped();

    memset(&in, 0, sizeof(in));
    portAudioPaceEnvApply(&cfg, &in, &rep);

    CHECK(same(&cfg, &want));
    CHECK(rep.applied == 0);
    CHECK(rep.rejected == 0);
    CHECK(rep.ceiling_raised == 0);

    /* ...and through the real environment, with nothing set. */
    clearEnv();
    cfg = shipped();
    CHECK(portAudioPaceCfgFromEnv(&cfg, &rep) == 0);
    CHECK(same(&cfg, &want));
    CHECK(rep.rejected == 0);
}

/* ---- each field, and only the one that was named ------------------------ */

static void test_rate_only(void)
{
    PortAudioPaceEnvIn in;
    PortAudioPaceEnvReport rep;
    PortAudioPaceCfg cfg = shipped();

    memset(&in, 0, sizeof(in));
    in.rate = "48000";

    portAudioPaceEnvApply(&cfg, &in, &rep);

    CHECK(cfg.rate == 48000u);
    CHECK(rep.applied == 1);
    /* Nothing else moved. A parser that re-derived the target from the new rate
     * would silently change latency on anyone setting only the rate. */
    CHECK(cfg.target == shipped().target);
    CHECK(cfg.target_max == shipped().target_max);
    CHECK(cfg.frame_align == shipped().frame_align);
    CHECK(cfg.max_frame == shipped().max_frame);
}

static void test_target_ms_derives_bytes(void)
{
    PortAudioPaceEnvIn in;
    PortAudioPaceEnvReport rep;
    PortAudioPaceCfg cfg = shipped();

    memset(&in, 0, sizeof(in));
    in.target_ms = "20"; /* smaller than the 40 ms default, so the ceiling holds */

    portAudioPaceEnvApply(&cfg, &in, &rep);

    /* Same derivation as the compile-time macro: rate*4/1000*ms. */
    CHECK(cfg.target == 22050u * 4u / 1000u * 20u);
    CHECK(cfg.target == 1760u);
    CHECK(cfg.target_max == shipped().target_max);
    CHECK(rep.ceiling_raised == 0);
    CHECK(rep.rejected == 0);
}

static void test_target_ms_set_to_the_default_agrees_with_the_macro(void)
{
    PortAudioPaceEnvIn in;
    PortAudioPaceEnvReport rep;
    PortAudioPaceCfg cfg = shipped();
    PortAudioPaceCfg want = shipped();

    memset(&in, 0, sizeof(in));
    in.target_ms = "40";

    portAudioPaceEnvApply(&cfg, &in, &rep);

    /* The one case that proves the derivation is not off by a rounding step. */
    CHECK(same(&cfg, &want));
}

static void test_each_remaining_field(void)
{
    PortAudioPaceEnvReport rep;
    PortAudioPaceEnvIn in;
    PortAudioPaceCfg cfg;

    memset(&in, 0, sizeof(in));
    in.target_step = "2048";
    cfg = shipped();
    portAudioPaceEnvApply(&cfg, &in, &rep);
    CHECK(cfg.target_step == 2048u);
    CHECK(rep.applied == 1);

    memset(&in, 0, sizeof(in));
    in.target_ease = "512";
    cfg = shipped();
    portAudioPaceEnvApply(&cfg, &in, &rep);
    CHECK(cfg.target_ease == 512u);
    CHECK(rep.applied == 1);

    memset(&in, 0, sizeof(in));
    in.catchup_ms = "100";
    cfg = shipped();
    portAudioPaceEnvApply(&cfg, &in, &rep);
    CHECK(cfg.catchup_ms == 100u);
    CHECK(rep.applied == 1);

    memset(&in, 0, sizeof(in));
    in.drain_ms = "500";
    cfg = shipped();
    portAudioPaceEnvApply(&cfg, &in, &rep);
    CHECK(cfg.drain_ms == 500u);
    CHECK(rep.applied == 1);

    memset(&in, 0, sizeof(in));
    in.queue_max = "32768";
    cfg = shipped();
    portAudioPaceEnvApply(&cfg, &in, &rep);
    CHECK(cfg.queue_max == 32768u);
    CHECK(rep.applied == 1);

    memset(&in, 0, sizeof(in));
    in.target_max = "8192";
    cfg = shipped();
    portAudioPaceEnvApply(&cfg, &in, &rep);
    CHECK(cfg.target_max == 8192u);
    CHECK(rep.applied == 1);
}

/* ---- the ceiling/target asymmetry --------------------------------------- */

static void test_a_target_above_the_default_raises_the_ceiling(void)
{
    PortAudioPaceEnvIn in;
    PortAudioPaceEnvReport rep;
    PortAudioPaceCfg cfg = shipped();
    unsigned headroom = PORT_AUDIO_TARGET_MAX - PORT_AUDIO_TARGET;

    memset(&in, 0, sizeof(in));
    in.target_ms = "250"; /* 22000 bytes, above the 10240 default ceiling */

    portAudioPaceEnvApply(&cfg, &in, &rep);

    /* This is the case a launcher needs: one setting, and it works. */
    CHECK(cfg.target == 22000u);
    CHECK(rep.applied == 1);
    CHECK(rep.rejected == 0);
    CHECK(rep.ceiling_raised == 1);

    /* ...but the ceiling carries the defaults' own learnable headroom, rather
     * than being pinned to the target. Raising it to exactly the target would
     * leave the underrun ratchet nothing to step into. */
    CHECK(headroom == 6720u);
    CHECK(cfg.target_max == cfg.target + headroom);
    CHECK(cfg.target_max > cfg.target);
}

/* The ratchet is the reason target_max exists, and it is used in exactly one
 * place in the policy. This drives the shipped function across the config the
 * parser just produced, so "a lone target still learns" is a measurement rather
 * than a claim about the arithmetic. */
static void test_a_lone_target_still_lets_the_ratchet_learn(void)
{
    PortAudioPaceEnvIn in;
    PortAudioPaceCfg cfg = shipped();
    PortAudioPaceTarget t;
    unsigned primed, afterDry, steps = 0;
    int i;

    memset(&in, 0, sizeof(in));
    in.target_ms = "250";
    portAudioPaceEnvApply(&cfg, &in, NULL);

    t.target = 0;
    t.primed = 0;
    primed = portAudioPaceTargetStep(&t, 0, 0, &cfg);

    for (i = 0; i < 10; i++) {
        afterDry = portAudioPaceTargetStep(&t, 1, 0, &cfg);
        if (afterDry > primed) {
            steps++;
        }
    }

    /* It starts at the configured target and climbs to the ceiling, exactly as
     * it does with the stock config. Zero room here is the silent regression
     * this option exists to prevent: the config would satisfy
     * `target <= target_max` while the ratchet could never step once. */
    CHECK(primed == cfg.target);
    CHECK(headroomBytes(&cfg) > 0);
    CHECK(afterDry == cfg.target_max);
    CHECK(steps > 0);
}

/* One second of audio is the hard bound on the raised ceiling. At a rate whose
 * one-second figure is exactly reachable by the target derivation, a 1000 ms
 * target has no room above it, so the ratchet is dead by arithmetic; the report
 * has to say so rather than passing it as an ordinary raise.
 *
 * 48000 Hz is that rate: rate*4/1000*1000 == rate*4 == one second exactly, so the
 * cap lands ON the target. (At 22050 Hz the macro's integer division leaves the
 * target at 88000 and one second at 88200 - 200 bytes of room, not zero.) */
static void test_a_target_at_one_second_reports_no_headroom(void)
{
    PortAudioPaceEnvIn in;
    PortAudioPaceEnvReport rep;
    PortAudioPaceCfg cfg = shipped();
    unsigned one_second = 48000u * PACE_BYTES_PER_SAMPLE;

    memset(&in, 0, sizeof(in));
    in.rate = "48000";
    in.target_ms = "1000";

    portAudioPaceEnvApply(&cfg, &in, &rep);

    CHECK(cfg.rate == 48000u);
    CHECK(cfg.target == one_second);
    CHECK(cfg.target_max == one_second);
    CHECK(rep.ceiling_raised == 1);
    CHECK(rep.ceiling_capped == 1);
}

/* The same target at the default rate keeps a sliver of room rather than
 * reporting a cap, because 88200 - 88000 is not zero. This is the boundary the
 * case above would have hidden. */
static void test_one_second_at_the_default_rate_keeps_its_sliver(void)
{
    PortAudioPaceEnvIn in;
    PortAudioPaceEnvReport rep;
    PortAudioPaceCfg cfg = shipped();

    memset(&in, 0, sizeof(in));
    in.target_ms = "1000";

    portAudioPaceEnvApply(&cfg, &in, &rep);

    CHECK(cfg.target == 88000u);  /* 22050*4/1000 = 88, *1000 */
    CHECK(cfg.target_max == 88200u);
    CHECK(cfg.target_max > cfg.target);
    CHECK(rep.ceiling_capped == 0);
}

/* The headroom is a time, not a byte count: at twice the rate the same 76 ms of
 * learning room needs twice the bytes. A byte-for-byte carry-over would halve
 * the ratchet's room on anyone who overrode the rate. */
static void test_headroom_scales_with_the_rate(void)
{
    PortAudioPaceEnvIn in;
    PortAudioPaceCfg cfg = shipped();
    unsigned slow, fast;

    memset(&in, 0, sizeof(in));
    in.target_ms = "250";

    portAudioPaceEnvApply(&cfg, &in, NULL);
    slow = cfg.target_max - cfg.target;

    memset(&in, 0, sizeof(in));
    in.target_ms = "250";
    in.rate = "44100";
    portAudioPaceEnvApply(&cfg, &in, NULL);
    fast = cfg.target_max - cfg.target;

    CHECK(fast == slow * 2u);
}

static void test_a_stated_ceiling_refuses_a_target_above_it(void)
{
    PortAudioPaceEnvIn in;
    PortAudioPaceEnvReport rep;
    PortAudioPaceCfg cfg = shipped();

    memset(&in, 0, sizeof(in));
    in.target_ms = "250";  /* 22000 */
    in.target_max = "20000";

    portAudioPaceEnvApply(&cfg, &in, &rep);

    /* The ceiling was stated, so it is a constraint: the target yields, the
     * ceiling stands, and the invariant still holds. */
    CHECK(cfg.target == shipped().target);
    CHECK(cfg.target_max == 20000u);
    CHECK(cfg.target <= cfg.target_max);
    CHECK(rep.rejected == 1);
    CHECK(strstr(rep.why[0], "GE_AUDIO_TARGET_MS") != NULL);
    CHECK(strstr(rep.why[0], "20000") != NULL);
}

static void test_both_above_the_default_are_both_applied(void)
{
    PortAudioPaceEnvIn in;
    PortAudioPaceEnvReport rep;
    PortAudioPaceCfg cfg = shipped();

    memset(&in, 0, sizeof(in));
    in.target_ms = "250";
    in.target_max = "22000";

    portAudioPaceEnvApply(&cfg, &in, &rep);

    CHECK(cfg.target == 22000u);
    CHECK(cfg.target_max == 22000u);
    CHECK(rep.rejected == 0);
    CHECK(rep.ceiling_raised == 0); /* it did not have to follow anything */
    CHECK(rep.applied == 2);
}

static void test_a_ceiling_below_the_current_target_is_refused(void)
{
    PortAudioPaceEnvIn in;
    PortAudioPaceEnvReport rep;
    PortAudioPaceCfg cfg = shipped();

    memset(&in, 0, sizeof(in));
    in.target_max = "1024";

    portAudioPaceEnvApply(&cfg, &in, &rep);

    /* Accepting this would leave target=3520 > target_max=1024, which is the
     * invariant audio_pace.c's ratchet is written against. */
    CHECK(cfg.target_max == shipped().target_max);
    CHECK(cfg.target <= cfg.target_max);
    CHECK(rep.rejected == 1);
    CHECK(rep.applied == 0);
}

/* ---- the refusals ------------------------------------------------------- */

static void expectRejected(const char *label, const PortAudioPaceEnvIn *in,
                           const char *needle)
{
    PortAudioPaceEnvReport rep;
    PortAudioPaceCfg cfg = shipped();

    portAudioPaceEnvApply(&cfg, in, &rep);

    if (rep.rejected == 0) {
        fprintf(stderr, "%s: expected a rejection, got none (applied=%d)\n",
                label, rep.applied);
        abort();
    }
    if (needle != NULL && strstr(rep.why[0], needle) == NULL) {
        fprintf(stderr, "%s: message %.*s does not mention %s\n",
                label, PORT_AUDIO_PACE_ENV_WHY_LEN, rep.why[0], needle);
        abort();
    }
    /* A refused field must leave its default in place, not a clamped value. */
    if (cfg.rate == 0) {
        fprintf(stderr, "%s: rate was left at zero\n", label);
        abort();
    }
    CHECK(cfg.target <= cfg.target_max);
}

static void test_rate_refusals(void)
{
    PortAudioPaceEnvIn in;

    memset(&in, 0, sizeof(in));
    in.rate = "0";
    expectRejected("rate 0", &in, "1000..192000");

    in.rate = "notanumber";
    expectRejected("rate text", &in, "not an unsigned");

    in.rate = "-22050";
    expectRejected("rate negative", &in, "not an unsigned");

    in.rate = "";
    expectRejected("rate empty", &in, "empty");

    in.rate = "22050x";
    expectRejected("rate suffix", &in, "not an unsigned");

    in.rate = "99999999999999999999";
    expectRejected("rate overflow", &in, "not an unsigned");
}

static void test_step_and_ease_refusals(void)
{
    PortAudioPaceEnvIn in;

    memset(&in, 0, sizeof(in));
    in.target_step = "0";
    expectRejected("step 0", &in, "1..");

    in.target_step = NULL;
    in.target_ease = "0";
    expectRejected("ease 0", &in, "1..");
}

static void test_timing_refusals(void)
{
    PortAudioPaceEnvIn in;

    memset(&in, 0, sizeof(in));
    in.catchup_ms = "0";
    expectRejected("catchup 0", &in, "1..1000");

    memset(&in, 0, sizeof(in));
    in.drain_ms = "0";
    expectRejected("drain 0", &in, "1..60000");

    memset(&in, 0, sizeof(in));
    in.target_ms = "0";
    expectRejected("target_ms 0", &in, "1..1000");

    memset(&in, 0, sizeof(in));
    in.target_ms = "99999";
    expectRejected("target_ms huge", &in, "1..1000");

    memset(&in, 0, sizeof(in));
    in.queue_max = "0";
    expectRejected("queue_max 0", &in, "1..");
}

static void test_a_refusal_does_not_block_the_other_fields(void)
{
    PortAudioPaceEnvIn in;
    PortAudioPaceEnvReport rep;
    PortAudioPaceCfg cfg = shipped();

    memset(&in, 0, sizeof(in));
    in.rate = "0";          /* refused */
    in.drain_ms = "500";    /* fine */
    in.target_step = "2048";/* fine, against the default ceiling */

    portAudioPaceEnvApply(&cfg, &in, &rep);

    CHECK(rep.rejected == 1);
    CHECK(rep.applied == 2);
    CHECK(cfg.rate == shipped().rate);
    CHECK(cfg.drain_ms == 500u);
    CHECK(cfg.target_step == 2048u);
}

/* ---- the getenv wiring: the names themselves ---------------------------- */

static void test_env_names_are_the_documented_ones(void)
{
    CHECK(strcmp(portAudioPaceEnvName(0), "GE_AUDIO_TARGET_MS") == 0);
    CHECK(strcmp(portAudioPaceEnvName(1), "GE_AUDIO_TARGET_MAX") == 0);
    CHECK(strcmp(portAudioPaceEnvName(2), "GE_AUDIO_TARGET_STEP") == 0);
    CHECK(strcmp(portAudioPaceEnvName(3), "GE_AUDIO_TARGET_EASE") == 0);
    CHECK(strcmp(portAudioPaceEnvName(4), "GE_AUDIO_CATCHUP_MS") == 0);
    CHECK(strcmp(portAudioPaceEnvName(5), "GE_AUDIO_DRAIN_MS") == 0);
    CHECK(strcmp(portAudioPaceEnvName(6), "GE_AUDIO_QUEUE_MAX") == 0);
    CHECK(strcmp(portAudioPaceEnvName(7), "GE_AUDIO_RATE") == 0);
    CHECK(portAudioPaceEnvName(-1) == NULL);
    CHECK(portAudioPaceEnvName(PORT_AUDIO_PACE_ENV_COUNT) == NULL);
}

/* Every variable must reach its own field. A copy/paste slip in
 * portAudioPaceEnvRead - two names swapped - passes every synthetic test above
 * and silently tunes the wrong thing. */
static void test_every_variable_reaches_its_own_field(void)
{
    static const char *const names[] = {
        "GE_AUDIO_TARGET_MS", "GE_AUDIO_TARGET_MAX", "GE_AUDIO_TARGET_STEP",
        "GE_AUDIO_TARGET_EASE", "GE_AUDIO_CATCHUP_MS", "GE_AUDIO_DRAIN_MS",
        "GE_AUDIO_QUEUE_MAX", "GE_AUDIO_RATE",
    };
    /* One in-range value per field, so "applied" means "reached this field"
     * rather than "was refused for being absurd". */
    static const char *const probes[] = {
        "20", "20000", "512", "512", "100", "500", "32768", "48000",
    };
    int i;

    for (i = 0; i < PORT_AUDIO_PACE_ENV_COUNT; i++) {
        PortAudioPaceEnvIn in;
        PortAudioPaceCfg cfg = shipped();
        PortAudioPaceEnvReport rep;

        clearEnv();
        setenv(names[i], probes[i], 1);
        portAudioPaceEnvRead(&in);

        /* Exactly one slot filled, and it is this one. */
        {
            const char *const *slots = (const char *const *) &in;
            int filled = 0, which = -1, k;

            for (k = 0; k < PORT_AUDIO_PACE_ENV_COUNT; k++) {
                if (slots[k] != NULL) {
                    filled++;
                    which = k;
                }
            }
            if (filled != 1 || which != i) {
                fprintf(stderr, "setenv %s filled %d slot(s), first %d\n",
                        names[i], filled, which);
                abort();
            }
        }

        /* And applying it must move a field rather than be refused. */
        portAudioPaceEnvApply(&cfg, &in, &rep);
        if (rep.applied != 1) {
            fprintf(stderr, "%s=%s was not applied: %s\n", names[i], probes[i],
                    rep.rejected > 0 ? rep.why[0] : "(no reason)");
            abort();
        }
    }
    clearEnv();
}

static void test_from_env_matches_the_variable_set(void)
{
    PortAudioPaceCfg cfg = shipped();
    PortAudioPaceEnvReport rep;

    clearEnv();
    setenv("GE_AUDIO_DRAIN_MS", "750", 1);
    setenv("GE_AUDIO_TARGET_MS", "80", 1);

    CHECK(portAudioPaceCfgFromEnv(&cfg, &rep) == 2);
    CHECK(cfg.drain_ms == 750u);
    CHECK(cfg.target == 22050u * 4u / 1000u * 80u);
    CHECK(rep.rejected == 0);

    clearEnv();
}

int main(void)
{
    test_unset_is_a_no_op();

    test_rate_only();
    test_target_ms_derives_bytes();
    test_target_ms_set_to_the_default_agrees_with_the_macro();
    test_each_remaining_field();

    test_a_target_above_the_default_raises_the_ceiling();
    test_a_lone_target_still_lets_the_ratchet_learn();
    test_a_target_at_one_second_reports_no_headroom();
    test_one_second_at_the_default_rate_keeps_its_sliver();
    test_headroom_scales_with_the_rate();
    test_a_stated_ceiling_refuses_a_target_above_it();
    test_both_above_the_default_are_both_applied();
    test_a_ceiling_below_the_current_target_is_refused();

    test_rate_refusals();
    test_step_and_ease_refusals();
    test_timing_refusals();
    test_a_refusal_does_not_block_the_other_fields();

    test_env_names_are_the_documented_ones();
    test_every_variable_reaches_its_own_field();
    test_from_env_matches_the_variable_set();

    printf("audio_pace_env: all checks passed\n");
    return 0;
}
