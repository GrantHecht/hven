# `t84/` — inside T8.4 (W5 T8.9r-attrib3, 2026-09-11)

`reading.md` §11 charged the interior leg's **+2.49 %** to T8.4 and stopped
there. This leg asks the two questions §11 left: **which commit inside T8.4**,
and **by what mechanism**, with the mechanism to be proven by a scratch build
whose smallest edit moves the cost.

| file | what it is |
|---|---|
| `arms.txt` | the six bisect arms and the five experiment arms — build recipe, submodule pins, hashes, the calibration, the measurement pin, the argv lock |
| `predeclaration.txt` | the scoring rules, hashed at 18:50:35 UTC, before the first timed sample existed (`logs/B0-predeclaration.log`) |
| `steps.md` | **Step A** — the per-commit per-row wall table and the carrier |
| `mechanism.md` | **Step B** — the diff reading with file:line, the four experiments, and what is left |
| `layout.txt` | the solver object's member offsets at every arm and under both layout patches |
| `experiment-{1,2}.patch`, `experiment-3-{4096,9712,16384}.patch` | the probes. **None is a proposed fix and none is committed.** |
| `bisect_t84.py` + `.sha256` + `.out`, `wall.csv` | Step A's tool, its saved output, and the tidy per-row wall it emits |
| `experiments.py` + `.sha256` + `.out`, `experiments.csv` | experiments 1 and 2's tool and output, including the counter-identity check |
| `mechanism_tables.py` + `.sha256`, `mechanism-tables.txt` | §B0–B4 of `mechanism.md`: the all-row table, `perf stat` across six arms, the cycle profile, the page-fault tables, experiment 4 |
| `logs/` | every batch's brackets, the five idle proofs, the build logs, the identity/calibration log, the fold proof both ways |
| `raw/` | every CSV every arm wrote, the binaries' hashes, the smoke, and the superseded first experiment round set with its README |
| `perf/` | the `perf record` data and reports, the `perf stat` outputs, the annotations |
| `scripts/` | every script this leg ran, including the T8.9r `idle_proof.py` it reuses unchanged |

**Everything here asserting wall clock was taken under the T8.9r fix1 R2
discipline** — one solve at a time, `taskset -c 2`, `MKL_NUM_THREADS=1`, the box
lock held for the whole batch, the driving shell pinned off the measurement core
and its SMT sibling. **Twenty-five timed wall batches across five legs, all
PINNED-CLEAN**; `scripts/idle_proof.py` exits 0 over each of the five
(`logs/IDLE-PROOF.md`, `-X.md`, `-Y.md`, `-A.md`, `-F.md`). Foreign un-niced user
time on the pinned core reads exactly zero on twenty of the twenty-five and never
exceeds 0.050 s (0.1292 %) on the rest. **Nine batches** failed their window on a
first pass and were **re-run** — `xwall-r1..r4` on the pinned core, `alloc-r3` and
`wallpf-r1..r4` on its SMT sibling; the re-runs are the retained data and nothing
was ever signalled.

**No library, test or bench source in this repository was changed by this leg.**
The experiment patches were applied only to `git archive` extractions under a
scratch root and built there.

**No disposition is offered — §11.1 and the owner have it.**
APPLE / ACCELERATE: UNOBSERVED.  WINDOWS: UNOBSERVED.
