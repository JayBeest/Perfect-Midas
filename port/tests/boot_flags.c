/*
 * The diagnostic boot flags, as a pure parse.
 *
 * Drives portBootFlagsParse against synthetic argument vectors, so the matching
 * rules are pinned without launching the game. That matters more here than the
 * line count suggests: a boot flag that is silently ignored leaves no trace
 * except what the game does not do, which is indistinguishable from a flag that
 * worked. The cases below are the spellings a person actually types.
 *
 * Build standalone:
 *   cc -std=c99 -Iport/include port/tests/boot_flags.c port/src/boot_flags.c -o t
 *   ./t
 */

#include <stdio.h>
#include <stdlib.h>

#include "boot_flags.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); abort(); } } while (0)

/* argv[0] is the program; pass it always, as a real launch does, so a bug that
 * reads from index 0 cannot hide. */
static void parse(PortBootFlags *f, int argc, char **argv)
{
    portBootFlagsParse(argc, argv, f);
}

static void no_flags(void)
{
    char *argv[] = { (char *) "ProjectMidas", NULL };
    PortBootFlags f;

    parse(&f, 1, argv);
    CHECK(f.n64_aa == 0);
    CHECK(f.audio_pump == 0);
    CHECK(f.audio_log == 0);
}

/* Each flag on its own sets only itself. The negative halves are the point: a
 * matching bug that lit every flag would pass a "was it set" check. */
static void each_alone(void)
{
    PortBootFlags f;

    { char *argv[] = { (char *) "P", (char *) "-n64_aa", NULL };
      parse(&f, 2, argv);
      CHECK(f.n64_aa == 1); CHECK(f.audio_pump == 0); CHECK(f.audio_log == 0); }

    { char *argv[] = { (char *) "P", (char *) "-audio_pump", NULL };
      parse(&f, 2, argv);
      CHECK(f.n64_aa == 0); CHECK(f.audio_pump == 1); CHECK(f.audio_log == 0); }

    { char *argv[] = { (char *) "P", (char *) "-audio_log", NULL };
      parse(&f, 2, argv);
      CHECK(f.n64_aa == 0); CHECK(f.audio_pump == 0); CHECK(f.audio_log == 1); }
}

/* The command the flags exist for. */
static void all_three(void)
{
    char *argv[] = { (char *) "P", (char *) "-n64_aa", (char *) "-audio_pump",
                     (char *) "-audio_log", NULL };
    PortBootFlags f;

    parse(&f, 4, argv);
    CHECK(f.n64_aa == 1);
    CHECK(f.audio_pump == 1);
    CHECK(f.audio_log == 1);
}

/* Mix and match, and order must not matter. */
static void subset_and_order(void)
{
    PortBootFlags f;

    { char *argv[] = { (char *) "P", (char *) "-audio_log", (char *) "-n64_aa", NULL };
      parse(&f, 3, argv);
      CHECK(f.n64_aa == 1); CHECK(f.audio_pump == 0); CHECK(f.audio_log == 1); }

    { char *argv[] = { (char *) "P", (char *) "-audio_pump", (char *) "-audio_log", NULL };
      parse(&f, 3, argv);
      CHECK(f.n64_aa == 0); CHECK(f.audio_pump == 1); CHECK(f.audio_log == 1); }
}

/* One or two dashes, either name separator, either case. */
static void spelling_variants(void)
{
    PortBootFlags f;

    { char *argv[] = { (char *) "P", (char *) "--n64_aa", NULL };
      parse(&f, 2, argv); CHECK(f.n64_aa == 1); }

    { char *argv[] = { (char *) "P", (char *) "-audio-pump", NULL };
      parse(&f, 2, argv); CHECK(f.audio_pump == 1); }

    { char *argv[] = { (char *) "P", (char *) "--audio-log", NULL };
      parse(&f, 2, argv); CHECK(f.audio_log == 1); }

    { char *argv[] = { (char *) "P", (char *) "-N64_AA", NULL };
      parse(&f, 2, argv); CHECK(f.n64_aa == 1); }

    { char *argv[] = { (char *) "P", (char *) "-N64-AA", NULL };
      parse(&f, 2, argv); CHECK(f.n64_aa == 1); }
}

/* A bare word is NOT a flag: the first bare argument is the drag-and-drop ROM
 * path (romdataInit), and nothing may consume it. */
static void bare_word_is_not_a_flag(void)
{
    PortBootFlags f;

    { char *argv[] = { (char *) "P", (char *) "audio_pump", NULL };
      parse(&f, 2, argv); CHECK(f.audio_pump == 0); }

    { char *argv[] = { (char *) "P", (char *) "n64_aa", NULL };
      parse(&f, 2, argv); CHECK(f.n64_aa == 0); }
}

/* Unknown flags are ignored, not errors - and must not partially match. */
static void unknown_and_prefix(void)
{
    PortBootFlags f;

    { char *argv[] = { (char *) "P", (char *) "-audio", NULL };
      parse(&f, 2, argv);
      CHECK(f.audio_pump == 0); CHECK(f.audio_log == 0); }

    { char *argv[] = { (char *) "P", (char *) "-audio_pumped", NULL };
      parse(&f, 2, argv); CHECK(f.audio_pump == 0); }

    { char *argv[] = { (char *) "P", (char *) "-n64", NULL };
      parse(&f, 2, argv); CHECK(f.n64_aa == 0); }

    { char *argv[] = { (char *) "P", (char *) "-n64_aaa", NULL };
      parse(&f, 2, argv); CHECK(f.n64_aa == 0); }

    /* The one spelling deliberately not accepted: no `=value` form. Ignored,
     * like any other unknown flag. */
    { char *argv[] = { (char *) "P", (char *) "-n64_aa=1", NULL };
      parse(&f, 2, argv); CHECK(f.n64_aa == 0); }

    { char *argv[] = { (char *) "P", (char *) "-", NULL };
      parse(&f, 2, argv); CHECK(f.n64_aa == 0); }

    { char *argv[] = { (char *) "P", (char *) "--", NULL };
      parse(&f, 2, argv); CHECK(f.audio_pump == 0); }
}

/* The real command line has a ROM path in it. Flags must survive beside it,
 * before or after, and the path itself must not be mistaken for one. */
static void beside_a_rom_path(void)
{
    PortBootFlags f;

    { char *argv[] = { (char *) "P", (char *) "--rom-file", (char *) "/tmp/x.n64",
                       (char *) "-audio_pump", NULL };
      parse(&f, 4, argv); CHECK(f.audio_pump == 1); CHECK(f.n64_aa == 0); }

    { char *argv[] = { (char *) "P", (char *) "-n64_aa", (char *) "/tmp/x.n64", NULL };
      parse(&f, 3, argv); CHECK(f.n64_aa == 1); CHECK(f.audio_pump == 0); }

    /* A path that happens to contain a flag name is not a flag. */
    { char *argv[] = { (char *) "P", (char *) "/carts/audio_pump.n64", NULL };
      parse(&f, 2, argv); CHECK(f.audio_pump == 0); }
}

/* Parse writes every field, so a caller may reuse the struct without clearing. */
static void parse_clears_previous_state(void)
{
    char *yes[] = { (char *) "P", (char *) "-n64_aa", (char *) "-audio_log", NULL };
    char *no[]  = { (char *) "P", NULL };
    PortBootFlags f;

    parse(&f, 3, yes);
    CHECK(f.n64_aa == 1 && f.audio_log == 1);
    parse(&f, 1, no);
    CHECK(f.n64_aa == 0 && f.audio_pump == 0 && f.audio_log == 0);
}

int main(void)
{
    no_flags();
    each_alone();
    all_three();
    subset_and_order();
    spelling_variants();
    bare_word_is_not_a_flag();
    unknown_and_prefix();
    beside_a_rom_path();
    parse_clears_previous_state();

    printf("boot_flags: all checks passed\n");
    return 0;
}
