#ifndef PORT_CLASSIC_MODE_H
#define PORT_CLASSIC_MODE_H

/*
 * Classic Mode's forced values, as pure arithmetic.
 *
 * Classic Mode is a preset: turning it on remembers what the seven settings
 * below currently are, then forces its own values on them; turning it off puts
 * the remembered ones back. That rule is implemented three times today
 * (port_options.c portClassicSetLive, port_watchpages.c rowClassicSync,
 * front.c portSheetClassicPreset/Revert), and four other entry points apply no
 * rule at all - the console, the ini, GE_N64AA and a hand edit. See
 * docs/port/settings-and-save-audit.md.
 *
 * This file is the rule itself, once, with nothing around it: no SDL, no engine
 * headers, no globals, no ini, no key names. The caller supplies the parts, the
 * values each part forces, the current values and any saved ones, and gets back
 * the list of writes to perform. port/tests/classic_mode.c drives the same
 * functions, so the rule can be judged instead of eyeballed.
 *
 * WHY THE CALLER SUPPLIES THE KEY NAMES' VALUES AND NOT THE NAMES
 *
 * The single table of classic keys and their forced values lives in
 * port_options.c (sClassicKeys/sClassicVals) and stays there - it is
 * append-only, and both the menus and the watch read it. This file takes the
 * values positionally so it can be linked into a test with no engine at all.
 * The only ordering contract is that every array here is in the same order as
 * portClassicKey().
 *
 * WHAT IT DELIBERATELY DOES NOT DECIDE
 *
 * Whether a write is applied immediately or pended for an Apply Changes screen
 * to land. The VIDEO sheet pends its picks so the player can see them in red
 * before committing; the console and the watch write at once. The plan says
 * WHICH values and in what order; the surface decides when.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Buffer sizing for the fixed arrays below. The classic key list is 7 today and
 * append-only; this is headroom, not the count. Callers pass their own `n`
 * (portClassicCount()), which is what the loops use. */
#define CLASSIC_MODE_MAX 16

/* What the caller is doing. The op decides whether a snapshot is taken - which
 * is the whole difference between "turn it on" and "repair it":
 *
 *   TOGGLE_ON   entering the mode: remember `current`, then force.
 *   TOGGLE_OFF  leaving it: restore what `saved` holds.
 *   FIX         repair in place, no snapshot. Forces where the mode is on and
 *               restores where it is off. This is the one the console verb and
 *               a boot-time mismatch report need, because re-snapshotting
 *               there would overwrite the player's real values with the forced
 *               ones - the damage portClassicToggle's !s_CfgN64Aa guard exists
 *               to prevent, and the reason -n64_aa declines to act on a mode
 *               that is already on. */
typedef enum {
    CLASSIC_OP_TOGGLE_ON = 0,
    CLASSIC_OP_TOGGLE_OFF,
    CLASSIC_OP_FIX
} ClassicOp;

typedef struct {
    ClassicOp op;
    int mode_on;          /* the state being moved to; for FIX, the state now  */
    const int *parts_on;  /* n entries: 1 = the CLASSIC sheet forces this part  */
    const int *classic;   /* n entries: the value that part forces              */
    const int *current;   /* n entries: the live value now                      */
    const int *saved;     /* saved_n entries: what classic_saved holds          */
    int saved_n;          /* 0 = nothing remembered (an old or hand-edited ini) */
    int n;                /* number of parts; portClassicCount()                */
} ClassicInput;

typedef struct {
    int snapshot;             /* 1 = store `saved[0..saved_n)` BEFORE the writes */
    int saved[CLASSIC_MODE_MAX];
    int saved_n;
    int write_idx[CLASSIC_MODE_MAX];  /* part index to write */
    int write_val[CLASSIC_MODE_MAX];  /* the value to write */
    int write_n;
} ClassicPlan;

/*
 * Fill `out` from `in`. Every field of `out` is written on every call, so it
 * does not need clearing first.
 *
 * Only values that actually differ are emitted, so:
 *   - a fully-classic file plans no writes (FIX is a true no-op there);
 *   - a second identical TOGGLE_ON plans no writes either, which is what makes
 *     turning a mode on twice harmless without a separate guard.
 * The SNAPSHOT is emitted regardless of whether any write differs: if you turn
 * the mode on while already at the classic values, turning it off must restore
 * those same values, and that requires remembering them.
 *
 * Ordering: writes are emitted in part order (0..n), and the snapshot, when
 * present, must land before them - turning the mode on must remember what it is
 * about to overwrite.
 */
void portClassicPlan(const ClassicInput *in, ClassicPlan *out);

/*
 * How many parts the mode forces but the live values do not match - the
 * mismatch a boot or a console change reports. `idx_out`, when not NULL,
 * receives the part indices that differ (up to `n` of them) so a log line can
 * name them rather than only counting them.
 *
 * Returns 0 when the mode is consistent, which is also the case while every
 * part is switched off: nothing is forced, so nothing can be wrong.
 */
int portClassicMismatch(const int *parts_on, const int *classic, const int *current,
                        int n, int *idx_out);

#ifdef __cplusplus
}
#endif

#endif /* PORT_CLASSIC_MODE_H */
