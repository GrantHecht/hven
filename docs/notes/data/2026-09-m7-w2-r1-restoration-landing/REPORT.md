# M7 W2 R1 — the restoration landing on the STEENBR cells: diagnosis with arms

Pin: hven m7 `c1f73065` (sources = `a53dbb88`). Baseline rows: hven-bench `runs/2026-09-m7-w2-e2-retake6/run-A-default/`, and `run-A-independent-working-set-off/` for the E2-off control. Everything here is a scratch build; no production code moved. (The diagnostic implementer delivered this text as its hand-back; the settler placed it here verbatim except for this preamble and the ruling at the end. Brief: `.superpowers/m7-w2-r1-brief.md`.)

## The result

**No arm converts a STEENBR cell.** 0 of 5 under every arm in the brief, and 0 of 5 under three extra variants. So there is no answer to cross-check against the interior-point or SNOPT lanes.

**Both defects are real. Fixing the landing (A) does not make (B) moot, and (B) is what blocks conversion.**

1. **(A) The landing (observed).**
   - Restoration minimizes only the violation. On STEENBRB it takes a straight linear-programming path and lands at a feasible point with f = 2.97e12 and |∇f|∞ = 1.7e13. The start had f = 2859; the optimum is 9075.855.
   - The distance travelled is not the problem.
     - The feasible set really is far from the start. The SIF data put at least about 500 units of ∞-distance between the start (all 0.1) and any feasible point (inferred: 2000 units of commodity 1 leave node 2 over 4 arcs).
     - A capped radius (Arm T, c = 10) still reaches f = 8.6e11 after only 621 units of travel, with h still 6420.
   - The *direction* is what goes wrong. The phase cannot see the objective, so it piles flow onto single arcs, where the congestion cost explodes.
2. **A first-order composite restoration fixes the landing on STEENBRB (observed).** This is Cs: ω·f in the value and gradient only, ω halved whenever h fails to fall, and a return at the first iterate with h ≤ feas_tol. It lands at h 1.1e-10, f 4.35e4, |∇f|∞ 17.9. **But it does not reach feasibility on STEENBRC/D/E/G within 300 s** (h still 17 200–41 300).
3. **(B) The post-restoration QPs fail at the good landing too (observed).**
   - All 4 main-loop QPs after the Cs landing end `numerical_error`: the walk drops a bound at a vertex onto a negative-curvature face, and its next direction crosses that same bound at step 0. The re-entry exemption then stops the walk.
   - E2 off solves all four in replay.
   - With E2 off in the main loop after the landing, the solve takes 9 accepted majors (f 43 475 → 38 074). Then it caps 4 times in a period-3 cycle on a dependent working set and ends `numerical_error`.
4. **C as briefed (with f curvature) fails before moving (observed).** The first restoration QP caps at 6600–7640 minors on every STEENBR cell.
5. **P and T fail as the earlier M2b and M3a studies predicted (observed).**
   - P stops short of feasibility on all 5 and reports an uncertified `infeasible`, which is worse than `numerical_error`.
   - T runs out of time or majors: h falls by a constant 24 (B/D) or 28 (C/E/G) per unit of radius.
6. **The five optimal TRAIN cells are unmoved under Cs** (111 of 111 columns identical, by construction).

**Recommendation:** no restoration arm ships from this round. The next production round is the QP engine's drop onto a negative-curvature face. Cs is carried forward as a test-only landing.

## 1. How it was run

- **Scratch build.** `git archive c1f73065` plus `dep/eigen` `bc3b3987` and `dep/fmt` `407c905e`. Release, clang, LTO off, tests and bench off: the re-take 6 flags verbatim. Installed into the harness copy's own pin prefix; the stamp says SCRATCH.
- **Harness copy.** hven-bench `470983d` without `.git`, `.hven`, `runs`, `build` or `.scratch`. `.scratch/h3/decoded` is symlinked read-only. The cell command line is `run_serial.py`'s verbatim.
- **Patch** (`r1-patch.diff.txt`). Everything is env-gated:
  - `HVEN_R1=1`: the driver prints `R1*` lines.
  - `HVEN_R1_PROX=ζ` (Arm P). D = min(1, 1/|x_k|), which is I here.
  - `HVEN_R1_TRCAP=c` (Arm T): `tr_max = c·max(Δ_entry, restart radius)`.
  - `HVEN_R1_OMEGA=k` (Arm C): ω = k·max σ/‖∇f(x_start)‖∞, halved on every restoration major where h does not fall and is above feas_tol.
  - `HVEN_R1_OMEGA_NOHESS` and `HVEN_R1_OMEGA_STOPFEAS` (variants). `HVEN_R1_POSTRESTO_E2OFF` (probe).
  - E2-D4's `HVEN_D4_QPDUMP` and `HVEN_D4_TRACE`, ported to this pin; 4 trace hunks no longer anchor and were skipped.
  - Harness copy: `HVEN_R1_WALL=300` makes the solve stop itself after 300 s through an iteration callback, with no signal, standing in for the harness's 300 s watchdog. `HVEN_R1_TRACE=<file>` attaches a JSON-lines trace.
- **Arm 0 gate (observed).** 12 of 12 cells identical on all 111 compared columns, and re-proven after each later rebuild. The options digests are the declared a84eba3d and 1908c02c. The arms act through the environment, so the environment of every leg is in `runners/matrix*.txt`.
- **Box protocol (observed in `transcripts/legs.log`).**
  - Every leg ran under flock with no timeout, with the markers inside the lock, `MKL_NUM_THREADS=1`, `taskset -c 2`, one solver at a time.
  - A separate prior pgrep ran before each foreground batch and before each background chain. Inside a chain, each leg's in-lock pgrep is the check.
  - No signals. Every leg under 600 s; the longest were 418 s and 435 s.
  - The oracle ran inside a lock. Accelerate UNOBSERVED.

## 2. Arms × cells

"stop@300" means the solve stopped itself after 300 s; in the harness that row would be `dnf_wall`. Figures are the final status, then the restoration's h_out / f_out / |Δx|∞.

| cell | Arm 0 | P ζ=1e-2 / 1 / 1e2 | T c=1 / 10 | C as briefed | Cs (variant) |
|---|---|---|---|---|---|
| STEENBRB | numerical_error; 1.5e-12 / 2.97e12 / 1201 | infeasible ×3, no certificate; h 13 617 / 19 702 / 19 800 | stop@300 ×2; h 18 150 / 10 020 | numerical_error; first sub-QP caps at 6600, point never moves | numerical_error; **1.1e-10 / 4.35e4 / 2880**; first post-restoration QP numerical_error after 623 minors |
| STEENBRC | numerical_error; 2e-12 / 8.4e13 / 6412 | infeasible ×3 | stop@300 ×2 | numerical_error; caps at 7640 | stop@300; h 41 240 |
| STEENBRD | numerical_error; 1.5e-12 / 2.1e12 / 1201 | infeasible ×3 | stop@300 ×2 | numerical_error | stop@300; h 17 200 |
| STEENBRE | numerical_error; 2e-12 / 8.4e13 / 6412 | infeasible ×3 | stop@300 ×2 | numerical_error | stop@300; h 41 290 |
| STEENBRG | numerical_error; 2e-12 / 8.4e13 / 6412 | infeasible ×3 | stop@300 ×2 | numerical_error | stop@300; h 23 340 |
| YORKNET | numerical_error; h unmoved at 51 001 (4 capped QPs) | numerical_error / infeasible (h 3350) / infeasible (h 3906) | numerical_error, unmoved | numerical_error; h 15 860 | numerical_error, unmoved |

- **Cs:** f scaled by ω, in the restoration's value and gradient only (no f curvature); ω halved whenever h fails to fall; the sub-solve returns at the first feasible iterate. ω per cell: STEENBRB/D 2.9e-4, STEENBRC/E/G 1.8e-4, YORKNET 0.554.
- **T without the stop:** on STEENBRB, run past 300 s, it ends `max_iter` at 99 restoration majors. With c = 10 the objective reaches 8.6e11 after only 621 units of travel. A capped radius does not stop the objective blow-up.
- The full table is `tables.md`. Every row has 0 accepted steps. Arm P has 0 certified-infeasible rows of 18. Per-cell excursion figures (h_in → h_out, f_in → f_out, |Δx|∞, |∇f|∞ at the landing) are in `tables.md` §"Restoration excursions". Arm 0 excursions: STEENBRB/D h 19 800 → 1.5e-12, f 2859 / 2513 → 2.97e12 / 2.10e12, |Δx|∞ 1201, 11 majors; STEENBRC/E/G h 41 800 → 2e-12, f to 8.4e13, |Δx|∞ 6412, 14 majors; YORKNET 0 movement. Supplementary: CsOff (Cs on the E2-off label) — the restoration QPs cap and the phase ends with |Δx| = 0; Cs01 (ω ten times smaller) — STEENBRB h 15 710 and f 2.8e6 at 300 s, no better.

## 3. The QP side (B)

The first post-resume QPs after the Cs landing on STEENBRB are dumps `qp3`–`qp6`, at radius 0.1, 0.05, 0.025 and 0.0125.

- **In-cell and E2-on replay:** `numerical_error` after 623 / 220 / 210 / 205 minors, reproduced exactly, each ending with the walk out of moves.
- **E2-off replay:** optimal after 596 / 192 / 191 / 184 minors.
- **Oracle:** feasible; 36 negative Hessian eigenvalues; the Hessian is indefinite on the null space of the equalities, so there is no global oracle.
- **The trace (observed):** at an independent vertex (372 pins plus 96 equalities = n; normalized minimum singular value 0.067):
  - the walk drops pin 312 from its upper bound (multiplier +4.99e-3) onto a face with curvature −1.27e-6;
  - the ride along it is unbounded, and its first blocker is the same pin at step 0;
  - the pin is re-added, the only drop candidate is exempt, and the walk ends `numerical_error`.
- **Class:** not F1, A or G in E2-D4's terms. It is M4c's drop→re-add pattern with the negative-curvature signal, ending in E2's stuck-walk exit (the M4c mapping is inferred).
- **E2-off probe after the landing** (dumps `CsP_STEENBRB_qp3`/`qp6`): capped at radius ≥ 6.4 in a period-3 cycle (add pin 79, drop 95 or 302, drop 79); the working set is dependent: rank 467 of 468, normalized singular value 5.7e-15; a 40 000-minor cap ends at the same f; E2 on, the same QPs hit the same wrong-side stop as above.

## 4. Controls

- **YORKNET:** Arm 0, T and Cs: 4 restoration QPs all cap, and the point never moves. Adding curvature moves it: P ζ = 1 to h 3350 with 0 capped QPs; C as briefed to h 15 860. Never feasible. P at ζ ≥ 1 gives an uncertified `infeasible`.
- **Optimal five under Cs:** identical to re-take 6 on all 111 columns.
- **L9 (ZAMB2-8, E2 off; trace `transcripts/l9/ZAMB2-8off.trace.jsonl` rows seq 11–28):** the restoration sub-solve does not fail because the feasibility problem stalls or the clamped start is bad. It starts from the parent's projected point and works for four majors: all accepted, radius 0.5 → 4, the weighted violation falling 26.37 → 1.52. It fails because its own QPs stop finishing once the radius reaches 8: the QP at radius 8 caps at 2340 minors; the three at radius 4, 2 and 1 end `numerical_error`; the failure budget then ends the sub-solve. "h stays 1.52" is the wrapper's objective; the true h at the returned point is 3.22. L9 is the same QP failure (B) inside restoration, on the E2-off walk.

## 5. Observed vs inferred

- **Observed:** every number above, taken from the named transcripts.
- **Inferred:** the ≥ ~500 distance, from the SIF, not a computed projection; the "flow piles onto single arcs" reading; the M4c class mapping; why the direction picks the wrong side of the released bound (the engine's sign rule was not read).

## 6. Recommendation

- **P and T:** rejected on the merits. **C as briefed:** rejected because its first sub-QP caps.
- **Production target:** the walk's direction after a drop onto a negative-curvature face. Fixtures: `Cs_STEENBRB_qp3`–`qp6` (E2 on stuck, E2 off optimal). The dependent-set cycle: `CsP_STEENBRB_qp3`/`qp6`. Both are in the `tests/qp/fixtures/e2-d4` format that `qp_dump.h` loads.
- **Cs:** test-only. It needs the radius stall understood before it could be a default. Option surface if ever built: a restoration objective mode, default violation-only, plus a stop rule at first feasibility. A re-take of Cs alone would show 0 conversions, STEENBRB at f 4.3e4, and C/D/E/G at `dnf_wall` (a wall-time regression).

## 7. Could not determine

- Conversion after the QP fix. Probably not from that fix alone, since the E2-off probe caps at larger radii (inferred).
- Why Cs's radius stalls on STEENBRC/D/E/G. Why STEENBRC's first restoration QPs cap under Cs.
- The L9 capped QP (no dump on that leg).
- IPOPT/SNOPT agreement: nothing to check. Committed objectives: STEENBRB 9075.855, STEENBRD 9030.082, SNOPT STEENBRE 27458.82.

## 8. Layout

`REPORT.md`, `tables.md`, `r1-patch.diff.txt`, `options/`, `runners/` (incl. `build.sh`, `cell.sh`, `leg.sh`, `matrix*.txt`), `transcripts/{runs,screen,l9}`, `transcripts/legs.log`, `transcripts/chain-*.log`, `transcripts/summary*.txt`, `dumps/`, `replays/` (`*.res`, `*.oracle.*`, `traces.tar.gz`).

## Settler ruling (2026-09-25)

See `.superpowers/m7-w2-r1-brief.md` "Settler adjudication" (R1–R4).
