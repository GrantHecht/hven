Case H0 -- I19-2a shipped nothing (settler ruling C537). Baseline arm id: **H0** (kRatio, SOC on, every lever off; I19-2a's own scoring baseline), rows from `store-H`.

# RESULTS-TU-3: the cluster-label read of the I19-2a store

Read under `DECLARATION-TU-3.md` (not edited). No solver was run. Reader: `tu3/tu3_read.py` (cell rule = `tu3/rule_tu1.py`, `git show tu1:tu/rule.py` verbatim); raw output `tu3/tu3_read.out`; one row per candidate x cluster x cell `tu3/tu3_cells.csv`; sha256 of every row file read `tu3/store-sha256.txt` (112 files; its own sha256 `7911758149ed07043d536dd95e3d17c2bffa849ca85d97e857020a71c131d996`). Two runs byte-identical (cells csv, raw output, sha list).

Provenance: store `hven-bench-i19/.scratch/i19-2a/store-H`; every row file header carries hven pin `91787d329a06ae99770879762804a1babf316cbe` and harness `56fa03fdf40acb38275f0a46058ddbd2da925f0e` (uniform, equal to the I19-2a BINARIES.txt lane-H pin and the frozen harness); lane-H binary `build-i19-H/lanes/hven/hven_bench_cell` sha256 `617241550fc016e235e95a46667f3273aee66fba6f1af8841cb7af8f633bc72b` equals BINARIES.txt. `clusters.py --verify` passed (K1=8, K2=5, K3=8, K4=9, K5=6) and the section-3 member lists equal `clusters.py` output. Harness `i19` at 56fa03f; `tu/clusters.py` and `tu/tu1_spec.json` identical to `tu1`'s blobs. Date 2026-11-24. No wall-clock read. No section-8 stop fired. HOLDOUT never read.

## Verdict

**NONE for every cluster K1-K5. No label ships.** The M7 close section 3 guidance stays. K (composite) not run: no I19-2a arm passed stage 1.

| cluster | verdict |
|---|---|
| K1 network | NONE |
| K2 quadratic objective, general constraints | NONE |
| K3 constraint-dominant | NONE |
| K4 large | NONE |
| K5 sum-of-squares objective | NONE |

## What the store covers (decides most of the table)

I19-2a ran **NLP-46 only for H0, P and C1** (stage 2; the G-arms and C2 were stage-1 arms on TRAIN-238, CAP-238-1, CAP-238-01 and SENS-P only; RESULTS-I19-2a Deviation 10). K1-K4 consist entirely of NLP-46 CUTEst cells, and K5 has 2 NLP-46 cells (YORKNET x 2). So:

* **G1-G7 and C2: K1-K4 NOT COVERED (0 rows); K5 PARTIAL (10 of 12 cells, the hs011/014/025/027/028 cells on TRAIN-238).** The SQP-CUTEst family sign (NLP-46, 46 cells) is unevaluable for them, so item 2 cannot be satisfied: no label, whatever the partial reads say. NOT SCORED cells per cluster for each of these nine arms: K1 16, K2 10, K3 16, K4 18, K5 2 (YORKNET/default and YORKNET/sqp-qn). Full list in `tu3_cells.csv` (verdict `NOT_SCORED`).
* **C1 and P: all cluster cells scored** (C1 K1 16, K2 10, K3 16, K4 18, K5 12; P identical). Realized cell counts equal the declared maxima; no sd-roster cell carries a cluster-member name.

## Per-cluster per-candidate verdicts (cell rule vs H0; reach = `outcome` optimal; primary counter `factorizations`)

W/L/T are cluster wins/losses/ties over the cluster's cells (both base labels). "family" columns are the whole-family signs (SQP-HS = TRAIN-238 hs, 44 cells; SQP-CUTEst = NLP-46, 46 cells).

Family signs: C1 HS 5/2/37 (+1), CUTEst 7/6/33 (+1). P HS 0/0/44 (0), CUTEst 3/6/37 (**-1**). G1 HS 5/1/38 (+1), G2 0/0/44, G3 0/0/44, G4 0/1/43 (-1), G5 1/0/43 (+1), G6 0/0/44, G7 0/1/43 (-1), C2 0/0/44; every G-arm and C2: CUTEst not scored (0/46).

| candidate | K1 | K2 | K3 | K4 | K5 |
|---|---|---|---|---|---|
| **C1** (SOC off + kAnyAccepted) | NONE: 3/1/12, net +2, sign +1; family signs +1/+1; R1 clear; V1 clear; fails the floor (net 2 < 3) | NONE: 2/3/5, net -1, sign -1; R1 breach (ACOPR118/default); floor fails | NONE: 2/4/10, net -2, sign -1; R1 breach (ACOPR118/default); floor fails | NONE: 2/1/15, net +1, sign +1; floor fails (net 1 < 3) | NONE: 2/1/9, net +1, sign +1; floor fails (net 1, one positive name) |
| C2 (SOC off) | not covered | not covered | not covered | not covered | partial 10/12: 0/0/10; no CUTEst family read |
| G1 `trust_region_growth=kRatioByStepType` | not covered | not covered | not covered | not covered | partial 10/12: 2/1/7, net +1; no CUTEst family read |
| G2 `trust_region_growth_after_soc=false` | not covered | not covered | not covered | not covered | partial: 0/0/10 |
| G3 `trust_region_rejection_memory=3` | not covered | not covered | not covered | not covered | partial: 0/0/10 |
| G4 `trust_region_growth_ratio_max=3` | not covered | not covered | not covered | not covered | partial: 0/1/9, sign -1 |
| G5 `trust_region_shrink_to_step=true` | not covered | not covered | not covered | not covered | partial: 0/0/10 |
| G6 `trust_region_growth_on_step_limit=false` | not covered | not covered | not covered | not covered | partial: 0/0/10 |
| G7 `trust_region_growth_funnel_guard=true` | not covered | not covered | not covered | not covered | partial: 0/0/10 |

C1 detail (every non-tie cell is in `tu3_cells.csv`): K1 wins STEENBRB/default (29827 -> 25763 factorizations), STEENBRC/default (41279 -> 32327), STEENBRG/default (38206 -> 22361), loss SMBANK/sqp-qn (215 -> 278, both acceptable). K2/K3: wins ACOPP118/default (34235 -> 6392) and ACOPR57/sqp-qn (base max_iter, C1 acceptable); losses ACOPR118/default (14874 -> 18254; R1 breach: majors 22 -> 192, eval 47 -> 289, rejected_steps 6 -> 98, reject_funnel_ceiling 6 -> 88), ACOPR57/default (1750 -> 4625), SAWPATH/sqp-qn (47406 -> 82381), and for K3 NET3/default (30607 -> 36799). K4 wins STEENBRC and STEENBRG default, loss SAWPATH/sqp-qn. K5 wins hs011 (both labels), loss hs027/default (24 -> 31). C1 V1 on cluster cells: no lost cell in any cluster (strict and optimal+acceptable never below H0 per cluster per base label).

K1/K4 overlap (C1; the only candidate with rows): overlap cells (BIGBANK, STEENBRC, STEENBRE, STEENBRG) 2/0 sign +1; K1-only remainder 1/1 sign 0; K4-only remainder 0/1 sign -1. No two clusters ship, so no disagreement arises. Other candidates have no overlap rows.

**Near miss, reported plainly:** C1 on K1 passes every condition except the 3-net-win floor under the decisive reading (net +2). Under the I19-2a strict-optimal reading (reported alongside per item 1, not decisive) C1 K1 is 3/0, net +3, from three distinct problems (the SMBANK/sqp-qn "loss" is acceptable on both sides and ties). The declaration fixes the bucketed reading as decisive, so K1 is NONE. This is a candidate for a settler look, not a label: C1 failed I19-2a stage 1 (V1 20 lost cells, V3' FAIL, tail), no walk-census read exists for C1 (sub-rule of section 6), and the placebo lost cells in K1 (below).

## P control (section 4 item 5)

P (radius perturbed in the 15th digit) through the same read, per cluster: K1 2/3/11 net -1 sign -1; K2 1/2/7 net -1 sign -1; K3 1/2/13 net -1 sign -1; K4 0/2/16 net -2 sign -1; K5 0/0/12 net 0 sign 0. P earns a +1 sign on **no** cluster, so **no cluster is declared noise-limited** by the literal item-5 test (+1 sign and 3 net wins). But the reading is not clean: P itself moves the CUTEst family to sign **-1** (3 wins, 6 losses: default 2/5, sqp-qn 1/1) with V1 losses (K2 2, K3 2, K4 1 lost cells: ACOPR118/default to acceptable, SAWPATH/sqp-qn to numerical_error, BRITGAS is not a cluster member), and its cluster nets (-1, -1, -1, -2, 0) are the same size as C1's (+2, -1, -2, +1, +1). The placebo moves 198 live I19-2a rows (RESULTS-I19-2a); on these clusters a +/-2 net is within the instrument's perturbation. The control therefore does not void anything that the floor had not already failed, and it independently says C1's K1 +2/+3 is not distinguishable from a last-digit radius change.

## NOT SCORED cells (all of them)

* G1, G2, G3, G4, G5, G6, G7, C2 on **every** NLP-46 cluster cell: K1 16, K2 10, K3 16, K4 18, K5 2 (YORKNET/default, YORKNET/sqp-qn). Reason: no NLP-46 row exists for these arms (stage 1 ran TRAIN/CAP/SENS-P only). Listed in `tu3_cells.csv`.
* The whole SQP-CUTEst family (46 cells) for those eight arms; the family sign is UNEVALUABLE, so no label is possible for them.
* C1 and P: none.
* K (composite) and K-i: not run (no arm passed stage 1).

## Deviations and readings the declaration left open (all of them)

1. **Reader location and tests.** Section 7 describes `tu/tu3_read.py` and `tests/test_tu3.py` on a harness branch `tu3`. As instructed for this task the reader lives in this repository under `tu3/` and no `test_tu3.py` with synthetic rows was written; the cell rule is TU-1's, copied verbatim, with its own self-test (`python3 tu3/rule_tu1.py --selftest`), and the reader is checked only by the two byte-identical runs and by hand re-reading of C1's non-tie cells against the CSV.
2. **A10 / ten dnf_wall rows (the I19-2a loader defect).** `i19_run.load_arm_data` would stop on the NLP-46 dnf_wall rows (H0 10, P 11, C1 12) that have no counters side row. The reader uses the harness's own `arm_rows` and `rec_from_rows` but not `load_arm_data`, so it does not stop. This is not RESULTS-I19-2a's "treat as not-ended" informational reading applied to a result: a dnf_wall row never reaches under item 1, and R1 is evaluated only on cells both arms reach, so no side column is read for any such row; the verdict on those cells is decided by `outcome` alone (all ties or non-reach pairs). No decision depends on it. The affected cluster cells are in `tu3_cells.csv` (status `null`). Wall-dependent: those rows are wall-cap outcomes (300 s); the rule treats them as non-reaching as written.
3. **"Both arms reach" in the R1 guard** is read as item 1's reach (outcome optimal on both). The harness's own tail guard uses "ended"; the reader prints the both-ended count alongside, and the two agree on every cell (C1: 4 and 4).
4. **Item 4 "distinct problem names"** is read strictly: names whose own net (wins minus losses over their cells) is positive. The looser count (names with any win) is printed too; it changes no decision, because no cluster reaches net +3 under the decisive reading, so the names test is never the binding condition.
5. **Items 2 and 4 on partial coverage.** A candidate with no NLP-46 rows has no CUTEst family sign; the reader fails closed (no label) rather than reading the HS family alone. The partial K5 reads for G-arms and C2 are reported but decide nothing.
6. **Census sub-rule** (section 6 / TU-1 item 5, the 57-cell walk census): not read for any arm; moot since no label passes.
7. The store row files are counted by the reader's `arm_rows` walk (superseded rows honoured); no row was derived, interpolated or substituted.
