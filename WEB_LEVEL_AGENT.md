# Handoff: Plumb, and moving the docs to Option B

Paste this whole file into a fresh session as the task. It is self-contained.

You are picking up **Project Dark (Perfect Midas)**, a fork of the GoldenEye 007
PC port, on branch `diagnostic-launch-kit`. The engine work is in good shape.
This session has two subjects, in this order: **finish and land the pending
work**, then **start the documentation tiering that the README now promises**.

Read this file completely before running anything. Re-derive counts and SHAs
rather than trusting the numbers here — they were correct when written and the
branch moves.

---

## 1. Where things stand

The branch carries a diagnostic toolkit called **Plumb** (public name, decided
this session) plus the Classic Mode repair and the `n64_aa` key split. Latest
work, all verified and committed:

- `-skip [N]` and `-stage <name>` boot flags — get past the logos/legal screen,
  or straight into a mission.
- The Classic Mode key split: `classic_mode` (the preset) and `n64_aa` (the AA
  bit) are separate settings with a one-time migration.
- The audio_pump block: sized to its text, padded by one measured character on
  all four sides.

**Uncommitted at handoff time** (verify with `git status`): the README rewrite,
the pure-Classic-Mode work, and the snapshot fix. See §2.

Baseline, measured:

```bash
cd /Users/jcorneli/Developer/DeepSeek/PM-fork
cmake --build build_macos -j8           # only the pre-existing __DATA,__common warning
./scripts/port/port_test.sh --no-game   # 14 passed
./scripts/port/test_the_tests.sh        # every mutant killed
md5 -q build_macos/Data/Saves/eeprom.bin # 3590c7df0edb0930f4e4248826e8ab64
```

---

## 2. Land the pending work

Three things were done in the session that wrote this file. Check each against
the tree before committing; some may already be committed.

1. **README.** `readme.md` became `PORT.md` (the base port's play/build guide,
   with a note at the top saying so) and `README.md` is a new front door written
   for this fork. The old file was `READ.me.md` and **did not render** — eleven
   four-space indents had turned whole sections into code blocks, there were no
   headings and no fences. Keep the new structure: headings, bullets, one fenced
   build block.

2. **Pure Classic Mode.** New key `[display] classic_pure`, default **1**.
   While on, `portClassicPart()` returns 1 for every part, so the preset means
   the whole thing and the CLASSIC sheet's eleven rows are ignored. It does not
   write those rows, so switching it off reveals the player's own selection.
   **Choosing any part row turns it off** (`classicPureDemote` in
   `port_options.c`) — picking a part IS choosing custom. Exempt under
   `portClassicSetHold`, so Reset All and the VIDEO sheet's Apply Changes (which
   land the whole table) do not demote mid-pass.

3. **The snapshot fix — this closes a real user-visible defect.** `classic_saved`
   is documented as "the values from before Classic Mode forced them", and it was
   not. Nothing maintained it while the mode was off, so a repair that ran with
   the mode on recorded the mode's own values. A file could therefore hold
   `classic_saved=0,2,0,-1,1,0,0` — a *classic* `maxfps` in the "before" slot — and
   Tab-off would restore `-1`, so the frame-rate half of the toggle appeared dead
   while low_res/msaa/anisotropy restored correctly. Reproduced live, fixed:

   - `classic_mode.c`'s rule now refuses to overwrite an existing snapshot
     (contract change; `port/tests/classic_mode.c` updated with its rationale).
   - `port_options.c`'s `classicLearn` keeps the snapshot true while the mode is
     **off** — any change to an owned setting updates it. Nothing is learned while
     the mode is on, because those writes are the mode.
   - `portVideoClassicSavedPut` (video.c) updates one field; `classicLearn` only
     uses it when a COMPLETE snapshot is already held, otherwise it rebuilds the
     whole set from live values (an older 4-field line must not be half-updated).
   - `cl_classic forget` (alias `relearn`) repairs a file poisoned before this
     existed. **Refused while the mode is on**, since the live values are then
     the mode's own.

Live evidence: boot `-classic` then toggle off from `maxfps=60` restores `60`,
not `-1`.

Tests: `port_options_classic.c` gained cases for the fps toggle, learning while
off, not learning while on, `relearn` (including its refusal), pure-is-default,
part-choice demotes, other keys do not, and the hold suppresses demotion.
Mutants were added for each and every one is killed — including a
`repair re-snapshots` mutant that had silently stopped being observable until a
"nothing remembered yet" case was added for it.

Commit them one subject at a time, per this branch's rule: a fix, a test, a doc,
never mixed.

---

## 3. The main event: docs to Option B

The README now promises a public/private split. The repository does not
currently have one — it has a rule that is already broken.

**The problem, stated exactly:** `.gitignore` says

```
# the docs live in the private repo
/docs/*
!/docs/macos.md
```

and **eleven files under `docs/` are tracked anyway** (force-added past that
rule). Eight more are ignored. So the written policy says "private", the tree
says "mostly public", and nobody can tell which is true without running
`git ls-files`.

**Option B is the destination: this repo becomes the toolkit.** Public is what a
contributor needs to build, run, or report a bug:

- `README.md`, `PORT.md`, `BUILD.md`, `CLI.md`, `SETTINGS.md`, `LICENSE`
- `tools/midas_ini_help.txt`, `tools/console_help.txt`, `Data/README.txt`
- `docs/macos.md` (already the one allowlisted file)

Private is everything whose value is historical rather than operational: the
design records and audits (`settings-and-save-audit.md`,
`tick-and-field-accounting.md`, `plan.md`, the `NEXT_AGENT*` handoffs, the
`PR_*.md` reports). Those move to a private engineering repo, and stay citable
from public commits by SHA.

**The test to apply to each file:** *would a contributor need this to build, run,
or report a bug?* If yes, publish. If it records how a decision was reached,
it belongs in the private tier.

**Do not just flip the ignore rule.** Three things have to be true before the
split is honest:

1. **Decide the tracked-but-ignored eleven.** Either force-remove them from the
   index (`git rm --cached`) as part of the move, or move the directory. Leaving
   tracked-ignored files behind recreates the current confusion.
2. **Fix the rule so it states the truth.** If public docs stay in `docs/`, the
   rule becomes `docs/private/` rather than `/docs/*`. A rule that needs
   `git add -f` to obey is not a rule.
3. **Keep the inbound links working.** `README.md` links `BUILD.md`,
   `PORT.md`, `CLI.md`, `SETTINGS.md`. Moving a file that something links to is
   how a README rots.

**Sequencing suggestion.** Do the split on a branch of its own, not on
`diagnostic-launch-kit`. It touches nothing in the engine, so it reviews fast,
and mixing it into a rendering branch is the sort of subject-mixing this
project's own policy forbids.

### On the rest of the ecosystem

The README states that the Universal Modding Format, packaging and modular
netplay are at *design* status, and that only the diagnostic tooling ships. Keep
that honesty: the format should stay private until there is a reference
implementation to validate it against, because a public spec with no
implementation is a promise you will be held to and cannot yet keep.

---

## 4. Known traps

1. **`build_macos/midas.ini` is the user's real settings file.** Back it up
   before every experiment, restore it after, and check
   `md5 -q build_macos/midas.ini` against the backup. `eeprom.bin` must stay
   `3590c7df0edb0930f4e4248826e8ab64`. A run that ends in a capture script's
   restore trap landing after yours can reintroduce content you thought you had
   reverted — re-check after a few seconds.
2. **`docs/` is gitignored** (`.gitignore:281`). `git add docs/port/x.md` fails
   with "paths are ignored". Verify with `git ls-files docs/port/`, never by
   reading the `git add` warning.
3. **A twelve-commit-deep stash is not a backup.** This session stashed to A/B
   against HEAD and the pop was interrupted; the work sat in `stash@{0}` while
   the tree looked clean. If you stash, pop it before doing anything else, and
   check `git stash list` is empty afterwards.
4. **`SDL_AUDIODRIVER=disk` is not a mute test.** It writes to a file instead of
   opening a device; a zeros-only dump from it is not evidence that the app
   emits silence. This session drew exactly that wrong conclusion before the
   user launched the game and heard sound. Verify audio the way a player does.
5. **Two `readme` files on a case-insensitive filesystem are one file.** macOS
   APFS is case-insensitive, so `README.md` and `readme.md` collide. Use
   `git mv` to a different name, then create the new one.
6. **The choke point's key is one string comparison** in `optSet`. Miss it and
   nothing happens anywhere; get it and every surface follows.
7. **`classic_saved` is 7 fields and must stay 7.** Do not add the AA bit or
   anything else to it.

---

## 5. Definition of done

- Everything in §2 is committed, one subject per commit, with the build green,
  `ctest` at 14+, and the mutation audit killing every mutant.
- `core.autocrlf`-style surprises aside: `git status` shows no tracked
  modification left uncommitted.
- The docs split is planned on its own branch with §3's three conditions met,
  or explicitly deferred with the reason recorded.
- Anything not verified is named as unverified in the commit message. Phases 0–3
  did that consistently; it is why their messages are still usable.
