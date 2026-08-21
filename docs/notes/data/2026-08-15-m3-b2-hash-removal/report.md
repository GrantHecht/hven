# B2 — removing the retarget's redundant per-major pattern hash

**Task:** M3 phase C, B2 (`docs/notes/2026-08-15-m3-phase-c-plan.md` §3), fired
GATE-BLOCKING by B1's measured 6.12–7.76 % against the plan's pre-stated 5 % bar
(`docs/notes/data/2026-08-15-m3-b1-hash-cost/report.md` §6).

**Engine change:** `ff461ad` — *perf(sqp): thread the analysis decision through
factorize_checked (phase-C B2)*. This artifact is its evidence.

**Verdict: the change ships. Steady state is back to TWO `hven::pattern_hash`
calls per SSN major, no counter moves anywhere the change is observable, and the
shipped engine reproduces B1's measurement-patch arm to within 0.23 %.**

---

## 1. Provenance

| | |
|---|---|
| Engine commit measured | **`ff461ad`** (`perf(sqp): thread the analysis decision through factorize_checked`) |
| Baseline compared against | B1's committed artifacts at `31e57b0` (`docs/notes/data/2026-08-15-m3-b1-hash-cost/`) |
| Date | 2026-08-15 (P-BENCH window 05:45 → 06:15 −04:00) |
| Host | fedora, AMD Ryzen 7 5800X3D (8C/16T), 31 GiB, Linux 7.1.5-201.fc44, `scaling_governor = powersave` |
| Toolchain | clang 22.1.8 (Fedora 22.1.8-4.fc44), Ninja, `CMAKE_BUILD_TYPE=Release` |
| Backend | Intel oneAPI MKL **2026.1**, LP64, `libmkl_intel_thread` + `libiomp5` |
| **`MKL_NUM_THREADS`** | **1**, exported by both runners for every solve-running step |
| Serialization | Strictly one solve at a time, machine otherwise idle, no build in flight, for every wall figure quoted |
| Build parallelism | `-j6`, every build completed **before** any measurement window opened |

Two Release builds, both configured `HVEN_BUILD_TESTS=OFF`, `HVEN_BUILD_BENCH=ON`
— the same options B1's three arms used:

- **arm P** — `ff461ad` as-is, clean configure. Its compiled-in stamp is
  **`ff461ada8b57`**, with **no `-dirty`**: it names a real commit, so every CSV
  it wrote satisfies the gate's provenance requirement.
- **arm S2 (the scaffold)** — `ff461ad` + B1's `patch_S.py` applied **before** the
  configure and reverted with `git checkout -- include src` immediately after the
  build linked; the working tree was verified clean (`git status --porcelain`
  empty) before the measurement started. Its own compile flags were checked token
  for token against the representative command line B1 committed in
  `compile-flags-diff.txt` — identical.

**Claim class (CLAUDE.md §7).** The counters are **asserted**: the hash count per
factorization and per major, and the 13-column identity of the three corpus
arms. The wall figures are **informational**, and are reported
with the provenance above because B1's obligation — and therefore B2's — was
stated as a wall-clock one.

**P-SYM is NOT APPLICABLE.** P-SYM asserts per-symbol disassembly identity, which
is the proof for a *relocation*. B2 is a **content change on the solve path**: the
codegen is intended to differ, so P-SYM could not hold and asserting it would be
meaningless. §4 records what stands in the census's place.

## 2. What changed, and the arithmetic it was aimed at

`AnalysisDecision` carries `needs_analysis()`'s answer together with the
`hven::pattern_hash` it was taken on; the three call sites that count
`symbolic_analyses` take it once, before the factorize, and hand it to a new
`factorize_checked(k, K, decision)` overload; the analysis branch records the
decision's hash instead of computing a fresh one.

`pattern` is `std::optional` and is **disengaged on the `!k.analyzed`
short-circuit**, which is preserved exactly as `needs_analysis` always had it —
`pattern_hash()` throws on an uncompressed matrix, so hashing unconditionally
there would move a throw site (`docs/retarget-design-sqp.md` §11 ledger row 8).
It costs nothing: that path hashes once for the record instead of once for the
decision.

| Situation | pre-retarget seam | phase-C base `31e57b0` | **post-B2 `ff461ad`** |
|---|---|---|---|
| Steady state (analyzed, same pattern) | 2 | 3 | **2** |
| First factorize (nothing analyzed yet) | 3 | 3 | **3** |
| Cross-pattern re-analysis | 3 | 5 | **3** |

**Two is the floor, not a stopping point of convenience.** The remaining hash is
`SymmetricFactor::factorize()`'s own pattern guard — `hven::linear`'s published
contract, pinned by `FactorizeRejectsAForeignPattern`. Going to one would be a
`hven::linear` contract change: out of B2's scope and not proposed. This is the
same floor B1's A3 end-state row records, and the row stands: **3 → 2 → 2**.

## 3. Proof 1 — P-SUITE (asserted)

Full `ctest` at `ff461ad`, `MKL_NUM_THREADS=1`, `-j6`:

| Config | Registered | Run | Failed |
|---|---|---|---|
| Release | 1153 | 1151 | **0** |
| Debug | 1153 | 1151 | **0** |

The four not run in each config are the standing pair of disabled tests
(`EqpRefinementAb.FootprintRuleProbe`, `EqpRefinementAb.FullBattery`) and the
standing pair of environment-skipped ones (`FailByDesignControl…`,
`JetMklGuard…`). **No counter moved and no test was added** — the count
arithmetic row is **+0 against 1153**.

`KktFactor.NeedsAnalysisPreservesCallSiteCounting` — the pin the plan names for
this task, as strengthened by C0.2 — passes unchanged, on unchanged assertions.
It exercises the bare `factorize_checked(k, K)` path; the decision-threaded path
is exercised by the three engine call sites and is asserted by §5.4's 13-column
identity check over 17 cells × 3 arms × 4 reps.

## 4. Proof 2 — P-CENSUS: **NOT REQUIRED, by ruling**

The phase-C plan's §10 O3 made P-CENSUS mandatory on every content change, B2
named among them. **That obligation was released for B2** by the SQP execution
reviewer's census-frequency ruling (2026-08-15): intermediate content-change
proofs do not each run a census; **one intermediate counter census runs after
H3** and covers all of phase C's content changes cumulatively, with the full
57-cell bar unchanged at Gate C.

Recorded so the record is not silently thinner than the plan reads:

- A full 57-cell census **was started** on the clean `ff461ada8b57` binary and
  **stopped when the ruling landed**, with tier 1 complete (44/44 cells) and
  tier 2 one cell in. **No partial census result is claimed, offered, or
  committed here** — a 44-of-57 sweep is not a census, and presenting one would
  be a weaker claim wearing a stronger name.
- The engine commit `ff461ad`'s own proof block was written before the ruling and
  says P-CENSUS "is committed as this task's evidence in the follow-up commit".
  **That line is superseded by this section.** It is left standing rather than
  amended because the commit is what the committed `corpus_P_r{1..4}.csv`
  provenance stamps (`ff461ada8b57`) name, and rewriting it would break the
  chain of custody on this artifact's own raw data to fix a sentence.

What carries B2's counter neutrality in the census's place, per condition 4 of
the ruling: the both-config full suite (§3), B1's already-committed A/B counter
identity, and **this artifact's own** two change-specific instruments — the
scaffold hash-count readback (§5.1) and the 17-cell × 3-arm × 4-rep 13-column
identity check (§5.4), which asserts the same columns a census asserts, over the
SSN-heavy population the change actually touches.

## 5. Proof 3 — P-BENCH

### 5.1 The asserted half: the hash count per major

B1's arm-S scaffold (`patch_S.py`, unchanged) re-run on the shipped engine over
the **11 headline cells** of B1's §5 table, one process per cell, serial.
Raw dumps in `scaffold-raw-corpus.txt`; the derived table in
`pbench-analysis.txt`.

| Quantity | arm S (base `31e57b0`) | **arm S2 (shipped `ff461ad`)** |
|---|---|---|
| `pattern_hash` calls **per factorization** | **3.00** on every cell | **2.10 – 2.17** |
| `pattern_hash` calls **per SSN major** | 3.27 – 3.41 | **2.31 – 2.45** |
| `factorizations` per cell | — | **identical on all 11 cells** |
| SSN major iterations per cell | — | **identical on all 11 cells** |

The per-factorization figure is the clean one, and it lands where the arithmetic
says it must. Base was **exactly** `3 × factorizations` on every cell; post-B2 is
`2 × factorizations + a`, where `a` is the cell's handful of genuine analysis
events (5 on `f7_n750_path_neutral_control`: 81 = 2·38 + 5). One hash removed per
steady-state factorization, none removed anywhere else — which is the whole claim.

### 5.2 The informational half: SSN-major wall

Same instrument, same denominator B1's decision rule was keyed on:

| | min | median | max | pooled |
|---|---|---|---|---|
| **measured** SSN-major wall recovery | 6.24 % | **7.45 %** | 8.05 % | 6.76 % |
| **B1's per-cell prediction** (X%maj) | 6.12 % | **7.34 %** | 7.76 % | — |

Per-cell, predicted against measured, the two agree to within about a point
everywhere (`pbench-analysis.txt` has the full table). Arm S and arm S2 are both
single-rep, exactly as B1's arm S was, and carry the same caveat: their
confidence comes from whole-cell accumulator totals over 11 independent cells
spanning a 27× size range, not from a run-to-run spread of their own.

### 5.3 The corroborating half: whole-solve wall, and the arm-B reproduction

B1's A/B runner re-run over its own 17-cell list, 4 reps, serial, on the shipped
engine (arm **P**), compared against B1's committed arm **A** (base) and arm **B**
(the measurement patch). Per-cell reported solve wall, median of 4 reps:

| Population | arm A | arm B | **arm P** | P vs A | P vs B |
|---|---|---|---|---|---|
| **13 pure-SSN cells** (B1's headline population) | 11.2952 s | 10.6653 s | **10.6903 s** | **−5.36 %** | **+0.23 %** |
| 11 headline cells | 10.2538 s | 9.6990 s | 9.7216 s | −5.19 % | +0.23 % |
| all 17 pooled | 35.9475 s | 35.2135 s | 35.4454 s | −1.40 % | +0.66 % |

B1 predicted **−5.58 % pooled** over the 13 pure-SSN cells; the shipped change
measures **−5.36 %**. The A and B columns are recomputed here from the committed
CSVs and reproduce B1 §4's own figures to the digit (11.2952 / 10.6653), so the
comparison is like-for-like and not a re-derivation under a different median rule.

**The stronger statement is the last column.** Arm B was a throwaway patch built
to isolate exactly one variable; the shipped, production-shaped change lands
within **0.23 %** of it on B1's own population. Whatever the production shape
costs over the measurement shape — the `std::optional`, the preserved
short-circuit, the extra call layer — it is inside a quarter of a percent, on an
instrument whose run-to-run spread B1 measured at 0.06 %–0.3 %.

### 5.4 Counter identity across all three arms

Over **17 cells × 3 arms × 4 reps**, all **13 asserted columns** (`status`,
`factorizations`, `qp_minors`, `escapes`, `qp_subproblems`, `qp_fact_per_qp`,
`kkt_residual` and the identifying columns) are **byte-identical**. Arm P changes
when the pattern is hashed and nothing else — the same assertion B1 made for arm
B, now made for what actually ships.

## 6. Files

| File | What |
|---|---|
| `report.md` | this |
| `scaffold-raw-corpus.txt` | arm S2's raw `[B1PROBE]` accumulator dumps, 11 cells |
| `corpus_P_r{1..4}.csv` | arm P's four raw corpus artifacts, own provenance headers |
| `pbench-analysis.txt` | the derived P-BENCH tables (§5.1–§5.4) |
| `pbench-run.log` | the arm-P runner's timing log |
| `run_pbench_b2.sh`, `run_prod_ab.sh` | the two runners |
| `analyze_b2.py` | the analysis that produced `pbench-analysis.txt` |

**Reproduce.** Both runners hard-code a session scratchpad path (`SP`) holding
`build-census` and `build-S2`; repoint it first. Then: configure two Release
builds (`HVEN_BUILD_TESTS=OFF`, `HVEN_BUILD_BENCH=ON`) — one clean at `ff461ad`,
one with `docs/notes/data/2026-08-15-m3-b1-hash-cost/patch_S.py` applied before
the configure and reverted after it links — export `MKL_NUM_THREADS=1`, leave the
machine idle, run `run_pbench_b2.sh` then `run_prod_ab.sh`, and
`python3 analyze_b2.py`.
