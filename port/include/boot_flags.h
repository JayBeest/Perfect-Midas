#ifndef PORT_BOOT_FLAGS_H
#define PORT_BOOT_FLAGS_H

/*
 * The three diagnostic boot flags, as a pure parse.
 *
 *   ProjectMidas -n64_aa -audio_pump -audio_log
 *
 * Each is independent and mix-and-match. Nothing here touches the engine, SDL,
 * a clock or a file: the caller passes the argument vector and gets back three
 * booleans. video.c acts on them; port/tests/boot_flags.c drives them.
 *
 * WHY THIS IS ITS OWN FILE AND NOT SIX LINES IN video.c
 *
 * A boot flag that is silently ignored is indistinguishable from one that
 * worked, because the only evidence is what the game does afterwards - and by
 * then the reader is looking at the game, not the command line. That is the same
 * defect class as GE_TICKHZ=hw (strtol reads it as 0, so "hw" silently means
 * uncapped), which cost a session. So the matching rules are pinned by a test:
 *
 *   - at least one leading '-' is REQUIRED. `audio_pump` as a bare word is not a
 *     flag, because a bare first argument is the drag-and-drop ROM path and
 *     nothing may quietly eat it.
 *   - more than one dash is accepted: `-x` and `--x` are the same flag. The
 *     ROM/lang flags already in use are `--rom-file`/`--lang`, so both spellings
 *     have to work.
 *   - '-' and '_' are interchangeable in the name: `-audio-pump` and
 *     `-audio_pump` both work. The console verb is `cl_audio_pump`, so the
 *     underscore is the canonical spelling; a hyphen is what most people type.
 *     Accepting both costs a comparison and removes a silent no-op.
 *   - case does not matter.
 *   - an unrecognised flag is ignored, like the rest of this command line, and
 *     is deliberately NOT an error. `-n64_aa=1` is therefore not a flag; it is
 *     ignored, and that is the one spelling this deliberately does not accept.
 */

typedef struct {
    int n64_aa;     /* -n64_aa: start in Classic Mode (what the Tab key toggles) */
    int audio_pump; /* -audio_pump: draw the cl_audio_pump block                */
    int audio_log;  /* -audio_log: write Data/Logs/audio_pump.log               */
} PortBootFlags;

/*
 * Fill `out` from the argument vector. argv[0] (the program) is skipped.
 * Every field is written on every call, so `out` does not need clearing first.
 */
void portBootFlagsParse(int argc, char **argv, PortBootFlags *out);

/*
 * 1 when `arg` names `name`. `name` is the canonical spelling with underscores.
 * Exposed for the test; the caller wants Parse.
 */
int portBootFlagIs(const char *arg, const char *name);

#endif /* PORT_BOOT_FLAGS_H */
