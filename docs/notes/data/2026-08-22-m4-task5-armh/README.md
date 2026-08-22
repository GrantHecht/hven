# ARM-H — the Task 5 IPM band-trip discriminator, run as a noise-floor calibration

**Relabeled by owner ruling** (`docs/notes/2026-08-m4-ledger.md`, 2026-08-22):
layout-class wall-band trips are instrument noise, so this session runs ONCE as
a noise-floor calibration and the accept-vs-mitigate decision tree it was
written under is superseded. See "What this session is for" below; the arms,
protocol and estimators are unchanged.

**Status: PREPARED, NOT EXECUTED.** This directory's contents (`arm_h.patch`,
`run_arm_h.sh`, `bit_identity_check.sh`, `quiet_check.sh`,
`aggregate_armh.py`) were written under the SQP lane's hold while a
measurement-critical process (the Task 5 census + W-gate scorer leg,
`docs/notes/data/2026-08-21-m4-task5-wgate-leg/`) owned the machine. A file
named `HOLD` sits beside this README for exactly that reason: `run_arm_h.sh`
and `bit_identity_check.sh` both refuse to start while it exists. The
controller removes `HOLD` at the measurement window's end mark; ARM-H's
builds start then, independent of the boundary review's verdict on
`07d5ee1..8503f39`.

## Why ARM-H exists

The standing IPM wall leg (`docs/notes/data/2026-08-21-m4-task5-wall/README.md`,
runs `docs/notes/data/m4-ipm-wall-leg/runs/2026-08-22-task5-{1,2}.log`) found
the `n = 240` IPM cells **OUT OF BAND**: +2.2…+2.4% across two repetitions,
both estimators, all six cells (3 arms shown there × 2 modes) positive,
clearing both the base arm's own rep-spread and the standing precedent's
+0.97% upper end. Answers were bit-identical (`flag`/`xnorm2` unchanged
base-vs-landed on every cell of both repetitions). The named mechanism was
code **layout**: `include/hven/model/candidate_point.h` grew (three new
`EvalRequest` shape constants, an extended `is_legal_request`, mapping-table
prose), which recompiles two already-timed-path translation units
(`interior_point_solver.cpp.o`, `non_linear_program.cpp.o`) with grown cold
paths — an inlining input — while a new archive member
(`aggregate_eval_seam.cpp.o`) shifts every symbol after it in the link. The
dispatch tail's own new work (`src/model/non_linear_program.cpp`'s
`assemble_impl`, three integer-equality tests plus a never-taken terminal
throw) was ruled too small to be the delta and, per the standing leg, never
fires on any timed cell (every recorded `flag` is 0).

That attribution — header growth, not the dispatch edit — was **measured,
not proven**: no arm existed that held the header exactly as landed while
reverting only the dispatch code. ARM-H is that arm.

## What ARM-H isolates

The Task 5 Level 2 consumption switch (base `07d5ee1` → landed `8503f39`)
changed two things in the same commit (`0451f35`):

- **(a)** grew `include/hven/model/candidate_point.h` — three new
  `EvalRequest` shape constants, extended `is_legal_request`, mapping-table
  text.
- **(b)** changed `src/model/non_linear_program.cpp`'s `assemble_impl`
  dispatch tail from a bare `} else { <full-KKT body> }` to
  `} else if (request == kRequestFullKkt) { <same body> } else { <terminal
  refuse throw> }`.

**ARM-H isolates (a) from (b):** header exactly at landed state, dispatch
reverted to its pre-task bare-else form. `arm_h.patch` carries only (b)'s
reversal (`git diff 0451f35 0451f35^ -- src/model/non_linear_program.cpp`,
verified `git apply --check` clean against `8503f39` — the file has had no
further edits since `0451f35`, so the reverse of that commit's diff for this
one file applies without conflict).

## The four arms

| arm | header (candidate_point.h) | dispatch tail (non_linear_program.cpp) | how it's built |
|---|---|---|---|
| BASE | pre-task | pre-task (bare else) | `git worktree` at `07d5ee1` |
| ARM-H | **landed** | **reverted** to pre-task bare else | `git worktree` at `8503f39` + `arm_h.patch` |
| LANDED | landed | landed (else-if / refuse-throw) | `git worktree` at `8503f39`, unpatched |
| FIXED | landed | **post-fix-batch**: the refuse throw now enumerates the eight supported shapes inline | main tree at HEAD, unpatched |

BASE, ARM-H and LANDED build from throwaway `git worktree` checkouts under
`.scratch/task5-armh/` — the same shape the standing legs use for their base
arm — so the patch never touches the main tree. FIXED builds directly from
the main tree, the same shape the standing legs use for their head arm, since
the main tree already sits at the wanted commit.

**Why FIXED is a fourth arm rather than a re-labelled LANDED.** The boundary
review's fix batch changed the two things this leg is timing around: the
dispatch tail's refuse message grew (it names the eight supported shapes and
their masks instead of characterising them), and the bridge's scatter gained a
per-entry coordinate check on the KKT path. LANDED's adjudication is about a
commit that no longer ships, so it does not transfer to the shipping state by
assumption; the leg re-asks the same question of FIXED, on the same occasion,
by the same estimator, against the same BASE. LANDED and ARM-H are pinned to
`8503f39` by commit for the same reason — `arm_h.patch` reverts the exact text
the fix batch rewrote and does not apply at the current HEAD, nor should it be
made to.

## Protocol (adjudicated, verbatim)

n = 240 IPM cells, both estimators (median-of-per-rep-medians,
minimum-of-per-rep-minimums) + the base arm's rep-spread, two repetitions,
quiet machine, eight cells (4 arms × {serial, threaded}), four-way read BASE
vs ARM-H vs LANDED vs FIXED. This mirrors `docs/notes/data/m4-ipm-wall-leg/ipm_wall_leg.sh`'s
own protocol exactly, extended from two sides to four; the probe binary
(`ipm_time.cpp`) is reused unmodified, so `n = 60` and `n = 120` rows print
alongside `n = 240` as a free consistency check but are not part of the eight
adjudicated cells.

## What this session is for (owner ruling — supersedes the decision tree)

**The accept-vs-mitigate decision tree this directory was written under is
superseded.** The owner ruled on 2026-08-22 that **layout-class wall-band trips
are instrument noise, not product regressions** — see
`docs/notes/2026-08-m4-ledger.md` (entry dated 2026-08-22, "OWNER RULING …
LAYOUT-CLASS WALL-BAND TRIPS ARE INSTRUMENT NOISE"). The basis is that
production hven builds with link-time optimization maxed
(`cmake/hven_compile_options.cmake`, `LINK_TIME_OPT` → `-flto=full`/`thin` +
IPO), so code placement measured under this instrument's pinned **non-LTO**
regime does not survive into the product binary. The wall instrument's job is
unchanged: regression detection in its own fixed configuration.

What that makes this session:

- **It is a NOISE-FLOOR CALIBRATION, run ONCE.** Its output re-derives the band
  ceiling above the measured layout-luck variance. That re-derived band, plus
  the auto-close rule below, become the standing instrument policy.
- **A band trip proven layout-class AUTO-CLOSES thereafter** — no multi-lane
  adjudication. "Proven layout-class" is the mechanism check: no hot-path work
  added, and answers bit-identical (which is what `bit_identity_check.sh` is
  for, now over ARM-H *and* FIXED).
- **The mitigation branch is CLOSED** — no header split, no alignment work, no
  cold-outlining — unless a trip ever shows real added hot-path work.
- The current +2.2…+2.4% trip, already proven pure layout at symbol level,
  **closes under this ruling**, with this calibration as its residual value.

Open for Task 9 planning (owner choice, not ruled): whether one LTO-on bench
leg runs as product-truth context — never a gate.

Nothing else in this harness changes: the per-arm layout classification, the
repetition protocol, the minimum estimator and the rep-spread rules, and the
four arms all stand exactly as amended. Only the interpretation of the numbers
does.

## Four-way read (BASE / ARM-H / LANDED / FIXED) — TO BE FILLED IN AFTER EXECUTION

Fill from `armh_wall_1_aggregate.txt` / `armh_wall_2_aggregate.txt` (produced
by `run_arm_h.sh` via `aggregate_armh.py`), reading the **minimum** estimator
per the standing leg's own convention (the median's rep-to-rep noise on
these problem sizes, 0.5–0.8%, is comparable to the effects being looked
for).

```
repetition 1
mode      n     min(base)  min(armh)  min(landed)  min(fixed)  armh_vs_base  landed_vs_base  armh_vs_landed  fixed_vs_base  fixed_vs_landed  base rep-spread
serial    240   TBD        TBD        TBD          TBD         TBD%          TBD%            TBD%            TBD%           TBD%             TBD%
threaded  240   TBD        TBD        TBD          TBD         TBD%          TBD%            TBD%            TBD%           TBD%             TBD%

repetition 2
mode      n     min(base)  min(armh)  min(landed)  min(fixed)  armh_vs_base  landed_vs_base  armh_vs_landed  fixed_vs_base  fixed_vs_landed  base rep-spread
serial    240   TBD        TBD        TBD          TBD         TBD%          TBD%            TBD%            TBD%           TBD%             TBD%
threaded  240   TBD        TBD        TBD          TBD         TBD%          TBD%            TBD%            TBD%           TBD%             TBD%
```

`fixed_vs_base` is the row the shipping state is judged on; `landed_vs_base`
stays in the table as the same-occasion recomputation of the already-adjudicated
trip, and `fixed_vs_landed` says whether the fix batch moved the number at all.
All four arms feed the calibration: the spread ACROSS them, at bit-identical
answers, is the layout-luck variance the re-derived band ceiling has to sit
above. `fixed_vs_base` is additionally the row the shipping state is read on.
If `fixed_vs_base` and `landed_vs_base` disagree in sign or clear the old band
differently, that is a datum for the calibration, not a trip to adjudicate —
unless the mechanism check shows real added hot-path work, which is the one
condition that reopens the closed mitigation branch.

**Bit-identity falsification result (`bit_identity_check.sh`):** TBD — PASS
means ARM-H's *and* FIXED's `flag`/`xnorm2` are byte-identical to LANDED's on
every probed size (60, 120, 240) in both modes (serial, threaded); a FAIL
falsifies the semantic-inertness premise the discriminator relies on (ARM-H)
or the fix batch's own (FIXED — both of its changes are refusal paths, so no
answer on a timed cell may move), and must be explained before any wall-time
reading is trusted.

**Calibration output: TBD** — the re-derived band ceiling, stated above the
measured layout-luck variance across the four arms, plus the mechanism-check
result that decides whether the standing trip auto-closes under the ruling
(`docs/notes/2026-08-m4-ledger.md`, 2026-08-22).

## Provenance-header skeleton

```
# armh.commit: <HEAD sha at execution time -- the FIXED arm>
# armh.base_commit: 07d5ee1
# armh.landed_commit: 8503f39   (LANDED arm, and ARM-H's patch base)
# armh.patch: arm_h.patch (git diff 0451f35 0451f35^ -- src/model/non_linear_program.cpp)
# armh.patch_apply_check: <git apply --check result at execution time>
# armh.date: <UTC date of the run>
# armh.host: fedora, AMD Ryzen 7 5800X3D 8C/16T, 31 GiB, Linux 7.1.5-201.fc44
# armh.compiler: /usr/bin/clang++, MKL LP64, Release, HVEN_FP_MODE=SAFER_FAST
# armh.quiet_check.pre_build: <contents of pgrep-before-build.txt>
# armh.quiet_check.pre_rep1: <contents of pgrep-before-armh-1.txt>
# armh.quiet_check.pre_rep2: <contents of pgrep-before-armh-2.txt>
# armh.reps: 9
# armh.inner: 15
```

## Files

| file | role |
|---|---|
| `arm_h.patch` | reverts only the dispatch tail of `src/model/non_linear_program.cpp` to its pre-task bare-else form; `git apply --check` clean against `8503f39`, and deliberately NOT against the current HEAD (the fix batch rewrote the text it removes) |
| `run_arm_h.sh` | builds BASE/ARM-H/LANDED/FIXED, builds the four IPM probes, runs the four-arm wall leg (2 repetitions), aggregates. Refuses to start while `HOLD` exists, and refuses if the main tree has uncommitted changes under `include/ src/ bench/ tests/` — the FIXED arm measures a commit, not a working state. |
| `bit_identity_check.sh` | falsification harness: diffs ARM-H's vs LANDED's `flag`/`xnorm2` fields (the probe's only per-cell answer output — there is no separate captured-answer file in this instrument's standing convention, unlike the bench CSV leg; see the script's own header) |
| `quiet_check.sh` | machine-quiet pre-check, copied convention from `docs/notes/data/2026-08-21-m4-task5-wall/quiet_check.sh` |
| `aggregate_armh.py` | four-arm adaptation of `docs/notes/data/m4-ipm-wall-leg/aggregate.py` |
| `HOLD` | the interlock; removed by the controller at the window's end mark |
