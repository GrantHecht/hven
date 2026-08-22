# ARM-H — the Task 5 IPM band-trip discriminator

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

## The three arms

| arm | header (candidate_point.h) | dispatch tail (non_linear_program.cpp) | how it's built |
|---|---|---|---|
| BASE | pre-task | pre-task (bare else) | `git worktree` at `07d5ee1` |
| ARM-H | **landed** | **reverted** to pre-task bare else | `git worktree` at HEAD + `arm_h.patch` |
| LANDED | landed | landed (else-if / refuse-throw) | main tree at HEAD, unpatched |

BASE and ARM-H build from throwaway `git worktree` checkouts under
`.scratch/task5-armh/` — the same shape the standing legs use for their base
arm — so the patch never touches the main tree. LANDED builds directly from
the main tree, the same shape the standing legs use for their head/landed
arm, since the main tree already sits at the wanted commit.

## Protocol (adjudicated, verbatim)

n = 240 IPM cells, both estimators (median-of-per-rep-medians,
minimum-of-per-rep-minimums) + the base arm's rep-spread, two repetitions,
quiet machine, six cells (3 arms × {serial, threaded}), three-way read BASE
vs ARM-H vs LANDED. This mirrors `docs/notes/data/m4-ipm-wall-leg/ipm_wall_leg.sh`'s
own protocol exactly, extended from two sides to three; the probe binary
(`ipm_time.cpp`) is reused unmodified, so `n = 60` and `n = 120` rows print
alongside `n = 240` as a free consistency check but are not part of the six
adjudicated cells.

## Decision tree (verbatim, as handed down)

- **ARM-H ≈ LANDED, both out of band** → mechanism confirmed. Accept with
  mechanism, as **layout-class exemplar #2**, with the discriminator
  attached. The ceiling is **not** raised. Owner visibility at the **Task 9
  PR** (tycho also flags to Grant now).
- **ARM-H in band** (i.e., reverting only the dispatch tail brings the
  number back down near BASE) → the dispatch edit is implicated, not the
  header. Mitigation round: cold/noinline-outline the refuse arm, then
  re-measure.
- **Ambiguous** → joint settler + tycho adjudication.

Noted tension carried alongside this tree (progress ledger, not this
harness's to resolve): the "ARM-H ≈ LANDED" accept branch sits against the
settler's separately-stated named-mitigation lean (split `candidate_point.h`
so the table/constants/docs move out of the hot path's includes), on the
accretion-vector argument that the mapping table grows again at the
transcription-provider task and unbounded accretion should not be accepted
now just because this trip's mechanism is confirmed as layout.

Band-trip fork agreed alongside ARM-H (for context, not this harness's
scope): the post-window sequence is (a) zero-build objdump/nm forensics on
the two IPM TUs base-vs-landed, (b) this ARM-H run, (c) mitigation targeted
by (a)+(b) — dispatch-arm-implicated → cold/noinline outline;
header-code-implicated (the `is_legal_request` disjunct chain) →
table-driven legality check sized for future shapes; pure alignment luck →
acceptance-with-mechanism, priced to the owner. Mitigation custody, if
needed, is the tycho lane's.

## Three-way read (BASE / ARM-H / LANDED) — TO BE FILLED IN AFTER EXECUTION

Fill from `armh_wall_1_aggregate.txt` / `armh_wall_2_aggregate.txt` (produced
by `run_arm_h.sh` via `aggregate_armh.py`), reading the **minimum** estimator
per the standing leg's own convention (the median's rep-to-rep noise on
these problem sizes, 0.5–0.8%, is comparable to the effects being looked
for).

```
repetition 1
mode      n     min(base)   min(armh)   min(landed)   armh_vs_base   landed_vs_base   armh_vs_landed   base rep-spread
serial    240   TBD         TBD         TBD            TBD%           TBD%             TBD%             TBD%
threaded  240   TBD         TBD         TBD            TBD%           TBD%             TBD%             TBD%

repetition 2
mode      n     min(base)   min(armh)   min(landed)   armh_vs_base   landed_vs_base   armh_vs_landed   base rep-spread
serial    240   TBD         TBD         TBD            TBD%           TBD%             TBD%             TBD%
threaded  240   TBD         TBD         TBD            TBD%           TBD%             TBD%             TBD%
```

**Bit-identity falsification result (`bit_identity_check.sh`):** TBD — PASS
means ARM-H's `flag`/`xnorm2` are byte-identical to LANDED's on every probed
size (60, 120, 240) in both modes (serial, threaded); a FAIL falsifies the
semantic-inertness premise the discriminator relies on and must be explained
before any wall-time reading is trusted.

**Verdict (decision tree above): TBD.**

## Provenance-header skeleton

```
# armh.commit: <HEAD sha at execution time, e.g. 8503f39...>
# armh.base_commit: 07d5ee1
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
| `arm_h.patch` | reverts only the dispatch tail of `src/model/non_linear_program.cpp` to its pre-task bare-else form; `git apply --check` clean against HEAD |
| `run_arm_h.sh` | builds BASE/ARM-H/LANDED, builds the three IPM probes, runs the three-arm wall leg (2 repetitions), aggregates. Refuses to start while `HOLD` exists. |
| `bit_identity_check.sh` | falsification harness: diffs ARM-H's vs LANDED's `flag`/`xnorm2` fields (the probe's only per-cell answer output — there is no separate captured-answer file in this instrument's standing convention, unlike the bench CSV leg; see the script's own header) |
| `quiet_check.sh` | machine-quiet pre-check, copied convention from `docs/notes/data/2026-08-21-m4-task5-wall/quiet_check.sh` |
| `aggregate_armh.py` | three-arm adaptation of `docs/notes/data/m4-ipm-wall-leg/aggregate.py` |
| `HOLD` | the interlock; removed by the controller at the window's end mark |
