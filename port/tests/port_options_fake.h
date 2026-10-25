#ifndef PORT_OPTIONS_FAKE_H
#define PORT_OPTIONS_FAKE_H

/*
 * A fake world for driving the shipped port_options.c without a game.
 *
 * port_options.c reaches out through six bridges: the [display] and [input]
 * getters/setters, the classic part mask, the classic_saved slot, and the ini
 * writer. This file fakes exactly those and nothing else, so a test links the
 * REAL option table, the REAL portOptFind, the REAL optSet choke point and the
 * REAL adapters - which is where the defects have been.
 *
 * WHAT IT RECORDS, AND WHY
 *
 * Values alone are not enough to test this code. Two implementations can leave
 * identical values while doing different amounts of work:
 *
 *   - a transition applied twice leaves the same final values, but takes the
 *     player's snapshot twice and the second one overwrites the first with the
 *     FORCED values - the bug that destroys someone's settings;
 *   - a transition applied before the flag is written, rather than after,
 *     leaves the same values for both directions and differs only under failure.
 *
 * So the fake keeps an ordered write log and counts saves and snapshots. A test
 * that only compares values cannot see either defect; one that counts can.
 *
 * The keys are the real option keys, so a test names "low_res" rather than an
 * index. The REAL table's ordering is port_options.c's business and the tests
 * assert against portOptFind, never against a position.
 */

/* Marker key used in the ordered write log for an ini save, so a test can see
 * whether the file was written before or after a value landed. Never a real key. */
#define FAKE_SAVE_MARK "__save__"

/* Sentinel for "this key does not exist" / "no value". */
#define FAKE_MISSING (-99999)

/* Player's own values, Classic Mode off, every part enabled, nothing saved. */
void fakeReset(void);

/* Classic Mode ON with its values in force, and the player's own values held in
 * classic_saved. Use before testing a transition OUT of the mode. */
void fakeClassicOn(void);

/* Player's own values, mode off - the baseline every "entering" case starts at. */
void fakeResetPlayerValues(void);

int  fakeGet(const char *key);
void fakeSet(const char *key, int v);      /* also logs a write */

/* classic_saved, as portVideoClassicSavedGet reports it. */
int  fakeSavedN(void);
int  fakeSaved(int i);

/* How many times the ini writer was called (portOptSave calls it once per
 * non-empty section, so one save is normally 2). */
int  fakeIniCalls(void);
void fakeIniCallsReset(void);

/* How many times a snapshot was stored (portVideoClassicSavedSet). */
int  fakeSnapshotCalls(void);

/* The ordered log of key writes, newest last. Returns 0 past the end.
 * `out_key` receives a static string, `out_val` the value written. */
int  fakeWriteCount(void);
int  fakeWriteAt(int n, const char **out_key, int *out_val);

/* Make the setter REFUSE this key (returns 0), as a live key does when its
 * value is out of range. Cleared by fakeReset. */
void fakeRefuse(const char *key);

/* Make this [input] row settable. [input] rows are refused by default so a test
 * that means a display key cannot silently pass through an input one. */
void fakeAcceptInput(const char *key);

#endif /* PORT_OPTIONS_FAKE_H */
