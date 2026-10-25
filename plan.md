# plan — Classic Mode: one authority for the forced values

Implements fixes for `docs/port/settings-and-save-audit.md`. Chosen shape:
**option 4 now** (nothing is ever silent), delivered by **option C** (fix the
broken surface, then centralise), on the road to **option 3** (split the key).

Everything below is grounded in probes run on 2026-10-25 against
`build_macos` at `abec289`. Where a claim is `read` rather than `executed` it
says so, because this branch's recorded failure mode is a fix verified on one
shape of input and reported as the mechanism working.

---

## 1. The problem, stated precisely

Classic Mode's forced values are applied **at the moment of the toggle** and
never re-asserted. There is no rule anywhere that says "if the mode is on, these
seven values are classic". There are three separate implementations that each
establish it once, and four entry points that establish nothing.

`n64_aa` is also **two concepts in one key** (14 references in `video.c`):

- the **VI AA bit** — `videoApplyAa` reads `vi = s_CfgN64Aa` and passes it to
  `gfx_set_aa`, so it genuinely is an AA setting;
- **"the preset is on"** — `portVideoN64AaOn`, the mipmap gate
  (`videoMipmapMode`), the pillar gate, the watch's row lock, and the boot
  flag's `!s_CfgN64Aa` guard all read it that way.

That conflation is why the entry points disagree, and it is what Phase 3 ends.

### The entry points, measured

- **Tab / `classic_toggle`** → `portClassicToggle` → `portClassicSetLive`.
  Complete. (`bondview2.c`; read.)
- **VIDEO sheet** → `portSheetClassicPreset` on ON, `portSheetClassicRevert`
  on OFF, landed by one APPLY CHANGES. Complete, and the most careful of the
  three: it handles a saved value the row's list does not name (`maxfps=165`)
  through `classicRaw`. (read)
- **Watch** → `portOptSetIndex` then `rowClassicSync`; also reconciles on page
  entry and synthesises a snapshot when none exists. Complete. (read)
- **Console `set n64_aa 1|0` and `cl_n64_aa 1|0`** → `portOptSetIni` →
  `optSet` → `classicPartChanged`, which returns early for `n64_aa`. **Broken,
  both directions** (executed — exactly one line changed each way).
- **`-n64_aa`** → `portClassicToggle`, guarded by `!s_CfgN64Aa`. Correct.
- **Hand-edited `n64_aa=1`** → no transition at all. **Broken** (executed:
  `low_res=480` under the preset).
- **`GE_N64AA=1`** → writes `s_CfgN64Aa` directly, bypassing `optSet`.
  **Broken** (executed: `maxfps=60`, `low_res=480`, preset reported on).

The Port Options page is the VIDEO sheet (`portOptStep` / `portOptSetIndex` at
`front.c:5781`, `5693`), so it is covered by the sheet row above, not a
separate surface.

### Why the obvious fix is wrong

Hooking `optSet()` centrally *today* breaks three working paths (read, from the
call graph):

- `portClassicToggle` would apply twice (its own `SetLive` plus the hook);
- the watch would run `rowClassicSync` after the hook already transitioned,
  and its `was` comparison would then be against the new value — so the
  no-op guard would fire on a real transition;
- the sheet's APPLY loop would transition **mid-apply**, and the outcome would
  depend on whether `n64_aa` is pended before or after `low_res` in sheet
  order — a latent order dependency of exactly the kind F3-A already cost a
  session;
- `portOptResetDefaults` writes `n64_aa` raw first on purpose.

So centralisation (Phase 2) must *delete* the duplicates, not layer under them.

---

## 2. Phase 0 — option 4: make it impossible to be silent

**Goal:** every entry point that can produce a mismatch says so, with a name for
the gap, and there is a way to repair it on request. No behaviour change to any
working path.

### 0.1 The pure rule (the testable core)

New `port/include/classic_mode.h` + `port/src/classic_mode.c`, no SDL, no
globals, no ini — the same shape as `boot_flags.c` / `tick_stats.c`:

```c
#define CLASSIC_MAX 7

/* Per classic key, in portClassicKey() order. 1 = should be forced but is not. */
unsigned portClassicMismatch(const int *parts_on, const int *current, int n);

/* What a transition should write. filled = 0 when there is nothing to do. */
typedef struct {
    int snapshot;            /* 1 = store saved[0..saved_n) first */
    int saved[CLASSIC_MAX];
    int saved_n;
    int write_idx[CLASSIC_MAX];
    int write_val[CLASSIC_MAX];
    int write_n;
} ClassicPlan;

void portClassicPlan(int turning_on, const int *parts_on,
                     const int *current, const int *saved, int saved_n,
                     ClassicPlan *out);
```

`portClassicPlan` is a straight transcription of the three existing
implementations' *shared* semantics, with the surface-specific parts left out:

- turning **on**: snapshot `current` (so the plan always has a snapshot to
  offer), then force `classic_val[i]` for every `parts_on[i]`;
- turning **off**: restore `saved[i]` for every `parts_on[i]`, where `i` is
  bounded by both `saved_n` and `CLASSIC_MAX` — which is what keeps the 4-field
  older snapshot readable;
- parts off stay the player's.

It deliberately does **not** decide pend-vs-write. That is the applier's job and
is why the sheet can keep its red pending picks.

**Test:** `port/tests/classic_mode.c`, registered explicitly in
`CMakeLists.txt` inside `if(GE_PORT_TESTS)` (there is no glob):

```cmake
add_executable(classic_mode_test port/tests/classic_mode.c port/src/classic_mode.c)
add_test(NAME classic_mode COMMAND classic_mode_test)
```

Cases that must exist, written from the four defects rather than from the code:

- mismatch detects each of the seven individually, and none when consistent;
- a 4-field snapshot restores only four;
- a part off is not forced and not restored;
- turning on with an existing snapshot still forces (the toggle's job), and the
  caller — not the planner — decides whether to re-snapshot;
- turning off with no snapshot writes nothing;
- `parts_on` all zero: turning on writes nothing, turning off writes nothing.

That is the phase's real deliverable: the rule becomes executable, so Phases 1
and 2 can be judged rather than eyeballed.

### 0.2 The eight call sites that report

Three places can observe a mismatch, and each gets one line:

1. **`videoInit`, after `videoLoadConfig` and after `GE_N64AA`.** This is the
   only place both the hand-edit and the env var are visible. Log, don't act:

   ```
   ge: video: Classic Mode is on but 3 of its settings are not classic
              (low_res=480, maxfps=60, anisotropy=2) - run `cl_classic fix`
   ```

   Silent when consistent. Your own file is consistent (read), so this stays
   quiet for you.
2. **`optSet`, on a change to `n64_aa`** — a single log, no transition, naming
   the surface as the console (it is the only surface that reaches `optSet`
   directly):
   ```
   ge: options: n64_aa changed through the console - Classic Mode's values not applied
   ```
   This is temporary and is deleted in Phase 1, which makes it untrue.
3. **`GE_N64AA`** — the same announcement as the hand-edit case, since it is the
   same state arriving by a different door.

### 0.3 The repair verb

`cl_classic [fix|on|off|status]` in `port_console.c`, defaulting to `status`:

- `cl_classic` (or `cl_classic status`) prints the mismatch and writes nothing;
- `cl_classic fix` applies what is missing and **never retakes the snapshot**: it
  forces the enabled parts where the mode is on (leaving `classic_saved` alone,
  since those values were already saved at the original toggle), and restores
  them where the mode is off. This is the case the boot flag cannot cover —
  `-n64_aa` declines to act when the mode is already on, so nothing repairs a
  half-state;
- `cl_classic on` / `cl_classic off` are transitions and are no-ops when already
  in that state, matching `portClassicToggle`'s guard (a re-entry must not
  overwrite the player's saved values).

A bare subcommand over a `_fix` suffix because this is a mode worth inspecting
as much as repairing, and the console already uses `cl_<thing>` for "tell me
about this thing". Written against `portClassicSetLive` and
`portClassicPlan`, not against a new implementation, so the verb cannot drift
from the toggle. This is also the manual escape hatch for anyone already stuck.

### 0.4 Exit criteria for Phase 0

- `ctest` 11/11 (`classic_mode` added).
- All three mismatch states announce, and the consistent state is silent —
  executed on all four: console, hand-edit, `GE_N64AA=1`, and consistent.
- `cl_classic fix` repairs a hand-edited `n64_aa=1`, verified by the ini diff:
  seven values forced to the classic set, and `classic_saved` **byte-identical**
  to what it was before the command (the assertion is that `fix` does not
  re-snapshot, so the check is that the line did not change).
- No working path changes behaviour: Tab, sheet APPLY, watch entry all re-run
  unchanged (executed by their harnesses where they have one; the sheet and
  watch are read-only claims and must be labelled as such).

**Commits:** two, matching the rule that a test and a fix are separate subjects.
`port/tests·port/src classic_mode` adds the rule and its test — additive only,
so that commit is green on its own but nothing calls the rule yet. Then
`console: cl_classic, and the mismatch announcements` adds the three call
sites and the verb. Neither reverts the other, and the second is meaningless
without the first, which is the normal shape for a new pure module.

---

## 3. Phase 1 — option C, slice A: fix the console

**Goal:** the console's two directions do what the setting says. Small, and
verifiable by the probe that found the bug.

`conCmdClient`'s row path calls `portOptSetIni(i, value)` with no knowledge of
`n64_aa`. The watch already has the exact helper — capture the value before the
write, call `rowClassicSync`'s equivalent after.

**Option A1 — reuse the watch's helper.** Export the sync from
`port_watchpages.c` (or move it beside `portClassicSetLive` in
`port_options.c`) and call it from `conCmdSet` for `n64_aa` only.
- Pro: one implementation, the watch's semantics exactly.
- Con: `port_watchpages.c` is front-end code; exporting from it inverts a
  dependency (`port_console.c` would depend on the watch). Moving the helper
  into `port_options.c` avoids that and is where it belongs anyway.

**Option A2 — the console asks the planner.** `conCmdSet` computes
`ClassicPlan` from Phase 0 and applies it.
- Pro: no dependency on the watch; uses the rule that is already tested.
- Con: duplicates the watch's applier shape for a second time instead of
  deleting one — which is what Phase 2 exists to stop.

**Recommendation: A1 with the helper relocated into `port_options.c`.** It is
the smallest change that adds no fourth implementation, and Phase 2 will make
the helper the only one.

### Exit criteria

- `cl_n64_aa 0` from a Classic-on state restores all seven values and
  `classic_saved` becomes irrelevant (executed — the same probe as the audit,
  now showing seven restored values instead of one line changed).
- `cl_n64_aa 1` from `480/bilinear/2/60/4/0/0` forces seven and writes a
  snapshot (executed).
- Both are idempotent on a second run (executed).
- Phase 0's temporary `optSet` log line is deleted in this commit, and the audit
  entry for the console defect is updated to point at the fix.

---

## 4. Phase 2 — option C, slice B: one authority

**Goal:** delete the duplicates. After this, an eighth entry point added next
year cannot get it wrong by omission.

Move the transition into `optSet` (or a thin wrapper every public setter calls),
then **remove**:

- `portClassicToggle`'s explicit `portClassicSetLive` call;
- `rowClassicSync` in the watch;
- the console call added in Phase 1.

and convert the sheet's `portSheetClassicPreset` / `portSheetClassicRevert` to
produce a `ClassicPlan` instead of poking `port_sheet_pending` directly — or, if
the sheet's pending-pick UX is too entwined, keep it and document it as the one
deliberate exception with a test pinning its behaviour.

The hazards from probe 4, each with its defusing move:

- **double apply** — the duplicates are deleted in the same commit, not left;
- **sheet mid-apply re-snapshot** — the sheet sets a suppression flag around
  its apply loop (`portClassicSetHold(1)` … `portClassicSetHold(0)`), the same
  shape as `portOptSaveHold`; while held, `optSet` computes nothing and the
  sheet's own plan applies. The flag is the *mechanism* that makes the ordering
  question unaskable, rather than a comment asking reviewers not to worry;
- **Reset All** — runs under the same hold, since it writes `n64_aa` raw on
  purpose;
- **ordering** — with the hold in place, a pending `n64_aa` anywhere in sheet
  order lands the same values. Pin it with a test that applies a plan with
  `n64_aa` first and last and asserts identical results.

### Exit criteria

- Every entry point (all seven from §1) produces consistent state, executed.
- `grep -c portClassicSetLive` outside `port_options.c` is 0.
- `ctest` 11/11, plus the ordering test above.
- The audit's headline finding is closed.

**Risk:** this is the slice most likely to regress a working path, and it is
also the one that makes Phase 3 cheap. If it cannot be landed safely in one
commit, it stays unlanded — Phase 0 and 1 already deliver the user-visible fix.

---

## 5. Phase 3 — option 3: split the key

**Goal:** `n64_aa` stops meaning two things.

Both splits were considered; the first is chosen.

### Chosen: `classic_mode` is the preset, `n64_aa` is the AA bit

- `classic_mode=0|1` — the preset is on. Every "is the mode on" consumer moves
  here: `portVideoN64AaOn`, the mipmap and pillar gates, the watch's row lock,
  the boot flag's guard.
- `n64_aa=0|1` — the VI AA bit only. `videoApplyAa` already treats it as exactly
  that (`vi`), so this side needs no new concept, only the removal of its
  preset duties.
- `classic_n64aa` already exists as the part that decides whether the preset
  touches the bit. That becomes the *only* link between them.

**Pros.** Matches the ini help and the menu label; migration is mechanical; the
AA setting becomes independently usable, which is what a player who wants N64
edges without 240p actually wants; the mismatch defect becomes structurally
impossible because forcing is a function of `classic_mode` + parts, not of a key
that is also a render setting.

**Cons.** Ini migration; a new menu row and a new ini key; every "preset is on"
consumer changes meaning and must be audited (14 references, 5 of them
semantic); `classic_saved`'s meaning is unchanged but its write sites all move.

### Rejected: keep `n64_aa` as the preset, add `classic_aa` as the bit

- **Pro:** no migration.
- **Con:** leaves a key named `n64_aa` doing preset duty, which is the
  confusion the split exists to end. Cheaper now, permanent confusion later.

### Migration

Read-side, in `videoLoadConfig`, once:

- old `n64_aa=1` with no `classic_mode` line → `classic_mode=1` **and**
  `n64_aa=1` (the old key meant both);
- old `n64_aa=0` → `classic_mode=0`, `n64_aa=0`;
- new files carry both. A marker key (`classic_split=1`) makes it run once, the
  same pattern `videoLoadConfig` already uses for the four empty `analog_*`
  binds.

Write-side: one pass rewrites the pair, and the old key keeps its AA meaning so
a downgrade is not catastrophic.

### Exit criteria

- Every Phase 2 criterion still holds, re-run.
- A pre-split `midas.ini` with `n64_aa=1 low_res=480` migrates to
  `classic_mode=1 low_res=240 classic_saved=480,...` on first launch — executed,
  with the file restored afterwards.
- `n64_aa=0 classic_mode=1` (N64 edges off, preset on) is a state the menus can
  express and the renderer honours — executed via the config line.
- The two keys cannot desynchronise into a mismatch, because only
  `classic_mode` forces anything (a test, not a review).

---

## 6. Documentation, carried by whichever phase lands first

Each is a one-line correction and gets its own commit:

- `tools/midas_ini_help.txt`: `room_cache` default is **0** on PC, not 2; and
  state that hardware pacing owns it.
- `port/fast3d/gfx_roomcache.inl`: "default OFF everywhere" contradicts the
  Xbox arm in `video.c`. Resolve or mark.
- `CLI.md` §3.2: document the mismatch announcement and `cl_classic`.
- `docs/port/settings-and-save-audit.md`: update each finding as its phase
  lands, and never rewrite an earlier conclusion to match a later one — the
  record of a wrong first reading is worth more than a tidy one.

---

## 7. Verification, every phase

```bash
cd /Users/jcorneli/Developer/DeepSeek/PM-fork
cp build_macos/midas.ini /tmp/ini.before
./scripts/port/port_test.sh --no-game        # build line + one line per test
diff /tmp/ini.before build_macos/midas.ini   # must be empty
md5 -q build_macos/Data/Saves/eeprom.bin     # 3590c7df0edb0930f4e4248826e8ab64
```

**Precedence probes must set `n64_aa=0` explicitly and check the
`-n64_aa -> Classic Mode on (applied)` line first.** Two false findings in the
audit came from the user's ini being Classic-on, which makes the flag a no-op.
Any probe that does not do this is measuring the env var, not the flag.

Live runs must report their renderer; from a sandboxed shell this machine gets
`Apple Software Renderer` and a window that may not composite (`CLI.md` §7.2).

---

## 8. Out of scope, deliberately

- **Window restore onto a disconnected display** (audit §2.5). Real, but it is a
  window-management question with no relation to Classic Mode.
- **Crash-time loss of a held ramp value.** `atexit` is the mechanism and it
  cannot run on a crash; changing that means a periodic flush, which is a
  different subject with its own cost.
- **The inert `mipmaps` menu row** beyond its documentation line. Removing a row
  from a shipped menu is a product decision, not a cleanup.
