# M7 W2 E2-D2 — the f7 warm-cell regression and the removal certificate (fix1b)

Hven b25ab7ed (E2 fix round 1). The fix1 offline G4 moved two U0 walk cells from Optimal to
NumericalError, and the fix1 implementer traced both to F2 alone:

- `f7_n800_path_warm`: factorizations 2 → 1115, minors 6 → 1627.
- `f7_n1000_path_warm`: factorizations 2 → 9984, minors 12 → 10496.

This round traced `f7_n800_path_warm`, compared candidate rules and landed one of them, the
**removal certificate**, at bf37c305. Every statement below is marked observed or inferred. The
transcripts are in `transcripts/`; the instrumentation and runners are the `*.txt` files next
to this report. (The diagnostic implementer delivered this narrative as its hand-back; the
settler placed it here verbatim except for this preamble and the ruling at the end.)

## How it was run

- **Build:** a scratch Release copy of b25ab7ed (`.scratch/m7w2e2d2/tree`) with env-gated
  instrumentation (`HVEN_D2*`), building `hven_sqp_corpus`. The candidates are env toggles
  (`HVEN_D2_CAND`). A scratch Debug `hven_qp_walk_seam_tests` carries the fix1 random sweep
  (`ZzzExplore`, 24 000 trials × 2 algebras).
- **Faithfulness:** with the toggles unset, the build reproduces the replay row exactly
  (`NumericalError,1115,1627,…,278;279;279;279`). With F2 off it reproduces the M6 row exactly
  (`Optimal,2,6,…,1;1`). The sweep reproduces fix1's `final-*.txt` byte for byte.
- **Box protocol:** every cell, build and sweep leg ran under the lock with no timeout,
  `pgrep -x hven_bench_cell` empty (20/20 cell legs logged PGREP_EMPTY), `taskset -c 2` and
  `MKL_NUM_THREADS=1`. Two runs were stopped by exact pid: the unbounded candidate-D n1000 run,
  and the orphaned internal worker that a shell `timeout` left holding the lock.
- **Independent checks:** numpy/scipy in a scratchpad venv, never vendored. The working set
  was dumped at solve 1, iterations 213–216, and the rank of the row-normalized [Ae; Ai_W] on
  the free columns was taken from its singular values. A first-order KKT recomputation from the
  returned x and λ was added to the scratch binary.

## What the cell runs

The cell is a full-warm hop: a cold setup solve at p0 = 0.80 (n = 4000, me = 2400, mi = 800),
then the designated warm solve at p = 0.85. The cell counts only the designated solve's QPs; in
the base those are solves 4 and 5.

**Observed:** the divergence is in the setup solve's second QP (solve 1), not in the counted
hop. The setup ends at a different point, and the hop started from it fails the same way.

## Answers (observed unless marked)

**(1) The first divergence** is at solve 1, iteration 214: the eliminated-path re-check of a
deferred add.
- At iteration 213, P3768 (the upper bound of x3768) blocked at α 2.63e-4. Its test was
  UNDECIDABLE (ρ 6.26e-4), so it was deferred.
- At iteration 214 the detector converged: residual 9.30e-7, ‖A_Wᵀŷ‖ = 1.315e-6. The support
  held all 2400 equalities, the working rows (max scaled coefficient 0.450) and 211 pins. P3768
  was at **9.21e-5**, which is 2.0e-4 of the max and above kDepCoeffTol·max = 4.5e-7. R756 was
  at 7.81e-2.
- Newest-first, b25ab7ed removed **P3768** and marked it known-dependent. With F2 off, the same
  detection removed **R756**, the newest shift-driven row.
- Rank check: the set had **7** singular values ≈ 1e-15 at iteration 213 and **8** at
  iteration 214, and rank([rows; pins]) is 3304 at both. So P3768 *is* exactly dependent on the
  iteration-213 set.
- But those 7 dependencies were already there, inherited from the warm seed. The entry detector
  at iteration 0 stalled at 1.90e-6, just above kDepTol, and removed nothing: the F6 class. The
  next singular value is 1.4e-6.
- ŷ is a mix of those dependencies and proves P3768 dependent only to
  ρ_k = ‖A_Wᵀŷ‖/(|ŷ_k|‖a_k‖) ≈ 1.4e-2, which the candidate test would call independent
  (≥ 1e-3). R756: ρ ≈ 1.7e-5.
- **The verdict "dependent set" is correct; the member ŷ blames is not certified.**

**(2) The re-block.** At iteration 215, P3768 blocks again at α = 0 (UNDECIDABLE, ρ 6.26e-4),
with rate |aᵀp|/(‖a‖‖p‖) = 0.740/268.9 = **2.75e-3**. That is not a noise blocker.
- Along the same p, the working rows have rates up to 1.43e-4, equalities 2.33e-6, pins 0.
- The multipliers are |λ_e| 1.84e5 and |λ_i| 1.80e5 at μ = 1e-8, so μ‖λ‖ ≈ 1.8e-3: the
  regularized A_W p = μλ leak through the 7 unremoved seed dependencies (the 1/μ multipliers of
  E2-D1's D2).
- **Inferred:** a member that depends on a leaking set inherits the leak.

**(3) The end.** P3768 is known-dependent at zero motion with an undecided test, so F3(b)
refuses it (1 refusal); it carries no coefficients, so there is no exchange and no certificate
attempt.
- N2 excludes it. The next blocker is P3159 at α 1.41e-4, but P3768 would end that step
  violated by ≈ 1.0e-4, above feas_tol·scale.
- So `dep_blocker_terminations` = 1 and the solve ends kNumericalError at minor 216, with 88
  factorizations. Neither D1's certificate nor D3(a)'s retreat is reached, because the pass is
  eliminated.
- The setup produces three more N3 QPs. In the hop, solves 5–8 each remove the deferred pin
  P3249, re-block, refuse and terminate at about 406 minors, which gives the cell's 1115
  factorizations and 1627 minors.

**(4) The base (F2 off)** removed R756 and moved on. The next blocker was P3159 at α 1.49e-4.
- Solve 1 ran 8497 minors (7965 factorizations): 17 re-check removals, all rows; 22 exchanges;
  15 deferred adds; no refusal.
- It ended kOptimal with bounded final multipliers: |λ_e| 8.1e-3, |λ_i| 2.1e-3.
- It did tolerate the leaking dependent set (mid-walk multipliers 1.8e5, 8497 minors). But
  every member it removed carried a large coefficient in ŷ, so none came back at zero motion.
- The pin was rank-dependent, but it was the wrong member to remove on the strength of this ŷ.

**(5) The QP at that major** is feasible. The base's answer is a first-order KKT point:
- stationarity 2.0e-15, equality violation 3.5e-14, inequality violation 7.2e-14, box
  violation 0, λ_i ≥ 0;
- objective 1.2241173414643e-2.

b25ab7ed's N3 point has stationarity 1.2e-3, equality violation 3.2e-5 and inequality violation
4.2e-2. Second-order was not checked.

## Candidates

"Sweep" compares with b25ab7ed, whose gains over 6d123e75 are 755 N3 → verified kInfeasible,
14 N3 → kOptimal and 20 false kInfeasible → kOptimal, against 16 regressions to N3.

| Candidate | f7_n800 | f7_n1000 | Sweep | Verdict |
|---|---|---|---|---|
| (A) pins only if the rows-only support has nothing removable | Optimal 2/6 | Optimal 2/12 | loses the 20 gains; reverts the 16 regressions | a heuristic ordering that gives back a gain |
| (B) rows' threshold on the rows' own maximum | NumericalError 1115/1627 | NumericalError 9984/10496 | — | no effect: rows_max = scaled_max |
| (C) prefer a row over a pin on a re-check | NumericalError 1115/1627 | dnf_budget | +2 N3 → Optimal | the rows were shift-driven (key 2⁴⁰) and sort after pins (2³⁹) (inferred) |
| (D) skip a known-dependent re-blocker at α≈0 with rate ≤ tol; a nonzero rate means the mark is stale, so clear it and admit | unbounded (20-min timeout) | stopped at 9.5 min | — | the premise fails on a leaking set (rate 2.75e-3). The stale branch re-admits the pin and the next re-check removes it: a 2-minor zero-motion cycle (1809 stale clears in 90 s) |
| **(G) removal certificate** | **Optimal 2/6** | **Optimal 2/12** | = b25ab7ed except 1 solve (t4922 family 1, border path: verified kInfeasible → N3; its refactorize path was already N3); all gains kept | follows from ŷ |
| (E) G with a whole-support fallback | Optimal 2/6 | Optimal 2/12 | same statuses as G | the fallback is uncertified |
| (F) G with no fallback | — | — | 2 kInfeasible → N3 | worse |

With G, the setup solve's removal sequence and every solve's end are identical to the F2-off
walk.

**Why G and not D.** "A dependent member has rate 0" holds only for an independent working set
and the unregularized p. On these cells the entry detector left 7 exact dependencies in the set
(F6), and the μ-regularized step leaks μλ ≈ 1.8e-3 through them. A rank-certified dependent
member therefore blocks at rate 2.75e-3, and the rate cannot tell stale from dependent.

What *is* a theorem is ŷ's own certificate: A_Wᵀŷ = r gives member k's relative distance from
the span of the others as ρ_k = ‖r‖/(|ŷ_k|‖a_k‖). G removes k only if ρ_k ≤ kDepRemovalTol,
which equals kDepUndecidable, the candidate test's own dependent/independent boundary. So the
detector never removes a member the test would call independent. On exact dependencies
‖r‖ ≈ 1e-16 and the rule does nothing; it acts only in the swamped near-dependent regime f7 is
in.

## The fix (bf37c305)

- **`detect_dependency`:** a removable support member is listed only when ŷ certifies it.
  Mandatory members stay listed. With none certified, the best-certified member is listed alone,
  which keeps D2's attribution.
- **New names:** `detail::kDepRemovalTol = kDepUndecidable`;
  `EntryDetection::{uncertified_dropped, best_certified_fallback}`;
  `QpCounters::{dep_uncertified_trims, dep_certified_fallbacks}`.
- **Fixtures:**
  - `WsIndependenceDetector.APinInTheSupportByASmallCoefficientIsNotOfferedForRemoval`
  - `WsIndependenceDetector.WithNoRemovableMemberCertifiedTheBestCertifiedOneIsOfferedAlone`
  - `BothAlgebras/WsIndependence.AnUncertifiedPinIsKeptAndACertifiedRowRemoved/{0,1}`
- **Not covered by a fixture:** F3(b), which the f7 cells no longer reach. A faithful F7 hop
  reduction at N = 420 exists (F2 → NumericalError, fix → Optimal 2/10) but takes 50 s in
  Release. The replay cells remain the pinned rows, through G4.
- **Suites:** Debug 2817/2817 plus the long test 1/1; Release 2818/2818; no re-pins.
- **Offline G4:** 0 differences (walk 7 within the gate); both f7 cells at their M6 values.

## Observed vs inferred

- **Observed:** the divergence, the supports and coefficients, the removed members, the rates,
  the leak, the multipliers, the rank counts, the KKT numbers, every candidate's cell and sweep
  result, and D's cycle.
- **Inferred:**
  - that the leak causes the nonzero rate (consistent with μ‖λ‖ ≈ 1.8e-3; not isolated by a
    μ = 0 re-solve);
  - that C fails because the rows are shift-driven;
  - that the seed's 7 dependencies are the F6 swamped-entry class.

## Settler ruling (2026-09-24)

Rule G ACCEPTED; the settler's candidate (D) is refuted on this evidence. F6 (seed dependencies
left in by an entry detector stalled just above kDepTol; the base walk paid 8497 minors for
them) is re-registered as a conversion-relevant item. Gates for fix1 + fix1b (declaration
3e2bed07) ran GREEN: G1–G6, fold proofs 46/0, P-SYM 39 = 3 A + 36 B both configs.
