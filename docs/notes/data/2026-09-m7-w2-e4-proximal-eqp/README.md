# M7 W2 E4 (the proximal EQP) + fix rounds 1 and 2 — the task record

> Base `84ea0385` (the E3 docs record) → head **`e212a308`** (fix round 4b). **Six rounds, FOUR
> code commits on `m7`**; landed commits are never rewritten, so each fix round after the squash is
> a new commit on top.
>
> **THE GATES RAN TWICE, AND BOTH RUNS ARE ON RECORD.** Run 1 (declaration **`13013b4c`**, range
> `84ea0385..0d519a2b`) returned **G1–G5 GREEN, G6 FAIL** — see *Run 1* below. Run 2 (declaration
> **this commit**, range `84ea0385..e212a308`) is the SUCCESSOR declaration after fix rounds 4 and
> 4b, with every value re-derived and re-measured on the new pair. **Run 1's files are byte-frozen
> and were not edited**: `psym-derivation-e4.txt`'s own RULE forbids adjusting a declaration after
> its chain's first leg, so run 2 lives in parallel `*-r2` files and a reader can see which chain
> measured what.

## The range

| | |
|---|---|
| **BEFORE** | `84ea0385` — `docs(m7): W2 E3 -- the flat-face factor, CLOSED as measurement …` (branch `m7`). Its sources **are** NC2 fix round 1's gated sources: `git diff --stat 85dc01ac 84ea0385 -- include src tests cmake` is EMPTY and so is the same diff against `0fa59cdc`, NC2's gated head (both OBSERVED) |
| **HEAD** | **`e212a308`** — fix round 4b, the cherry-pick of `84fb520e`; its parent `5756530e` is fix round 4, the cherry-pick of `a8184985`. OBSERVED, both empty — each cherry-pick preserved every source byte: `git diff --stat a8184985 5756530e -- include src tests bench cmake scripts docs/build.md` and `git diff --stat 84fb520e e212a308 -- include src tests bench cmake scripts`. Earlier in the chain `0d519a2b` (fix 3) and `25e11280` (the squash of rounds 0/1/2) are likewise source-identical to their worktree originals |
| range (run 2) | `84ea0385..e212a308`. It contains run 1's own declaration commit `13013b4c`, which adds only `docs/notes/data/` and is therefore invisible to every derivation. The runner re-derives the changed surface from git at run time, so it cannot silently disagree with the record |
| range (run 1) | `84ea0385..0d519a2b`, declaration `13013b4c` — frozen |
| origin | **E3** (`84ea0385`), which CLOSED the flat-face factor as a measurement: the ordinary step ascends on a flat face by the absolute centring's own term, the equality drift is the factor's solve rather than the model, and E3's proximal pair converted all six `Cs_STEENBRB` cells and zeroed the drift but broke termination and the runaway guard — which are in the wrong currency for a 1/δ-sized step. E4 is that pair rebuilt with the walk's own tests in the walk's currency, plus the EQP-site residual check. E3's arm lived on branch `e3` and was never merged |

## The six rounds, four code commits

| round | worktree commit | what it is | Gates |
|---|---|---|---|
| 0 | `c46485d8` | the proximal EQP with the walk's tests in its currency, and the EQP-site residual check | gated only cumulatively, as part of the squash — see the → row |
| fix 1 | `f2226c28` | the same, plus the UNREGULARIZED POLISH at both sites and the TERMINAL POLISH at every certified exit | gated only cumulatively, as part of the squash — see the → row |
| fix 2 | `799b527d` | the step's **ROW-DRIFT CHECK**, the exhausted ladder made **ADVISORY** for both refusals, the terminal polish's acceptance given a **ROUNDING MARGIN**, the pathology fixtures' **DECLARED RENAME EVENT**, and the DALLASM certificates **VERIFIED TRUE** | gated only cumulatively, as part of the squash — see the → row |
| → | **`25e11280`** | the cherry-pick of `799b527d` onto `m7`. The three worktree rounds are squashed into it | run 1 `gates-run1/` G1–G5 GREEN / G6 FAIL (13013b4c); run 2 `gates/` GREEN at bba2fba6 (record this commit) |
| **fix 3** | **`0d519a2b`** (cherry-pick of **`5ed7bf16`**) | **settler rulings E4-27 / E4-28**, and it moved **nothing measurable**. The contract text aligned to E4-4-as-amended (the exhausted ladder is ADVISORY and the TERMINAL SITE certifies) at the six review sites plus two the implementer found; the STEP-currency runaway guard taken out of the contract text at both sites, with one clause recording that it was built, measured and removed; `polish_terminal_point`'s declaration reconciled with ruling E4-17; two stale narrations and the settlement prose in `.cpp` comments trimmed (the header-comment discipline). **ONE code change**: `adopt_polished_candidate` additionally requires the candidate to pass the walk's own KKT-point test (`after.stationarity <= opts.opt_tol` on the relative `TerminalKkt.stationarity`, NaN rejecting) — a contract closure, since a candidate up to 4× the incumbent's stationarity could otherwise be exported `kOptimal` from a walk that certified at `<= opt_tol`. **NO MOVER**: corpus 0 rows changed vs fix 2 on both algebras and every column, U0's whole comparison block byte-identical to fix 2's, census/HS/B1Gate 38/38, Release 0/2870, Debug 0/2869 + the four declared skips (both suites identical to fix 2 test by test), the long test alone 619.78 s, clang-format clean. 7 files, every one already in the range, **none under `tests/`** — which is why the rename event's by-name sets and the suite counts are unchanged, and were nevertheless RE-DERIVED on the final range rather than assumed (`REPORT-fix3.md`) | run 1 `gates-run1/` G1–G5 GREEN / G6 FAIL (13013b4c); run 2 `gates/` GREEN at bba2fba6 (record this commit) |
| **fix 4** | **`5756530e`** (cherry-pick of **`a8184985`**) | **settler rulings E4-31 / E4-32** — the round that closed run 1's G6. **The mechanism in one clause: a stale GCC `.gch` replayed by ccache after a comment-only header edit.** A GCC precompiled header records no input validation — what stops a consuming TU re-reading a `#pragma once` header already inside it is a table of (size, md5) pairs baked in at build time, which GCC never re-checks against the files on disk — and ccache's preprocessor-mode key is the preprocessed text, which a comment-only, line-count-preserving edit leaves byte-identical. So a `.gch` stale for `core/solver_status.h` was served, every consuming TU re-parsed it, and `src/drivers/ipm_solver.cpp` died on "multiple definition of enum class `hven::solvers::QpStatus`". **No engine change**: the fix guards `target_precompile_headers` on `CMAKE_CXX_COMPILER_ID MATCHES "Clang"` (`src/CMakeLists.txt`), plus `docs/build.md` and `scripts/check_pch_neutrality.sh`. **No C++ at all**, so no class A or class B mover — measured, not assumed | run 1 `gates-run1/` G1–G5 GREEN / G6 FAIL (13013b4c) — the round this failure diagnosed; run 2 `gates/` GREEN at bba2fba6 (record this commit) |
| **fix 4b** | **`e212a308`** (cherry-pick of **`84fb520e`**) | test-only. The suite's own C++ `residual_columns_agree` gets the **1e-13 absolute floor** it was missing, so the shared corpus gate is the whole rule the suite and both comparators already state (1e-5 relative **with** the floor) rather than its relative half; its contract test's literals move with it. `scripts/compare_replay.py` and `scripts/census_compare.py` get one citation sentence each — **comment-only, and proven so**: `compare_replay.py`'s AST with docstrings stripped is identical across the edit and its output on run 1's own replay CSVs is byte-identical including the exit code, so the comparator that produces G4's set cannot have moved it. `libhven.a` byte-identical (sha256 `57c458e0…`), so **no mover** here either | run 2 `gates/` GREEN at bba2fba6 (record this commit) — the first head this round's own gates ran at; not in run 1's range |
| **fix round 5 (diagnosis, no commit)** | — | **the ACOPR118 RETAKE9 regression, diagnosed.** NOT an E4 defect: the four failing sub-QPs fail identically on the pre-E4 pin. Root cause **R1** in the E2 lane — an undecidable dependency verdict with rho never computed, read as dependent, a deferred add, then a kWrong dead end on an indefinite face; E2 off solves the same QP in 682 minors. **R2** driver-side. **R3** the E4 cost profile. **E4-36**: E4 did not cause the regression and there is no E4 defect to fix. **E4-37**: E4 is carried forward, ACOPR118 a known loss; next E2 fix 7, then NC3 (rulings in `.superpowers/m7-w2-e4-brief.md` §10). Report `REPORT-fix5.md`, evidence `evidence-fix5/` | not gated — diagnosis only, no source/test/build change |

Reports and the review, copied verbatim (`cmp`-checked body for body against `.superpowers/`):
`REPORT.md` (round 0), `REPORT-fix1.md`, `REPORT-fix2.md`, `REPORT-fix3.md`, `REPORT-fix4.md`
(rounds 4 and 4b together), and `REVIEW-fable.md` — the Fable review of `25e11280`, which is what
returned fix round 3. Each carries a three-line
preamble naming its source and nothing else is edited. The brief and its settler rulings
**E4-1 – E4-30** live in `.superpowers/m7-w2-e4-brief.md`; §4 holds the rulings, of which
**E4-23** is the ship rule, **E4-24** the acceptance of the fix-2 head, **E4-25** the declared
events, **E4-26** the register, **E4-27** the amended E4-4, **E4-28** fix round 3's seven items,
**E4-29** the ledger corrections and **E4-30** the acceptance of `25e11280 + 0d519a2b` with the
gates on `84ea0385..HEAD`. **No separate Fable re-check of fix 3**: the code change is one guard
line with a measured empty footprint and the review's other items were text (E4-30).

## THE DECLARED EVENTS (settler ruling E4-25)

| | the event | status |
|---|---|---|
| **(a)** | **THE FIXTURE RENAME EVENT.** 24 TEST instances REMOVED, 32 ADDED, net +8. Twenty-four of the thirty-two are renames of the twenty-four removed — after E4 **no cell in this tree still cycles**, so the anti-cycling and degeneracy arm fixtures no longer describe cycling cells — and the remaining eight are `QpE4ProximalEqp`'s own new cases | **DERIVED AND CHECKED.** Both lists are in the runner **by name**; every one of the 56 names is added/removed by exactly one `TEST(`/`TEST_F(` line in the range (counted with `grep -c`, never `grep -q`), no name is both, neither list has a duplicate, every ADDED name is absent from the baseline and every REMOVED name is present in it. Suites: Debug 2861 − 24 + 32 = **2869**, Release 2862 − 24 + 32 = **2870** |
| **(b)** | **G4 = 43 DECLARED DIFFERENCES against the M6 pin, not 0** — the first non-zero G4 declaration since U0 itself. walk 17, ssn 13, ipm 13; 5 integer `qp_minors` 2 → 3, 37 residual columns strictly better, **one worse** (`ipm` / `f7_n1000_path_warm` / `kkt_stationarity`, 1.3e-05 relative), which is in the list on the same terms as the other 42 | **MEASURED ON THE FINAL SOURCES AND FALSIFIED.** The gate became *"the measured difference SET equals the declared list"*, compared order-free as normalized `(engine, cell, column)` triples, with the 1e-5 residual gate still applying to every cell **not** in the list. **Re-measured from my own Release build of `0d519a2b`** (`wrappers-gates/rehearsal/rehearsal-g4-comparison.txt`): walk 17 / ssn 13 / ipm 13, and `--check-g4` on that transcript returns rc 0 — the set equals the declared list. Three independent replay runs of these sources now agree on the same 43 coordinates (fix 2's, fix 3's, mine). Falsified with two cases (`preflight.txt` transcript C), including the one a declared *count* cannot catch: same count, one coordinate moved. The EXACT arm against the previous gate record is 88 / 81 / 85 — recorded, not asserted, because E4 moves the replay by construction |
| **(c)** | the accounting contract E4-17 (`terminal_polish_solves` is not inside `factorizations`) | stated in `REPORT-fix2.md` §7; not a gate value |
| **(d)** | **P-SYM.** `solver_counters.h` and `qp_types.h` both change (six headers in all), so nearly every object in the capture universe moves; the DEBUG_ONLY and section shape differ from NC2's | **MEASURED on BOTH final pairs — run 2's numbers below are its own rehearsal's, and run 1's came out the same, which is a comparison and not the derivation.** both configurations, by a fresh rehearsal of the runner's own `g5` (one box-lock hold) plus the offline re-classification at the measured counts (rc 0 in both). **Release DIFFERING 69 = class A 30 + class B 39**, control 78; **Debug 70 = 30 + 40**, control 61; UNEXPLAINED **0** in both; `after = before = 151` in both; `ONLY-AFTER []`. The two class A sets are **identical** (`cmp` rc 0) and equal the in-universe MODIFIED `.cpp` set exactly. Sections: 44 objects, byte-identical 35 / 30, **DEBUG_ONLY 0 / 5**, **nine** non-debug movers — the same nine in both — REFUSED 0. NC2's 38 = 1 + 37 and its DEBUG_ONLY 0 / 6 are nowhere reused |
| **(d′)** | **the one configuration-asymmetric class B object**, which the carried check could not admit | `tests/drivers/.../test_solve_status.cpp.o` is class B in Debug and **byte-identical in Release** — no verdict line in the Release compare, named in the Release negative control — while Debug differs on exactly one symbol (`RestorationSubStatus_TheMapIsTotal…::TestBody()`, 1363 → 1370 lines). The E2 rounds' admission required **NOISE-ONLY** in Release, a *narrower* fact than byte-identical, so it would have refused a correct measurement. Widened here to *"not a mover in Release — NOISE-ONLY **or** in the negative control — with which of the two named per object"*, plus a new fail-closed assertion that the negative-control listing is **complete** (the classifier prints at most 80; Release's is 78, checked). Nothing else relaxes: declared by name, the set difference exactly the declared list with nothing Release-only, positively found in one of the two non-mover classes, the cross-config copy removing exactly the declared lines, UNEXPLAINED 0 both. Falsified offline in three runs — the measured declaration rc 0, a Release mover rc 1, a non-asymmetric object rc 1 |
| **(e)** | the control lane MOVING at re-take 9 (E2-off changes on 25/51 corpus cells, three certified cells lost, named) | `REPORT-fix2.md` §5c and E4-26; a harness-lane item, not a gate value |

## The structural changes to the copied runner

**Run 1: `bench/` enters every source derivation.**
`bench/bench_scale.cpp` is MODIFIED in this range and its object is in the P-SYM capture
universe (`scripts/psym_compare.sh`'s `PSYM_GROUPS` names `hven_sqp_bench`). NC2 derived
`CHANGED_SOURCES`, `MODIFIED_CXX` and `ADDED_CXX` over `src/ tests/` only — correct for a range
that touched no bench source, wrong here: the classifier keys class A on the changed-source list
**by basename**, so `bench_scale.cpp.o` would have been read as a class B includer whose source
is untouched, and the pre-flight's own *"no declared class B object may have a source MODIFIED in
the range"* check would have refused it. The clang-format set gains `bench/*.cpp` for the same
reason. Derived, not carried.

**Run 1: the E2 rounds' class-B asymmetry admission, widened** — see the `(d′)` row above.

**Run 2: one new fail-closed refusal, and an honest note about it.** Fix round 4's own report
flagged that G6 passes the derived changed-C++ set to `clang-format --dry-run -Werror`, and
clang-format with **no file argument reads stdin** — so a range with no C++ change would make that
leg *hang* rather than pass or fail. The hazard is real (confirmed directly: no file plus a
readable stdin keeps reading, `timeout` rc 124) and the r2 runner now refuses on an empty derived
set before any leg. **But it is unreachable through this runner as it stands, and that is said
rather than dressed up as a falsification**: a non-empty changed-header set is itself in the format
set, and the pre-existing *"no changed header under `include/`"* refusal fires first on any
build-only range — checked on `13013b4c..5756530e` (fix 4 alone: 0 changed headers, 0 format
files). The predicate was tested in isolation, not end to end. It earns its place as defence in
depth if that header refusal is ever relaxed; this range has 40 format files, so it is inert here.

**Run 2 changes nothing else.** The emitted `g1`, `g2`, `g3`, `g5` and `g6` segments are
byte-identical to run 1's; only `g4`'s label line differs.

## Run 2 — what its own rehearsal measured

Re-derived and re-measured on `84ea0385..e212a308`; **nothing carried from run 1.**

| | run 2, MEASURED | run 1, for comparison |
|---|---|---|
| §1 paths (excl. `docs/notes/data/`) | **49** = run 1's 44 + `src/CMakeLists.txt`, `docs/build.md`, `scripts/check_pch_neutrality.sh`, `scripts/compare_replay.py`, `scripts/census_compare.py` | 44 |
| headers / MODIFIED `.cpp` / ADDED `.cpp` | **6 / 33 / 0** — the 33 are the SAME 33 path for path (`diff` of the two listings empty) | 6 / 33 / 0 |
| outside every universe | **5**: the two `docs/`, the three `scripts/`. `src/CMakeLists.txt` is under `src/` so it *does* enter `CHANGED_SOURCES` (38 entries vs run 1's 37) and is harmless there by mechanism: class A compares an **object's** basename less `.o`, and no object is named `CMakeLists.txt` | — |
| by-name | **32 ADDED / 24 REMOVED**, re-derived and `cmp`-identical to the runner's literals; fix 4b moves **no** TEST line (0 added, 0 removed over `5756530e..e212a308`) | same |
| suites | **2869 / 2870** | same |
| P-SYM Release | **69 = A 30 + B 39**, UNEXPLAINED 0, control 78 | same |
| P-SYM Debug | **70 = A 30 + B 40**, UNEXPLAINED 0, control 61 | same |
| sections | 44 obj; byte-identical **35 / 30**; DEBUG_ONLY **0 / 5**; **9** non-debug movers, the same nine in both; REFUSED 0 | same |
| G4 | **43 = walk 17 / ssn 13 / ipm 13**, `--check-g4` rc 0 on run 2's own offline transcript | same |

**Two things could have moved the G4 set and neither did, measured not assumed.** Fix 4b edits the
comparator that produces it — proven behaviourally unchanged (AST identical modulo docstrings;
byte-identical output on run 1's own CSVs). Fix 4 changes how the library is built — and the PCH
guard does not reach these builds: run 2's own configure log reads *"The CXX compiler
identification is Clang 22.1.8"* and its build log builds `CMakeFiles/hven.dir/cmake_pch.hxx.pch`,
so the Clang-only guard leaves the PCH **on** for every P-SYM build.

**And the strongest reading, which run 1 could not make:** run 2's replay CSVs are **byte-exactly
identical to run 1's own gate CSVs** — exact string equality, no gate, **0 differences on all 75
columns on all three engines**. Two builds, two processes, two source trees, not one digit moved.
The 43 are not merely the same count but the same cells at the same values, and the thin `ipm`
cell has now been a difference in **five** independent replay runs of these sources.

## Run 1 — what it measured, and why there is a run 2

Run 1's chain ran from `CHAIN_START 11:16:02` to `CHAIN_END 12:23:26` at HEAD `13013b4c`,
`dirty-files=0`.

| | |
|---|---|
| `GATES_SUMMARY` | `G1=0 G2=0 G3=0 G4=0 G5=0 G6=1` |
| `GATES_AGGREGATE` | `rc=1 over [0 0 0 0 0 1]` |
| `CHAIN_SUMMARY` | `gates=1 fold_proofs=0` |
| fold proofs | **46 / 0** |
| G4 | measured the declared **43**-difference set **exactly** |
| G5 | class A **30** + class B **39** (Release) / **40** (Debug), **UNEXPLAINED 0** in both |
| G6, sub-gate by sub-gate | `INSTALL_SMOKE_RC=1`; `EXPORT_CONTRACT_RC=1` **as a consequence** (the install never happened, so there was no `hvenTargets.cmake` to check); `CLANG_FORMAT_RC=0`; `DIFF_CHECK_RC=0`; `DIRTY_FILES=0`; `G6_ASSERT_RC=1` |

**Declaration `13013b4c` stands.** Nothing in it was wrong: every value it declared, G1–G5
measured. What failed was the install smoke, whose default arm uses the **system** compiler (GCC
here and on CI's Linux runner) rather than the clang toolchain G1/G3/G5 use — which is precisely
why a GCC-only precompiled-header hazard could reach G6 and nothing else. Fix round 4 fixes it in
the build, not in the engine. Run 1's transcripts live in `.scratch/m7w2e4gates/` and are untouched
by run 2, for the record round to file as `gates-run1/`.

## What is in this directory

| path | what |
|---|---|
| `wrappers-gates/gates-e4.sh` | the gate runner. NC2's, re-pointed. The six emitted segments are byte-identical to NC2's **except `g4`**, which differs by **2 removed / 7 added** lines — the comparison label, and the rc contract that now expects the comparator's `rc 1` because this round declares differences and refuses `rc 0` as an undeclared shape (4 of the 7 added lines are that contract's comment). It carries two offline modes: `--check-names debug\|release LOG` (carried) and **`--check-g4 COMPARISON`** (new, this round's G4 set gate run against an already-written transcript) |
| `wrappers-gates/chain_gates_e4.sh` | the detached chain: the six gates, then the gate-assert library's own fold proofs. It does **not** name the scratch tree — it asks the runner with `--print-scratch` |
| **`wrappers-gates/gates-e4-r2.sh`**, **`chain_gates_e4_r2.sh`**, **`psym-derivation-e4-r2.txt`**, **`preflight-r2.txt`** | **RUN 2**, the successor declaration over `84ea0385..e212a308`. Scratch `.scratch/m7w2e4gates-r2`, log `chain-e4-r2.log`, rc variable `GATES_E4_R2_RC`, G4 label `M7 W2 E4 R2`. The emitted segments are byte-identical to run 1's except `g4`'s label line, so run 1's widened class-B admission carries forward unaltered; the one new refusal is in the runner body |
| `wrappers-gates/rehearsal/rehearse-r2.sh`, `offline-r2.sh` + `rehearsal-r2-*.txt` | run 2's own rehearsal and its ten transcripts: the g5 rehearsal refused at its hypothesis in both configurations **by design**, the offline re-classification at 69 / 70 (**rc 0** both), the sections output, the offline U0 replay, and the configure/PCH evidence |
| `wrappers-gates/psym-derivation-e4.txt` | **RUN 1's** derivation, every value on `84ea0385..0d519a2b` — **frozen**. §1/§3/§6 derived from git, §2/§4 measured by its rehearsal, §5 measured from its own build, §9 the file provenance |
| `wrappers-gates/preflight.txt` | **RUN 1's** pre-flight — **frozen**. Five transcripts: the real pre-flight with every slot measured (`PREFLIGHT_OK`, rc 0); then the machinery's falsification record from the held state at `25e11280` — the hypothesis run, the `UNMEASURED`-sentinel refusal (rc 2), the G4 set gate + **two falsifiers** (including the one a declared *count* cannot catch: same count, one coordinate moved), and `--check-names`. Run 2's `preflight-r2.txt` does not re-run those falsifiers because the assertion they exercise is byte-identical in the r2 runner |
| `wrappers-gates/rehearsal/` (run 1) | `rehearse.sh` and `offline.sh` — **frozen** — and what they measured: `rehearsal-log-summary.txt` (the g5 rehearsal, 09:50:20–10:23:28, one lock hold, refused at the hypothesis in both configurations **by design**), `rehearsal-classification-{Release,Debug}-refused-at-30.txt` (those refusals), `rehearsal-classification-{Release,Debug}.txt` (the offline re-classification at 69 / 70, **rc 0** both), `rehearsal-sections-{Release,Debug}.txt`, `rehearsal-g4-comparison.txt` (the offline U0 replay), `rehearsal-offline-log.txt` |
| `REPORT.md`, `REPORT-fix1.md`, `REPORT-fix2.md`, `REPORT-fix3.md`, `REVIEW-fable.md` | the four implementer reports and the review, verbatim |
| `evidence/` | the implementers' own measurements, 264 files, 7.6 MB, every one `cmp`-verified against its source — including `evidence/logs3/`, the fix-3 battery at the final tree. `evidence/README.md` lists what was copied and what was left out |

The record round has run: run 1's chain outputs are filed as `gates-run1/` (declaration `13013b4c`,
G1–G5 GREEN / G6 FAIL) and run 2's as `gates/` (declaration `bba2fba6`, G1–G6 GREEN). Each has its
own `README.md` separating DECLARED from MEASURED cell by cell, in NC2's shape, and each file under
both directories is `cmp`-verified against the `.scratch/` transcript it was copied from.

## Box protocol

Four rehearsal legs across the two runs each took `flock /tmp/box-build.lock` for one hold, with a
separate prior `pgrep -a hven_bench_cell` empty before each launch and `PGREP_EMPTY` recorded
inside the hold; each restored the tree with `dirty=0` — run 1's to `m7 0d519a2b`, run 2's to
`m7 e212a308`. Nothing else in this
directory has taken the lock: the pre-flight and the two offline check modes run no build and no
test — they read git, the build files, the committed baselines and already written transcripts. The rehearsal and the chain each take `flock /tmp/box-build.lock`
per leg with `PGREP_EMPTY` recorded before every foreground start, and `MKL_NUM_THREADS=1` in
every measuring process. **G5 checks the main tree out to BEFORE** while it builds its before arm,
once per configuration, so nothing may edit or commit in the main tree from the chain's launch
until `CHAIN_END`.

**WALL IS INFORMATIONAL.** Nothing in this directory is a timing claim.
