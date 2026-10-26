/*
 * The audio pump's pacing config, overridable from the environment - see
 * audio_pace_env.h for the contract and the reasoning.
 *
 * This file reads the environment and validates; it does not touch SDL, a
 * clock, or any engine header, and it never logs - it reports. The caller
 * (port/src/audio_pc.c) owns the log lines, so this can be driven from a test
 * with synthetic inputs and no process environment at all.
 */

#include "audio_pace_env.h"
#include "audio_pace_defaults.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Sanity bounds. Each one exists to keep an arithmetic assumption true rather
 * than to express taste:
 *
 *   rate          the target is derived as rate*4/1000*ms; below 1000 Hz there
 *                 is no device, above 192000 the derivation would be absurd.
 *   target_ms     one second is the longest interval the pump will bridge
 *                 (PORT_AUDIO_MAX_INTERVAL_MS), so a deeper target buys latency
 *                 nobody asked for.
 *   target_max    at most one second of audio, and the struct field is 32-bit.
 *   step / ease   bounded by the ceiling they move within.
 *   catchup_ms    one pump may not credit more than a second of wall time.
 *   drain_ms      a minute is already far past "stalled".
 *   queue_max     16 MB is far beyond any sane backlog.
 */
#define PACE_RATE_MIN        1000u
#define PACE_RATE_MAX        192000u
#define PACE_TARGET_MS_MIN   1u
#define PACE_TARGET_MS_MAX   1000u
#define PACE_CATCHUP_MIN     1u
#define PACE_CATCHUP_MAX     1000u
#define PACE_DRAIN_MIN       1u
#define PACE_DRAIN_MAX       60000u
#define PACE_QUEUE_MAX_MIN   1u
#define PACE_QUEUE_MAX_MAX   (16u * 1024u * 1024u)

/* The defaults' learnable headroom, in bytes at `rate`.
 *
 * (PORT_AUDIO_TARGET_MAX - PORT_AUDIO_TARGET) is 6720 bytes = 76 ms of audio at
 * the default rate. Expressed as a TIME and scaled to the rate in force, so that
 * overriding GE_AUDIO_RATE does not quietly change how much room the underrun
 * ratchet has to learn into. At the default rate this is exactly 6720. */
static unsigned defaultHeadroom(unsigned rate)
{
    unsigned long long room = (unsigned long long) (PORT_AUDIO_TARGET_MAX - PORT_AUDIO_TARGET)
                            * (unsigned long long) rate
                            / (unsigned long long) PORT_AUDIO_RATE;

    return (unsigned) room;
}

static const char *const kNames[PORT_AUDIO_PACE_ENV_COUNT] = {
    "GE_AUDIO_TARGET_MS",
    "GE_AUDIO_TARGET_MAX",
    "GE_AUDIO_TARGET_STEP",
    "GE_AUDIO_TARGET_EASE",
    "GE_AUDIO_CATCHUP_MS",
    "GE_AUDIO_DRAIN_MS",
    "GE_AUDIO_QUEUE_MAX",
    "GE_AUDIO_RATE",
};

const char *portAudioPaceEnvName(int i)
{
    if (i < 0 || i >= PORT_AUDIO_PACE_ENV_COUNT) {
        return NULL;
    }
    return kNames[i];
}

void portAudioPaceEnvRead(PortAudioPaceEnvIn *in)
{
    if (in == NULL) {
        return;
    }
    in->target_ms = getenv(kNames[0]);
    in->target_max = getenv(kNames[1]);
    in->target_step = getenv(kNames[2]);
    in->target_ease = getenv(kNames[3]);
    in->catchup_ms = getenv(kNames[4]);
    in->drain_ms = getenv(kNames[5]);
    in->queue_max = getenv(kNames[6]);
    in->rate = getenv(kNames[7]);
}

/* Strict unsigned decimal: digits only, no sign, no whitespace, no suffix. An
 * empty string is "set but unusable" and is rejected, not treated as unset -
 * GE_AUDIO_RATE= in a script is a mistake worth reporting. */
static int parseUnsigned(const char *text, unsigned *out)
{
    unsigned long long v = 0;
    const char *p;

    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    for (p = text; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') {
            return 0;
        }
        v = v * 10u + (unsigned long long) (*p - '0');
        if (v > 0xFFFFFFFFull) {
            return 0;
        }
    }
    *out = (unsigned) v;
    return 1;
}

static void reject(PortAudioPaceEnvReport *rep, const char *name,
                   const char *text, const char *why)
{
    int slot;

    if (rep == NULL) {
        return;
    }
    rep->rejected++;
    slot = rep->rejected - 1;
    if (slot >= PORT_AUDIO_PACE_ENV_WHY_MAX) {
        return; /* the count is still right; only the text is dropped */
    }
    /* snprintf, not sprintf: the reported text is user input of unbounded
     * length, and a truncated message is fine while an overrun is not. */
    snprintf(rep->why[slot], PORT_AUDIO_PACE_ENV_WHY_LEN,
             "%s=%s refused: %s", name, text != NULL ? text : "", why);
}

/* Parse one variable into `dst` when present and in range. Returns 1 when it
 * was applied. A NULL/absent variable is not a rejection. */
static int field(const char *name, const char *text, unsigned lo, unsigned hi,
                 unsigned *dst, PortAudioPaceEnvReport *rep)
{
    unsigned v;

    if (text == NULL || text[0] == '\0') {
        if (text != NULL && text[0] == '\0') {
            reject(rep, name, text, "empty value");
        }
        return 0;
    }
    if (!parseUnsigned(text, &v)) {
        reject(rep, name, text, "not an unsigned decimal number");
        return 0;
    }
    if (v < lo || v > hi) {
        char range[64];

        snprintf(range, sizeof(range), "must be %u..%u", lo, hi);
        reject(rep, name, text, range);
        return 0;
    }
    *dst = v;
    if (rep != NULL) {
        rep->applied++;
    }
    return 1;
}

void portAudioPaceEnvApply(PortAudioPaceCfg *out, const PortAudioPaceEnvIn *in,
                           PortAudioPaceEnvReport *rep)
{
    unsigned want_target = 0;
    int have_target = 0;
    unsigned want_max = 0;
    int have_max = 0;

    if (rep != NULL) {
        memset(rep, 0, sizeof(*rep));
    }
    if (out == NULL || in == NULL) {
        return;
    }

    /* 1. rate first: the target is derived from it. */
    field(kNames[7], in->rate, PACE_RATE_MIN, PACE_RATE_MAX, &out->rate, rep);

    /* 2. the ceiling, so the target can be checked against it below. It may not
     *    fall below the target currently in force, or step 3's invariant is
     *    already broken. */
    if (in->target_max != NULL && in->target_max[0] != '\0') {
        unsigned v;

        if (!parseUnsigned(in->target_max, &v)) {
            reject(rep, kNames[1], in->target_max, "not an unsigned decimal number");
        } else if (v < 1) {
            reject(rep, kNames[1], in->target_max, "must be at least 1");
        } else if (v < out->target) {
            reject(rep, kNames[1], in->target_max,
                   "below the current target; lower GE_AUDIO_TARGET_MS first");
        } else if (v > out->rate * 4u) {
            reject(rep, kNames[1], in->target_max,
                   "above one second of audio at this rate");
        } else {
            want_max = v;
            have_max = 1;
            if (rep != NULL) {
                rep->applied++;
            }
        }
    }

    /* 3. the target itself. GE_AUDIO_TARGET_MS is in milliseconds; the field is
     *    bytes. The derivation matches the compile-time macro exactly -
     *    rate*4/1000*ms, integer division first - so an unset variable and a
     *    variable set to the default value agree byte for byte. */
    if (in->target_ms != NULL && in->target_ms[0] != '\0') {
        unsigned v;

        if (!parseUnsigned(in->target_ms, &v)) {
            reject(rep, kNames[0], in->target_ms, "not an unsigned decimal number");
        } else if (v < PACE_TARGET_MS_MIN || v > PACE_TARGET_MS_MAX) {
            reject(rep, kNames[0], in->target_ms, "must be 1..1000 ms");
        } else {
            want_target = out->rate * 4u / 1000u * v;
            have_target = 1;
        }
    }

    if (have_target) {
        unsigned ceiling = have_max ? want_max : out->target_max;

        if (want_target > ceiling) {
            if (have_max) {
                /* Both were stated and they contradict. The ceiling is the
                 * constraint the ratchet is built on, so the target yields. */
                char why[64];

                snprintf(why, sizeof(why),
                         "exceeds the ceiling GE_AUDIO_TARGET_MAX=%u", ceiling);
                reject(rep, kNames[0], in->target_ms, why);
                have_target = 0;
            } else {
                /* Only the target was stated. An unset ceiling is a default, not
                 * a constraint: let it follow, keeping the defaults' own
                 * learnable headroom so the ratchet still works. Raising the
                 * ceiling to exactly the target would satisfy the invariant
                 * while leaving nothing to learn into - see the header. */
                unsigned room = defaultHeadroom(out->rate);
                unsigned one_second = out->rate * (unsigned) PACE_BYTES_PER_SAMPLE;
                unsigned raised = want_target + room;

                if (raised > one_second) {
                    raised = one_second;
                    if (raised <= want_target) {
                        /* A target at one second has nowhere to go: the ratchet
                         * cannot learn here, and that is arithmetic, not a
                         * choice. Say so instead of passing it as normal. */
                        if (rep != NULL) {
                            rep->ceiling_capped = 1;
                        }
                    }
                }
                want_max = raised;
                have_max = 1;
                if (rep != NULL) {
                    rep->ceiling_raised = 1;
                }
            }
        }
    }

    if (have_target) {
        out->target = want_target;
        if (rep != NULL) {
            rep->applied++;
        }
    }
    if (have_max) {
        out->target_max = want_max;
    }

    /* 4. the ratchet's step and ease, bounded by the ceiling they move within. */
    field(kNames[2], in->target_step, 1u, out->target_max, &out->target_step, rep);
    field(kNames[3], in->target_ease, 1u, out->target_max, &out->target_ease, rep);

    /* 5. the wall-clock fallback and the watchdog. */
    field(kNames[4], in->catchup_ms, PACE_CATCHUP_MIN, PACE_CATCHUP_MAX,
          &out->catchup_ms, rep);
    field(kNames[5], in->drain_ms, PACE_DRAIN_MIN, PACE_DRAIN_MAX,
          &out->drain_ms, rep);

    /* 6. the backlog floor. It need not exceed the target: portAudioPaceCap
     *    widens to max(target + one frame, queue_max), so a shallow floor is
     *    tolerated by design. */
    field(kNames[6], in->queue_max, PACE_QUEUE_MAX_MIN, PACE_QUEUE_MAX_MAX,
          &out->queue_max, rep);
}

int portAudioPaceCfgFromEnv(PortAudioPaceCfg *out, PortAudioPaceEnvReport *rep)
{
    PortAudioPaceEnvIn in;

    portAudioPaceEnvRead(&in);
    portAudioPaceEnvApply(out, &in, rep);

    return (rep != NULL) ? rep->applied : 0;
}
