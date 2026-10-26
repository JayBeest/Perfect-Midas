#ifndef PORT_AUDIO_PACE_ENV_H
#define PORT_AUDIO_PACE_ENV_H

/*
 * The audio pump's pacing config, overridable from the environment.
 *
 * WHY THIS IS A SEPARATE FILE FROM audio_pace.h
 *
 * audio_pace.h's promise is that nothing in it "reads a clock, touches SDL, or
 * includes an engine header" - which is what makes the policy testable without a
 * device. getenv() is I/O, so the reading lives here and the policy stays pure.
 * The split also lets the OBC launcher (proto_editor/) hand the game a pace the
 * user chose, without the game needing a settings UI for it.
 *
 * THE CONTRACT
 *
 *   unset            -> the field keeps the compiled-in default; a run with no
 *                       GE_AUDIO_* variables behaves exactly as before.
 *   set and valid    -> the field is overridden.
 *   set and invalid  -> the field keeps the default and a reason is reported.
 *                       Never silently ignored, never silently clamped.
 *
 * Validation exists because audio_pace.c's functions assume
 * `target <= target_max`, a non-zero rate, a non-zero step and a non-zero
 * alignment. Those assumptions are cheap to keep true here and expensive to
 * discover as an assertion (or a silent starvation) at run time.
 *
 * ONE ASYMMETRY, DELIBERATE: target vs target_max
 *
 * Raising GE_AUDIO_TARGET_MS above the current ceiling means the user asked for
 * more buffer. Refusing that would make the one setting people actually reach
 * for - "give me a 250 ms target" - the one that cannot be set alone. So:
 *
 *   - if GE_AUDIO_TARGET_MAX was NOT set, the ceiling follows the target up, and
 *     it follows it with the DEFAULTS' OWN HEADROOM preserved
 *     (TARGET_MAX - TARGET, 76 ms of audio at the default rate, scaled to the
 *     rate in force). The report says ceiling_raised and names the result.
 *   - if GE_AUDIO_TARGET_MAX WAS set and the target exceeds it, the target is
 *     refused. A stated ceiling is a constraint, and target <= target_max is
 *     the invariant that keeps the ratchet sane.
 *
 * WHY THE HEADROOM IS PRESERVED, AND NOT JUST THE INVARIANT
 *
 * target_max is used in exactly one place in the policy: as the ceiling the
 * underrun ratchet may climb to (portAudioPaceTargetStep). A ceiling raised to
 * exactly the target satisfies `target <= target_max` while making the ratchet
 * unable to step even once - the config looks valid and the learning is
 * silently dead. Measured with the shipped policy, ten simulated underruns:
 *
 *     target 3520, ceiling 10240              learned +6720
 *     target 22000, ceiling raised TO target  learned    +0
 *     target 22000, ceiling + headroom        learned +6720
 *
 * So "raise to the target" fixes latency and breaks the thing the ceiling
 * exists for. Preserving the headroom keeps both: a lone GE_AUDIO_TARGET_MS
 * works, and a sink that starves can still be given more room.
 *
 * The headroom is a TIME, not a byte count: (TARGET_MAX - TARGET) at the default
 * rate is 76 ms, and at another rate the same 76 ms needs more bytes. Scaled by
 * the rate in force so a rate override does not quietly change how much learning
 * room there is.
 *
 * The raised ceiling is capped at one second of audio at the rate in force -
 * the same bound PORT_AUDIO_MAX_INTERVAL_MS puts on latency. At a target already
 * at one second there is no room above it, so the ratchet is dead there by
 * arithmetic rather than by policy; that case is reported (ceiling_capped)
 * rather than passing as normal.
 *
 * The four fields with no variable are structural, not tunable here:
 * frame_align is the synthesizer's own rounding contract, max_frame bounds one
 * render call, device_period_ms describes the device, and max_interval_ms is a
 * latency policy bound. Changing any of them changes what the policy MEANS
 * rather than how it is tuned.
 */

#include "audio_pace.h"

#define PORT_AUDIO_PACE_ENV_COUNT 8
#define PORT_AUDIO_PACE_ENV_WHY_MAX 8
#define PORT_AUDIO_PACE_ENV_WHY_LEN 112

/* The raw text of each variable, or NULL when unset. Text rather than numbers
 * so that the parser owns numeric validation and can say what it saw - "not a
 * number" is a much better message than "0". */
typedef struct {
    const char *target_ms;   /* GE_AUDIO_TARGET_MS   -> target, derived       */
    const char *target_max;  /* GE_AUDIO_TARGET_MAX  -> target_max, bytes     */
    const char *target_step; /* GE_AUDIO_TARGET_STEP -> target_step, bytes    */
    const char *target_ease; /* GE_AUDIO_TARGET_EASE -> target_ease, bytes    */
    const char *catchup_ms;  /* GE_AUDIO_CATCHUP_MS  -> catchup_ms            */
    const char *drain_ms;    /* GE_AUDIO_DRAIN_MS    -> drain_ms              */
    const char *queue_max;   /* GE_AUDIO_QUEUE_MAX   -> queue_max, bytes      */
    const char *rate;        /* GE_AUDIO_RATE        -> rate, Hz              */
} PortAudioPaceEnvIn;

typedef struct {
    int applied;         /* fields overridden from the environment           */
    int rejected;        /* fields refused; why[] holds one line each        */
    int ceiling_raised;  /* target_max followed a target above the default   */
    int ceiling_capped;  /* ...but one second of audio left it no headroom    */
    char why[PORT_AUDIO_PACE_ENV_WHY_MAX][PORT_AUDIO_PACE_ENV_WHY_LEN];
} PortAudioPaceEnvReport;

/* The variable name for slot i, in the same order as PortAudioPaceEnvIn's
 * fields. i outside 0..PORT_AUDIO_PACE_ENV_COUNT-1 returns NULL. */
const char *portAudioPaceEnvName(int i);

/* Fill `in` from the process environment. Unset variables become NULL. */
void portAudioPaceEnvRead(PortAudioPaceEnvIn *in);

/*
 * Apply `in` on top of the cfg already in `out`.
 *
 * `out` is IN/OUT: the caller fills it with the compiled-in defaults first, and
 * this only ever overrides fields. That is why there is no "defaults" argument
 * and no second copy of the macros - the pump's own initialiser stays the single
 * source of truth for what "unset" means.
 *
 * Safe with `in` all-NULL (does nothing) and with `rep` NULL.
 */
void portAudioPaceEnvApply(PortAudioPaceCfg *out, const PortAudioPaceEnvIn *in,
                           PortAudioPaceEnvReport *rep);

/*
 * Read the environment and apply it. Returns the number of fields overridden.
 * `rep` may be NULL.
 */
int portAudioPaceCfgFromEnv(PortAudioPaceCfg *out, PortAudioPaceEnvReport *rep);

#endif /* PORT_AUDIO_PACE_ENV_H */
