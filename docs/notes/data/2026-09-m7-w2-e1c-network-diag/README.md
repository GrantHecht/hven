# M7 W2 E1c — why the four network cells did not convert under the equality basis

Diagnostic probe at hven `43800aae` (the E1 head; the probe build and smoke are
committed at `ef171dcd`). One chain, 8 legs (DALLASM, STEENBRB, STEENBRC,
STEENBRG, each with `equality_basis` ON, the default, and OFF), 8 rows kept,
**0 censored, 0 hit the deadline, 0 observer-truncated** (`logs/chain.log`).
All four ON rows **reproduce the committed E1-retake harness rows exactly:
24/24 fields each, 0 differing, 0 accepted by the near-ulp tolerance**
(`logs/identity-on-vs-e1-retake.txt`). No solver was run for this read-out.

Labels: **OBSERVED** = read directly from a row or a trace. **DERIVED** =
arithmetic on observed columns, done in `wrappers/readout_e1c.py`.
**INFERRED** = a reading of the code or an argument, not a measurement.

## The answers

**Q3 (the one that decides what happens next). What happened to STEENBR's
feasibility, and did E1 block restoration? No. E1's restoration change (E1-R),
its certified-inconsistent exit, and the re-split after a failed QP are all
innocent. Restoration was never requested on the ON runs, so none of that code
ran. The driver's existing QP-failure rule, unchanged by E1, ended each solve
first.**

- OBSERVED, ON (B, C and G look the same). All four majors ran at x0 with
  radius 1, 0.5, 0.25, 0.125. Every QP hit its cap (`max_iter`), every verdict
  was `Reject`, `restoration_entries=0`, `c_elastic_activations=0`,
  `steps_accepted=0`. The fourth capped QP ended the solve.
- OBSERVED, OFF, at the same x0 (the major-0 f and feasibility match ON's
  bit for bit). STEENBRB's **first** QP (radius 1) stopped as **infeasible**
  after 827 minors (`first_qp_record_status=infeasible`). STEENBRC/G's first QP
  hit its cap as ON's did, but at radius 0.5 the next QP stopped as infeasible
  after 515 minors. Each time the elastic ladder came back unusable (B:
  `numerical_error`, 414 minors; C/G: `max_iter`, 5680), the verdict was
  `Restore`, and restoration ran (11 or 16 iterations) and returned optimal
  with feasibility 1e-13. After that, OFF's QPs hit their cap as well.
- INFERRED, from the code (`src/drivers/sqp_solver.cpp`; identical at HEAD and
  at `43800aae`). Restoration has exactly three ways in, and the ON trajectory
  reaches none of them:
  1. **An infeasible QP goes to the elastic ladder, then restoration.**
     `6199: if (mj.qs.status == QpStatus::kInfeasible) {` …
     `6216: if (!report.usable) {` … `6226: mj.row.verdict = StepVerdict::kRestore;` …
     `6248: enter_restoration(st, mj, seam, bridge, iter, elastic_cand);`.
     This is how every OFF run got into restoration. On ON, not one QP returned
     `kInfeasible`.
  2. **A routed QP failure that hits the radius floor.**
     `6350: if (st.qp_failures_in_a_row < opts_.qp_failure_budget &&`
     `6351:     qp_failure_is_retryable(st.qp, mj.qs, opts_.feas_tol)) {` …
     `6367: if (shrink_hits_floor(st.delta)) {` → `enter_restoration`. The floor
     is `6909: return std::isfinite(delta) && delta * kTrShrinkFactor < opts_.tr_min;`
     with `tr_min = 1e-10`. ON's radius only fell to 0.125.
  3. **A funnel `kRestore` verdict on a judged step** (6590). Only a QP that
     reaches `kOptimal` gets judged, and on ON none did.

  With `qp_failure_budget = 3` (sqp_solver_types.h:767), the fourth capped
  failure skips the gate and reaches `6413: return MajorOutcome::kFinishQpFailure;`.
  That returns the pre-trial iterate (6820: "No restoration was consulted on
  this path -- x is still the pre-trial iterate"). E1-R lives *inside*
  `enter_restoration` (`4653: if (st.eqb.basis.reduces()) {` →
  `resto_rows.emplace(...)`), so it has no entry condition that could refuse
  restoration. `recompute_after_qp_failure` (6398) was a no-op because x never
  moved, so Je's hash never changed (`eqb_recomputes=0`). E1's diff
  (`53676599..43800aae`) touched none of lines 6199–6413 except to add that
  call.
- **The returned point is x0, the starting point.** No phase produced it.
  OBSERVED: ON's f and feasibility equal OFF's major-0 row; nothing was
  accepted and nothing was restored. Its infeasibility sits on **both** the
  retained and the dropped rows, and it is the starting point's own residual:
  STEENBRB dropped-row max 2000 (row 12), retained max 2000 (row 1); STEENBRC/G
  dropped max 2000 (row 12), retained max 10000 (row 108)
  (`eqb_ce_dropped_rows`, `eqb_ce_*_max_abs`).
- **The projection is not spreading an inconsistency.** OBSERVED: the
  dropped-row gap `eqb_max_gap` is 6.8e-13 (B) and 3.1e-12 (C/G).
- **The certified-inconsistent (two-iterate) exit played no part.** OBSERVED:
  `eqb_inconsistent_row=-1`, `eqb_evidence_major_*=-1`.
- **So what E1 actually changed, INFERRED:** the QP that OFF reported as
  infeasible, ON walks until it hits the cap. The dropped rows are consistent
  combinations of the retained ones (gap ≤ 3.1e-12), so the reduced QP has the
  same feasible set as the full one. That makes ON's QP at radius 1 (B) and at
  radius ≤ 0.5 (C/G) infeasible, which OFF proved at those radii. The walk
  cycled instead of proving it (Q1). The OFF route into restoration was itself
  fragile: STEENBRC/G's OFF QP at radius 1 also hit its cap and cycled, and only
  the radius-0.5 QP proved infeasibility. Restoration didn't convert OFF either:
  every post-restoration QP hits its cap.

**Q1. Does the first failing QP still cycle? Yes, on all four cells, with
zero motion and every step at α = 0.** OBSERVED (M4b's RECURRENT predicate
imported unmodified; `tables/q1-q2-per-solve.csv`):

| leg, first QP | rows dropped (driver report) | minors (= cap) | period | tail share | Σ‖Δx‖∞ over tail | tail pattern |
|---|---|---|---|---|---|---|
| on-DALLASM s0 | 1 (row 89) | 1960 | 2 | 0.911 | 0 | the same bound (`bound:25:lower`) dropped and re-added at α = −0; 892/892 units are core re-adds at α = 0 |
| on-STEENBRB s0 | 12 (rows 3,12,…,102) | 4680 | 4 | 0.867 | 0 | two bounds swap: add A (α = 0), drop B, drop A, ride-step B (α = 0) |
| on-STEENBRC s0 | 14 (rows 3,12,…,120) | 5400 | 4 | 0.848 | 0 | the same two-bound swap |
| on-STEENBRG s0 | 14 | 5400 | 4 | 0.849 | 0 | the same two-bound swap |

The drop counts 1/12/14/14 match M5r's prediction. Every later ON main QP is
also RECURRENT, with period 2 or 4, tail share 0.85–0.95, and Σ‖Δx‖∞ = 0 over
the tail. For STEENBR it is still a bound drop and re-add at α = 0, but over a
**pair** of bounds: M4c's "core" count (one identity immediately re-admitted)
is 0 there. OFF's first QP on STEENBRC/G has the same shape (period 4, α = 0).
The basis changed the rows, not the shape of the cycle.

**Q2. After the redundant rows are gone, does the working set still exceed n?
Mostly yes: the dependence is in the working set (population B). But not every
cycling tail has an integer proof.** DERIVED as `me' + ws_size − n` per pass,
with me' = the trace header's `me`. The script asserts that this equals
`eqb_me_reduced` on every leg (96/112/112/150), which confirms the QPs carried
the reduced rows. Tail range for each ON main QP:

| cell | ON first QP (x0, r=1) | ON retries / later majors | OFF first QP at the same x0, r=1 |
|---|---|---|---|
| DALLASM | +31..+32 | +24..+25, then +46..+55 after restoration | stopped as infeasible (no tail; solve max +50) |
| STEENBRB | +1..+3 | **−6..−4** (r=0.5), +7..+9, **−5..−3** | stopped as infeasible (no tail; solve max +52) |
| STEENBRC | +1..+3 | +2..+4, +3..+5, −1..+1 | +28..+30 |
| STEENBRG | +12..+14 | +8..+10, +9..+11, +8..+10 | +22..+24 |

Where both runs have a tail at the same point and radius (STEENBRC/G, radius
1), removing 14 rows lowers the over-count by 27 and 10. It still stays above
n, just barely on C. DALLASM lost only one row and remains 31 over. That
dependence is not an equality redundancy, so it is E2's question. On three ON
tails (STEENBRB s1 and s3, STEENBRC s3) the walk cycles at α = 0 **without**
me' + |W| > n. M4c's class there is `c` (inertia shows extra negative
directions) or `d` (the drop bought no direction), not `b`. So the cycle can
outlive the integer over-count. The trace carries no rank column, so rank is
not reported.

**Q4. DALLASM: where did the two extra majors go? They are two more capped,
routed failures before restoration, and its first QP still hits the cap.**
OBSERVED (`tables/ladder-per-major.csv`):
- OFF: major 0's QP (r=1) stopped as infeasible after 232 minors, then elastic
  (2150, capped), then restoration (2 iterations, optimal). Majors 1–4 then hit
  their cap at r = 0.1 … 0.0125, and the budget ends the solve.
- ON: majors 0 and 1 hit their cap (1960 each, the period-2 α = 0 cycle) at
  r = 1 and 0.5 and were routed. At major 2 (r=0.25) the QP stopped as
  infeasible after 295 minors, then elastic (2150, capped), then restoration
  (3 iterations, optimal). It lands on a different feasible point from
  OFF's: f 61972190 vs 61974881. Majors 3–6 then match OFF's
  majors 1–4.

That is 7 = 5 + 2: `rejected_steps` 5 vs 3, `qp_maxiter_exits` 6 vs 4, one
restoration each. The returned point is feasible (5.7e-14) on both. DALLASM's
over-count of +31 after dropping one row puts it squarely in the working-set
class.

**Q5. One sentence per cell.**
- **DALLASM:** E1 dropped the one redundant row (consistent, gap 8e-14) and
  the harness row moved as a result: the first QP now cycles to the cap
  instead of stopping as infeasible, so restoration arrives two majors later.
  It did not touch the cause, a working set 31 rows over n that cycles at α = 0
  before and after restoration, so the cell is still `numerical_error`.
- **STEENBRB:** E1 dropped 12 consistent rows. The reduced QP at x0 no longer
  proves its own infeasibility the way OFF's did, so the existing 3-failure
  budget ends the solve at the starting point (feasibility 2000). It did not
  touch the α = 0 bound-swap cycle, which persists even where me' + |W| ≤ n.
- **STEENBRC:** E1 dropped 14 consistent rows, bringing the first QP's
  over-count down from +28..+30 to +1..+3. The QPs at radius 0.5, 0.25 and 0.125
  still never prove the infeasibility OFF proved at 0.5, so the solve ends at x0
  (feasibility 1e4) without restoration. The period-4 α = 0 cycle is the same
  as OFF's.
- **STEENBRG:** the same as STEENBRC: 14 rows dropped, first-QP over-count
  +22..+24 down to +12..+14, never proves infeasibility, ends at x0
  (feasibility 1e4); the cycle is unchanged.

## The settler's first look, checked
- "every QP hits its cap ON and OFF (qp_maxiter_exits == majors)": **true on
  ON STEENBR (4/4), false on OFF** (4 of 5 majors on B, 5 of 6 on C/G). The
  exception is exactly the major whose QP stopped as infeasible and led into
  restoration, which is the Q3 answer. On DALLASM it is 6/7 ON and 4/5 OFF, for
  the same reason.
- STEENBR ON 4 majors, 3 rejections, 0 restorations, feasibility equal to the
  start point's: confirmed. OFF 5–6 majors, 1 restoration, feasibility
  ~1e-13: confirmed. DALLASM 7/5/6/1 vs 5/3/4/1: confirmed.
- Early sighting in the STATE note: the probe's `qp_minor_iters` (DALLASM
  14604) differs from the brief's "4715 minors". The brief's figure is the
  `factorizations` column (on-DALLASM `factorizations=4715`; OFF 4919). The
  identity gate compares counter to counter and passes.

## Caveats
- `tables/tails-<leg>.{txt,json}` come from the copied `m4c_tails.py`. It takes
  one (n, me) from the trace header, which is the **main** QP's. Its class `b`
  on elastic-rung and restoration solves (OFF s1/s7 and the like, ON-DALLASM s3)
  therefore uses the wrong dimensions and is **not a reading**. The README cites
  its classes only for main QPs.
- A solve's role (main / elastic rung / restoration) is DERIVED by walking the
  history block against the solve summary, with assertions. That the main QP
  just before an elastic ladder stopped `kInfeasible` is INFERRED from line
  6199 (the only consumer of `kInfeasible`). The ledger confirms it directly
  only for a leg's first QP (OFF-B and OFF-DALLASM:
  `first_qp_record_status=infeasible`).
- The M5-lineage trace never wrote `dep_rows_after_drop` / `add_dep_screen`.
  Q2's over-count is the same `qp.me() + qp_walk_ws_size(ws)` quantity,
  recomposed and labelled DERIVED.

## Layout
- `rows/` holds the 8 probe rows (+ `SHA256-rows.txt`; they match the chain's
  own `logs/chain-sha256.txt`).
- `traces/` holds 8 committed slices (`SLICES.txt` says why each one;
  `SHA256-slices.txt`). The 8 parent traces (116 MB) are pinned, not
  committed: `SHA256-parent-traces-e1c.txt`.
- `tables/`: `q1-q2-per-solve.csv` (one row per traced solve: role, RECURRENT
  predicate, tail Σ‖Δx‖∞, DERIVED over-count, M4c class),
  `ladder-per-major.csv` (the history block with solve roles), and
  `tails-<leg>.{txt,json}` (m4c_tails output).
- `logs/`: chain transcript, per-leg stderr (all empty), chain sha list, the
  ON-row identity gate, plus the build and prepare logs from `ef171dcd`.
- `wrappers/`: `readout_e1c.py` (this read-out; it imports M4b's
  `readouts_driver_m4b` unmodified); `slice_m5.py` and `m4c_tails.py` are
  byte-identical copies of M5's.
- `MANIFEST.sha256` covers every file here.

Reproduce (no solver):
```
python3 wrappers/m4c_tails.py OUT/<leg> <trace>/<leg>.csv      # then copy OUT/<leg>/tails.json -> tables/tails-<leg>.json
python3 wrappers/readout_e1c.py rows <parent-trace-dir> tables tables
python3 wrappers/identity_e1c.py --rows <dir of CELL.txt -> on-CELL.txt> --cells DALLASM,STEENBRB,STEENBRC,STEENBRG
```
