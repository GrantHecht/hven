# DECLARATION-TU-2-r2: successor amendments to DECLARATION-TU-2

Task: M8 TU-2. This file is a successor declaration in the sense of DECLARATION-TU-2's header. It is committed on hven
branch `tu2` next to the original, BEFORE any TU-2 row exists, and is never edited afterwards. DECLARATION-TU-2.md
(committed `c81a5894f`) stays byte-for-byte untouched. Where this file speaks, it governs; where it is silent, the original
governs. The machine copy `tu/tu2_spec.json` (hven-bench `tu2`) agrees with the original as amended by this file, and
`tu/spec_check.py` checks both texts. Settler rulings: ledger entry C487.

## 1. Elimination is DISABLED (amends section 7, "Order and elimination")

The sentence of section 7 that lets an arm skip its remaining legs when its family rule can no longer pass is WITHDRAWN.
Every arm runs every leg. The ordering sentence stands: within an arm, co-run legs run before exclusive legs.

Reason: the per-arm bound is not exact for composed mode-split configurations C(s, t_x, t_q), which use different arms on the
`default` and `sqp-qn` cells, and the saving is small. `rule.can_still_pass` stays in the harness as a tested function; no
runner calls it.

A4 and A5 stay conditional exactly as section 4.1 states ("iff A1 has no family or mode-row sign -1 against A0"; the families
are the two pooled families and the four mode-row families). That is a condition on whether the arms exist, not elimination.
An arm that does not run is reported as such and its configurations are not candidates (section 5.4 item 1).

## 2. HS071 runs under hs-27's terms (confirms section 6)

HS071 is in the HS family (TU-1 section 2) but its `set_version` is `h1-bootstrap`, which no set pin names. Section 6 applies
`budget_facts` to "every hven lane cell". The runner therefore maps `h1-bootstrap` to the `hs-27` pin: HS071 runs with
`common.max_factorizations = 10,000`. No other set_version is aliased.

## 3. A certificate at a feasible point is FALSE (amends sections 5.3 V2 and 9)

Section 9's classification table gains a first row, read before the others:

| reading | condition |
|---|---|
| NOT STATIONARY (feasible point) | v0 <= 1e-6 (the lane's `common.feasibility_tol`, which the lane never moves) |

A point whose l1 constraint violation v0 is within the lane's feasibility tolerance is a feasible point. A certificate of
infeasibility there is FALSE under V2, with no LP reading needed. The ratios are not read for such a point. The other three
rows of the table apply unchanged to a point with v0 above 1e-6. The original's treatment of v0 = 0 as "referred" is
superseded by this row.

## 4. The leg driver (adds to section 12)

`tu/tu2_run.py` (hven-bench `tu2`, Apache-2.0, with tests) runs the arms. It is harness code; TU-2 still adds no hven code.

* **Order.** Per arm: all co-run legs, then all exclusive legs. Arms run one at a time, A0 and B0 first, then the tier
  assignment is written (section 12 H4), then A1 to A3, then A4 and A5 if their condition holds, then B1.
* **Legs.** A leg is one run_serial invocation for one (arm, base label, set, tier). Each leg is one `box-lane` acquisition;
  only exclusive legs use `--exclusive`, and each holds the box for that leg alone.
* **Stops (section 11), enforced after every phase of every arm:**
  * an `error` row the solver did not report (status null and not in `solver_exceptions.csv`);
  * a cell of A0 or B0 that threw;
  * a reached row with more than B/2 factorizations (B the set's `budget_facts`; "reached" is R1 for SQP and
    `outcome = optimal` for IPM);
  * a false certificate under A0 (after the certificate checks);
  * a pre-flight mismatch or a spec/declaration disagreement (before the first leg).
  A stop ends the run with the stop named; it is resolved by a successor declaration.
* **Certificates.** After each SQP arm, every certificate row (outcome `infeasible`, `sqp_infeasibility_certified = 1`) is
  re-run as a single cell with `--dump-point` (one `box-lane` leg) and read by the section 9 LP. The re-run must reproduce the
  row's status and certified bit; a mismatch is reported.

## 5. Spelling

Arm B1's label spells `barrier_initial=0.01`, the value section 4.2 writes as `1e-2`.
