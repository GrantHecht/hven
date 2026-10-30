# M7 W2 A7: the `ws_algebra` selection rule, the round's measurements and verdict

Branch `w2-a7`, off `m7` `f424c8db`. The protocol is
`docs/notes/2026-10-m7-w2-a7-ws-algebra-design.md` §4, **pre-registered at `c7359f3d`**, before any
measurement. No criterion was edited. Commits:

| commit | what |
|---|---|
| `c7359f3d` | design page + protocol (PRE-REGISTRATION) |
| `3ef44544` | `SqpOptions::ws_algebra_rule {kFixed, kMeasuredCost}`, counters, tests, corpus lever. **The measured head**: TRAIN pin, U0 binary, suites |
| `c673dff2` | tests only: F-4 reordered so mutation survivor M16 is killed |

**Toolchain stamp: OLD.** Everything here was measured on 2026-10-23/24, before the box's
2026-10-27 system update (which moved MKL to intel-oneapi-mkl 2026.2.0-881 and probably LLVM). The
old stamp reads clang 22.1.8 (Fedora 22.1.8-4.fc44), with MKL as installed before the update; the
round did not record the MKL version. No leg was re-run after the update, so nothing here mixes
toolchains. On the new toolchain these values carry over as a prediction, not as an identity
baseline.

Box terms throughout: one solver at a time, `MKL_NUM_THREADS=1`, every leg its own
`flock /tmp/box-build.lock` hold (the TRAIN legs ran through the harness `scripts/leg.sh`, with its
in-lock gate), a separate `pgrep -a hven_bench_cell` before each leg, and ctest at `-j4` under
`setsid`. Counters are asserted and wall-clock is informational only. Apple/Accelerate is
**UNOBSERVED**. HOLDOUT was never run.

## Verdict per clause: predicted vs measured

| clause | predicted | measured | evidence |
|---|---|---|---|
| G1 U0 OFF exact vs SCW gates record | 0/0/0 | **0/0/0** (27 cells × 75 columns × 3 engines) | `u0-off/` |
| G2 suites | green | **Release 3211/3211, Debug 3210/3210, long 1/1** (at 3ef44544). After c673dff2 the `WsAlgebraRule*` suite is 9/9 in both configs | `suites/` |
| G3 OFF counters 0 | PASS | **PASS**: 0 on every OFF test, and no OFF TRAIN cell fired (`train/T2-fire-and-identity.txt`) | |
| G4 mutants | all killed or equivalent | **20 mutants: 16 killed, 4 equivalent** (M6, M6b, M15, M15b: each removes ONE of two redundant gates; the gate pairs removed together, M6c and M15c, are killed). M16 survived pass 1, and the strengthened F-4 test kills it | `mutants/pass{1,2,3}.txt` |
| F-1 a churning border fires once, fewer factorizations | PASS | **PASS** on HS45 at `schur_cond_max = 1`: 26 → 22 factorizations, same status, same x | test + `exploration/` |
| F-2 inert until it fires | PASS | **PASS**: every shipped-default HS problem is bit-identical | test |
| F-3 inert on a configured refactorize | PASS | **PASS** | test |
| F-4 per solve | PASS | **PASS** | test |
| F-5 validation and dump | PASS | **PASS** | test |
| U-1 U0 ON exact, fires nowhere | PASS | **PASS**: 0/0/0, 0 fire lines | `u0-on/` |
| T-0 controls reproduce RETAKE18 | PASS | **PASS**: `default` 0/23 differ, `sqp-qn` 0/23 differ | `train/T0-*` |
| T-1 determinism, run A vs run B | N/N/0 | **N/N/0** for all 4 labels × 2 families | `train/determinism-*` |
| T-2 inert until it fires (TRAIN) | PASS | **PASS**: 0 violations; quiet cells hs 21+21, cutest 5 (exact) + 8 (qn) | `train/T2-fire-and-identity.txt` |
| T-3 `sqp-wsrule` vs `default`, hs | TIE | **TIE** (21/21, fires on no cell) | `train/decisions-tree-exact.txt` |
| **T-4 `sqp-wsrule` vs `default`, cutest** | WIN, low confidence | **LOSE: errors 2 > 1** (and median factorizations per major 206.6 > 191.3) | same |
| T-5 `sqp-qn-wsrule` vs `sqp-qn`, hs | TIE | **TIE** | `train/decisions-tree-qn.txt` |
| T-6 `sqp-qn-wsrule` vs `sqp-qn`, cutest | TIE | **TIE** (8/8, median 1.462 both) | same |

Predicted fire sets (scored, not criteria). `default` cutest was predicted to include {ACOPP118,
ACOPR118, NET3, READING6, STEENBRB/C/D/E/G, YORKNET}. All 10 fired, plus 3 the record did not
predict: ACOPP57, ACOPR57 (early transients) and BRITGAS. `sqp-qn` cutest was predicted to include
STEENBR×5. All 5 fired, plus BRITGAS, NET3 and SAWPATH. hs was predicted to fire nowhere, and it
fired nowhere.

## Reading the cutest loss (`train/percell-exact-cutest.md`)

* **BRITGAS decides clause 2.** Under `default` it is `dnf_wall` in the scored run: it reaches
  `optimal` at about 376 s solo, which is above the 300 s ceiling. Under the rule it ends
  `numerical_error` after 1 major and 81 379 factorizations. The rule switched twice; switches
  fold in from restoration sub-solves, and the round did not record which solve each came from. That gives errors 2 against 1, and the label
  cannot win.
* **The median rises, so the cost clause fails as well.** Firing on an early transient made the
  ACOP family more expensive in factorizations per major: ACOPP57 16.4 → 63.4, ACOPR57 101 → 172,
  ACOPR118 592 → 823. ACOPP118 got cheaper (1421 → 630, 8 → 5 majors), and so did READING6
  (71 → 59). The STEENBR family moved by less than 1%, with every objective unchanged to the
  printed digits. This is the witness bias the design page named in §2: factorizations that both
  algebras pay are in F and not in C, so the rule fires where `kRefactorize` is not actually
  cheaper. The development exploration shows the same thing on fixtures
  (`exploration/hs-churn-configs.txt`). A configured `kRefactorize` pays more than
  `minor_iters + 1` on HS33 and HS45. At `schur_cap = 0` the switch made HS06 and HS07 worse
  (23 → 27, 29 → 34). Under the forced conditioning guard, HS33 tied 31 = 31 and HS45 improved
  26 → 22, which is the F-1 fixture.
* **Wall-clock is informational and decides nothing.** The fired STEENBR cells ran about 25% faster
  with factorization counts unchanged within 1%: the refactorize algebra factorizes the smaller
  bound-eliminated K. The A4 currency counts factorizations, not their size. That is registered
  below, not used.

## Default recommendation

**Keep `kFixed`.** The pre-registered recommendation needed a WIN on one family and a lose on
none, and T-4 is a lose. The rule as built is a measured negative: it should not ship ON, and the
default is not moved. Whether the OFF field stays in the tree is the settler's call. On the A1
precedent, I recommend the field **does not land**: there is no win, and it adds a surface.

## Registered

* **A7-R4**: the A4 cost currency counts factorizations, not their size. For an algebra choice
  between a full-variable K0 and a bound-eliminated K, that is not a neutral measure. Any rerun of
  this question needs a size-aware cost or a serial-wall clause, pre-registered.
* **A7-R3** (design page): a witness that subtracts the factorizations both algebras pay. The ACOP
  transients and the fixture data say this is what failed.
* A7-R1 and A7-R2 as on the design page.
* `docs/counters.md`'s group table (the "50 direct" `SqpCounters` count) was already stale before
  this round, and the round did not touch it.
* The A2 branch (`w2-a2`) also appends fields to `sqp_solver_types.h`, `solver_counters.h`, the
  options dump and the two golden trace lines. A merge of both branches re-derives those pins
  (field counts, sizeof, golden literals) once.

## What is here

`u0-off/`, `u0-on/`: the replay CSVs and comparisons. `train/`: merged run A and run B per label
and set (`merged/`), the two decision trees, determinism, controls, T-2 and the fire log, per-cell
tables, the label dumps (each A7 label differs from its base by the one `ws_algebra_rule` line), the
chain summary, and `bench-clone-82cae23.patch`, the scratch hven-bench clone's only commit (pin
`3ef44544`, two labels, the `A7_FIRE_LOG` hook, never pushed). `mutants/`, `suites/`,
`exploration/` (development runs, NOT protocol data), and `scripts/`, every script the round ran.
