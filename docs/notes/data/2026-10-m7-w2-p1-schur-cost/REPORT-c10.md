> Copied verbatim from `.superpowers/` (the round's own report/review) on 2026-10-01; nothing below this preamble is edited.

# M7 W2 C10 — failure census of the SNOPT-only set (8 cells) — PARTIAL-MEASUREMENTS-COMPLETE

**Status: PARTIAL-MEASUREMENTS-COMPLETE. No SIGNOFF line; the settler writes the ranking.** All measurements were taken in
worktree `e2fix8` (`.claude/worktrees/agent-a69f46e11796560b9`, HEAD `8cfc12ed`, same sources as m7 `1f8d3118`), scratch
state in `.scratch/c10/` (`STATE.md` is the log). Nothing committed. hven-bench was not written. HOLDOUT not run.
Labels: OBSERVED = read off a run, trace, profile, gdb hit count, replay or oracle. INFERRED = reasoned from observations.

## Method
* A scratch lane (`.scratch/c10/bench`, a git archive of hven-bench `97dc81b` plus an env-gated `C10_TRACE` JSONL sink) built
  against fix 9's own install. A scratch diagnostic hven build (`tools/patch_qp.py`) writes one `C10QP` line per QpEngine exit
  with the EXIT SITE (source line that set the status) and dumps every non-optimal QP in D4QP format. Neutrality: diag rows for
  STEENBRB/D/E and ACOPP118 are byte-identical to the plain traced rows and to RE-TAKE 10.
* Every leg: one at a time inside `flock /tmp/box-build.lock`, `MKL_NUM_THREADS=1`, `taskset -c 2`, pgrep-empty checked first.
  Wall cells capped at 580 s and stopped by their own exact pid (logged in `logs/wallcap.log`). Profiles: `perf record`
  (`--call-graph dwarf`, attached to the cell's own pid) and gdb ignore-count breakpoints (hit counts), both under the same lock.
  Time splits below are fractions of samples; wall-clock is informational only. gdb/perf slow the run, so counts are rates
  within a capped window, not totals.
* Exit-site names: KWRONG_DEADEND = stationary point on a face whose inertia reads kWrong, no recovery (`qp_engine.cpp:1335`,
  the NC3 saddle exit). ZM_EXHAUSTED = zero-motion exhaustion (`:1354`). ADD_REJ/BLKTERM = a blocker could not be added and the
  walk terminated (`:2048`, status numerical_error). CAP = minor-iteration cap (`:662`).

## Per-cell findings

| cell | terminal story | exit sites | mechanism |
|---|---|---|---|
| STEENBRB | Major 0: QP TR-infeasible at Δ=1 (certified), elastic rungs solve, step accepted. Major 1: elastic QP fails, restoration (n=660) fails on every QP; restoration TR shrinks 1→0.125, ends numerical_error; cell numerical_error at major 2. | elastic QP and 6 of 7 restoration QPs: KWRONG_DEADEND | NC3 direct; R2 |
| STEENBRD | Major 0 TR-infeasible, elastic KWRONG_DEADEND, restoration converges (11 majors) but lands at f=1.66e12; main QPs at TR 0.05–0.4 alternate optimal/failing; 3 consecutive failures end numerical_error at major 11. | elastic KWRONG_DEADEND; main ZM_EXHAUSTED x4, ADD_REJ/BLKTERM x2 | NC3 at entry; restoration landing; walk failures; R2 |
| STEENBRE | As STEENBRD; restoration lands at f=3e13, main QPs fail at TR 0.1→0.0125; numerical_error at major 5. | elastic KWRONG_DEADEND; main KWRONG_DEADEND, ZM_EXHAUSTED, ADD_REJ/BLKTERM, CAP | as STEENBRD |
| ACOPP118 | Majors 3–6: four QPs hit the 4710-minor cap. Standstill retreat fires in each (sstr=1) and they still cap; equality drift 760–3447. TR 2→0.25; numerical_error at major 7. | CAP x4 | F81-1/F81-2 |
| YORKNET | Major 0 TR-infeasible (7 minors), elastic QP NUM (ADD_REJ/BLKTERM), then restoration (n=824) for the whole 580 s: 10 restoration majors, accept/reject alternate, TR 0.25..1, restoration f 51001→10408. | restoration QPs KKT (optimal); elastic ADD_REJ/BLKTERM | slow restoration + Schur-rebuild cost (new); NOT dependency tests |
| BRITGAS | Finished 497 s numerical_error at major 1. TR-infeasible, elastic QP CAP (6980), restoration (n=1170): 3 KWRONG_DEADEND then progress, then CAPs at 11700 minors; restoration numerical_error. | elastic CAP; restoration KWRONG_DEADEND x3, CAP x6 | NC3 at restoration; negative curvature on the capped QPs' final working set; Schur-rebuild cost |
| READING8 | n=2002, me=1000. No QP finished in 580 s: the first (dispatch) QP is inside the start vertex repair the whole time. | none (no exit) | start-repair pin-probe loop with a full Pardiso re-analysis per probe (new) |
| SAWPATH | Major 0: QP TR-infeasible (5 minors, certified), elastic QP NUM (ADD_REJ/BLKTERM, 6 minors), restoration (n=1935, me=578, mi=196) for the rest. Only 3 restoration majors exited in 580 s: maj0 NUM 828 minors (reject, TR 1), maj1 NUM 1368 minors (reject, TR 0.5, drift 333), maj2 OPT 916 minors (accept_f, TR 0.25); never leaves restoration. | restoration: ADD_REJ/BLKTERM x2, KKT x1 | R2 (TR shrink on NUM); ADD_REJ/BLKTERM; Schur-rebuild cost (new) |

## READING8: where the first QP spends its 580 s (profiles)
* OBSERVED: no QP exited, no ledger line; only `sqp.solve.begin` in the trace.
* OBSERVED (perf dwarf, 60 s; flat 200 s agrees): 93% of samples under `QpEngine::repair_temporary_vertex` (called ONCE, from
  `run`) → `probe_inertia` 90% → `border_candidate` → `eliminated_candidate` → `solve_eqp` → `factorize_checked` 79%. Split of the
  whole: Pardiso symbolic analysis (`FactorSession::analyze`: reorder/METIS/symfct) 57%; numeric factorization 22%; triangular solves
  11% (+ refinement matvec ~6%); dependency tests 3%; polish/pattern hash ~2%.
* OBSERVED (gdb, 560 s window): `repair_temporary_vertex` 1 call; `probe_inertia` 1001 = exactly the pin budget
  `repair_pin_budget(2002) = ceil(n/2) = 1001`; `SymmetricFactor::analyze` 2506 and `factorize` 2508 (one analysis per
  factorization: no pattern reuse, because each pin changes the pattern); `solve_single` 12513; `dependency_test` 1984;
  `add_border` only 257 (of 2763 `border_candidate` calls, ~2508 fell back to `eliminated_candidate`); `polish_eqp_core` 2508.
  ~0.22 s per analyze+factorize+solves cycle.
* Verdict (INFERRED from the above): the first QP is NOT stuck and not cycling in the walk. It is in the start repair, a
  bounded loop (budget 1001 pins; each pin is one probe = one full re-analysis and factorization of a 3000-row KKT) and the
  budget's design figure (p50 1 pin, measured on other problems) does not hold here: n=2002 with an indefinite Hessian reads
  kWrong through all 1001 pins (the probe count reached the budget). At ~0.22 s per cycle the pin loop alone is ~220 s, and the
  repair is followed by further factorizations (2508 total in 560 s) before any minor exits. Whether the repair finally succeeds or
  fails, and how long the walk after it would take, is UNKNOWN (no run longer than 580 s; none attempted).
* Not done: no dump of READING8's first QP exists (the QP never exits), so no replay/oracle for this cell.

## YORKNET: restoration time profile (n=824)
* OBSERVED (perf dwarf 150 s, restoration QPs): **90.7% self time in `SchurComplement::rebuild_schur()`** (93.75% inclusive
  under `add_border` ← `sync_borders`). MKL Pardiso total 4.5% (factorization 1.8%, solves 2.75%); `dependency_test`/`test_candidate`
  0.5–0.6%; `probe_inertia` 0.5%; `polish_eqp_core` 0.25%; residual refinement ~0.1%; KKT assembly 0.05%.
* OBSERVED (gdb, 100 s): `add_border` = `rebuild_schur` = dense `try_factorize` = 10061 calls; `sync_borders` 177 (so ~57 borders
  added per sync: the stack is re-added from scratch and EVERY add rebuilds the whole m x m C = O(m² n0), O(m³ n0) per sync);
  `analyze` 2955, `factorize` 3012, `solve_single` 28708, `dependency_test` 3013; 2644 minors completed in the window.
* OBSERVED per-minor cost: ~38 ms (150 s / ~3960 minors in the perf run; 2644 minors in 100 s under gdb). Offline replays agree
  (qp1: 468 minors in 5 s; qp2: 654 minors in 15 s, ~11–23 ms/minor).
* INFERRED: the cost is the C assembly (`-v_i . k0inv_v_j` over all i,j, n0 ~ 10³) not the dense factor (MKL dense factor
  frames are ~1–3%). The registered R3 hypothesis ("the E4 cost", i.e. the independent-working-set dependency tests) is NOT where
  the time goes here: dependency tests are ~0.5%.

## SAWPATH and BRITGAS profiles (OBSERVED, extra)
* SAWPATH (perf dwarf 200 s): 95.9% self in `rebuild_schur` (97% inclusive under `sync_borders`/`add_border`). gdb 250 s: 2828
  `add_border`, 35 `sync_borders` (~81 adds/sync), 736 factorizations. The cold restoration QP (solve 2, 828 minors) replays in
  4 s (135 `add_border`, 131 syncs); the warm-seeded QP (solve 3, seed from the previous major, tr 0.5) replays in **325 s**
  (1298 minors, ~250 ms/minor): the seeded working set makes `sync_borders` re-add hundreds of borders each sync. So the SAWPATH
  wall time is the warm restoration solves, not the first one.
* BRITGAS (perf dwarf 300 s): 74.4% self in `rebuild_schur`, the next symbol 2.6%.
* ADD (INFERRED): the same cost sits behind STEENBRB/D/E's longer QPs only mildly (their n is ~500–660; their cells finish in
  seconds), so it matters where n and the working-set seed are large.

## Representative sub-QP replays and oracle checks
Replays: `dump_replay` (prod 8cfc12ed), dump's own options, single thread. Oracle: `qp_oracle.py` (HiGHS phase-1 LP for
feasibility; Clarabel for the optimum where H is PSD on null(Ae); dense SVD reduced Hessian on the walk's final working set; scipy
venv in the scratchpad, an oracle only). In-cell vs replay status/minors/facts:

| dump | in-cell | replay | oracle feasibility | oracle optimum / curvature |
|---|---|---|---|---|
| STEENBRB qp0 (TR-INF cert) | INF 87/139 | INF 87/139 identical | infeasible-in-TR certificate confirmed: min l1 violation 19694 | nonconvex: no global oracle |
| STEENBRB qp2 (elastic KWRONG) | NUM 289/303 | identical | FEASIBLE (0 violation) | nonconvex; final working set null space dim 36, Z'HZ min eig -12.3: a saddle with live negative curvature (confirms NC3) |
| STEENBRD qp1 (elastic KWRONG) | NUM 573/690 | identical | feasible | final WS null dim 35, min eig -250: same saddle |
| STEENBRE qp1 (elastic KWRONG) | NUM 1040/1132 | identical | feasible | final WS null dim 35, min eig -250: same saddle |
| ACOPP118 qp0 (CAP) | CAP 4710/4627 | identical | feasible | PSD on final WS; Clarabel f*=12.0116, walk f=12.8876 (gap 0.88), eq residual 1.2e-5; final working set me+\|W\|=343 but rank 338 (rank-deficient, smallest normalized sv 1.5e-16): the cap hits a working set that cannot be independent |
| YORKNET qp0 (TR-INF cert) | INF 7/3 | identical | infeasible confirmed: min l1 violation 17018 | convex; Clarabel PrimalInfeasible |
| YORKNET qp1 (elastic NUM) | NUM 468/489 | identical | feasible | convex; Clarabel f*=1.6965e6; walk exits NUM with eq violation 1333, f=5.3e4 (nowhere near optimal), working set rank 504 of 508 |
| YORKNET qp2 (restoration NUM) | NUM 654/662 | identical | feasible | nonconvex; final WS null dim 1, eig -9.7e-3, eq violation 100 |
| SAWPATH qp0 (TR-INF cert) | INF 5/3 | identical | infeasible confirmed: min l1 violation 2.56 | convex; Clarabel PrimalInfeasible |
| SAWPATH qp1 (elastic NUM) | NUM 6/10 | identical | feasible | convex; Clarabel f*=-376.2 (AlmostSolved); walk exits NUM at eq violation 2.56 after 6 minors |
| SAWPATH qp2 (restoration NUM, cold) | NUM 828/832 | identical | feasible | LP (H=0); Clarabel f*=-1.0005; walk f=1.73e7, 558 bounds that are active at the optimum not pinned: far from optimal |
| SAWPATH qp3 (restoration NUM, warm) | NUM 1368/1372 | NUM 1298/1302 (325 s): status same, counts NOT identical (hot-state not reproduced) | feasible | not run through the oracle |
| BRITGAS qp0 (TR-INF cert) | INF 508/1024 | identical | infeasible confirmed: min l1 violation 46.65 | nonconvex |
| BRITGAS qp2 (restoration KWRONG) | NUM 1902/2662 | identical | feasible | LP (H=0); Clarabel f*=-67.8186110861, walk final f=-67.8186110857 (|gap| 3.5e-10): the walk stopped AT the optimum and still exited as a failure (the saddle exit firing on an LP) |
| BRITGAS qp5 (restoration CAP) | CAP 11700/11706 | identical (32 s) | feasible | nonconvex; final WS null dim 8, min eig -0.86 (negative curvature remains), 1147 exchanges |

OBSERVED summary: every non-optimal sub-QP is feasible (the four TR-infeasible certificates are correct), so every failure is
a walk failure, none is a modelling infeasibility. 15 of 16 dumps replay to the in-cell status, minors and factorizations
exactly; the warm hot-seeded one (SAWPATH qp3) does not. READING8 has no dump.

## Mechanism counts (cells where it appears; stage)
Counts are OBSERVED where an exit site / profile / oracle backs them, INFERRED where marked.
* **NC3, the saddle dead end (`:1335`)**: 4 of 8 — STEENBRB (elastic QP at major 1 and 6 of 7 restoration QPs), STEENBRD (elastic QP,
  major 0), STEENBRE (elastic QP major 0, then a main-phase QP), BRITGAS (3 restoration QPs at restoration entry). The oracle confirms the
  saddle (negative reduced-Hessian eigenvalue on the final working set's null space) on STEENBRB/D/E; on BRITGAS qp2 the walk is already
  at the LP optimum. Stage: elastic QP at major 0/1 (3 cells), restoration entry (2 cells). Not seen in YORKNET, SAWPATH, ACOPP118;
  READING8 unknown.
* **R2, the driver's response to a walk-owned failure (TR shrink only)**: 6 of 8 — STEENBRB, BRITGAS, SAWPATH inside restoration
  (restoration TR shrink 1→0.125, 1→0.25); STEENBRD, STEENBRE, ACOPP118 in the main phase (TR 0.4→0.05, 0.1→0.0125, 2→0.25 on CAPs).
  INFERRED as a mechanism (the shrink never helps in these traces: failing QPs recur at smaller TR); the traces show the response,
  not a counterfactual.
* **Restoration landing problem**: 2 of 8 — STEENBRD (f=1.66e12) and STEENBRE (f=3e13) after a converged restoration, then main-phase
  failure. 4 more cells (STEENBRB, BRITGAS, YORKNET, SAWPATH) are inside restoration when the cell ends or is capped and never land.
* **F81-1/F81-2 (the cap with the fix-9 retreat fired, equality drift)**: 1 of 8 certain (ACOPP118, all four majors). BRITGAS's 6
  restoration CAPs and STEENBRE's one main-phase CAP are also caps; BRITGAS qp5 is a negative-curvature working set with 1147 exchanges
  (more NC3-like than drift) — INFERRED, not diagnosed to the F81 level.
* **R3, the E4 cost (dependency tests)**: **0 of 8 supported.** In the three wall cells with a profile (YORKNET, SAWPATH, BRITGAS) the
  time is not in the dependency tests (0.5% in YORKNET).
* **New: Schur C rebuild per add / whole-stack re-add per sync (`SchurComplement::rebuild_schur`)**: 3 of 8 by profile (YORKNET 90.7%,
  SAWPATH 95.9%, BRITGAS 74.4% of samples), all in restoration QPs (n 824–1935) and worst under warm seeds. It is the wall-clock cause
  of YORKNET and SAWPATH and a large part of BRITGAS's 497 s. Cost is O(m² n0) per add, O(m³ n0) per sync.
* **New: start-repair pin loop with per-probe full Pardiso re-analysis**: 1 of 8 (READING8; the whole 580 s, at major 0's first QP).
  INFERRED to apply to any large indefinite first QP; not seen in the others (their repairs are short).
* **New/unregistered: ADD_REJ/BLKTERM numerical_error exits (`:2048`)**: 4 of 8 — YORKNET and SAWPATH (the elastic QP at major 0, then
  restoration QPs), STEENBRD, STEENBRE (main phase). The walk terminates on a blocker it cannot add or refuse; oracle: YORKNET qp1 and
  SAWPATH qp1 are feasible convex QPs on which the walk stops with equality violation 1333 and 2.56, YORKNET qp1's working set rank
  504 of 508. Stage: elastic QP at major 0 (2 cells), restoration (2 cells), main phase (2 cells).
* Common opening (OBSERVED): 6 of 8 cells (STEENBRB/D/E, BRITGAS, YORKNET, SAWPATH) start with a major-0 QP that is TR-infeasible at
  Δ=1 with a certificate the oracle confirms; 6 of 8 enter restoration. ACOPP118 fails in the main phase (majors 3–6), READING8 never
  completes its first QP.

## Open items
* READING8: longer-than-580 s measurement (does the repair complete; what happens after) is not done; no dump exists.
* SAWPATH qp3's replay differs from the cell (1298 vs 1368 minors), so the hot-state is not reproduced by the dump loader.
* Ranking and the recommendation: the settler's.

## Settler ranking and rulings (2026-10-01)

C1 **Two rounds, in parallel.** By conversions per unit of risk:
1. **P1, the cost round (behaviour-neutral by construction).** The C10 profiles retire R3 (dependency-test cost is ~0.5 % of samples, 0/8 cells) and name two pure efficiency defects:
   (a) `SchurComplement::rebuild_schur()` recomputes the WHOLE m×m C from dot products on every `add_border`/`drop_border` (O(m²·n0) per border, ~57 borders re-added per `sync_borders`): 90.7 % of YORKNET's restoration samples, 95.9 % of SAWPATH's, 74.4 % of BRITGAS's. The remedy keeps every C entry computed by the same dot product (append a row/column on add, delete on drop, and/or a batch add that rebuilds once), so C is BIT-IDENTICAL and the factor is too.
   (b) READING8's start repair re-runs Pardiso's SYMBOLIC analysis on every pin probe (2506 analyses / 2508 factorizations, 57 % of samples, 1001 probes at n = 2002). Reuse the analysis when the KKT pattern is unchanged. Whether it is unchanged is to be established, not assumed.
   Acceptance is identity: counters and statuses exact on corpus, U0, HS and the 7 optimal + measured TRAIN cells. Then measure solo (informational) whether YORKNET, SAWPATH, BRITGAS and READING8 now finish inside 300 s, and what they end at.
2. **NC3, the saddle exit** (NC2 brief M13). The saddle dead end (`qp_engine.cpp:1335`) is a TRUE saddle on STEENBRB/D/E (null-space eigenvalues −12.3 / −250 / −250) and appears in 4/8 cells (STEENBRB/D/E at the elastic QP, BRITGAS in restoration). It is the one numerics mechanism measured (NC2) to convert the STEENBRD family. Fixtures come first, including the redesign of the 4 driver fixtures whose premise is a QP kNumericalError.
C2 **Behind them:** R2 (the driver answers a walk-owned failure with TR shrink only; 6/8 cells) and the restoration landing problem (STEENBRD/E land at f ≈ 1e12–1e13). R2 is the next round after NC3; its census is C10's table. Registered: the ADD_REJ/BLKTERM exits (`:2048`, 4 cells); BRITGAS qp2, an LP whose walk stops AT the optimum (gap 3.5e-10) and still exits as a failure (a termination-test defect; small, fixture-ready); ACOPP118 = F81-1 (rank 338 of 343); the fix-8 repair budget's premise of p50 = 1 pin fails at n = 2002 with an indefinite Hessian (F8-2's deficit-aware budget becomes relevant); the SAWPATH qp3 hot-state dump not reproducing.

SIGNOFF: M7-W2-C10-REPORT

C3 (2026-10-01) **P1 (a) ACCEPTED pending review and gates** (worktree `904350e2` → m7 `1f46e34d`; report `.superpowers/m7-w2-p1-report.md`): the incremental Schur C plus once-per-batch factorization in `sync_borders`. Identity holds on every measured surface (corpus 0, U0 block byte-identical, 13 TRAIN cells 0/132 columns, nothing re-pinned). Payoff, wall-clock informational: YORKNET over 580 s → 49 s, SAWPATH over 580 s → 25 s, BRITGAS 497 → 106 s; all three now END numerical_error at major 1 inside the budget (they FINISH, they do not convert). ACOPR118 1.76× faster; the long test 626 → 165 s. **P1 (b) NOT implemented, correctly:** each pin changes the KKT's dimension, so analysis reuse needs a user permutation (an iparm change, out of scope) or changed numerics. READING8 stays over 580 s, inside the repair (F8-2's deficit-aware budget, or a repair that does not refactorize per pin, is its route; registered). Consequence for RE-TAKE 11: YORKNET/SAWPATH/BRITGAS move dnf_wall → numerical_error. That is a declared status move, not a regression. Their failures are now visible for R2/NC3.
C4 **Fable review of P1: CLOSED** (`.superpowers/m7-w2-p1-review-fable.md`). C and its Bunch–Kaufman input are bit-identical to the old rebuild by inspection. The batch path has no reader mid-batch and every exception path closes it. The evidence was re-verified from raw artifacts. Part (b)'s deferral is correct (each probe changes the eliminated K's dimension; `analysis_decision` re-analyses on any pattern-hash change). Riders, non-blocking, for P1's next touch: R1 add a reference-factor solve to the bitwise test; R2 make `replay_batch` allocation-free (factor the top-left prefixes in place); R3 note the throw-path solve-count difference if drops are ever batched.
