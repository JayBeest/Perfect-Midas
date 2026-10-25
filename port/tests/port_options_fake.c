/*
 * A fake world for the Classic Mode tests. See port_options_fake.h for why it
 * records work and not only values.
 *
 * The bridges defined here are the six port_options.c declares extern. Keep them
 * signature-identical to video.c's: a drifting fake would test a program the
 * game does not run.
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <ultra64.h>
#include "port_options_fake.h"

#define MAXKEYS 16
#define MAXLOG  64
#define MAXSAVED 8

/* The keys a test can touch. Deliberately NOT ordered to put n64_aa first: a
 * test that depended on position would pass here and be wrong in the game. */
static const char *const sKeys[MAXKEYS] = {
    "low_res", "filtering", "anisotropy", "maxfps", "msaa", "mipmaps", "filter_2d",
    "n64_aa",
    "hud_scale",
    "no_aim_crouch",          /* an [input] row, refused unless accepted */
    NULL
};

/* Classic Mode's forced values, in portClassicKey() order. */
static const int sClassic[7] = { 240, 2, 8, -1, 2, 1, 1 };
/* What the player had before entering. */
static const int sMine[7] = { 480, 1, 2, 60, 4, 0, 0 };

static int sVal[MAXKEYS];
static int sRefused[MAXKEYS];
static int sInputOK[MAXKEYS];
static int sSaved[MAXSAVED];
static int sSavedN;
static int sSnapshotCalls;
static int sIniCalls;

static const char *sLogKey[MAXLOG];
static int sLogVal[MAXLOG];
static int sLogN;

static int fakeIndex(const char *key)
{
    int i;

    for (i = 0; i < MAXKEYS && sKeys[i] != NULL; i++) {
        if (strcmp(sKeys[i], key) == 0) {
            return i;
        }
    }
    return -1;
}

static void logWrite(const char *key, int v)
{
    if (sLogN < MAXLOG) {
        sLogKey[sLogN] = key;
        sLogVal[sLogN] = v;
        sLogN++;
    }
}

static void clearWork(void)
{
    sSnapshotCalls = 0;
    sIniCalls = 0;
    sLogN = 0;
}

void fakeResetPlayerValues(void)
{
    int i;

    for (i = 0; i < MAXKEYS; i++) {
        sVal[i] = 0;
        sRefused[i] = 0;
        sInputOK[i] = 0;
    }
    memset(sSaved, 0, sizeof(sSaved));
    sSavedN = 0;
    clearWork();

    sVal[fakeIndex("low_res")] = sMine[0];
    sVal[fakeIndex("filtering")] = sMine[1];
    sVal[fakeIndex("anisotropy")] = sMine[2];
    sVal[fakeIndex("maxfps")] = sMine[3];
    sVal[fakeIndex("msaa")] = sMine[4];
    sVal[fakeIndex("mipmaps")] = sMine[5];
    sVal[fakeIndex("filter_2d")] = sMine[6];
    sVal[fakeIndex("n64_aa")] = 0;
    sVal[fakeIndex("hud_scale")] = 1;
}

void fakeReset(void)
{
    fakeResetPlayerValues();
}

void fakeClassicOn(void)
{
    int i;

    fakeResetPlayerValues();
    sVal[fakeIndex("n64_aa")] = 1;
    sVal[fakeIndex("low_res")] = sClassic[0];
    sVal[fakeIndex("filtering")] = sClassic[1];
    sVal[fakeIndex("anisotropy")] = sClassic[2];
    sVal[fakeIndex("maxfps")] = sClassic[3];
    sVal[fakeIndex("msaa")] = sClassic[4];
    sVal[fakeIndex("mipmaps")] = sClassic[5];
    sVal[fakeIndex("filter_2d")] = sClassic[6];
    for (i = 0; i < 7; i++) {
        sSaved[i] = sMine[i];
    }
    sSavedN = 7;
    clearWork();
}

int fakeGet(const char *key)
{
    const int i = fakeIndex(key);

    return (i < 0) ? FAKE_MISSING : sVal[i];
}

void fakeSet(const char *key, int v)
{
    const int i = fakeIndex(key);

    if (i >= 0) {
        sVal[i] = v;
        logWrite(key, v);
    }
}

int fakeSavedN(void)            { return sSavedN; }
int fakeSaved(int i)            { return (i >= 0 && i < MAXSAVED) ? sSaved[i] : FAKE_MISSING; }
int fakeIniCalls(void)          { return sIniCalls; }
void fakeIniCallsReset(void)    { sIniCalls = 0; sLogN = 0; }
int fakeSnapshotCalls(void)     { return sSnapshotCalls; }
int fakeWriteCount(void)        { return sLogN; }

int fakeWriteAt(int n, const char **out_key, int *out_val)
{
    if (n < 0 || n >= sLogN) {
        return 0;
    }
    if (out_key != NULL) {
        *out_key = sLogKey[n];
    }
    if (out_val != NULL) {
        *out_val = sLogVal[n];
    }
    return 1;
}

void fakeRefuse(const char *key)
{
    const int i = fakeIndex(key);

    if (i >= 0) {
        sRefused[i] = 1;
    }
}

void fakeAcceptInput(const char *key)
{
    const int i = fakeIndex(key);

    if (i >= 0) {
        sInputOK[i] = 1;
    }
}

/* ---- the six bridges port_options.c expects ---------------------------- */

s32 portVideoOptionGet(const char *key, s32 *out)
{
    const int i = fakeIndex(key);

    /* n64_aa and the [input] row are routed by their own bridges below, exactly
     * as video.c leaves them to input_pc.c. Keeping the split faithful matters:
     * a fake that answered everything from one table would hide a key that the
     * real build routes to a different owner. */
    if (i < 0 || i == fakeIndex("no_aim_crouch")) {
        return 0;
    }
    *out = sVal[i];
    return 1;
}

s32 portVideoOptionSet(const char *key, s32 value)
{
    const int i = fakeIndex(key);

    if (i < 0 || i == fakeIndex("no_aim_crouch")) {
        return 0;
    }
    if (sRefused[i]) {
        return 0;
    }
    sVal[i] = (int) value;
    logWrite(key, (int) value);
    return 1;
}

s32 portInputOptionGet(const char *key, s32 *out)
{
    const int i = fakeIndex(key);

    if (i < 0 || i != fakeIndex("no_aim_crouch")) {
        return 0;
    }
    *out = sVal[i];
    return 1;
}

s32 portInputOptionSet(const char *key, s32 value)
{
    const int i = fakeIndex(key);

    if (i < 0 || i != fakeIndex("no_aim_crouch") || !sInputOK[i]) {
        return 0;
    }
    sVal[i] = (int) value;
    logWrite(key, (int) value);
    return 1;
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
    return (i >= 0 && i < 8) ? 1 : 0;   /* every part enabled: Classic Mode as shipped */
}

/* Mirrors video.c's portVideoClassicSavedSet: it always stores CLASSIC_SAVED_MAX
 * (7) fields, which is why the real caller passes an array of that size
 * regardless of what the plan asked for. */
s32 portVideoClassicSavedGet(s32 *v)
{
    int i;

    for (i = 0; i < sSavedN; i++) {
        v[i] = sSaved[i];
    }
    return sSavedN;
}

void portVideoClassicSavedSet(const s32 *v)
{
    int i;

    for (i = 0; i < 7; i++) {
        sSaved[i] = v[i];
    }
    sSavedN = 7;
    sSnapshotCalls++;
}

s32 portIniWriteStrs(const char *section, const char *const *keys, const char *const *vals, s32 n)
{
    (void) section; (void) keys; (void) vals; (void) n;
    sIniCalls++;
    /* Logged into the SAME ordered stream as the key writes, so a test can see
     * whether the file was written before or after the value landed. A count
     * alone cannot tell "wrote the new value" from "wrote, then changed it". */
    logWrite(FAKE_SAVE_MARK, 0);
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
