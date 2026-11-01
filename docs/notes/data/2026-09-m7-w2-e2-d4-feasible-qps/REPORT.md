# M7 W2 E2-D4: why the QP walk fails on feasible QPs

Hven 763cda00 (src/include identical to the re-take-2 pin 7ab88b81 up to comments). The diagnostic
implementer delivered this narrative as its hand-back (the harness refused a subagent-written `.md`);
the settler placed it here verbatim below the first heading, with the ruling appended at the end.
OBSERVED = seen in a run recorded in this directory; INFERRED = not proved by a run.

# M7 W2 E2-D4: why the QP walk fails on feasible QPs

## Verdict

**The corpus (OBSERVED).**
- 31 QPs were dumped from 7 cells.
- 26 are feasible: HiGHS phase-1 minimum ℓ1 violation is 0. The 5 exceptions are DALLASM's primary QPs, with minimum violation 0.448 to 9.07.
- Where H is PSD on null(Ae), Clarabel gives an independent optimum.

**The walk fails these QPs through five mechanisms, not one.** Ranked by the TRAIN cells each gates:

1. **F1: the dependency test admits exact dependencies as "independent", and the working set then leaks.**
   - The admitted pins are exactly dependent: dense distance from span(Ae; W) is about 1e-15.
   - The engine's ρ reads 3e-6 to 1e-4 on DALLASM (the "converged grey zone"). On STEENBRB it reads 5e-4 up to 1e18 (a stalled or diverged refinement).
   - 11 of the 18 exact admissions found complete a working or equality row. The existing `structural_dependency` would have decided them, but it is only consulted on an undecidable verdict.
   - What the leak looks like: the EQP step is nonzero at count me+|W| = n, multipliers reach 1/μ size (up to 5.7e8), and the walk either runs zero-motion drop/re-add cycles to the cap or ends N3.
   - Gates DALLASM and STEENBRB (OBSERVED), STEENBRC and STEENBRG (INFERRED), and YORKNET's restoration QP.
2. **A: the locked degenerate vertex.**
   - At a zero-motion point, every member with a positive dependency coefficient entered at that point. The S15 lock makes all of them non-removable.
   - With no admissible member and no verifiable certificate (the QP is feasible), the solve ends N3.
   - Gates YORKNET (elastic QP) and ACOPR57 (primary QPs).
3. **B: D5 is a singular PSD face, not a false saddle.**
   - On ACOPP57 the walk's final face equals the optimal face: the oracle's active set is identical, and |x − x*| = 9e-5.
   - Z'HZ on that face has λmin +3.0e-12 and next eigenvalue 50.4: positive semidefinite and singular, not positive definite.
   - The μ-regularized inertia reads kWrong. The μ-free inertia reads **kSuspect**, not kOk. So the "re-read without μ" alone does not suffice; it must also accept kSuspect.
4. **G: SAWPATH's mandatory rows are badly scaled and numerically rank-deficient.**
   - Row norms run from 1.7 to 6e10, and the normalized σ_min is 1.7e-11.
   - E1b refused to drop any rows (`eqb_undecidable`, kSpectrum).
   - The count screen fires on a blocker whose own ρ is 4.9e-5. Its coefficients sit only on equality rows (cond1, vmax 2.3e8), so the certificate is rejected and the solve ends kNumericalError.
5. **C′/F2: noise-multiplier cycles in ACOPR57's restoration QPs.**
   - A period 5–10 drop/re-add at |m| ≈ 1e-7 against the absolute opt_tol of 1e-9, with the restoration objective near 3e-9.
   - At 763cda00 these QPs cap at kMaxIter. They are no longer D1's undecidable ping-pong.

**The E2-OFF walk solves 9 of these feasible QPs to the oracle optimum, and E2 turns each into a failure (OBSERVED).**
- The 9 are: YORKNET elastic qp0 and qp1, SAWPATH elastic qp0 and qp2, DALLASM's elastic QP, STEENBRB's elastic QP, ACOPR57 primary qp0–qp2, and ACOPR57's elastic rung.
- With E2 on they end N3, certificate rejection, zero-motion exhaustion, or the cap.
- So class (E) as stated ("E2 turned an infinite loop into an early exit at the same point") is **false in general**. It holds only where both walks fail: DALLASM's feasible major-1 QP (OFF caps at f*, uncertified), STEENBRB's post-restoration QPs (both sit near −9.906e11, within 0.04 % of f*), ACOPR57's restoration QPs, and ACOPP57.
- This also contradicts E2-D3's line "with E2 off on the rungs they end at kMaxIter". That was measured on the projected-start variants, not on these QPs.

**Cells.**
- **ACOPR57 converts on the cell: numerical_error → optimal (OBSERVED).** 17 majors with `unlock` alone; 15 majors with the combined candidate.
- READING6 stays optimal under the combined candidate (8 majors, against 11). ZAMB2-8 and SMBANK are identical.
- No other cell converts.
- **The combined candidate fails U0**: f7_n1000_path_warm and f7_n800_path_warm go Optimal → dnf_setup (138 differences). The regressor is `greyd`: `structfirst`, `unlock`, `f5+f5s` and `screenadmit` each give 0 U0 differences, and `structfirst+greys+greyd` alone already shows f7_n1000_path_warm dnf_setup with the other 25 cells identical.

## How it was run

- **Scratch build.** `.scratch/m7w2e2d4/tree` is `git archive 763cda00` plus the pinned submodules, with:
  - E2-D3's `instrument.py`, verbatim;
  - `tools/instrument_d4.py`: `HVEN_D4_QPDUMP` dumps each kNumericalError or kMaxIter solve; `HVEN_D4_TRACE` gives a per-pass trace of blocker, ρ, rate, exchange pick with every positive-coefficient member and its lock flags, Farkas, pass-over, drops and terminal classification;
  - the candidate rules as `HVEN_D4_FIX` toggles.
  - The full diff against 763cda00 is `d4-patch.diff.txt`; only `sqp_solver.cpp` and `qp_engine.cpp` are touched.
- **Harness.** A copy of hven-bench `ea1aa4e`, pin prefix pointed at the scratch install, plus E2-D3's x-dump line. The real harness was untouched.
- **Replay tool.** `runners/qp_replay.cpp` re-solves a dump on a fresh `QpEngine`.
- **Faithfulness (OBSERVED).**
  - The cells reproduce the re-take-2 and E2-D3 columns exactly: YORKNET 1/1309, DALLASM 4/8, DALLASM under F4 2/701, SAWPATH 2/22344, STEENBRB 4/15038, ACOPP57 7/96, ACOPR57 5/29268 (majors/factorizations).
  - Replays match the in-cell status and minors on 31 of 31 dumps, and the factorizations on 30 of 31. ACOPR57 qp3 is +1 because the cell reused a hot K0.
- **Oracle.** `runners/qp_oracle.py` runs scipy, HiGHS and Clarabel in a scratchpad venv, never vendored. It reports phase-1 feasibility, H and Z'HZ spectra, the Clarabel optimum and active-set comparison, and a check of any hven point.
- **Exact dependence.** `runners/addseq.py` computes the dense distance and the structural test for every admission in a trace. On mi > 0 dumps the distances are upper bounds, because shift adds are not traced.
- **U0 and HS checks.**
  - U0: `hven_sqp_corpus --engine walk` on the 27 U0 cells, compared with `compare_replay.py --residual-gate 1e-5` against the M6-close head. The untoggled scratch binary gives 0 differences, with 7 within the gate.
  - HS: the 21 **TRAIN** problems of hven-bench `hs-27`, never the HOLDOUT ones. This is my stand-in for the "HS corpus quick check".
- **Box protocol.**
  - Every cell, replay, oracle, build, U0 and HS leg ran in `flock /tmp/box-build.lock` with no timeout, logged PGREP_EMPTY and FOREGROUND markers, and used `MKL_NUM_THREADS=1` and `taskset -c 2`, one solver at a time. The markers are in `transcripts/legs.log`, `u0hs5.log` and `cells5.log`.
  - My legs queued behind the E2 fix-round-2 implementer's builds and P-SYM gate. I never signalled a process, and no run went unbounded.
  - **Deviation:** 4 short offline numpy analyses ran outside the lock: the first DALLASM addseq, the SAWPATH rank check, and the ρ calibration and addseq over 3 traces. They used OpenBLAS, not MKL; no solver or cell binary ran in them. One overlapped a locked replay leg, which produces deterministic counters, not timings.
  - Legs expected to exceed 600 s ran detached and were waited on.
- The main tree and `.claude/worktrees/*` were never touched. Accelerate: UNOBSERVED.

## Deliverable 1: the corpus (`dumps/`; the format is in `dumps/FORMAT.txt`)

Status codes: 0 optimal, 1 max_iter, 2 infeasible, 3 numerical_error. "OFF" is the same dump replayed with `independent_working_set = false`.

| Cell / QP | Context | n/me/mi | In cell | Feasible? | Optimum known? | OFF replay | Class |
|---|---|---|---|---|---|---|---|
| YORKNET qp0 | elastic, major 0 | 508/256/0 | 3 N3, 242 minors | yes | f*=1964037.12937 | **0 at f\*** | A |
| YORKNET qp1 | restoration's elastic | 900/256/0 | 3 N3, 594 | yes | f*=664247.80146 | **0 at f\*** | F1 (+A) |
| SAWPATH qp0 | elastic, major 0 | 585/578/196 | 3 cert. rejected, 6 | yes | ≈−388.87 (AlmostSolved) | **0**, −388.81 | G |
| SAWPATH qp1 | restoration's elastic | 1937/578/196 | 1 cap 20350 | yes | 156.60 (AlmostSolved) | 1 | G (leak, \|m\| 6.6e12) |
| SAWPATH qp2 | elastic, major 1 | 585/578/196 | 3 cert. rejected | yes | 252.7867 | **0 at f\*** | G |
| DALLASM qp0–3 (= f4 qp0) | primary, Δ 1…0.125 | 196/150/0 | 3 N3, 64 | **no** (min violation .448 / 2.37 / 6.02 / 9.07) | n/a | 1 | F1, then N3 where a certificate belongs |
| DALLASM f4 qp1 | elastic, major 0 | 215/150/0 | 3 N3, 194 | yes | −923778.932 | **0 at f\*** | F1 + A |
| DALLASM f4 qp2 / qp3 | major 1 at the feasible point / its elastic QP (no slacks) | 196/150/0 | 3 N3, 64 | yes | −92643.835 | 1 cap at ≈f* | F1 + A (degenerate optimum: 112 active bounds, 46 dof) |
| STEENBRB qp0 | elastic, major 0 | 492/96/0 | 3 zero-motion exhausted | yes | nonconvex (λmin −0.61) | **0** | F1 |
| STEENBRB qp1, qp2 | post-restoration, Δ .1, .05 | 468/96/0 | 1 cap 4680 | yes | ≈−9.911e11 / −9.907e11 | 1 | F1, then a period-25 cycle, \|m\| 5.7e8 |
| STEENBRB qp3 | post-restoration, Δ .025 | 468/96/0 | 3, **x = NaN** | yes | −9.2875e11 | 1 | F1, then NaN (unexplained) |
| ACOPP57 qp0–3 | primary, majors 3–6 | 128/114/160 | 3 saddle exit, 2 minors | yes | 0.2437426 | 1 cap 1795 | **B** |
| ACOPR57 qp0, qp1 | primary, Δ 1, .5 | 128/114/274 | 3 N3 | yes | 432.675 / 501.806 | **0** | A (shift-seeded over-count +49, trimmed) |
| ACOPR57 qp2 | primary, Δ .25 | 128/114/274 | 3 N3 | yes | 649.121 | **0** | A (not fixed) |
| ACOPR57 qp3 | elastic rung ρ=1e8 | 299/114/274 | 1 cap 4360 | yes | nonconvex | **0** | period-6 exchange cycle, \|m\| 7.8e7 |
| ACOPR57 qp4 | restoration, major 1 | 630/114/274 | 3 N3 | yes | nonconvex | 1 | B-like (`f5` alone solves it) |
| ACOPR57 qp5–7 | restoration, majors 6–8 | 630/114/274 | 1 cap 7100 | yes | nonconvex | 1 | C′/F2 |
| ACOPR57 qp8 | restoration, major 9 | 630/114/274 | 3 saddle exit | yes | nonconvex | 1 | B, then a D3(b) refusal of a member with relative rate 0.76 (F) |

## Deliverable 2: the first wrong decision, per QP (OBSERVED from the traces and addseq)

- **DALLASM f4 qp2** (qp0–3 and f4 qp1/qp3 start the same way).
  - Pass 3: pin 55 is admitted as independent with engine ρ = 3.13e-6. Its true distance is 2.4e-15, and it structurally completes equality row 49. Its relative rate along p is 3.6e-12, so it blocks only through the μ-leak.
  - The same happens at passes 6, 32, 42 and 43 (pins 98, 132, 127, 93).
  - At count n, rank is 190 of 196 and the step is still 35.
  - Every exchange after that runs on multipliers of 1e4–1e5, and the solve ends N3 at pass 63.
  - **Correct decision:** dependent, so not a real blocker (its exact rate is 0).
- **STEENBRB qp0.** Pass 234: pin 407 is admitted with ρ = 5.7e10 (diverged refinement). It is exact and structural (equality row 63). The walk ends in zero-motion exhaustion at pass 907. With `structfirst` it ends kOptimal in 658 minors.
- **STEENBRB qp1, qp2, qp3.** Exact structural pins are admitted at passes 31, 20 and 208 (ρ 6.2e-2, 1.3e18, 5.5e-4). The tail is a zero-motion cycle with |m| ≈ 5e8, near f*. `structfirst` alone does not fix these; a non-structural leak remains (INFERRED).
- **YORKNET qp0.**
  - Passes 0–240 are 240 α = 0 adds. H is zero on the slacks, so the EQP step is 1.5e13. addseq finds no dependency among them.
  - Pass 241: a dependent blocker (ρ 1e-16). Every positive-coefficient member is flagged "entered at this point" and locked, so N3.
  - OFF over-admits here (count n+3) and reaches f* in 1030 minors.
- **YORKNET qp1.** Pass 218: pin 36 is admitted at ρ = 7.7e-4 (grey zone). At pass 593 the step is 2e8 at count n (a leak). `greyd`+`unlock` reaches f*. That pin 36 is truly dependent is INFERRED: n = 900, no dense check.
- **SAWPATH qp0 / qp2.** Pass 5: pin 8 at count n. The screen fires; ρ = 4.9e-5; the dense distance is 2e-7. The coefficients sit only on equality rows (cond1), so the certificate is rejected. OFF admits and solves.
- **ACOPP57.**
  - Pass 0: kWrong. The start repair finds no pin, because the integer screen refuses every one.
  - Pass 1: a KKT point on the optimal face is sent to the saddle exit.
  - **Correct:** kOptimal, a weak minimizer. `f5`+`f5s` gives kOptimal in 8 minors.
- **ACOPR57 qp0 / qp1.** The +49 seeded over-count is trimmed one member per pass, and the trim removes all 6 pins. The pins re-block at α = 0 against locked members: N3. `unlock` gives kOptimal.
- **ACOPR57 qp5–7.** Drop/re-add of a pin or row with |m| ≈ 1e-7 (E3's absolute-tolerance family).
- **ACOPR57 qp8.** A known-dependent mark sits on a member whose relative rate is 0.76 (the D2 lesson recurring).

## Candidate rules (toggles in `d4-patch.diff.txt`; all results OBSERVED)

| Rule | Statement | Dumps converted | Cell | U0 | HS TRAIN |
|---|---|---|---|---|---|
| `structfirst` | An exact structural dependency decides before any numerical "independent" | STEENBRB qp0 | STEENBRB still numerical_error | 0 diffs | 21/21, identical |
| `unlock` | With no admissible member on a zero-motion step, members locked only by entering at this point (never released here) become admissible; min-ratio rule, lowest-index ties | ACOPR57 qp0, qp1 | **ACOPR57 → optimal** (17 majors, 404 factorizations) | 0 diffs | 21/21, identical |
| `f5`+`f5s` | A kWrong is re-read on a μ-free eliminated factor. A kOk or kSuspect read replaces it, and the existing suspect-stall stationarity gate decides | ACOPP57 qp0–3, ACOPR57 qp4 | ACOPP57: 7 → 29 majors, still numerical_error | 0 diffs | 21/21, identical (factorizations +2…+5 on hs006/007/033/045 under the combination) |
| `screenadmit` | A dependency proved only by the count (ρ > kDepTol), with no admissible member and no verified certificate, is admitted as a deferred add | YORKNET qp0 (only in combination) | YORKNET no conversion | 0 diffs | 21/21 |
| `greys`+`greyd` | A converged ρ in (kDepTol, kDepUndecidable) without a structural dependency counts as dependent | YORKNET qp1 (with `unlock`) | — | **f7_n1000_path_warm Optimal → dnf_setup** (n800 still running at hand-back) | pending |
| **Combined** (`structfirst,greys,greyd,unlock,f5,f5s,screenadmit`) | all of the above | 7 of 18: YORKNET qp0/qp1 at f*, STEENBRB qp0, ACOPR57 qp0/qp1/qp4, ACOPP57 qp0 | **ACOPR57 optimal**; READING6 optimal (8 majors); ZAMB2-8 and SMBANK identical; YORKNET, DALLASM, STEENBRB, ACOPP57, SAWPATH not converted | **138 diffs: both f7 warm cells dnf_setup** | 21/21, same iterations and minors |

**Rejected on evidence.**
- `leakpass` (pass over a leak-rate dependent blocker without the violation check) produces **false kInfeasible** on the feasible DALLASM qp2/qp3 and YORKNET qp0/qp1.
- `coefrho` (an exchange coefficient must exceed 10ρ) breaks ACOPR57 qp0/qp1 and YORKNET qp1.
- `unlockall` (also admit re-entered members) breaks ACOPR57 qp0–qp2.
- `leakpass2` (with the violation check) and `greyu` (grey as undecidable) convert nothing new.

**DALLASM's feasible QPs are converted by no rule or combination.**
- Under the combined candidate there is a new 172-fold cycle: `screenadmit` admits a grey blocker, the re-check removes it, and it re-blocks. Its threshold, ρ > kDepTol, overlaps the grey band that `greyd` claims.
- Earlier, exchanges pick noise-level coefficients. At pass 222, pin 108 is chosen with v = 1.8e-6 against a threshold of 1.4e-6 (σ = 4.9e10), and the set stays exactly dependent afterwards.

**Checked points.**
- YORKNET qp0/qp1 under the combined candidate reach f* to 5e-7 absolute.
- ACOPR57 qp0/qp1 and ACOPP57 end within 2e-4 of x*, carrying the walk's row-scaled primal residuals of 3e-7 to 6e-7. OFF lands at the same point.

## Deliverable 3: ranked classes, fix plan, predictions

| Rank | Class | TRAIN cells gated | Fix | Cost | Prediction |
|---|---|---|---|---|---|
| 1 | F1 (test admits exact dependencies; leak) | DALLASM, STEENBRB (OBSERVED); STEENBRC/G (INFERRED) | `structfirst` (U0-clean), plus a safe grey rule. `greyd` as tried breaks f7; it needs rate evidence or an eliminated-path re-test, and S24's "converged grey = independent" fixture revisited | 1 + 1–2 rounds | 0 conversions from F1 alone (INFERRED). STEENBRB's elastic QP solves, but the post-restoration QPs still cap, and restoration's f = 2.97e12 remains (E2-D3) |
| 2 | A (locked degenerate vertex) | ACOPR57 (OBSERVED), YORKNET | `unlock` (U0- and HS-clean) | 1 round | **ACOPR57 converts (OBSERVED on the cell).** ACOPR118 possible (INFERRED, not run). YORKNET no: the next QPs cap (elastic at major 1, restoration at 7680) |
| 3 | B (D5 singular PSD face) | ACOPP57 (OBSERVED), ACOPP118 (INFERRED) | `f5`+`f5s` (U0- and HS-clean) | 1 round, +≈1 for the second face | The cell goes 7 → 29 majors, then fails at Δ ≈ 1e-6 by zero-motion exhaustion: after a drop, the direction re-enters the released bound (the singular reduced Hessian again). 0 conversions until that is handled (INFERRED) |
| 4 | G (scaling / numerically rank-deficient mandatory rows) | SAWPATH | Row equilibration before E1b's spectrum test and before the walk; or `screenadmit` for cond1-only cases, which `greyd` currently pre-empts | 1–2 rounds | Unknown. OFF solves SAWPATH's elastic QPs, but OFF's cell was dnf_wall |
| 5 | C′/F2 (noise-multiplier drops) | ACOPR57 restoration (moot once A converts it), ACOPR118 (INFERRED) | Scaled drop tolerance (M5 arm A / E3 family) | 1 round | Not tried |
| — | DALLASM degenerate optimum (F1 + A + noise-coefficient exchanges) | DALLASM | None found. Needs exact dependency information and an anti-cycling pivot rule at a vertex with 112 active bounds and 46 dof. OFF reaches f* but cannot certify it | 2–3 rounds, research-grade | Unknown |

**Honest prediction for a round landing the three U0-clean rules (`structfirst` + `unlock` + `f5/f5s`):**
- ACOPR57 converts (OBSERVED, with `unlock` alone).
- All other TRAIN failures stay failing: OBSERVED for YORKNET, DALLASM, STEENBRB, ACOPP57 and SAWPATH under the superset combination; INFERRED for STEENBRC/G, ACOPP118, ACOPR118 and BRITGAS.
- READING6, ZAMB2-8 and SMBANK stay optimal (OBSERVED under the superset).

## What stays unknown

- Whether YORKNET qp1's pin 36 is truly dependent.
- Why STEENBRB qp3 produces a NaN iterate.
- Whether `unlock` terminates in general: it relies on the zero-motion cap and has no finiteness argument.
- ACOPP57's second D5 face.
- SAWPATH's true rank.
- Cells not run: ACOPP118, ACOPR118, STEENBRC, STEENBRG, BRITGAS.
- The nonconvex dumps (STEENBRB qp0, ACOPR57 qp3–8) were checked only locally: feasibility, not global optimality.

## Files in `.scratch/m7w2e2d4-readout/`

- `dumps/` + `FORMAT.txt`: the 31 base dumps, plus the dumps from each candidate cell run (`*_D`, `*_f5`, `*_sf`, `*_unlock`, `*_sa`, `*_gg`).
- `transcripts/`: per-cell `.out`, `.err` (D3 and D4DUMP lines) and options `.dump`; `legs.log`, `cells5.log`, `u0hs5.log`, `summary.txt`, `matrix1.log`.
- `replays/`: every configuration's summary, `faithfulness.txt`, `oracle_all1.txt` (+ `.summary`), `oracle_D_points.txt`, the addseq files, and three trace tarballs (base, the combined candidate's focus set, the ACOPP57 frontier). The full traces, including SAWPATH qp1's 24 MB one, stay in `.scratch/m7w2e2d4/replays/`.
- `u0/`, `hs/`: per-configuration CSVs, comparisons and summaries.
- `d4-patch.diff.txt`: the full scratch diff.
- `runners/`: every script, plus the replay tool and oracle sources.
- `STATE.md`: live state, including the still-running leg.


## Settler ruling (2026-09-24)

- The corpus (31 dumped QPs, 26 feasible, replay-faithful 31/31) is the FIXTURE SOURCE for every
  engine round from here; `dumps/FORMAT.txt` is the loader contract.
- **E2 fix round 3** lands the three U0-clean, HS-clean rules — `structfirst` (an exact structural
  dependency decides before any numerical "independent"), `unlock` (with no admissible member on a
  zero-motion step, members locked only by entering at this point become admissible under the
  min-ratio rule with lowest-index ties; a finiteness argument or a counted cap is required),
  `f5`+`f5s` (a kWrong inertia is re-read on a μ-free eliminated factor; kOk or kSuspect replaces it
  and the existing suspect-stall stationarity gate decides) — each behind its own fixture from the
  dumps (STEENBRB qp0; ACOPR57 qp0/qp1; ACOPP57 qp0), with the prediction ACOPR57 converts and
  nothing else does. `greyd` is NOT landed (it breaks the f7 warm cells: the E2-D2 leak class again).
- The finding that the E2-OFF walk solves 9 feasible dumps the E2-ON walk fails is a FINDING about
  E2's strictness on leaking/degenerate sets, not yet a ruling on the default: the fix-3 round
  reports ON+rules vs OFF on the full dump corpus, and the default is decided at E2's close on the
  re-take numbers.
- Registered: DALLASM's degenerate optimum (112 active bounds, 46 dof; no rule found; research-grade,
  2–3 rounds); the grey-zone rule with rate evidence or an eliminated-path re-test; SAWPATH's row
  equilibration (class G); ACOPP57's second face; the C′/F2 scaled drop tolerance (E3 family);
  STEENBRB qp3's NaN; `unlock`'s finiteness.
