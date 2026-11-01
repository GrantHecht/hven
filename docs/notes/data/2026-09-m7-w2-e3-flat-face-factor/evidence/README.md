# Evidence index — M7 W2 E3

Copied read-only, verified byte-identical against the worktree source with `cmp` (gzipped files with
`zcat | cmp`), from `<worktree agent-a69f46e11796560b9>/.scratch/e3/` (293 MB total there; this
directory is the trimmed subset the DO instructions named). Total here: ~5.4 MB.

## What was copied

- `corpus/*.txt` — the 4-mode × 2-algebra × E2-on/off attribution corpus (§3a/§3d/§3g/§3l of
  `../REPORT.md`): `base`/`baseoff` (mode 0, control), `m1`, `m2`/`m2off` (the candidate), `m3`
  (the refinement-only control), each `{border,refactorize}-{new15,old36}`.
- `logs/*.log` — every log the report cites: the cycle energy ledger (E3-1a), the offline dense
  re-solves (E3-1b/c: `e31c-yk3-it177.log`, `e31-cs1-it216.log`, `e31c-inengine-yk.log`), the
  non-termination and NaN-onset mechanism logs (§3e), the descent-invariant logs (§3f), the
  corpus/attribution logs (§3g), the census+HS logs per mode (§3i), the Release/Debug ctest logs
  per mode (§3h/§3k/§3l), and the U0 phase log.
- `probes/base/Cs_STEENBRB_qp{1,3,5}_*.{ord,all}.csv[.gz]` — the E3-1(a)/(b) headline per-pass and
  per-action CSVs: the mode-0 (absolute-centring, shipped) walk on the three Cs_STEENBRB cycle cells
  the report's tables in §1a/§1b are read from (qp1 both algebras — "border and refactorize summaries
  IDENTICAL" — qp3 and qp5 border). CSVs over 1 MB are gzipped (`Cs_STEENBRB_qp1_border.{all,ord}.csv`,
  `qp1_refactorize.ord.csv`, `qp3_border.all.csv`, `qp5_border.{all,ord}.csv`); `qp3_border.ord.csv`
  (784 KB) is kept plain. The `.out` run logs are kept alongside each CSV.
- `u0/`, `u0m3/` — the full U0 comparison outputs (27 cells × 75 columns × 3 engines) for mode 2 (the
  candidate, §3j) and mode 3 (the refinement-only control, §3l): `comparison.txt`, the per-engine
  `head-{walk,ssn,ipm}.csv`, and the `run-{walk,ssn,ipm}.log` transcripts.
- `tools/*.py` — every analysis/instrumentation script the report names: `patch_e3_measure.py` and
  `patch_e3_arm.py` (the two scratch patches, §1/§2), `eqp.py`/`eqp3.py` (the offline dense re-solve,
  §1c/§3b), `rowcmp.py` (the corpus comparator, §3a), `cyc.py`/`drift.py`/`face.py`/`fcmp.py`/`line.py`/
  `probe.py`/`table.py`/`patch_fix1.py` (supporting analysis used across the round's phases).
- `cs.list`, `dumps.list`, `new15.list`, `old36.list` — the corpus cell-list inputs the runners read.
- `STATE.md` — the round's own closing status note.

## What was left out, and why

- **`probes/m2/`, `probes/arm/`, `probes/diag/`** (the bulk of the 153 MB `probes/` tree: the full
  4-mode attribution corpus's per-pass CSVs for `A0_YORKNET` qp1–qp6, `Cs_STEENBRB` qp1–qp6 under
  E3_PROX 0–3, `ACOPP57_def`, `SAWPATH_def`, `STEENBRB_def`, `Hj_STEENBRD`). The DO instructions scope
  the probe copy to "the E3-1 measurement outputs" — §1's three measurements, which are backed by the
  `probes/base/` cells copied above; the §3 battery's numbers are already tabulated in `REPORT.md`
  itself and in `corpus/*.txt` and the `logs/*.log` this directory carries in full. Including the rest
  of `probes/` would run to ~150 MB, far past the ~25 MB budget, for detail the report's tables already
  report.
- **`corpus/*.err`** — stderr captures alongside each `.txt`; the DO instructions name `.txt/.csv/.md`
  only.
- **`.scratch/e3/tools/replay/`** (a small CMake C++ project, `dump_replay.cpp` — a build tree, not a
  script) and **`install/`, `tools-build-replay/`** (build/install trees) — excluded per the DO
  instructions (no build trees, no `install/`).
- **`chain{2,3,4,5}.out`, `PHASE{3,4,5,6,7}_DONE`** — empty/marker files and background-job stdout
  captures with no measurement content beyond what the corresponding `phase*.log` already carries.
- **`.seq`** — none existed in this round's scratch tree.
- **State dumps (`.state` files, e.g. `yk3_it177.state`, `cs1_it216.state`)** — these are the inputs to
  the offline dense re-solves (§1c/§3b); they live in `.scratch/nc2/` (an earlier round's read-only
  evidence, reused here) and were not part of this round's own `.scratch/e3/` tree to copy from. The
  `logs/e31c-yk3-it177.log` and `logs/e31-cs1-it216.log` carry the offline tool's full output.

## Verification

Every plain-text file above was compared with `cmp` against its worktree source; every gzipped CSV was
compared with `zcat <file>.gz | cmp - <source>`. All copies are byte-identical.
