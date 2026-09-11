# `redraw/` — the redraw experiment (W5 T8.9r-attrib4, 2026-09-11)

`../mechanism.md` (attrib3) charged a third of `9cebbbe`'s step to the result
core's per-call allocate/free churn, on an allocator intervention that removed
99.7 % of the extra page faults and 32.8 % of the step, and left two thirds
unexplained. **This leg asks whether making that storage PERSISTENT in SOURCE
recovers the step.** Two candidate fixes, four arms, five solo rounds.

**IT DOES NOT.** E2 recovers **+4.5 %** of the corpus step and E1 is a
**regression** (−16.5 %), both inside or about one round-to-round spread —
while the page-fault counter moves hard in both directions. **A source change
that removes 57.3 % of the head's excess faults buys 4.5 % of the wall step.**

| file | what it is |
|---|---|
| `steps.md` | the four-arm per-row wall table, the recovery, the gate, the solo evidence |
| `mechanism.md` | what E1/E2 change with file:line, the two facts that bound them, the fault instrument, E3 |
| `experiment-E1.patch` | `e51a7e0` + persistent result storage (`reset_for_call()`, copy at exit) |
| `experiment-E2.patch` | E1 + persistent iterate storage (`iters` hoisted to a member) |
| `predeclaration.txt` | the scoring rules, hashed at 20:11:57 UTC, before the first timed sample |
| `wall.csv`, `faults.csv` | the tidy per-row wall (4 arms × 11+ rows × 5 rounds) and every fault reading |
| `redraw.out`, `faults.out` | the two analysis tools' saved output |
| `scripts/` | every script this leg ran, including the nice-corrected `idle_proof.py` |
| `logs/` | every batch's brackets, the idle proof, the build/gate/fold-proof logs, E3 |
| `raw/` | every CSV every arm wrote, the gate captures, the fault and TLB readings |
| `perf/` | E3's dTLB/L1 summary (`perf record` data blobs are NOT retained; the reports are in `logs/E3-perf.log`) |

**NEITHER PATCH IS ADOPTED AND NEITHER IS COMMITTED AS SOURCE.** They were
applied only to `git archive` extractions under a scratch root and built there.
No library, test or bench source in this repository was changed by this leg.
E1 in particular should NOT be adopted: it is slower on both instruments.

**The wall figures were taken under the T8.9r fix1 R2 discipline with the
NICE-INCLUSIVE accounting correction** astra's fix1-review item 7 required —
five timed WALL batches, all PROVEN on the fix2 R2' re-audit (this leg's four
COUNT batches — `e3` and `pf-r1..r3` — bracket no timed run at all and are
therefore UNPROVEN; `../../../logs/IDLE-PROOF.md` says so), round 2
re-run twice (the second failure was visible ONLY under the correction).
Nothing was ever signalled.

**No disposition is offered — §11.1 and the owner have it.**
APPLE / ACCELERATE: UNOBSERVED.  WINDOWS: UNOBSERVED.
