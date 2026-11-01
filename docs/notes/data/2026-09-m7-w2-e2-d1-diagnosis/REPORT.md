# M7 W2 E2-D1 — diagnosis of the READING6 break and the ten non-conversions

Hven 4a9b237a (QP files identical at the E2 re-take pin 4858019e). Artifacts in this directory:
transcripts, excerpts, `variants_summary.txt`, `oracle_summary.txt`, the full scratch patch
(`d1-instrumentation-and-experiments.patch.txt`) and the runners. The diagnostic implementer's
narrative was delivered as its hand-back (the harness refused a subagent-written `.md`); the
settler placed it here verbatim except for this preamble. Observed vs inferred is labelled.

## Verdict

The settler's suspect is confirmed. E2's dependency verdicts are correct, but its termination
rules return `kNumericalError` on QPs that are infeasible. The driver cannot use that status: it
shrinks the radius three times and exits. Three defects are in E2's own code, one is in the
driver, and one pre-E2 defect is now exposed.

## How it was run

- Build: a scratch Release of `git archive 4858019e` with env-gated instrumentation. The harness
  cell binary was built from a copy of hven-bench `5a0b05bd` pointed at the scratch install; the
  real harness tree and its build were untouched.
- Faithfulness check: with no experiment armed, the instrumented runs reproduce the harness E2
  columns exactly: READING6 416/212/4/4/4, ACOPP57 68/2/2, STEENBRG dep_tests 23818, ACOPR57
  929/197.
- Independent check: a phase-1 LP oracle (scipy/HiGHS in a scratchpad venv, never vendored)
  tested feasibility of the dumped QPs.
- Box protocol followed on every leg; a foreign lane's cells (`e1b-fix1-partial`) ran between
  legs and this lane queued on the lock behind them.
- Clean-up: the main tree is clean; instrumented source and builds deleted; raw full traces
  remain in the git-ignored `.scratch/m7w2e2d1/runs/`.

## Defects

- **D1 – certificate gate (class c/e), `src/qp/qp_engine.cpp:1440`.** N1 only tries the Farkas
  certificate when no sign-constrained member has a positive coefficient (`if (xpick.cond1 && …)`).
  A locked positive-coefficient member (entered at this point) leaves no admissible member and
  cond1 false, so the solve ends in N3 `kNumericalError`. `verify_farkas`'s box-based certificate
  does not need cond1. The gap is in design §2.4 itself, not an implementation slip.
- **D2 – detector support omits pins (a wrong attribution, not a wrong ρ),
  `src/qp/ws_independence.cpp:363-373`.** On the eliminated path the entry/recheck detector lists
  only constraint rows. A dependency created by an optional pin is therefore attributed to
  mandatory equalities, nothing is removed, and the walk continues on a dependent set with
  multipliers of size 1/μ.
- **D3 – undecidable ping-pong (a new E2 cycle), `qp_engine.cpp:1416-1426` × `:737`,
  `ws_independence.cpp:293`.** On the border path an exactly (structurally) dependent pin tests
  UNDECIDABLE (ρ 3.9e-5 to 2.8e-4, refinement stalled — the E2-F1 swamped-C class). S17 admits
  it; the next pass's structural detector removes that same pin; it re-blocks at α=0. This
  repeats until the 2(n+mi) zero-motion cap.
- **D4 – driver never goes elastic (class d), `src/drivers/sqp_solver.cpp:6343` and
  `:6494-6557`.** Only `kInfeasible` opens the elastic tier. `kNumericalError` gets the shrink
  ladder (budget 3), which keeps an infeasible linearization infeasible, then `kFinishQpFailure`.
  Restoration is reached only when the radius floor is hit.
- **D5 – false saddle at a full face (not E2's), `qp_engine.cpp:2869` and `:1003-1035`.** On
  ACOPP57/118, at me+|W| = n with every member ρ-independent, the μ-regularized inertia still
  reads kWrong. Before E2, the start repair masked this by admitting a necessarily dependent
  12th pin at count n+1, and the walk then cycled to the cap. That this is a regularization
  artifact is INFERRED: σ_min(A)²/μ < |λ_min(H)|, with the last pin's ρ = 1.65e-3 and μ = 1e-8.

## Q1 – READING6 (observed)

- Symptom: all four QPs (n=102, me=50; Δ = 1, 0.5, 0.25, 0.125) end N3 `kNumericalError` in
  3 minors. Three are routed by the budget; the solve exits at major 4 with no elastic and no
  restoration.
- Pass 0: inertia kWrong, the start repair declines, and the pass holds no usable factor.
  Blocker pin 5 is UNDECIDABLE and admitted as a deferred add.
- Pass 1: the recheck detector finds a dependency (null residual 1.5e-15) but its support is
  equality rows 0–4 only (D2). Nothing is removed. The multipliers are ~1/μ (z5 = 4.75e8).
  Blocker pin 6 is genuinely dependent (ρ 2.7e-16). The exchange picks pin 5 with σ = 6.1e8,
  computed from those meaningless multipliers.
- Pass 2: pin 5 re-blocks at α=−0 (ρ 1.6e-16). Its only positive-coefficient member, pin 6, is
  locked, so no member is admissible and cond1=0; N1 is never tried (D1). N2's violation is
  2.6e-2 against a 1e-9 limit, so N3 fires.
- The QP is infeasible: row 0 with x0 fixed at 0 forces x1 = −2.113, outside Δ=1. The oracle's
  smallest feasible radius is 6.36, so all four QPs are infeasible. OFF on the same QP returns
  `kInfeasible` in 46 minors, which goes to elastic; the pre-E2 cell was optimal in 13 majors.
- Is it a member OFF adds and drops harmlessly? Partly. ON refuses to re-add pin 5 where OFF
  admits it and reaches its infeasibility classifier. But restoring admission at N3 (`n3admit`)
  brings back `kMaxIter` cycling in later QPs, so admission is not the fix.
- Class: D1 (c/e), compounded by D2; the cell's exit comes from D4. The bad exchange choice
  (class b) is a symptom of D2, not a separate defect.

| Variant (scratch, READING6) | Result |
|---|---|
| D1 fix alone (certificate tried whenever no member is admissible) | optimal, 15 majors, objective −144.6597 (same as OFF) |
| D1 + D2 fixes | optimal, 11 majors |
| D4 fix alone (E2 terminations routed to elastic) | optimal, 8 majors |

## Q2 – ACOPP57 (observed)

- QPs 0–2 end `kOptimal`, ON = OFF bit for bit, and QP 0 passes a recomputed KKT check at 1e-9.
- From major 3 every QP ends `kNumericalError` in 2 minors with no E2 counter set: the ordinary
  terminal saddle exit (kWrong).
- Both engines' start repair pins the same 11 variables and the probe stays kWrong at count
  128 = n. ON's integer screen then correctly refuses 114 further pins; OFF admits a dependent
  12th pin at count 129, the probe reads kOk, and it cycles to `kMaxIter` (1795 minors,
  stationarity 9.9e-2 against scale 4.3e3).
- Neither engine solves the QP, and it is feasible (oracle radius 4.3e-5). So the failure is D5,
  not E2 and not the outer step. The end point is byte-identical ON vs OFF.
- ACOPP118 is the same class: 236 pins skipped, terminal kWrong, QPs feasible.

## Q3 – STEENBRG (observed)

- The certificate is correct. At major 2 (Δ=0.25) the blocker is x212 at its lower side. The
  certificate uses y_e only, with residual 0, no dropped entries and no unbounded coordinates —
  exact, not S16's toleranced kind. Re-derived from the problem data independently of the
  engine: implied bound 1998.7000005 against a limit of 0.1; the box part matches the stored
  one exactly.
- The QP really is infeasible: oracle smallest radius 3333. OFF hits `kMaxIter` at its cap
  (5400) and at 5× the cap (27000), so it can never state this.
- QPs 0–1 end at the zero-motion cap (D3). After restoration, the QPs at Δ 0.1 and 0.05 hit
  `kMaxIter`, and those at 0.025 and 0.0125 end in N3. The cell exits at major 7.
- STEENBRB/C: UNDECIDABLE is the root, together with the recheck loop. 469 (B) and 541 (C)
  identical two-pass cycles: the blocker (pin 323 on B, pin 86 on C/G) tests UNDECIDABLE on the
  border path and is admitted; the structural detector finds an equality row (36 on B, 96 on
  C/G) with every nonzero at a pinned column, and removes that pin; the pin re-blocks at α=0;
  the loop runs to the cap. The QPs are infeasible (oracle radius 499.9 for B, 3333 for C). An
  experiment that retreats an undecidable border test to the eliminated path gives STEENBRB a
  verified `kInfeasible`.

## Q4 – ACOPR57 (observed)

- 203 violations are at entry in primary QPs 1–4. The seeding `refresh_shifts` puts in 43–55
  rows before the first pass, so each QP starts 43–49 over n.
- On the border path the entry detector misses: its null residual stalls at 1.5–2.4e-6, just
  above kDepTol 1e-6. That is where the 197 untested growths come from (44+65+43+45), each
  trimming one member per pass by count.
- 726 are mid-walk in the elastic QP: shift add → recheck removal → re-add, every 6 passes,
  until `kMaxIter` 4360.
- This does not explain the failure. The primary QPs end in N3, then a verified certificate at
  Δ=0.125. The terminal failures are four restoration QPs (n=630) at the zero-motion cap with
  905 undecidable/recheck cycles each, which is D3.

## Q5 – one line per cell

| Cell | Remaining failure after E2 removes the cycle | Basis |
|---|---|---|
| ACOPP57 | D5 false saddle; OFF also fails (`kMaxIter`) | observed |
| ACOPP118 | D5 | observed |
| ACOPR57 | N3 in primary QPs, then D3 in restoration; the untested growth is a cost, not the cause | observed |
| ACOPR118 | D5 plus D3 (3 retreats, 175 untested growths) | inferred, not traced (174–300 s cell) |
| BRITGAS | now dnf_wall, no counters row | not diagnosed |
| YORKNET | N3 on an infeasible QP (radius 135): D1 + D4 | observed |
| DALLASM | N3 on infeasible QPs (radius 1.448): a degenerate full vertex off its face, locks leave no admissible member, and no local certificate verifies | observed |
| STEENBRB, STEENBRC | D3 on infeasible QPs | observed |
| STEENBRG | D3 on QPs 0–1; the certificate is right; post-restoration QPs at small radii cap or end N3 | observed |
| READING6 | E2 broke it through D1 + D2 then D4; fixed by the D1 fix alone and by the D4 fix alone | observed |

Overall: E2 does produce wrong QP verdicts, but not wrong dependency verdicts. The ρ test and
the certificate are sound; the wrong part is the `kNumericalError` exit on six infeasible QPs,
each confirmed by the oracle. E2 also exposes the driver defect D4.

Behind D4 there is a new downstream finding, not E2's: with elastic routing on, restoration
declares DALLASM, STEENBRB and YORKNET `infeasible` within 1–2 majors, and these are feasible
CUTEst problems (registered by the settler as **E2-F2**). ZAMB2-8 and SMBANK stay optimal under
the full experimental stack.

## Fixes and cost (implementer rounds)

- **F1, for D1 (≈0.5, with F2):** when no member is admissible, always try `verify_farkas`, not
  only under cond1; amend design §2.4 N1. Fixture: READING6-shaped, asserting a verified
  `kInfeasible`.
- **F2, for D2 (≈0.5):** on the eliminated path, add the pinned columns' coefficients (A_Wᵀŷ at
  the pins) to the detector support with the same threshold. Fixture: equality chain + fixed pin
  + deferred pin, asserting the recheck removes the pin.
- **F3, for D3 (≈1–1.5):** an UNDECIDABLE blocker on the border path takes the existing border
  retreat before S17 admits it; a recheck removal of a deferred add marks that member "known
  dependent at this point", never re-admitted at zero motion; a fully pinned row found by the
  structural detector feeds `select_exchange` and `verify_farkas` directly as an exact dependency.
- **F4, for D4 (≈1, plus a restoration diagnosis round):** route E2 terminations
  (`dep_blocker_terminations`, `zero_motion_cap_hits` or `dep_certificate_rejections` > 0) to the
  elastic ladder instead of the shrink ladder; no enum break. Ship only after the restoration
  defect (E2-F2) is diagnosed.
- **F5, for D5 (≈1–2, design-level):** at a full-count face, re-read the inertia verdict without
  μ (refined) before the saddle exit, in both the terminal check and the start repair.
- **F6 (optional, ≈0.5):** send the seeding shift adds through the predicate once a factor
  exists, or retreat instead of trimming by count when the detector stalls just above tolerance.
  A cost fix, not a conversion.

Implementer's recommended order: flip E2 OFF now; land F1+F2+F3 as one engine round (~2 rounds
with fixtures and gates); then F4 after the restoration diagnosis; F5 separately. After F1–F3,
READING6 should stay optimal (observed with F1 alone). The STEENBR and DALLASM cells should end
with verified `kInfeasible` QPs but not convert until F4 and E2-F2 are fixed (inferred).

## Settler ruling (2026-09-23 22:10)

No separate OFF flip: nothing consumes m7 before the M7 close PR, and a flip costs a gate chain
each way. The E2 fix round lands F1+F2+F3 with the default ON; its gates and re-take decide:
READING6 (and every must-stay-optimal cell) must stay optimal, else the round's last commit
flips the default OFF. F4 waits on the E2-F2 restoration diagnosis; F5 is its own task.
