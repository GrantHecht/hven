# M8 PF-5 -- symbolic-analysis reuse, the landing round (DECLARATION)

Committed on `pf5b` before the first leg; never edited afterwards. A failed or void leg is answered by
`DECLARATION-PF-5-r1.md`. The design is `DESIGN-PF-5.md` (origin/pf5, the prototype round); this round lands it under
the settler's rulings. Everything this round writes lives in this directory (`legs/`, `gate/`).

## 1. The code and the binaries

- Base (arm A): `origin/m8` `0ee37c2ca31c19cef4034d7d985367ddc0e59c08` (PF-4 batching, TR-S1, AP-4/4b in).
- **Code commit (arm B): `01e84165d7ca200483c5ccedf0dac05608893360`** on `pf5b` (`a8cd3005c` the code, `01e84165d` its counter doc comments). The code paths `src include tests bench examples bindings
  CMakeLists.txt cmake scripts` of the leg HEAD must equal it.
- Trees: `.scratch/build-pf5/{rel,dbg}` (this worktree) and arm A's corpus tree `.scratch/build-pf5/armA` from
  `git archive 0ee37c2ca` (+ the unchanged `dep/` submodules). clang 22.1.8, `-DHVEN_FP_MODE=SAFER_FAST
  -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON`.
- Corpus binaries (`hven_sqp_corpus`): A `f4c987298a21420543e7e90a708bebdf4b29aa109febb48ee492b0fdcc364f92` (`.scratch/build-pf5/armA`), B `5d7c634138dd007e8f96c60bb20d1def1f68491309203b083e2f32da5d5667ec` (`.scratch/build-pf5/rel`).
- Lane binaries (`legs/lane_build.sh`, NEVER=1; the private hven-bench clone at `e03cbad` + `never-<base>` + the pin,
  never pushed): A (pin `0ee37c2ca`) `13e7eaebd0b3708349fbb3549be86591ab3d52dd4084f188d3bb37a0ef73d7ed`; B (pin the code commit) `912f2f1d6a6cd01b1a46871ac24c4949e835c76fc058de2e4c2521816370795f`.

Linux x86-64, AMD Ryzen 7 5800X3D, clang 22.1.8, MKL 2026.2, `MKL_NUM_THREADS=1`. Apple/Accelerate and Windows: CI only.

## 2. The design as built (settler rulings 1-2)

- `detail::KktFactor` keeps a most-recently-used list of at most `kRetainedAnalyses = 8` analyzed sessions besides its
  current one. Key: pattern hash + order + stored entries; a hit is confirmed by an exact comparison of the outer and
  inner index arrays each entry stores (and the current session stores, for when it is retained).
- On a hit the session is swapped in and refactorized through `SymmetricFactor::factorize()` (the existing
  factorize-into-the-existing-symbolic path). On a miss K is analyzed on a fresh session, committed only after the
  analysis succeeded. A first analysis (nothing to retain or look up) analyzes in place, as before.
- **Discard:** a swapped-in session whose `(session_id, epoch)` is unchanged when `factorize_checked()` exits (no
  committed refactorization: a `kBackendError` outcome or a throw) is replaced by an unanalyzed session built before
  the swap.
- **Carry:** `kkt_counters(k)` = current session's counters + those departed; `dense_response_tally()` reads it.
- **Lifetime:** freed at QP-solve exit, in the walk (`BorderRetakeExit`, K0's factor) and in the SSN engine
  (`SsnEngine::solve`'s exit scope). Per-solve factors die with the solve.
- **Counter:** `detail::AnalysisReuseTally` (the shape of PF-4's `hven::linear::AnalysisTally`, recorded on each hit).
  `QpCounters::symbolic_analysis_reuses` (the five `QpEngine::solve` entry points; `SsnResult` ->
  `ssn_result_to_qp_solution`), `SqpCounters::symbolic_analysis_reuses` (opened beside PF-4's analysis tally; trace key
  after `backend_symbolic_repeats`; `dump_counters` through the table). Not folded from QpCounters.
  `symbolic_analyses` keeps its meaning.
- Untouched: `hven::linear`, both MPL session files, both adapters, the iparm surface (no IPARM-SURFACE label).

## 3. The declared schema event and the expected movers (re-derived on the post-PF-4 base)

- Schema: `SqpCounters` 1384 -> 1392 bytes, table 108 -> 109 keys; the two v1 golden `sqp.solve.end` lines and both
  counters-object goldens re-derived; `test_callback`/`test_threads` arity pins moved. `QpCounters` gains one `Index`.
- Two existing pins follow the code (not trajectory movers): `KktFactor.NeedsAnalysisPreservesCallSiteCounting` reads
  `kkt_counters()` (a miss now analyzes on a fresh session object), and
  `QpWarmStart.HotStateEmitsCommittedIdentityNotLive`'s precondition reads the `(session_id, epoch)` pair (a fresh or
  swapped-in session's epoch sequence is its own; every library holder already compares the pair).
- **Trajectory movers: none.** Every TRAIN and nlp-35 row of B equals A's except the three PF-5 columns, which obey
  exactly, per row (R = B's `side.symbolic_analysis_reuses`, d = A's `backend_symbolic_analyses` minus B's):
  `0 <= d <= R` and `A.backend_symbolic_repeats - B.backend_symbolic_repeats == d`. A sparse-arm hit replaces one
  platform analysis (a repeat); a dense-arm hit replaces only the dense session's set-up, which neither backend
  counter sees (measured before this file: HS83 on lane B, 13 hits, 0 platform analyses), so R - d is the dense-arm
  hits. The counters' doc comments were corrected to say so in `01e84165d` before this file.
- Replay: 0 differences vs A in walk, ssn, ipm (its CSV carries no PF-5 column).
- Test suite: 4049 tests on both trees (base + 13 new: 9 kkt, 4 seam).

## 4. The legs and their criteria (fixed here, before data); outputs under `gate/`

| leg | instrument (`legs/`) | criterion |
|---|---|---|
| P1 | `ctest.sh` rel, dbg | each log reads "100% tests passed, 0 tests failed out of 4049" |
| P2 | `replay.sh` B x3, A x1 | every B run and mode equals A's run exactly (`scripts/compare_replay.py`, exact): 0 differences x3 |
| P3 | `corun.sh jobs-train.txt` + `pf5_check.py train` | `ROWS_AS_DECLARED` on all 476 rows (238 x D/K), none missing or capped |
| P4 | `corun.sh jobs-nlp.txt` + `pf5_check.py nlp` | `ROWS_AS_DECLARED` on the 15 nlp-35 rows |
| P5 | `instr_compare.py` on P3/P4's `.stat` | informational except: ACOPP118 B/A <= 0.90 and YORKNET B/A <= 0.95 (design: 0.842, 0.901); TRAIN hven-sqp total B/A <= 1.000; hven-ipm total B/A in [0.995, 1.005] |
| P6 | `mutations.sh` | control clean; X1 D1 F1 F2 M1 M2 M3 C1 T1 Q1 each KILLED (`MUTATIONS_AS_DECLARED`); S1 recorded; tree restored |
| P7 | `install_smoke.sh` | install smoke and export contract rc 0 on GCC and clang++ |

Mutations (`legs/mutate.py`): X1 exact compare off (hash/order/nnz only); D1 discard off; F1 walk free-at-exit off;
F2 SSN free-at-exit off; M1 cap 9 (MRU cap broken upward); M2 cap 7 (downward); M3 eviction takes the most recently
used entry (insert at the back); C1 counter carry off; T1 only the innermost tally records; Q1 the walk's QpCounters
never receive the count; S1 (recorded) the SQP's never receive it -- no SQP-level test pins a hit count.

Protocols:
- Every compute leg runs in one `box-lane` acquisition, `MKL_NUM_THREADS=1`, `TMPDIR` in the worktree, binary sha256
  checked first. No leg asserts or quotes wall-clock, so none takes `--exclusive`.
- **P3/P4 co-run protocol (CLAUDE.md section 7):** width = the lane's 4 physical cores, one cell process per pinned
  physical core (`BOX_LANE_PHYS`, SMT siblings idle), `MKL_NUM_THREADS=1` everywhere, A and B jobs interleaved cell by
  cell, CAP 300 s through `cell.sh` MODE stat. Asserted: deterministic row columns (residual-class within train_check's
  calibrated gate) and per-process `instructions:u`; wall-clock informational. A row that deviates is a candidate and
  is re-run ALONE (`cell.sh`, lane's first core, both arms) before it is called a deviation; the re-run is recorded.
- P2: serial, one process at a time.

## 5. Order, STOP and ON FAIL

Pre-gate builds (not legs) are done: rel/dbg trees, armA corpus tree, lanes A/B (frozen copies in
`.scratch/bin/{A,B}/`). Run outside any lock:

```
bash docs/notes/data/2026-11-m8-pf5/legs/orchestrate.sh 01e84165d7ca200483c5ccedf0dac05608893360 f4c987298a21420543e7e90a708bebdf4b29aa109febb48ee492b0fdcc364f92 5d7c634138dd007e8f96c60bb20d1def1f68491309203b083e2f32da5d5667ec 13e7eaebd0b3708349fbb3549be86591ab3d52dd4084f188d3bb37a0ef73d7ed 912f2f1d6a6cd01b1a46871ac24c4949e835c76fc058de2e4c2521816370795f 4049 > docs/notes/data/2026-11-m8-pf5/gate/orchestrate.out 2>&1
```

Legs in order P1 (rel, dbg), P2, P3,
P4, P5, P6, P7. A failed leg stops the chain; the evidence is committed and reported with no re-run and no criterion
change after data (except section 4's candidate re-run rule). `DECLARATION-PF-5-r1.md` follows a fix.

## 5a. Dev runs before this file (none is a leg)

- ctest Release and Debug at `a8cd3005c`: "100% tests passed, 0 tests failed out of 4049" each.
- Mutation pre-flight at `a8cd3005c` (`legs/mutations.sh`, `.scratch/dev/mut/`): control clean; X1 D1 F1 F2 M1 M2 M3
  C1 T1 Q1 KILLED; S1 survived (recorded).
- CI `ci.yml` run 44879627250 at `a8cd3005c`: success on all four lanes (linux-clang-release, linux-clang-coverage,
  windows-clang-release, macos-clang-release). On the macOS lane (Accelerate) `PF5/RetainedAnalysesIdentity.*` (all four
  cases) and `QpRetainedAnalyses.*` passed: a reused refactorization's status, arm, inertia and a solve equal a fresh
  analysis's bit for bit on Accelerate's sparse arm and on the dense arm -- OBSERVED on the CI runner, for these
  fixtures; `QpRetainedAnalyses.AWalkSubproblemCountsItsReuses` recorded 6 reuses there (23 on MKL).
- HS83 on lane B (`.scratch/dev/smoke/`): 13 reuses, 0 platform analyses -- the dense-arm case section 3 states.

## 6. NOT RUN

- HOLDOUT (never); the census; any wall-clock leg.
- Apple/Accelerate and Windows: CI only (section 5a). No end-to-end Accelerate trajectory comparison of A vs B: that
  part stays UNOBSERVED.
- `threads > 1`: identity not measured, not claimed.
