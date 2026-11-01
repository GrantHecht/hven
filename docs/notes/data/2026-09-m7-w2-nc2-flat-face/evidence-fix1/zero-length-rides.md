# NC2 fix round 1, item 1(a) — THE ZERO-LENGTH RIDE, measured on the walk seam

Tree: worktree `agent-a69f46e11796560b9` at `71f54c50` (= NC2 as landed), branch `nc2-fix1`,
sources UNCHANGED for this measurement. Builds `.scratch/nc2-build-{debug,release}`
(Debug / Release, `HVEN_FP_MODE=SAFER_FAST`, clang). Harness: a scratch gtest in
`hven_qp_walk_seam_tests` (`HVEN_TESTING`, so the seam's per-minor records exist), one solve per
(dump, algebra), `MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2`, every leg under
`flock /tmp/box-build.lock` with the PGREP/FOREGROUND markers. Dumps:
`docs/notes/data/2026-09-m7-w2-r2-composite-landing/dumps/Cs_STEENBRB/qp{1..6}.txt`
(`dims 660 96 0` — n = 660, me = 96, **mi = 0**; the `H` block is structurally present with
3276 entries and **every value is 0**, so these are LPs and `g(x) = Hx + g = g` exactly).
Raw transcripts: `probes/{rel-base,dbg-base}/<qp>_<alg>.{out,csv,seq}`.

A "ride" below is a pass whose action is `kRideStepped` (the seam's own label). "Zero-length"
is the ORDINARY ADD PATH's own test, negated: `!(dx_inf > step_tol)` with
`dx_inf = alpha * ||dir||inf` and `step_tol = feas_tol * max(1, ||x||inf)` — the two fields the
seam already records on a ride step.

## The table (OBSERVED; both algebras gave byte-identical summaries in every cell)

| config | dump | status | minors | rides | zero-length | % | moving | uphill rides | worst moving dx_inf |
|---|---|---|---|---|---|---|---|---|---|
| Release | qp1 | cap 6600 | 6600 | 2176 | **22** | 1.0% | 2154 | 1064 | 1.28e+02 |
| Release | qp5 | cap 6600 | 6600 | 2178 | **11** | 0.5% | 2167 | 1072 | 3.20e+01 |
| Release | qp6 | cap 6600 | 6600 | 2181 | **2144** | **98.3%** | 37 | 6 | 3.00e+01 |
| Debug | qp2 | cap 6600 | 6600 | 1641 | **1616** | **98.5%** | 25 | 1 | 2.55e+01 |
| Debug | qp3 | cap 6600 | 6600 | 2182 | **26** | 1.2% | 2156 | 1061 | 5.29e+01 |
| Release | qp2 | **opt** | 248 | 61 | 28 | 46% | 33 | 1 | 3.20e+01 |
| Release | qp3 | **opt** | 294 | 80 | 28 | 35% | 52 | 9 | 6.34e+01 |
| Release | qp4 | **opt** | 238 | 58 | 21 | 36% | 37 | 1 | 3.20e+01 |

(The last three rows are not in the brief's list; they are the CONVERGING Release cells, measured
because item 1(b) changes the point memory on every zero-length ride and they carry 21–28 of them.)

Ride totals reconcile with the corpus rows' `predsign` (2176 / 2178 / 2181 / 1640 / 2182 /
61 / 80 / 55–58): every ride on these faces takes the predicted sign.

**Zero-length rides DOMINATE in 2 of the 5 brief cells — Release qp6 (98.3%) and Debug qp2
(98.5%) — and are a 0.5–1.2% minority in the other three.**

## Per-cell: is f monotone across rides, and is there a period?

`f` is exact here: `H == 0`, so a ride's own contribution is
`df = alpha * sign * (g'p)` with `g'p` the seam's `ride_slope` (recorded at the pass's starting
iterate) and `sign` its `ride_sign_chosen`.

| config/dump | rides with df > 0 | sum of df | sum of the positive df | ws_sig_in period (last 400 passes) | distinct ws_sig_in | distinct x_inf | moving passes in last 120 |
|---|---|---|---|---|---|---|---|
| Rel qp1 | 1064 / 2154 moving | −4.87e-01 | +6.03e-03 | **6** | 3 | **1** | 40 |
| Rel qp5 | 1072 / 2167 | −3.31e-05 | +1.12e-04 | **6** | 3 | **1** | 40 |
| Rel qp6 | 6 / 37 | −8.40e-06 | +2.92e-06 | **6** | 3 | 2 | 40 |
| Dbg qp2 | 1 / 25 | −1.85e-04 | +9.91e-08 | **4** | 4 | **1** | **0** |
| Dbg qp3 | 1061 / 2156 | −1.41e-04 | +4.51e-04 | **6** | 3 | **1** | 40 |

**f is NOT monotone non-increasing across rides in any of the five.** In the three cells where
rides move (Rel qp1, Rel qp5, Dbg qp3) almost exactly HALF of them ascend: the sign came from the
drop's own multiplier, and on that ride the measured `g'p` — which on an LP is the exact
directional derivative, not an estimate — has the other sign. Over the last 600 rides of each,
the ascending and descending rides are 300/300 and the tails are exactly periodic.

**Every one of the five ends in an EXACT cycle**, not a slow certification. The tails repeat to
the last digit (same `ws_sig_in`, same `alpha`, same `dx_inf`, same `ride_slope`, one value of
`||x||inf` across 400 passes), on two constraints that trade places:

- Rel qp1, period 6: `drop 325 → ride(325→377, dx 1.235e2, df +5.56e-6) → moveonly(dx 3.34e1)
  → drop 377 → ride(377→325, dx 1.218e2, df −1.07e-4) → moveonly(dx 9.24e0)`.
- Rel qp5, period 6, the same shape on pins 8 ↔ 113 (dx 1.51e1 / 1.97e1, moveonly 4.20 / 4.60).
- Dbg qp3, period 6, pins 448 ↔ 371 (dx 1.60e1 / 1.55e1, moveonly 2.04e-1 / 5.00e-1).
- Rel qp6, period 6, pins 455 ↔ 298: the RIDE is zero-length (`dx_inf` exactly ±0.0) but the
  `moveonly` pass in the same period moves 7.164e-1 — **the cycle contains a moving pass**.
- Dbg qp2, period 4, pins 55 ↔ 400: `drop 55 → drop 400 → ride(400→55, dx 3.83e-13)
  → add(400, dx 3.83e-13)` — and **0 of the last 120 passes moved at all** (137 of 6600 in the
  whole solve). This is a pure zero-motion cycle at one point.

## What that means for item 1(b) (INFERRED, stated as the mechanism)

- **Dbg qp2 is exactly the hole the review named, and the fix closes it.** Per period the point
  memory should take 4 events (release 55, release 400, re-entry of 55 through the ride's blocker,
  re-entry of 400 through the add) and the iterate never moves, so nothing may clear it; the cap
  `2(n + mi) = 1320` must then fire inside ~1320 passes instead of the walk burning 6600. Today the
  ride's unconditional `pt_clear` zeroes the counter every fourth pass, and the ordinary add that
  follows sees a wiped `kPtReleased` flag, so it registers no re-entry either: the counter never
  passes 2.
- **The other four cells are NOT bounded by it, and no zero-motion rule can bound them.** Their
  period contains a pass that genuinely moves (a ride of dx 1.5e1…1.2e2, or — on Rel qp6 — an
  ordinary `moveonly` of 7.2e-1), so the point memory is legitimately cleared inside the cycle.
  They are CYCLING PROPER WITH MOTION: x returns to itself exactly, and §4c's finiteness argument
  ("a ride's objective decrease is strict whenever alpha > 0") does not hold, because under the
  predicted-sign rule a moving ride may ascend the exact slope. That is a finding about NC2's rule,
  not about the `pt_clear`, and it is registered rather than fixed here.
