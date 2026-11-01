# M7 W2 R2 — the composite landing after NC1: STEENBRB, the radius stall, YORKNET's restoration QPs — DIAGNOSIS

Pin: hven m7 `cef86073` (sources `5732a1f3`, NC1 landed). Baseline rows: hven-bench `runs/2026-09-m7-w2-retake7/run-A-{default,independent-working-set-off}/cutest/` (read-only). Brief: `.superpowers/m7-w2-r2-brief.md`. Everything here is a scratch build. No production code moved, nothing was committed, nothing was re-pinned. (The diagnostic implementer delivered this text as its hand-back; the settler placed it here verbatim except for this preamble and the ruling at the end.)

## The result

1. **Q1 — STEENBRB does not convert with Cs + NC1 (observed).** The solve never lands.
   - NC1 changed STEENBRB's first elastic QP. Major 0 is now an accepted h-type step (h 19 800 → 19 694, f 2859 → 9183), so restoration starts at major 1 from a different point than in R1.
   - The Cs sub-solve runs 44 majors, then stops itself at 300 s with h 6499, f 30 561, |Δx|∞ 807. In the harness that row would be `dnf_wall`.
   - It stalls for two reasons: the f term's rejections (see Q2), and, from radius 16 up, its own restoration QPs failing.
   - Those QPs are LPs (H ≡ 0). The walk ends within 1e-5 of the LP optimum and cannot certify it. The flat-face undo fires 5–1312 times per QP (class in §2).
   - R1's landing at `c1f73065` (f 4.35e4) came from a trajectory that no longer occurs.
   - There is no optimum, so there is no objective check for B.
2. **Q2 — the stall has one observed cause: the ω·f term's linear model dominates the sub-solve's predicted decrease.**
   - The sub-QP has no f curvature, so it promises decreases the true f does not deliver.
   - Long steps are rejected because f explodes (ared_f as low as −3e9).
   - Short steps are accepted at ratio < 0.75, so the radius never grows.
   - The violation part is exact on these cells: pred_v = ared_v = 24 or 28 per unit radius.
   - STEENBRG is the clean case: 49 of 50 accepted majors have ratio 0.003–0.03, the radius stays at 2, and f swings between 4.4e5 and 9.9e5 with period 2.
   - Two defects in the Cs mechanics add to this. The ω halving fires on rejected majors. The halved ω never reaches the QP until a step is accepted, because the sub-iterate's gradient is stale.
   - **The variant (Hj)** judges acceptance on the violation part alone while ω > 0, with a guard that rejects any step that does not reduce it. It removes the stall: the radius doubles every major. But f is then unprotected:

     | cell | what Hj does |
     |---|---|
     | STEENBRD | lands in 14 majors and **converts: `optimal`, f 9144.7249, 49 majors, 185 s** |
     | STEENBRG | lands at f 2.7e5, then hits the wall |
     | STEENBRE | lands at f 8.1e12 (Arm 0's landing), then ends `numerical_error` |
     | STEENBRB, STEENBRC | f reaches 1e14–5e14; the ω·∇f term then turns the QP step against the violation, and the guard rejects every step; never feasible |

   - **Objective check for D (observed):** 9144.7249 against 9030.082 for the committed SNOPT and interior-point lanes. That is a different local minimum, 1.3 % higher, with harness stationarity 1.7e-8 and h 4.7e-15. IPOPT's committed row ends in error at 15 039.
3. **Q3 — YORKNET's restoration QPs are feasible and nonconvex, and they fail on flat faces (observed).**
   - H has 80 negative and 632–653 zero eigenvalues. Z'HZ on null(Ae) has λmin −0.35.
   - Every E2-on capped walk ends on a face where Z'HZ ≡ 0.
   - With E2 on: the negative-curvature gate fires on a numerically zero quotient (about −1e-33); the ride is vetoed and the drop undone (up to 3016 times per QP), or the walk exchange-cycles; the walk drifts off the equality rows (|Ae x − be| up to 5.17 at the cap).
   - With E2 off: a period-2 drop/re-add of one pin at zero motion.
   - 0 of 6 solve with E2 on, on either algebra. 1 of 6 solves with E2 off (qp6). The refactorize algebra gives the same walk as the border algebra on every dump.
4. **Q4 — controls (observed).** The 5 optimal cells under Cs are identical to re-take 7 on all 114 compared columns. None enters restoration. ZAMB2-8 on the E2-off label under Cs ends `numerical_error` with the restoration's h_r at 22.42 (the parent's final feas is 2.63). Its ω0 is 97.7, because ‖∇f‖∞ = 0.01. The ω rule makes the f term as strong as the violation whatever f's scale.
5. **Supplementary Cc: Cs with f's curvature in the sub-QP (observed).** It repairs the ratio: 0 accepted steps without growth. But 30 of 50 sub-majors end on a failed nonconvex QP, and 0 of 5 cells land (`numerical_error` on all 5, within 17–164 s). So the arm that addresses the cause is blocked by the QP engine, not by the landing.

**Recommendation:** do not build the production landing option from Cs or Hj. The next production round is the QP engine, for two items: **(i) the flat face** — the gate fires on zero curvature, and the undo or an exchange cycle follows; this affects YORKNET's restoration QPs and every Cs sub-QP on STEENBRB; **(ii) the saddle exit** — the walk stops at a saddle point of an indefinite face it reached by an ADD, with no drop candidate; this is where D's post-landing QPs end N3 (dumped and replayed); the curvature-carrying composite (Cc) is inferred to stop on the same QP failures (its sub-QPs were not dumped). The landing round comes after, built on Cc, the f-curvature form (§5).

## 1. How it was run

- **Scratch build** (`.scratch/m7w2r2/`): `git archive cef86073` + `dep/eigen` `bc3b3987` + `dep/fmt` `407c905e`; Release, clang, LTO off, tests and bench off (the re-take flags verbatim); installed into the harness copy's own pin prefix (`install-cef86073d4ff`); the stamp says SCRATCH.
- **Harness copy:** hven-bench `9af1910` via `git archive`, without `runs`, `.hven`, `build` or `.scratch`; `.scratch/h3/decoded` symlinked read-only; the lane is `hven_bench_cell.cpp` from `9af1910` (carries the NC1 schema event); the cell argv is `run_serial.py`'s.
- **Patch (`r2-patch.diff.txt`).** Everything env-gated. R1's patch re-applied: `sqp_solver.cpp` and `soc_elastic_restoration.cpp` did not move between `c1f73065` and `cef86073`, so every R1 driver and restoration hunk anchored unchanged. The D4 dump/trace hunks in `qp_engine.cpp`: 11 anchored; the STEP trace re-anchored onto NC1's `if (ride_blocked)` branch and now prints `ride_blocked`; the same 4 hunks R1 skipped are still skipped (the inertia repair, `select_exchange`, `verify_farkas`, `!within`). New for R2 under `HVEN_R1=1`: `R2JUDGE` prints every major's judged pair (pred/ared/ratio, the f-type switch, the funnel width) and, inside a composite restoration, splits pred and ared into the ω·f part and the violation part through a hook the parent installs around the sub-solve; `R2VERDICT` prints the final verdict. `HVEN_R2_HJUDGE=1` is the Q2 variant: inside a composite restoration the funnel judges (v_old, v_new, pred_v) instead of the composite; a step with v_new ≥ v_old is rejected (`VGUARD`); the radius-growth test reads the same judged pair. `qp_replay` prints the ride counters (`dep_ride_blockers`, `ride_exchanges`, `ride_declines_fallthrough_blocked` (= undos), `ride_sign_vetoes`).
- **Arm 0 gate (observed).** 12 of 12 cells identical to re-take 7 on all 114 compared columns (the 11 default cells and ZAMB2-8 on the off label); re-proven after the second build on STEENBRB, YORKNET, READING6, ACOPR57 and ZAMB2-8 off: 5 of 5. Options dumps hash to the declared `a84eba3d` (default) and `1908c02c` (off).
- **Box protocol (observed in `transcripts/legs.log` and `replay-*.log`).** A separate prior `pgrep -a hven_bench_cell` before every build and chain, empty; every leg under `flock /tmp/box-build.lock` with no timeout; PGREP_EMPTY / FOREGROUND_START / FOREGROUND_END inside the lock; 0 PGREP_NOT_EMPTY; `MKL_NUM_THREADS=1`, `taskset -c 2`, one solver at a time (replay legs took the same lock); 41 cell and build legs, longest 336 s (Hj_STEENBRG — the in-process stop only checks at history rows); replay legs ≤ 70 s; no signals. The 300 s in-process stop (`HVEN_R1_WALL`) stands in for the harness wall, so `interrupted` = `dnf_wall`.
- **Arms** (exact env per leg in `runners/matrix-*.txt`): Cs = `HVEN_R1_OMEGA=1 HVEN_R1_OMEGA_NOHESS=1 HVEN_R1_OMEGA_STOPFEAS=1` (k = 1, ω0 = k·max σ/‖∇f(x_start)‖∞); Hj = Cs + `HVEN_R2_HJUDGE=1`; Cc = `HVEN_R1_OMEGA=1 HVEN_R1_OMEGA_STOPFEAS=1` (f curvature in the sub-QP: R1's "C as briefed" plus the stop at first feasibility).

## 2. Q1 — the pair on STEENBRB

**Main loop (observed, `transcripts/runs/Cs_STEENBRB.err`):** major 0, Δ 1: infeasible → elastic, accepted (accept_h), h 19 800 → 19 694.4, f 2858.8 → 9183.4; major 1, Δ 1: infeasible → elastic N3 (348 minors) → restore at h 19 694.4, f 9183.4.

**The restoration sub-solve.** ω0 = 3.45e-4 (‖∇f‖∞ 2896). 44 majors, then the stop at 305 s (full per-major table in `tables.md` §"Cs STEENBRB"). Majors 0–13: mostly accepted, radius 1 → 16; the step at radius 8 rejected (ared_f −362 against pred_f 5740), and at 32 (f → 2.1e11). Majors 14–30: every step at radius ≥ 32 rejected on ared_f (f → 2.5e12 at 64, 8.3e13 at 512); ω halved 17 times, down to 2.6e-9. Majors 26–43: the sub-QPs at radius 16–128 fail — 3 end N3 (zero-motion exhausted, 219–251 minors) and 3 cap at 6600; the radius pinned at 8–16, v falls 96 per major. The stop: h 6499.2 (from 19 694.4), f 30 561, |Δx|∞ 807.

**Where it stops (the class).** The six failing sub-QPs are dumped (`dumps/Cs_STEENBRB/qp1–qp6`) and replayed on 4 configurations (`replays/cs_steenbrb/`). Each is an **LP** (H ≡ 0: NOHESS, and the rows are linear), feasible, with a Clarabel optimum; the walk ends within 2e-4 / 4e-5 / 8e-6 / 7e-6 of f* (qp1 / qp3 / qp4 / qp6); the residual is the ω·f tie-break (ω ≈ 1e-8 there, so the multipliers that drive the drops are of ω·‖∇f‖ scale).

| dump | Δ | E2 on (border) | E2 on (refactorize) | E2 off (border / refactorize) | undos (sign vetoes) |
|---|---:|---|---|---|---:|
| qp1 | 128 | N3 219 (zmexh) | N3 219 | **optimal** 221 | 1 (1) |
| qp2 | 16 | N3 251 | N3 251 | **optimal** 237 | 5 (5) |
| qp3 | 32 | cap 6600 | cap | cap | 1312 (1312) |
| qp4 | 16 | N3 250 | N3 250 | cap | 18 (18) |
| qp5 | 16 | cap | cap | cap | 23 (23) |
| qp6 | 16 | cap | cap | cap | 816 (816) |

**Class.** Not F1, A, B, G or C′, and not NC1's dependent ride blocker (0 on every dump). It is **NC1's refusal (ii) on a flat face**: on H ≡ 0 every drop passes the negative-curvature gate (`curvature <= curvature_tol` holds at 0); `ride_sign` then reads the slope as flat, and the drop is undone. NC1 registered this ("(ii)'s 'impossible' claim fails as coded (noise-level slopes)"). E2 off solves 2 of the 6 and caps the other 4. Objective check: none. Committed STEENBRB: 9075.855 (IPOPT, SNOPT's harness row, and both interior-point pins).

**Under Hj, STEENBRB also fails (observed).** 10 accepted majors at ratio 1: radius 1 → 512, v 19 694 → 5856; but f climbs to 5.2e14 (|∇f|∞ 3.7e16); at radius 1024 the QP's ω·∇f term outweighs the violation (pred_v = −6144); `VGUARD` rejects that step and the next 43; the halved ω never reaches the QP (the `quad` column carries the stale ω·∇f, 2.3e15); the sub-solve ends `numerical_error` at 54 majors; the cell ends `numerical_error` at f 5.2e14. **Under Cc, STEENBRB fails too (observed):** the first 4 sub-QPs end N3 (946 / 663 / 525 / 451 minors); the point never moves; the cell ends `numerical_error` in 17.5 s.

## 3. Q2 — why the radius stalls on C/D/E/G

**Observed rows (Cs, NC1 pin; `tables.md`):**

| cell | sub majors | accepted | accepted with ratio < 0.75 (no growth) | rejected on the f term | rejected on a QP failure | max radius | end |
|---|---:|---:|---:|---:|---:|---:|---|
| STEENBRB | 45 | 28 | 9 | 10 | 6 | 512 | stop@300, h 6499 |
| STEENBRC | 25 | 19 | 1 | 4 | 1 | 8192 | **lands** h 8.8e-11, f 60 802, |Δx|∞ 7671; then the wall at main major 4 |
| STEENBRD | 44 | 36 | 18 | 7 | 0 | 2048 | **lands** h 6.5e-11, f 23 224, |Δx|∞ 2862; the main loop takes 14 accepted majors, f → 11 413.8, then the wall at major 18 |
| STEENBRE | 43 | 37 | 29 | 5 | 0 | 16 | stop@300, h 41 800 → 38 508, f 7233 |
| STEENBRG | 51 | 50 | 49 | 0 | 0 | 2 | stop@300, h 41 800 → 39 056, f 4.6e5, |∇f|∞ 2.2e6 |

R1's picture at `c1f73065` was C/D/E/G all stalled at radius ≤ 1. On NC1, C and D now land inside the wall (C only just), and E and G still stall. The trajectories differ because NC1 changed the main loop's first elastic QPs: C now takes 3 accepted h-type majors before restoration.

**Why (observed).** The violation part is modelled exactly (pred_v = ared_v on every row, 24 (B/D) or 28 (C/E/G) per unit of radius). The ω·f part is modelled only linearly (no f curvature in the sub-QP). That linear promise, pred_f, dominates pred whenever the QP can choose a box vertex that the f gradient favours. STEENBRG sub majors 3–21: pred alternates 1777 / 3933, of which pred_f is 1721 / 3877 and pred_v is 56; ared is 5.8 / 106, so the ratio alternates 0.003 / 0.027; f swings 4.4e5 ↔ 9.9e5 with period 2, and v falls a fixed 56 per major. Each step passes the funnel's sufficient-decrease test (ratio ≥ 1e-4) and is accepted; the halving rule watches only v, so it never fires; each step fails the growth test (ratio ≥ 0.75), so the radius stays at 2 (about 750 majors to clear 41 800 of violation — inferred). Where a step reaches the steep part of f it is rejected instead (ared_f −3.1e5 to −2.9e9 on C at radius 32–256) and the radius halves. So the stall is the missing f curvature, in both forms. It is **not a filter or funnel rule that the composite violates** (every sub-row has the sub's own h ≤ 7e-11 and is f-type: 196 of 196 rows), and **not capped QPs on C/D/E/G** (0 QP failures there; on B the sub-QPs fail from radius 16 up, §2). Two defects in Cs's mechanics are also observed: **(a)** the halving fires on rejected majors (the iteration callback sees the unmoved iterate; on B, 17 halvings drive ω to 2.6e-9); **(b)** the halved ω does not reach the QP until a step is accepted (the sub-iterate's f and ∇f are re-evaluated only on acceptance; the `stale` column is non-zero after every halving; the `quad` column under Hj shows the QP still carrying the old ω·∇f).

**The variant, Hj** (acceptance on h alone while ω > 0, plus the v guard):

| cell | sub majors | h_out | f_out | |Δx|∞ | rejects (all `VGUARD`) | end status |
|---|---:|---:|---:|---:|---:|---|
| STEENBRB | 54 | 5856 | 5.2e14 | 512 | 44 | numerical_error |
| STEENBRC | 39 | 31 808 | 1.07e14 | 511 | 29 | stop@300 (dnf_wall) |
| STEENBRD | 15 | 5.8e-11 | 59 862.7 | 7123 | 2 | **optimal, f 9144.7249, 49 majors, 185 s** |
| STEENBRE | 16 | 1.1e-10 | 8.1e12 | 27 611 | 1 | numerical_error at major 29 (f 1.14e9, capped QPs) |
| STEENBRG | 26 | 1.1e-10 | 2.74e5 | 17 711 | 6 | stop@300 at main major 1 (the leg ended at 336 s) |

Hj removes the no-growth stall (every accepted step has ratio 1, the radius doubles) but nothing protects f: on E, f lands at Arm 0's 8.1e12; on B and C, f grows to 1e14–5e14 before feasibility, after which the linear ω·∇f term outweighs the violation and every step raises v and is rejected. **D converts:** lands at f 59 863; 9 post-landing main-loop QPs end N3 (8 at radius 0.00625–0.1, 1 at 409.6); the radius shrink and retry carry the solve through: f 59 863 → 9144.72, stationarity 1.7e-8, h 4.7e-15. Objective check (observed): 9144.7249 vs 9030.082 for SNOPT (committed harness row) and the interior-point `aeebdccc` lane; the other interior-point pin `3904a477` is `dnf_budget` at 9555.66; IPOPT is an error at 15 039.35. A different local minimum, 1.27 % above the committed one.

**D's post-landing N3 QPs** (`dumps/Hj_STEENBRD/qp1,qp4,qp9`, all nonconvex, λmin(H) = −250). **qp1 and qp9: a saddle point of an indefinite face reached by an add** — final face dim 5 with Z'HZ ∈ [−250, −75] (qp1), dim 13 with λmin −3.3e-3 (qp9); inertia verdict kWrong, no drop candidate, the walk takes the TERMINAL exit; the ride is armed only after a drop, so a negative-curvature face met by an add is never ridden; NC1's exchanges fired (1 and 7) and did not reach this; E2 off caps qp1 and ends N3 on qp9. **qp4: R1's (B) pattern outside NC1's gate** — a drop (pin 195, multiplier −0.45) onto a face the inertia test reads kSuspect; the EQP step is 1.9e9 and the ordinary step is blocked by the same pin at α = 0; the pin is re-added, drop-exempt, and NODROP ends it N3; the curvature quotient did not pass `curvature_tol`, so NC1's rule never engaged; E2 off caps.

**Supplementary Cc** (`tables.md` §Cc): the ratio test is repaired (0 of 20 accepted steps have ratio < 0.75, the radius doubles on every accept) but 30 of 50 sub-majors end on a failed QP (N3, plus one cap on E); B and D never move (4 N3 sub-QPs each); C, E and G end at h 38 674 / 41 268 / 41 716; **0 of 5 land.**

## 4. Q3 — YORKNET's restoration QPs (Arm 0)

The Arm 0 restoration sub-solve (`transcripts/runs/A0_YORKNET.err`) runs 9 majors: 2 accepted (h 51 001 → 28 968, |Δx|∞ 0.75); sub major 2's QP ends N3; sub major 3's second-order-correction re-solve caps; sub majors 5–8 (radius 0.5 → 0.0625) all cap at 7680. The cell ends `numerical_error`, identical to re-take 7. All six failing QPs were dumped and replayed (`dumps/A0_YORKNET`, `replays/yorknet2`): n/me/mi = 824/256/0; all **feasible** (phase-1 LP violation 0); all **nonconvex** (H has 80 negative and 632–653 zero eigenvalues; λmin(Z'HZ on null(Ae)) = −0.35); no global oracle.

| dump | context | E2 on, border (= in cell) | E2 on, refactorize | E2 off, border / refactorize | undos (sign vetoes) | E2-on final face: dim, Z'HZ | |Ae x − be| at the end (on) | class |
|---|---|---|---|---|---:|---|---:|---|
| qp1 | sub major 2, Δ 1 | N3 777 (zmexh), f −21 156.18 | same | cap 7680, period-2 drop/re-add of pin 119 at zero motion | 2 (0) | 1, ≡ 0 | 1e-13 | flat face: zero-motion exhaustion after 2 dependent ride blockers (both exchanged) |
| qp2 | sub major 3, SOC re-solve, Δ 0.5 | cap 7680, f −10 830.17 | same | cap, period 2 (pin 245) | 3016 (2585) | 3, ≡ 0 | 2.5e-3 | **flat-face declined ride**: the gate fires at curvature −1e-33, the EQP step is 2e8–9e9 (1/δ-sized), the ride is vetoed and the drop undone; the walk cycles over the drop candidates (period 28) |
| qp3 | sub major 5, Δ 0.5 | cap, f −10 552.28 | same | cap, period 2 (pin 157) | 0 | 17, ≡ 0 | 0.25 | exchange cycle on a flat face (period 5, 3771 exchanges), equality drift |
| qp4 | sub major 6, Δ 0.25 | cap, f −5838.63 | same | cap, period 2 (pin 134), |m| 1.4e8 | 1 (0) | 8, ≡ 0 | **5.17** | exchange cycle on a flat face (period 7, 2371 exchanges), equality drift |
| qp5 | sub major 7, Δ 0.125 | cap, f −3013.38 | same | cap, period 2 (pin 128) | 694 (1) | 24, λmin −1.4e-2 | 2e-14 | flat-face declined ride, mostly null-space vetoes (i) |
| qp6 | sub major 8, Δ 0.0625 | cap, f −1518.81 | same | **optimal 941, f −1530.2196** | 800 (0) | 16, ≡ 0 | 7.8e-3 | flat-face declined ride (null-space vetoes (i)); multipliers 4.7e7 at the tail drops |

E2 on: 0 of 6 on either algebra (the refactorize algebra reproduces the border walk on all 12 E2-on and E2-off replays: same status, minors and f; factorizations differ by 1). E2 off: 1 of 6 (qp6). **Class: "other" — the flat face:** the reduced Hessian on the walk's face is exactly zero (the restoration QP's slack block); the negative-curvature gate treats curvature 0 (numerically ±1e-33) as negative; the regularized EQP step is 1/δ-sized; NC1's refusals (i) and (ii) then undo the drop, or the add site exchange-cycles; with E2 on the walk also drifts off the equality rows. This refines R1's "4 capped QPs, 0 movement": at the retake-7 pin the sub-solve moves 0.75 before the caps. E2 off shows R1's (B) pattern instead: a drop onto the flat face, then the ordinary step is blocked by the same pin at step 0, repeating as a period-2 zero-motion cycle.

## 5. Recommendation for the production landing round

- **Do not build a composite-landing option surface in the next round.** Measured: Cs landed 2 of 5 (C, D, just inside 300 s), converted 0; Hj landed 3 of 5 (D, E, G), converted 1 (D, at a different local minimum); Cc landed 0 of 5. A default built from Cs or Hj would move STEENBRC/E/G from `numerical_error` to `dnf_wall` (a wall-time regression) and re-land E at 8.1e12.
- **The production target is the QP engine.** Two items, with fixtures in hand: **(1) the flat face** — the negative-curvature gate should not fire on a zero quotient; a zero-curvature face after a drop is an LP ray: step to the first blocker along the descent sign, or certify the direction unbounded, rather than undoing the drop (fixtures `dumps/A0_YORKNET/qp1–qp6`, E2-on stuck, qp6 E2-off optimal; `dumps/Cs_STEENBRB/qp1–qp6`, LPs with a Clarabel optimum; would convert nothing on its own — it is the precondition for any restoration carrying a linear objective term and for YORKNET's restoration); **(2) the saddle exit on an indefinite face reached by an add** — at a stationary point with kWrong inertia and no drop candidate, compute a negative-curvature direction of the face and ride it instead of ending N3 (fixtures `dumps/Hj_STEENBRD/qp1, qp9`; qp4 is the kSuspect-face variant of R1's (B); R1's C/Cc first sub-QPs are inferred to be the same kind — not replayed this round).
- **Then the landing round**, built on Cc (the f-curvature form, the arm whose ratio test works): option surface `restoration_objective ∈ {violation (default), composite}`; ω = k·max σ/max(‖∇f(x_start)‖∞, an f-scale floor) (ZAMB2-8 off gives ω0 = 97.7 without the floor); halve ω only on an ACCEPTED major whose v did not fall, and re-evaluate the sub-iterate's f and ∇f at the new ω (fixes (a) and (b)); return at the first iterate with h ≤ feas_tol; keep the composite ratio test (Hj shows f escaping when judging on v alone). Would convert with NC1 landed (inferred): STEENBRD (Hj already converts it at 9144.72) and possibly STEENBRC (Cs lands at 60 802 just before the wall) — both only once the two QP items stop the Cc sub-QPs failing; B, E, G unknown.
- **If a landing option must ship before the QP items:** Hj as an opt-in, never a default (measured: 1 conversion (D, 9144.72); E `numerical_error` at f 1.14e9; B `numerical_error`; C and G `dnf_wall`).

## 6. Observed vs inferred
- **Observed:** every number in §§2–4, from the named transcripts, dumps and replays; the Arm 0 identity; the landing and stall rows; the D conversion and its objective; the YORKNET table.
- **Inferred:** that a Cc landing would convert D and C once the QP items land; that the R1 → R2 trajectory change on B and C comes from NC1's change to the first elastic QPs (NC1's report records STEENBRB_def qp0 moving 658 → 653 minors); the ~750 majors for G (arithmetic); that the flat face and the add-reached saddle are what stop Cc (Cc's sub-QPs were not replayed); the kSuspect reading on D qp4 (from the PASS line's verdict field).

## 7. Could not determine
Whether B converts under any composite on this pin (none landed); what R1's Cs entry point would give on `cef86073` (that entry no longer occurs); the class of Cc's failing sub-QPs (`Cc_STEENBRB` wrote dumps, not replayed); the full split of YORKNET qp5/qp6's undos into (i) and (ii) (sign vetoes 1 of 694 and 0 of 800; the rest (i)); whether D's 9144.72 is where IPOPT would go from the same landing; Accelerate UNOBSERVED.

## 8. Layout
`REPORT.md`, `tables.md` (per-major sub-solve tables for Cs/Hj/Cc × B–G), `r2-patch.diff.txt`; `runners/` (build, cell, leg, run, chain, prepare, replay scripts; `instrument_r1.py`, `instrument_d4_r2.py`, `instrument_r2.py`; `subsum.py`, `compare_baseline.py`; `matrix-{a0,cs,hj,cc}.txt`; `qp_replay.cpp`); `options/`; `transcripts/` (`runs/*.out|.err`, `legs.log`, `summary.txt`, `chain-*.log`, `replay-*.log`, `sqp-traces.tar.gz`); `dumps/` (`A0_YORKNET/qp1–6`, `Cs_STEENBRB/qp1–6`, `Hj_STEENBRD/qp1,4,9`, E2-D4 format); `replays/` (`yorknet2/`, `cs_steenbrb/`, `hj_steenbrd/`; `traces.tar.gz`).

## Settler ruling (2026-09-25)
See `.superpowers/m7-w2-r2-brief.md` "Settler adjudication" (R5–R7).
