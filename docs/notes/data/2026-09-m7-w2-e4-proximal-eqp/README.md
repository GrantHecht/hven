# M7 W2 E4 (the proximal EQP) + fix rounds 1 and 2 — the task record

> Base `84ea0385` (the E3 docs record) → head **`0d519a2b`** (fix round 3). **Four rounds, TWO
> code commits on `m7`**, gated TOGETHER on the final range in ONE declaration — the precedent is
> NC2 + its fix round 1 under settler ruling M16, and the acceptance is **E4-30**. The three
> worktree rounds were squashed into `25e11280`; fix round 3 is a NEW commit on top of it, because
> landed commits are not rewritten.

## The range

| | |
|---|---|
| **BEFORE** | `84ea0385` — `docs(m7): W2 E3 -- the flat-face factor, CLOSED as measurement …` (branch `m7`). Its sources **are** NC2 fix round 1's gated sources: `git diff --stat 85dc01ac 84ea0385 -- include src tests cmake` is EMPTY and so is the same diff against `0fa59cdc`, NC2's gated head (both OBSERVED) |
| **HEAD** | **`0d519a2b`** — fix round 3, the cherry-pick of `5ed7bf16`. OBSERVED: `git diff --stat 5ed7bf16 0d519a2b -- include src tests bench cmake docs/trace-schema-v0.md` is EMPTY, so the cherry-pick preserved every source byte. Its parent `25e11280` is the cherry-pick of `799b527d`, likewise source-identical (only `docs/notes/2026-09-m7-ledger.md` differs outside `docs/notes/data/`) |
| range | `84ea0385..0d519a2b` for every source and header derivation; the declaration commit that carries this record adds only `docs/notes/data/`, and the runner re-derives the changed surface from git at run time so it cannot silently disagree |
| origin | **E3** (`84ea0385`), which CLOSED the flat-face factor as a measurement: the ordinary step ascends on a flat face by the absolute centring's own term, the equality drift is the factor's solve rather than the model, and E3's proximal pair converted all six `Cs_STEENBRB` cells and zeroed the drift but broke termination and the runaway guard — which are in the wrong currency for a 1/δ-sized step. E4 is that pair rebuilt with the walk's own tests in the walk's currency, plus the EQP-site residual check. E3's arm lived on branch `e3` and was never merged |

## The three rounds, squashed into one commit

| round | worktree commit | what it is |
|---|---|---|
| 0 | `c46485d8` | the proximal EQP with the walk's tests in its currency, and the EQP-site residual check |
| fix 1 | `f2226c28` | the same, plus the UNREGULARIZED POLISH at both sites and the TERMINAL POLISH at every certified exit |
| fix 2 | `799b527d` | the step's **ROW-DRIFT CHECK**, the exhausted ladder made **ADVISORY** for both refusals, the terminal polish's acceptance given a **ROUNDING MARGIN**, the pathology fixtures' **DECLARED RENAME EVENT**, and the DALLASM certificates **VERIFIED TRUE** |
| → | **`25e11280`** | the cherry-pick of `799b527d` onto `m7`. The three worktree rounds are squashed into it |
| **fix 3** | **`0d519a2b`** (cherry-pick of **`5ed7bf16`**) | **settler rulings E4-27 / E4-28**, and it moved **nothing measurable**. The contract text aligned to E4-4-as-amended (the exhausted ladder is ADVISORY and the TERMINAL SITE certifies) at the six review sites plus two the implementer found; the STEP-currency runaway guard taken out of the contract text at both sites, with one clause recording that it was built, measured and removed; `polish_terminal_point`'s declaration reconciled with ruling E4-17; two stale narrations and the settlement prose in `.cpp` comments trimmed (the header-comment discipline). **ONE code change**: `adopt_polished_candidate` additionally requires the candidate to pass the walk's own KKT-point test (`after.stationarity <= opts.opt_tol` on the relative `TerminalKkt.stationarity`, NaN rejecting) — a contract closure, since a candidate up to 4× the incumbent's stationarity could otherwise be exported `kOptimal` from a walk that certified at `<= opt_tol`. **NO MOVER**: corpus 0 rows changed vs fix 2 on both algebras and every column, U0's whole comparison block byte-identical to fix 2's, census/HS/B1Gate 38/38, Release 0/2870, Debug 0/2869 + the four declared skips (both suites identical to fix 2 test by test), the long test alone 619.78 s, clang-format clean. 7 files, every one already in the range, **none under `tests/`** — which is why the rename event's by-name sets and the suite counts are unchanged, and were nevertheless RE-DERIVED on the final range rather than assumed (`REPORT-fix3.md`) |

Reports and the review, copied verbatim (`cmp`-checked body for body against `.superpowers/`):
`REPORT.md` (round 0), `REPORT-fix1.md`, `REPORT-fix2.md`, `REPORT-fix3.md`, and `REVIEW-fable.md`
— the Fable review of `25e11280`, which is what returned fix round 3. Each carries a three-line
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
| **(d)** | **P-SYM.** `solver_counters.h` and `qp_types.h` both change (six headers in all), so nearly every object in the capture universe moves; the DEBUG_ONLY and section shape differ from NC2's | **MEASURED on the final pair**, both configurations, by a fresh rehearsal of the runner's own `g5` (one box-lock hold) plus the offline re-classification at the measured counts (rc 0 in both). **Release DIFFERING 69 = class A 30 + class B 39**, control 78; **Debug 70 = 30 + 40**, control 61; UNEXPLAINED **0** in both; `after = before = 151` in both; `ONLY-AFTER []`. The two class A sets are **identical** (`cmp` rc 0) and equal the in-universe MODIFIED `.cpp` set exactly. Sections: 44 objects, byte-identical 35 / 30, **DEBUG_ONLY 0 / 5**, **nine** non-debug movers — the same nine in both — REFUSED 0. NC2's 38 = 1 + 37 and its DEBUG_ONLY 0 / 6 are nowhere reused |
| **(d′)** | **the one configuration-asymmetric class B object**, which the carried check could not admit | `tests/drivers/.../test_solve_status.cpp.o` is class B in Debug and **byte-identical in Release** — no verdict line in the Release compare, named in the Release negative control — while Debug differs on exactly one symbol (`RestorationSubStatus_TheMapIsTotal…::TestBody()`, 1363 → 1370 lines). The E2 rounds' admission required **NOISE-ONLY** in Release, a *narrower* fact than byte-identical, so it would have refused a correct measurement. Widened here to *"not a mover in Release — NOISE-ONLY **or** in the negative control — with which of the two named per object"*, plus a new fail-closed assertion that the negative-control listing is **complete** (the classifier prints at most 80; Release's is 78, checked). Nothing else relaxes: declared by name, the set difference exactly the declared list with nothing Release-only, positively found in one of the two non-mover classes, the cross-config copy removing exactly the declared lines, UNEXPLAINED 0 both. Falsified offline in three runs — the measured declaration rc 0, a Release mover rc 1, a non-asymmetric object rc 1 |
| **(e)** | the control lane MOVING at re-take 9 (E2-off changes on 25/51 corpus cells, three certified cells lost, named) | `REPORT-fix2.md` §5c and E4-26; a harness-lane item, not a gate value |

## The round's one structural change to the copied runner

Beyond the re-pointing every copied runner does, **`bench/` enters every source derivation**.
`bench/bench_scale.cpp` is MODIFIED in this range and its object is in the P-SYM capture
universe (`scripts/psym_compare.sh`'s `PSYM_GROUPS` names `hven_sqp_bench`). NC2 derived
`CHANGED_SOURCES`, `MODIFIED_CXX` and `ADDED_CXX` over `src/ tests/` only — correct for a range
that touched no bench source, wrong here: the classifier keys class A on the changed-source list
**by basename**, so `bench_scale.cpp.o` would have been read as a class B includer whose source
is untouched, and the pre-flight's own *"no declared class B object may have a source MODIFIED in
the range"* check would have refused it. The clang-format set gains `bench/*.cpp` for the same
reason. Derived, not carried.

## What is in this directory

| path | what |
|---|---|
| `wrappers-gates/gates-e4.sh` | the gate runner. NC2's, re-pointed. The six emitted segments are byte-identical to NC2's **except `g4`**, which differs by **2 removed / 7 added** lines — the comparison label, and the rc contract that now expects the comparator's `rc 1` because this round declares differences and refuses `rc 0` as an undeclared shape (4 of the 7 added lines are that contract's comment). It carries two offline modes: `--check-names debug\|release LOG` (carried) and **`--check-g4 COMPARISON`** (new, this round's G4 set gate run against an already-written transcript) |
| `wrappers-gates/chain_gates_e4.sh` | the detached chain: the six gates, then the gate-assert library's own fold proofs. It does **not** name the scratch tree — it asks the runner with `--print-scratch` |
| `wrappers-gates/psym-derivation-e4.txt` | the derivation, every value on `84ea0385..0d519a2b`. §1/§3/§6 derived from git, §2/§4 measured by the rehearsal, §5 measured from my own build, §9 the file provenance |
| `wrappers-gates/preflight.txt` | five transcripts: **FINAL** the real pre-flight with every slot measured (`PREFLIGHT_OK`, rc 0); then, kept as the machinery's falsification record from the held state at `25e11280`, **A** the hypothesis run, **B** the `UNMEASURED`-sentinel refusal (rc 2), **C** the G4 set gate + two falsifiers, **D** `--check-names` against the implementer's suite logs |
| `wrappers-gates/rehearsal/` | `rehearse.sh` and `offline.sh`, and **what they measured**: `rehearsal-log-summary.txt` (the g5 rehearsal, 09:50:20–10:23:28, one lock hold, refused at the hypothesis in both configurations **by design**), `rehearsal-classification-{Release,Debug}-refused-at-30.txt` (those refusals), `rehearsal-classification-{Release,Debug}.txt` (the offline re-classification at 69 / 70, **rc 0** both), `rehearsal-sections-{Release,Debug}.txt`, `rehearsal-g4-comparison.txt` (the offline U0 replay), `rehearsal-offline-log.txt` |
| `REPORT.md`, `REPORT-fix1.md`, `REPORT-fix2.md`, `REPORT-fix3.md`, `REVIEW-fable.md` | the four implementer reports and the review, verbatim |
| `evidence/` | the implementers' own measurements, 264 files, 7.6 MB, every one `cmp`-verified against its source — including `evidence/logs3/`, the fix-3 battery at the final tree. `evidence/README.md` lists what was copied and what was left out |

There is **no `gates/`** directory yet. It is written after the chain finishes, with its own
`README.md` separating DECLARED from MEASURED cell by cell, in NC2's shape.

## Box protocol

The rehearsal and its offline half each took `flock /tmp/box-build.lock` for one hold, with a
separate prior `pgrep -a hven_bench_cell` empty before each launch and `PGREP_EMPTY` recorded
inside the hold; both restored the tree to `m7 0d519a2b` with `dirty=0`. Nothing else in this
directory has taken the lock: the pre-flight and the two offline check modes run no build and no
test — they read git, the build files, the committed baselines and already written transcripts. The rehearsal and the chain each take `flock /tmp/box-build.lock`
per leg with `PGREP_EMPTY` recorded before every foreground start, and `MKL_NUM_THREADS=1` in
every measuring process. **G5 checks the main tree out to BEFORE** while it builds its before arm,
once per configuration, so nothing may edit or commit in the main tree from the chain's launch
until `CHAIN_END`.

**WALL IS INFORMATIONAL.** Nothing in this directory is a timing claim.
