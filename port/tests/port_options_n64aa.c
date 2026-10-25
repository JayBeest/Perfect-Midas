/*
 * Setting Classic Mode by text runs the preset.
 *
 * port/tests/classic_mode.c pins the RULE (what a transition writes). This file
 * pins the WIRING: that portOptSetIni - the setter the console and the Gameplay
 * Options page both come through - actually calls it. The bug in
 * docs/port/settings-and-save-audit.md was exactly a missing call at that seam,
 * and no test of the rule can see it: the rule was right the whole time.
 *
 * It links the shipped port_options.c against a fake option table, so the real
 * sOpts rows, the real portOptFind, the real optSet path and the real
 * portClassicSetLive all run. Only the six bridges out of the file are faked:
 * the [display] and [input] getters/setters, the classic part mask, the
 * classic_saved slot, and the ini writer (which only counts).
 *
 * WHAT THIS CANNOT TELL YOU
 *
 * That the player sees the right thing. The menu, the watch and the sheet each
 * render their own view of these values; this asserts the values themselves.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ultra64.h>
#include "port_options.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); abort(); } } while (0)

/* Declared by port_options.c and used by the menus; not in the public header
 * because no other translation unit needs them. The test does. */
s32 portOptFind(const char *key);
s32 portOptSetValue(s32 i, s32 v);

/* ---- the fake world ----------------------------------------------------- */

enum {
    F_N64AA = 0, F_LOWRES, F_FILTERING, F_ANISO, F_MAXFPS, F_MSAA, F_MIPMAPS, F_FILTER2D,
    F_HUDSCALE,
    F_COUNT
};

static const char *const sFakeKeys[F_COUNT] = {
    "n64_aa", "low_res", "filtering", "anisotropy", "maxfps", "msaa", "mipmaps", "filter_2d",
    "hud_scale"
};

static int sVal[F_COUNT];
static int sParts[8];              /* the CLASSIC sheet's rows */
static int sSaved[8];
static int sSavedN;
static int sIniWrites;

#define NCLASSIC 7

/* The value each classic part forces, in portClassicKey() order - the same
 * numbers port_options.c's sClassicVals holds, restated here so the assertions
 * read as values rather than as indices. */
static const int sClassicVal[NCLASSIC] = { 240, 2, 8, -1, 2, 1, 1 };
/* What the player had before entering the mode. */
static const int sMine[NCLASSIC] = { 480, 1, 2, 60, 4, 0, 0 };

static int fakeIndex(const char *key)
{
    int i;

    for (i = 0; i < F_COUNT; i++) {
        if (strcmp(sFakeKeys[i], key) == 0) {
            return i;
        }
    }
    return -1;
}

/* ---- the six bridges port_options.c expects ---------------------------- */

s32 portVideoOptionGet(const char *key, s32 *out)
{
    const int i = fakeIndex(key);

    if (i < 0) {
        return 0;
    }
    *out = sVal[i];
    return 1;
}

s32 portVideoOptionSet(const char *key, s32 value)
{
    const int i = fakeIndex(key);

    if (i < 0) {
        return 0;
    }
    sVal[i] = (int) value;
    return 1;
}

s32 portInputOptionGet(const char *key, s32 *out)
{
    (void) key; (void) out;
    return 0;
}

s32 portInputOptionSet(const char *key, s32 value)
{
    (void) key; (void) value;
    return 0;
}

s32 portJoyPortOptionGet(const char *key, s32 *out)
{
    (void) key; (void) out;
    return 0;
}

s32 portJoyPortOptionSet(const char *key, s32 value)
{
    (void) key; (void) value;
    return 0;
}

s32 portClassicPart(s32 i)
{
    return (i >= 0 && i < 8) ? sParts[i] : 0;
}

s32 portVideoClassicSavedGet(s32 *v)
{
    int i;

    for (i = 0; i < sSavedN; i++) {
        v[i] = sSaved[i];
    }
    return sSavedN;
}

/* Mirrors video.c's portVideoClassicSavedSet: it always stores
 * CLASSIC_SAVED_MAX (7) fields, which is why the real caller passes an array of
 * that size regardless of what the plan asked for. */
void portVideoClassicSavedSet(const s32 *v)
{
    int i;

    for (i = 0; i < NCLASSIC; i++) {
        sSaved[i] = v[i];
    }
    sSavedN = NCLASSIC;
}

/* Only counted: the ini's contents are port/tests/classic_mode.c's subject and
 * the live harnesses' - what matters here is that a write happened at all. */
s32 portIniWriteStrs(const char *section, const char *const *keys, const char *const *vals, s32 n)
{
    (void) section; (void) keys; (void) vals; (void) n;
    sIniWrites++;
    return 1;
}

/* include/PR/os.h does `#define sprintf portSprintf`. */
int portSprintf(char *buf, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, 1024, fmt, ap);
    va_end(ap);
    return n;
}

/* ---- setup ------------------------------------------------------------- */

/* The player's own values, mode off, nothing remembered. */
static void world_off_own(void)
{
    int i;

    memset(sVal, 0, sizeof(sVal));
    memset(sSaved, 0, sizeof(sSaved));
    sSavedN = 0;
    sIniWrites = 0;
    for (i = 0; i < 8; i++) {
        sParts[i] = 1;              /* every part enabled: Classic Mode as shipped */
    }
    sVal[F_N64AA] = 0;
    sVal[F_LOWRES] = sMine[0];
    sVal[F_FILTERING] = sMine[1];
    sVal[F_ANISO] = sMine[2];
    sVal[F_MAXFPS] = sMine[3];
    sVal[F_MSAA] = sMine[4];
    sVal[F_MIPMAPS] = sMine[5];
    sVal[F_FILTER2D] = sMine[6];
    sVal[F_HUDSCALE] = 1;
}

/* Classic Mode on, its values in force, the originals remembered. */
static void world_on_classic(void)
{
    int i;

    world_off_own();
    sVal[F_N64AA] = 1;
    sVal[F_LOWRES] = sClassicVal[0];
    sVal[F_FILTERING] = sClassicVal[1];
    sVal[F_ANISO] = sClassicVal[2];
    sVal[F_MAXFPS] = sClassicVal[3];
    sVal[F_MSAA] = sClassicVal[4];
    sVal[F_MIPMAPS] = sClassicVal[5];
    sVal[F_FILTER2D] = sClassicVal[6];
    for (i = 0; i < NCLASSIC; i++) {
        sSaved[i] = sMine[i];
    }
    sSavedN = NCLASSIC;
}

static int set_text(const char *key, const char *text)
{
    const s32 i = portOptFind(key);

    CHECK(i >= 0);
    return portOptSetIni(i, text);
}

static void expect_classic_forced(void)
{
    CHECK(sVal[F_N64AA] == 1);
    CHECK(sVal[F_LOWRES] == sClassicVal[0]);
    CHECK(sVal[F_FILTERING] == sClassicVal[1]);
    CHECK(sVal[F_ANISO] == sClassicVal[2]);
    CHECK(sVal[F_MAXFPS] == sClassicVal[3]);
    CHECK(sVal[F_MSAA] == sClassicVal[4]);
    CHECK(sVal[F_MIPMAPS] == sClassicVal[5]);
    CHECK(sVal[F_FILTER2D] == sClassicVal[6]);
}

static void expect_mine(void)
{
    CHECK(sVal[F_LOWRES] == sMine[0]);
    CHECK(sVal[F_FILTERING] == sMine[1]);
    CHECK(sVal[F_ANISO] == sMine[2]);
    CHECK(sVal[F_MAXFPS] == sMine[3]);
    CHECK(sVal[F_MSAA] == sMine[4]);
    CHECK(sVal[F_MIPMAPS] == sMine[5]);
    CHECK(sVal[F_FILTER2D] == sMine[6]);
}

/* ---- the cases --------------------------------------------------------- */

/* The defect: `cl_n64_aa 1` wrote the flag and forced nothing. */
static void text_on_forces_and_snapshots(void)
{
    int i;

    world_off_own();
    CHECK(set_text("n64_aa", "1") == 1);

    expect_classic_forced();
    CHECK(sSavedN == NCLASSIC);
    for (i = 0; i < NCLASSIC; i++) {
        CHECK(sSaved[i] == sMine[i]);   /* the player's values, not the forced ones */
    }
    CHECK(sIniWrites > 0);
}

/* The other direction: `cl_n64_aa 0` left all seven forced with the mode off. */
static void text_off_restores(void)
{
    world_on_classic();
    CHECK(set_text("n64_aa", "0") == 1);

    CHECK(sVal[F_N64AA] == 0);
    expect_mine();
    CHECK(sIniWrites > 0);
}

/* THE regression case. Setting the mode to the value it already has must not
 * re-enter it: entering overwrites classic_saved with the forced values, and the
 * player's originals would be gone for good. */
static void repeated_on_does_not_resnapshot(void)
{
    int i;

    world_on_classic();
    CHECK(set_text("n64_aa", "1") == 1);

    CHECK(sSavedN == NCLASSIC);
    for (i = 0; i < NCLASSIC; i++) {
        CHECK(sSaved[i] == sMine[i]);   /* still the originals, not 240/2/8/-1/2/1/1 */
    }
    expect_classic_forced();
}

/* ...and the same from the other side: turning off a mode that is already off
 * must not "restore" a stale snapshot over values the player has since chosen. */
static void repeated_off_does_not_restore(void)
{
    world_off_own();
    sSaved[0] = 1111;                  /* a stale snapshot from an earlier cycle */
    sSavedN = 1;

    CHECK(set_text("n64_aa", "0") == 1);

    CHECK(sVal[F_LOWRES] == sMine[0]);
    CHECK(sVal[F_LOWRES] != 1111);
}

/* A different key must not trigger the preset. Without this, a careless
 * "does the key mention classic" test would fire on every save. */
static void other_keys_do_not_transition(void)
{
    world_on_classic();
    sSaved[0] = 4242;                  /* sentinel: untouched proves no re-snapshot */
    sSavedN = 1;

    CHECK(set_text("hud_scale", "0") == 1);

    CHECK(sSavedN == 1);
    CHECK(sSaved[0] == 4242);
    expect_classic_forced();           /* untouched */
}

/* A part switched off on the CLASSIC sheet is the player's, not the preset's. */
static void disabled_part_is_left_alone(void)
{
    world_off_own();
    sParts[2] = 0;                     /* anisotropy */

    CHECK(set_text("n64_aa", "1") == 1);

    CHECK(sVal[F_LOWRES] == sClassicVal[0]);
    CHECK(sVal[F_ANISO] == sMine[2]);  /* not forced */
}

/* portOptSetValue is the toggle's and the watch's setter. Since the choke point
 * (optSet) applies the mode, this MUST transition now: the explicit calls that
 * used to follow it in portClassicToggle, in the watch's rowClassicSync and in
 * the console's cl_classic are gone, and without this the toggle would set the
 * flag and force nothing - the original bug, one layer up.
 *
 * This case asserted the opposite before plan.md Phase 2, when three surfaces
 * each applied the mode themselves and a transition here would have been a
 * double apply. The two assertions are a contract: whichever one is written
 * down, the other must fail. */
static void set_value_transitions(void)
{
    const s32 aa = portOptFind("n64_aa");
    int i;

    world_off_own();
    CHECK(portOptSetValue(aa, 1) == 1);

    CHECK(sVal[F_N64AA] == 1);
    expect_classic_forced();
    CHECK(sSavedN == NCLASSIC);
    for (i = 0; i < NCLASSIC; i++) {
        CHECK(sSaved[i] == sMine[i]);
    }
}

/* Exactly-once. If the mode were applied twice - once by the choke point and
 * once by a leftover explicit call - the second entry would snapshot the FORCED
 * values over the player's, which is the damage the guard exists to prevent.
 * So: one set, and classic_saved still holds 480/1/2/60/4/0/0. */
static void one_set_applies_once(void)
{
    const s32 aa = portOptFind("n64_aa");
    int i;

    world_off_own();
    CHECK(portOptSetValue(aa, 1) == 1);

    for (i = 0; i < NCLASSIC; i++) {
        CHECK(sSaved[i] == sMine[i]);       /* not sClassicVal */
        CHECK(sSaved[i] != sClassicVal[i]);
    }

    /* And a repeated ON through the same setter does not retake it. */
    CHECK(portOptSetValue(aa, 1) == 1);
    for (i = 0; i < NCLASSIC; i++) {
        CHECK(sSaved[i] == sMine[i]);
    }
}

/* The hold is the mechanism the VIDEO sheet uses: it lands its own pended
 * values and must not have the choke point apply a mode underneath it. */
static void the_hold_suppresses_the_transition(void)
{
    const s32 aa = portOptFind("n64_aa");

    world_off_own();
    portClassicSetHold(1);
    CHECK(portOptSetValue(aa, 1) == 1);
    CHECK(sVal[F_N64AA] == 1);
    CHECK(sVal[F_LOWRES] == sMine[0]);      /* NOT forced: the sheet does it */
    CHECK(sSavedN == 0);                    /* and nothing snapshotted */
    portClassicSetHold(0);

    /* Released, the transition works again - a hold must not leak. */
    world_off_own();
    CHECK(portOptSetValue(aa, 1) == 1);
    expect_classic_forced();
    CHECK(sSavedN == NCLASSIC);
}

int main(void)
{
    text_on_forces_and_snapshots();
    text_off_restores();
    repeated_on_does_not_resnapshot();
    repeated_off_does_not_restore();
    other_keys_do_not_transition();
    disabled_part_is_left_alone();
    set_value_transitions();
    one_set_applies_once();
    the_hold_suppresses_the_transition();

    printf("port_options_n64aa: all checks passed\n");
    return 0;
}
