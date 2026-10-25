#!/bin/bash
# Mutation-check the Classic Mode tests: break the shipped code on purpose and
# confirm a test fails.
#
# A passing test proves nothing. This script asks the only question that
# matters about a test - CAN IT FAIL? - by mutating the shipped source and
# checking that something aborts. A mutant that no test kills is a test gap; a
# mutation that does not change the source at all is a broken check, and is
# reported as such rather than counted as a pass.
#
# Copies live in a scratch dir; the repo is never modified.
#
#   scripts/port/test_the_tests.sh
#
# Exit status: 0 = every expected verdict held, 1 = a gap or a no-op mutation.

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT
CC_BASE="-std=c99 -I$ROOT/include -I$ROOT/port/include"

CM="$ROOT/port/src/classic_mode.c"
PO="$ROOT/port/src/port_options.c"
TS="$ROOT/port/tests/classic_mode.c"
TP="$ROOT/port/tests/port_options_n64aa.c"
TF="$ROOT/port/tests/port_options_fake.c"

status=0

# mutate <src-file> <perl-expr> : apply into $W/<base> and fail loudly if the
# source is byte-identical afterwards (a silent no-op would fake a pass).
mutate() {
    local src="$1" expr="$2"
    cp "$src" "$W/mut.c"
    perl -0pi -e "$expr" "$W/mut.c"
    if cmp -s "$src" "$W/mut.c"; then
        printf '  NO-OP    %s\n' "$3"
        status=1
        return 1
    fi
    return 0
}

# check <label> <want: kill|survive>
check() {
    local label="$1" want="$2"
    local killed=0

    if [ "$(basename "$MUT_SRC")" = "classic_mode.c" ]; then
        cc $CC_BASE "$TS" "$W/mut.c" -o "$W/t" 2>"$W/cc.log" || {
            printf '  NOBUILD  %s\n' "$label"; status=1; return; }
        "$W/t" >/dev/null 2>&1 || killed=1
    else
        cc $CC_BASE "$TP" "$TF" "$W/mut.c" "$CM" -o "$W/t" 2>"$W/cc.log" || {
            printf '  NOBUILD  %s\n' "$label"; status=1; return; }
        "$W/t" >/dev/null 2>&1 || killed=1
    fi

    local got="survive"
    [ "$killed" -eq 1 ] && got="kill"

    if [ "$got" = "$want" ]; then
        if [ "$got" = "kill" ]; then
            printf '  ok       %-50s killed\n' "$label"
        else
            printf '  ok       %-50s survives (expected)\n' "$label"
        fi
    else
        if [ "$want" = "kill" ]; then
            printf '  GAP      %-50s SURVIVED - no test kills this\n' "$label"
        else
            printf '  BROKEN   %-50s the baseline itself fails\n' "$label"
        fi
        status=1
    fi
}

echo "mutation audit of the Classic Mode tests"
echo

echo "the rule (port/src/classic_mode.c)"
MUT_SRC="$CM"
mutate "$CM" 's/if \(in->op == CLASSIC_OP_TOGGLE_ON && in->mode_on\) \{/if (in->op == CLASSIC_OP_TOGGLE_ON) {/' "rule: drop mode_on from the snapshot test" && \
    check "rule: snapshot ignores mode_on" kill
mutate "$CM" 's/    memset\(out, 0, sizeof\(\*out\)\);\n//' "rule: drop memset" && \
    check "rule: plan reuse keeps stale fields" kill
mutate "$CM" 's/if \(in->parts_on == NULL \|\| in->classic == NULL \|\| in->current == NULL\) \{/if (in->parts_on == NULL) {/' "rule: trim NULL guard" && \
    check "rule: NULL classic/current not guarded" kill
mutate "$CM" 's/return \(n > CLASSIC_MODE_MAX\) \? CLASSIC_MODE_MAX : n;/return n;/' "rule: drop the buffer clamp" && \
    check "rule: n past the buffer bound" kill
mutate "$CM" 's/if \(in->parts_on\[i\] && in->current\[i\] != in->classic\[i\]\) \{/if (in->parts_on[i]) {/' "rule: force unchanged parts too" && \
    check "rule: forces values already equal" kill
mutate "$CM" 's/            if \(in->parts_on\[i\] && in->current\[i\] != in->saved\[i\]\) \{/            if (in->current[i] != in->saved[i]) {/' "rule: restore disabled parts too" && \
    check "rule: restores a disabled part" kill
mutate "$CM" 's/        for \(i = 0; i < saved_n; i\+\+\) \{/        for (i = 0; i < n; i++) {/' "rule: ignore the snapshot length" && \
    check "rule: 4-field legacy snapshot" kill
mutate "$CM" 's/            if \(idx_out != NULL\) \{/            if (1) {/' "rule: mismatch without the NULL check" && \
    check "rule: mismatch wrote through NULL" kill

echo
echo "the seam (port/src/port_options.c)"
MUT_SRC="$PO"
mutate "$PO" 's/strcmp\(key, "n64_aa"\) == 0/strncmp(key, "classic_", 8) == 0/' "choke: wrong key test" && \
    check "choke point matches classic_* not n64_aa" kill
mutate "$PO" 's/\(v != 0\) != \(was != 0\)\)/1)/' "choke: always transition" && \
    check "choke point transitions on a same-value set" kill
mutate "$PO" 's/const s32 transitions = \(sClassicHold == 0\) && /const s32 transitions = /' "choke: ignore the hold" && \
    check "choke point ignores the hold" kill
mutate "$PO" 's/    in.op = CLASSIC_OP_FIX;/    in.op = CLASSIC_OP_TOGGLE_ON;/' "repair: re-snapshot" && \
    check "repair re-snapshots the forced values" kill
mutate "$PO" 's/    if \(!in.mode_on\)\n    \{\n        return 0;\n    \}/    return 0;/' "mismatch: always 0" && \
    check "mismatch always reports consistent" kill
mutate "$PO" 's/        const s32 room = \(max < n\) \? max : n;/        const s32 room = n;/' "mismatch: ignore max" && \
    check "mismatch overruns a short caller buffer" kill
mutate "$PO" 's/    in.mode_on = on \? 1 : 0;/    in.mode_on = on ? 0 : 1;/' "apply: invert mode_on" && \
    check "apply runs the wrong direction" kill
mutate "$PO" 's/        portVideoClassicSavedSet\(p->saved\);/        (void) 0;/' "apply: never snapshot" && \
    check "apply never stores the snapshot" kill
mutate "$PO" 's/    if \(!optSet\(o->key, v\)\)\n    \{\n        return 0;\n    \}\n    portOptSave\(\);/    portOptSave();\n    if (!optSet(o->key, v))\n    {\n        return 0;\n    }/' "setIni: save before set" && \
    check "portOptSetIni writes before applying" kill
mutate "$PO" 's/    classicPlanApply\(&plan\);\n    if \(plan.snapshot \|\| plan.write_n > 0\)\n    \{\n        portOptSave\(\);/    classicPlanApply(\&plan);\n    if (0)\n    {\n        portOptSave();/' "repair: never save" && \
    check "repair does not persist" kill

echo
echo "baseline"
MUT_SRC="$CM"
if cmp -s "$CM" "$CM"; then
    cc $CC_BASE "$TS" "$CM" -o "$W/b" 2>/dev/null && "$W/b" >/dev/null 2>&1 && \
        printf '  ok       %-50s passes unmutated\n' "classic_mode" || {
        printf '  BROKEN   the unmutated rule test fails\n'; status=1; }
fi
cc $CC_BASE "$TP" "$TF" "$PO" "$CM" -o "$W/b2" 2>/dev/null && "$W/b2" >/dev/null 2>&1 && \
    printf '  ok       %-50s passes unmutated\n' "port_options_n64aa" || {
    printf '  BROKEN   the unmutated seam test fails\n'; status=1; }

echo
if [ "$status" -eq 0 ]; then
    echo "RESULT: every mutant was killed, baseline passes"
else
    echo "RESULT: gaps found (see GAP / NO-OP / BROKEN above)"
fi
exit "$status"
