# Two-level model contract, stage 2 — wall-leg record (M4, hven side)

BASE `a062fcc` (stage-2 parent) vs HEAD `1939a3d` (stage-2 tip, before the
ledger commit). Instrument: the standing IPM wall leg — whole solves of a
dense-Jacobian NLP through NLPSolver, 9 reps x 15 inner solves, base/head
alternated per rep, serial arm pinned under `MKL_NUM_THREADS=1` and threaded
arm, n in {60, 120, 240}. `aggregate.py` prints median and minimum estimators
with the base rep-spread; deltas are read off the MINIMUM estimator. Binaries
built by `build_ipm_time.sh` against each side's `libhven.a`; provenance and
image checksums in `provenance.txt`.

Standing policy: one-sided +3.0% band at serial n=240; positive mechanism
evidence auto-closes a trip; hot-path symbol growth investigates at any size.

| arm | n | median | minimum | base rep-spread |
|---|---|---|---|---|
| serial | 60 | -0.77% | -0.69% | 0.55% |
| serial | 120 | -1.76% | -1.76% | 2.60% |
| **serial** | **240** | **-2.20%** | **-2.69%** | **0.54%** |
| threaded | 60 | -1.23% | -1.20% | 31.21% |
| threaded | 120 | -1.50% | -1.44% | 32.62% |
| threaded | 240 | -2.55% | -2.67% | 0.49% |

**ASSERTED.** The asserted cell is serial n=240 at -2.20% / -2.69% — inside the
band, and on the fast side of it. The box was quiet at the gate (loadavg 0.49,
no other compile or bench running; the leg waits for loadavg[1] < 0.6 while
holding the box lock) and the n=240 base rep-spread is 0.54%, well under the
~1.3% above which a run is informational rather than asserted. The two smallest
threaded cells carry their usual 31-33% rep-spread and are not read.

**Identity.** All 54 base/head pairs bit-identical: `flag=0` throughout, and one
`xnorm2` per size on both sides — 1.3750963021836506 (n=60),
2.7501926043856093 (n=120), 5.5003852082409646 (n=240).

**Mechanism (`nm`).** Of 287 filtered symbols in the two probe images, exactly
ONE changed size: `NonLinearProgram::assemble_impl`, 0x3249 -> 0x3259, +16
bytes. That is the refused-shape-name argument added to the terminal refusal
arm, which is cold and unreachable for every supported request. No other symbol
moved, so nothing on the hot path grew and the -2.2% is machine-state and code
layout rather than any change in work done.

**What this leg cannot see.** The interior-point probe image links no
`AggregateEvalSeam` symbol at all — the interior path never reaches the seam —
so the seam work in this stage (the stale-bundle refusal, the max-location
scan, the block disjointness check, the docstring corrections) is invisible
here by construction. The SQP-side bench is the arm that observes it; its
checks are per lay and per major, not per minor.

Files: `leg_per_rep.csv` (raw per-rep rows: arm, rep, side, n, inner, median_s,
min_s, flag, xnorm2), `leg_aggregate.txt`, `provenance.txt`, `nm_base_a062fcc.txt`
/ `nm_head_1939a3d.txt` (symbol-size dumps filtered to the seam, provider,
aggregate and driver symbols), and the three harness scripts as run.
