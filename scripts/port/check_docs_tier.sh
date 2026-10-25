#!/bin/bash
# Check the documentation tier: `docs/` is public, `docs/private/` is not.
#
# The tier exists because the old rule lied. `.gitignore` said "the docs live in
# the private repo" and ignored all of `docs/`, while ten files under it were
# force-added past that rule - so the written policy and the index disagreed and
# only `git ls-files` could say which one was true. A rule that needs
# `git add -f` to obey is not a rule; this checks that nobody has gone back to
# pretending.
#
#   scripts/port/check_docs_tier.sh     # or `make docs_check`
#
# Four questions, each of which has been false at least once:
#
#   1. Is anything from the private tier tracked?        (FAIL)
#   2. Does a tracked file name a private document?      (FAIL)
#   3. Is any tracked file also ignored?                 (FAIL)
#   4. Do the links in the public documents resolve?     (FAIL)
#
# And one that is currently true and should not be:
#
#   5. Do tracked files cite `docs/...` paths that do not exist?  (WARN)
#
# (5) is a warning because the tree arrived that way: about seventy source
# comments cite design notes that were never in this repository (romdata.md,
# PORT-PLAN.md, phase2-decisions.md and so on). Those are pre-existing debt, not
# a leak from this rule, and failing on them would train everyone to ignore this
# script. Fix them by writing the rule inline or pointing at a public document.

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT" || exit 2

fail=0
warn=0

ok()   { printf '  ok     %s\n' "$1"; }
bad()  { printf '  FAIL   %s\n' "$1"; fail=$((fail + 1)); }
note() { printf '  warn   %s\n' "$1"; warn=$((warn + 1)); }

# Files allowed to name the private tier itself: the rule, the policy, and this
# check. They name the directory, never a document inside it.
SELF="scripts/port/check_docs_tier.sh"

private_docs=""
if [ -d docs/private ]; then
    private_docs=$(find docs/private -type f \( -name '*.md' -o -name '*.txt' \) \
                       -exec basename {} \; | grep -vx 'README.md' | sort -u)
fi

echo "documentation tier"

# --- 1. nothing private is tracked -----------------------------------------
tracked_private=$(git ls-files -- docs/private)
if [ -z "$tracked_private" ]; then
    ok "no tracked file lives in docs/private"
else
    bad "tracked under docs/private (it must stay ignored and unpublished):"
    printf '%s\n' "$tracked_private" | sed 's/^/           /'
fi

# --- 2. no tracked file names a private document ----------------------------
# Match the full file name and every hyphen-separated prefix of at least two
# segments, so a path wrapped across comment lines is still found.
name_hits=""
for doc in $private_docs; do
    stem=${doc%.*}
    pattern="$doc"
    prefix=""
    old_ifs=$IFS; IFS='-'
    for seg in $stem; do
        if [ -z "$prefix" ]; then prefix=$seg; else prefix="$prefix-$seg"; fi
        case "$prefix" in
            *-*) [ ${#prefix} -ge 12 ] && pattern="$pattern|$prefix" ;;
        esac
    done
    IFS=$old_ifs
    hits=$(git grep -n -I -E "($pattern)" -- . ":!$SELF" 2>/dev/null)
    if [ -n "$hits" ]; then
        name_hits="$name_hits$hits
"
    fi
done
name_hits=$(printf '%s' "$name_hits" | sed '/^$/d')
if [ -z "$name_hits" ]; then
    ok "no tracked file names a private document"
else
    bad "tracked files name a document that is now private:"
    printf '%s\n' "$name_hits" | sed 's/^/           /'
    printf '           %s\n' "say the reasoning inline, or name a public document instead"
fi

# --- 3. nothing tracked is ignored -----------------------------------------
both=$(git ls-files -ci --exclude-standard)
if [ -z "$both" ]; then
    ok "no tracked file is also ignored"
else
    bad "tracked and ignored at the same time (the index and the rule disagree):"
    printf '%s\n' "$both" | sed 's/^/           /'
    printf '           %s\n' "either untrack it, or fix the rule so it says what is true"
fi

# --- 4. the public documents' relative links resolve -----------------------
public_docs=$( { git ls-files -- '*.md' | grep -vE '^(tools|port/fast3d|src|build)/' \
                  | grep -vx "$SELF"; } | sort -u)
link_bad=""
for doc in $public_docs; do
    dir=$(dirname "$doc")
    while IFS= read -r target; do
        case "$target" in
            http*|https*|mailto:*|'#'*) continue ;;   # external, or an in-page anchor
        esac
        t=${target%%#*}                              # drop any fragment
        [ -z "$t" ] && continue
        t=$(printf '%s' "$t" | sed 's/%20/ /g')
        [ -e "$dir/$t" ] || link_bad="$link_bad$doc -> $target
"
    done < <(grep -oE '\]\([^)]+\)' "$doc" 2>/dev/null | sed 's/^](//; s/)$//')
done
if [ -z "$link_bad" ]; then
    ok "the relative links in $(printf '%s\n' "$public_docs" | wc -l | tr -d ' ') public documents resolve"
else
    bad "broken relative link(s) in the public documents:"
    printf '%s' "$link_bad" | sed 's/^/           /'
fi

# --- 5. citations to documents that never existed (warning) ----------------
dangling=""
for path in $(git grep -oh -E 'docs/[A-Za-z0-9_./-]+\.(md|txt)' -- . 2>/dev/null | sort -u); do
    [ -e "$path" ] || dangling="$dangling$path
"
done
dangling=$(printf '%s' "$dangling" | sed '/^$/d' | sort -u)
if [ -z "$dangling" ]; then
    ok "every docs/ path cited by a tracked file exists"
else
    count=$(printf '%s\n' "$dangling" | wc -l | tr -d ' ')
    note "$count cited docs/ path(s) do not exist (pre-existing; see this script's header)"
    printf '%s\n' "$dangling" | sed 's/^/           /'
fi

echo
if [ "$fail" -eq 0 ]; then
    if [ "$warn" -eq 0 ]; then
        echo "docs tier: clean"
    else
        echo "docs tier: clean, with $warn warning group(s)"
    fi
    exit 0
fi
echo "docs tier: $fail check(s) failed"
exit 1
