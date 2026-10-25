/*
 * Boot flags - implementation. See boot_flags.h for the matching rules and why
 * they are tested rather than assumed.
 */

#include <stddef.h>

#include "boot_flags.h"

/* Case-insensitive, with '-' and '_' treated as the same character. Written
 * out rather than calling strcasecmp so this file needs neither <string.h>'s
 * locale-sensitive behaviour nor SDL. */
static char lowerDash(char c)
{
    if (c == '-') {
        return '_';
    }
    if (c >= 'A' && c <= 'Z') {
        return (char) (c - 'A' + 'a');
    }
    return c;
}

int portBootFlagIs(const char *arg, const char *name)
{
    if (arg == NULL || name == NULL) {
        return 0;
    }
    /* At least one dash, or this is not a flag. A bare first argument is the
     * drag-and-drop ROM path; nothing may consume it. */
    if (arg[0] != '-') {
        return 0;
    }
    while (*arg == '-') {
        arg++;
    }
    while (*arg != '\0' && *name != '\0') {
        if (lowerDash(*arg) != lowerDash(*name)) {
            return 0;
        }
        arg++;
        name++;
    }
    /* Both must end together: "-audio" must not match "audio_pump". */
    return (*arg == '\0' && *name == '\0') ? 1 : 0;
}

void portBootFlagsParse(int argc, char **argv, PortBootFlags *out)
{
    int i;

    if (out == NULL) {
        return;
    }
    out->n64_aa = 0;
    out->audio_pump = 0;
    out->audio_log = 0;

    if (argv == NULL) {
        return;
    }
    for (i = 1; i < argc; i++) {
        if (portBootFlagIs(argv[i], "n64_aa")) {
            out->n64_aa = 1;
        } else if (portBootFlagIs(argv[i], "audio_pump")) {
            out->audio_pump = 1;
        } else if (portBootFlagIs(argv[i], "audio_log")) {
            out->audio_log = 1;
        }
    }
}
