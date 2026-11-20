# PF-5 -- symbolic-analysis reuse: design and measured prototype

Status: DESIGN + PROTOTYPE (no code lands from this round). Base: `origin/m8` 331b31837. Prototype: local scratch
branch `pf5-proto` (never pushed), commit cb08a6d1f. Evidence: this directory.

## 0. Summary

- **Build it** as a bounded, per-`KktFactor` list of retained analyzed sessions (cap 8, MRU, released at the end of
  each QP solve). It lives in `KktFactor`, not in `hven::linear`, and it does not hoist the factor.
- **Why not hoisting.** The repeats come from one QP solve's working set returning to faces it has already factorized
  (A, B, A, C, B ...) on one long-lived factor. They are not repeats across solves. Hoisting the `KktFactor` would take
  about 0.2 % of ACOPP118's analyses; a multi-entry list on the existing factor takes 83 %.
- **Identity.** In every measured case, a refactorization into a retained session is bitwise identical to a fresh
  analysis plus factorization:
  - 19 195 retained-session hits across 8 cells, each verified in-process against a fresh factor of the same K: equal
    inertia, equal perturbed-pivot count, and bitwise-equal solve;
  - 0 row movers on 4 + 15 + 119 + 8 cells;
  - replay 27 cells x walk/ssn/ipm: 0 differences (the replay exercises the path only 3 times, see section 4).
- **Savings** (`instructions:u`, co-run per CLAUDE.md section 7):
  - ACOPP118 -15.8 %, YORKNET -9.9 %;
  - nlp-35 total -2.8 %;
  - TRAIN hven-sqp D total -0.11 %, worst cell +0.39 % (prototype overhead on tiny cells, section 5);
  - peak RSS +1 to +5 MB on the cells measured.
- **Footprint.** Nothing in either MPL session file or either platform adapter changes, and there is no `iparm` change.
  It lands after pf4, because it touches `src/kkt/kkt_factor.cpp`.

## 1. Where the analyses come from

PF-4 (`origin/pf4`, `RESULTS-PF-4.md` section 3, `step1/summary-probe2.txt`) counted every platform symbolic analysis
the SQP pays and the repeats among them. The decisive column is the last one PF-4's probe 2 printed: of the repeats, how
many fall **inside one `QpEngine::run`** and **on the same `SymmetricFactor` instance**:

| cell | analyses | repeats (process) | same instance | within one QP run |
|---|---:|---:|---:|---:|
| ACOPP118 | 8 950 | 7 520 | 7 491 | 7 503 |
| YORKNET | 14 520 | 8 219 | 7 697 | 7 797 |
| CRESC100 | 17 092 | 8 399 | 2 842 | 368 |
| CRESC50 | 3 969 | 1 459 | 1 421 | 55 |
| DALLASM | 296 | 3 % | -- | -- |

Read with section 3 of this note (the prototype's own probe, which splits by factor role and by LRU distance):

the prototype's per-role probe (cap 0, an unbounded per-factor LRU shadow cleared at QP-solve exit; `cells1/P/*.pf5`)
puts almost all of them on **role 0, the walk's elimination factor** (`QpEngine::run`'s per-solve `kkt`), at small LRU
stack distances: ACOPP118 distances 2/4/6/8 = 2 677/709/2 072/1 941 of 8 948 analyses; YORKNET distances 2/3 = 3 873/3 802
of 14 430; K0's factor (role 1) contributes little (CRESC100 ~80 of 5 704), the SSN factor nothing at the default
options. DALLASM has no repeats at all (277 analyses, all cold), so its 296-analysis count is not a reuse target.
PF-4's "same instance" figure for CRESC100/CRESC50 (2 842 / 1 421) is NOT reproduced by a within-solve or
across-solve per-`KktFactor` list (CRESC100: 418 + 39 hits even with K0's list kept across solves, `cells3/KV`); the
difference was not chased (section 7).

**Consequence for the shape of the design.** The repeats are not "the next QP solve starts on the pattern the previous
one ended on" (which hoisting the elimination factor across solves would take); they are **the working set of one
QP solve returning to a face it has already factorized** -- an A, B, A, C, B ... pattern stream on one long-lived
factor. A factor that remembers only its last pattern (today's `KktFactor::analyzed_pattern`) re-analyzes every one of
them. Hoisting the `KktFactor` alone therefore takes almost nothing (process-level repeats minus within-run repeats:
ACOPP118 17 of 8 950); what takes the repeats is letting ONE factor keep **several** analyzed sessions.

## 2. The design

### 2.1 Where the reuse lives: inside `KktFactor`, a bounded set of retained analyzed sessions

`hven::detail::KktFactor` (`include/hven/detail/kkt/kkt_factor.h`) gains a small most-recently-used list of retained
`SymmetricFactor` objects, each one an analyzed session with its pattern key:

```cpp
struct RetainedAnalysis { AnalysisKey key; std::unique_ptr<hven::linear::SymmetricFactor> factor; };
std::vector<RetainedAnalysis> retained;   // MRU first, size <= kRetainedAnalyses
```

`factorize_checked()`'s analysis step (`analyze_if_needed`, `src/kkt/kkt_factor.cpp`) becomes, when the decision says
the pattern changed:

1. look K's key up in `retained`; on a hit, **swap** that session into `k.factor` (the outgoing session becomes the
   MRU entry, the LRU tail is evicted past the cap) -- no backend analysis;
2. on a miss, analyze K on a **fresh** `SymmetricFactor` built from the factor's stored Options (current thread count),
   and only after that analysis succeeded, swap it in the same way (a throwing analysis leaves `k` exactly as it was --
   today's guarantee);
3. then `k.factor.factorize(K)` as today: Pardiso phase 22 / Accelerate's numeric `SparseFactor(symbolic, A)` /
   the dense arm's `dsytrf[_rook]` into the retained session's existing symbolic and workspace.

Everything downstream (`clear_shift_marker`, the dense singular response, the platform-solve response, the retake
sessions) is unchanged and sees an ordinary just-factorized `k.factor`.

Why here and not elsewhere:

- **Not a per-engine or process cache.** A cross-instance cache is shared mutable state (a thread-safety contract to
  write and test), a lifetime question (whose session is it when two engines hold one) and buys nothing: the measured
  repeats are on one instance (section 1).
- **Not hoisting.** Hoisting the walk's per-solve elimination `KktFactor` into the engine would take the
  across-solve repeats only (17 on ACOPP118, ~100 on YORKNET) and turns a per-solve local into engine state that a
  `const` `QpEngine::solve` mutates. Hoisting is orthogonal and can ride later if wanted; it is not this task's lever.
- **Not inside `hven::linear::SymmetricFactor`.** A retained-session option in the public class would touch both
  platform adapters, the public header's session/epoch contract and `Counters`, and would put an ABA identity
  ((session, epoch) reappearing) into a published API. At the `KktFactor` layer every swap goes through the published
  `SymmetricFactor` contract that already holds on both backends ("numeric factorization into the EXISTING symbolic --
  never re-analyzes"), and nothing in `hven::linear` changes -- so neither MPL-derived session file nor either adapter
  is touched, and there is no `iparm` change (no IPARM-SURFACE label needed).

Which factors get it: every `KktFactor` (one code path), but only the long-lived ones can hit: the walk's per-solve
elimination factor (`walk_engine.cpp`, `QpEngine::run`'s `kkt`), K0's factor (`BorderState::kkt`), the SSN engine's
`kkt_`. The short-lived per-call temporaries (`refine_on_face`'s, the verdict refinement's `local`, the degeneracy arm's,
the predictor's) analyze once and die; for them the list stays empty and the cost is one empty-vector check.

### 2.2 The key

A retained session may be reused for K iff a fresh analysis of K would have produced the same symbolic. What reaches the
symbolic phase:

| input | where it is fixed | in the key? |
|---|---|---|
| K's sparsity pattern (upper CSR: n, nnz, outer/inner indices) | per call | yes: `hven::pattern_hash(K)` (FNV-1a over dims, nnz and both index arrays -- the hash `factorize()`'s own guard compares) |
| decomposition, ordering, pivot strategy/perturbation, factorization algorithm, CNR | `sqp_kkt_options()`; constant per `KktFactor` | no: constant per factor, so a per-factor list cannot mix them |
| weighted matching (`iparm[12]`), matrix scaling (`iparm[10]`) | off on the platform session of every `KktFactor`; the **matched retake session** turns matching on | not needed on `factor`; the matched/scaled retake sessions are **out of scope** (they keep their own records) |
| the values of K | -- | not needed: with matching and scaling off, Pardiso phase 11 and Accelerate's symbolic `SparseFactor` read the pattern only; the dense arm's analyze allocates only |
| dense vs sparse arm | `DensePolicy` + `max_dense_dimension` (constant per factor) and n (in the pattern) | implied by the pattern |
| thread count | applied at call scope, never baked into the symbolic (`pardiso_session.h`) | no |

So the key is the pattern hash **plus** `(rows, nonZeros)` checked explicitly (the session's own size check repeats it),
the same structural identity the existing one-pattern cache (`analysis_decision`) already trusts. A collision on all of
them is the risk today's cache already carries at one entry; at N entries it is N times 2^-64. The design keeps it
(no full-pattern copy), and notes the alternative (store the index arrays, O(nnz) per entry, compare exactly on a hit)
if a reviewer wants exactness.

The matched retake session's analysis is value-dependent (the weighted matching reads K's values) and is NOT retained;
the scaled retake session already has its own one-pattern record (`scaled_analyzed_pattern`) and is left as is.

### 2.3 Invalidation and lifetime

- **Per factor**: a retained entry is valid for exactly the factor's Options, which never change after construction
  except the thread count; a swapped-in session takes the factor's current count (`set_threads` on the incoming
  session before its factorize -- the count lives at call scope, so this changes no symbolic).
- **End of a QP solve**: `release_retained_analyses(k)` beside the existing `release_idle_retake_sessions` (the walk's
  `BorderRetakeExit`, and the SSN engine's solve exit). The walk's elimination factor is a per-solve local and dies with
  the solve anyway. Measured (section 1): the across-solve repeats are about 0.2 % (ACOPP118) to 1 % (YORKNET) of the
  analyses, so releasing at solve exit costs nothing material and bounds every retained session's lifetime by one QP
  solve.
- **K0 detach/wipe**: `border_` detached onto a fresh `BorderState` drops the old one's list with it; a sole-owned wipe
  in place keeps the factor (and so, until solve exit, its list) exactly as it keeps the current analysis today.
- **A failed refactorization of a swapped-in session discards it** (2.4 hazard 1).

### 2.4 The hazards, one by one

**H1 -- a swapped-in session carries old numerics and an old (session, epoch).** Between the swap and a successful
refactorization, `k.factor` reports the session id and epoch it had when it was retained, and still holds those
numerics. Two holders compare (session, epoch): `EliminatedFace::holds` (walk_engine.h) and the hot-start reuse gate's
condition (e) on K0 (walk_engine.cpp). If the refactorization fails -- a `kBackendError` outcome (the session then has
no usable numerics but its epoch does not advance) or a throw from `factorize()`'s input checks (the old numerics stay
solvable) -- the OLD pair is live again, and an `EliminatedFace` captured on that session before it was retained would
match a working set whose K has the same pattern.

Closure: **a swapped-in session that does not complete its refactorization is discarded** -- a scope guard in both
`factorize_checked()` overloads replaces `k.factor` with a fresh, unanalyzed factor (session id 0, which no capture
carries) on every exit but a successful return, and clears `k.analyzed`. So the only way a retained session's
(session, epoch) is ever observable again is after a committed factorization advanced its epoch -- a pair no record
captured. Today a failed factorization leaves an analyzed-but-unfactorized factor; after the change a failed
refactorization of a SWAPPED-IN session leaves an unanalyzed one, so the next factorization of that pattern pays an
analysis it would not have paid. That is a counter difference on failure paths only (kBackendError is rare and ends
the QP solve); the hot-start gate's usable-numerics conjunct (`kkt_inertia(...).state == kObserved`) already refuses a
failed K0 independently. The prototype implements the guard (`Pf5SwapGuard`) and the gate run should mutate it
(remove the discard -> a seam test where a swapped-in refactorization fails and `EliminatedFace::holds` must be false).

Also closed by construction: the swap happens only inside `factorize_checked()`, immediately before `factorize()`, so
no reader can observe the factor between swap and refactorization (the walk is single-threaded per engine; see 2.7).

**H2 -- `DenseTallyScope` reads the factor's counters across the factorize call.** `dense_response_tally(k)` reads
`k.factor.counters()`; a swap replaces the object those counters live on mid-scope, so after - before would be the
difference of two unrelated sessions' counters (wrong, and possibly negative). Closure: `KktFactor` keeps a **carry** of
the five dense counters (`dense_factorize_count`, `dense_singular_count`, `dense_flagged_count`,
`dense_shift_refactor_count`, `dense_retake_count`): on every swap, `carry += outgoing.counters - incoming.counters`;
`dense_response_tally(k)` returns `k.factor.counters() + carry`. The tally is then the factor's cumulative count across
every session it has driven, exactly what the scope's before/after needs. `RetakeTallyScope` reads `KktFactor` fields
and is unaffected. The only other reader of a `SymmetricFactor`'s counters on this surface (`ws_independence.cpp`) is a
plain local `SymmetricFactor`, not a `KktFactor`. The prototype implements the carry; the replay's
`dense_factorizations` / `dense_flagged_factorizations` / `dense_singular_refactors` / `dense_platform_retakes`
columns being identical A vs B is the evidence (section 4).

**H3 -- retained Pardiso sessions hold memory.** A retained platform session keeps its symbolic, its CSR copy and its
L factor (phase-22 storage); a retained dense session keeps 8 n^2 bytes (n <= 105 under the SQP's kAuto: <= 88 KB). See
2.5 for the bound.

**H4 -- bit identity of a refactorization into a retained session vs a fresh analysis.** Measured, section 4.

**H5 -- Accelerate parity.** 2.6.

### 2.5 Memory bound

Per factor: at most `kRetainedAnalyses` retained sessions (proposal 8, from section 3's distance histogram), each
no larger than the factor's current one for that pattern -- the same size the factor already held when that pattern was
current. Live sessions per walk solve: elimination factor + K0 factor, each 1 + cap. Worst case = (1 + cap) x today's
peak factor storage per long-lived factor, released at solve exit. If a byte ceiling is wanted on top (large KKTs), the
retained list can additionally evict while the sum of `InertiaEvidence::factor_size_bytes` (reported by Pardiso;
absent on Accelerate, where only the count bound applies) exceeds a multiple of the current session's; the prototype
does not implement it and peak RSS measured on the cap-8 prototype (`tools/rss.log`, informational, co-run) moved ACOPP118 21.2 -> 25.6 MB,
CRESC100 20.8 -> 26.2 MB, YORKNET 39.3 -> 41.5 MB, STEENBRC 42.8 -> 44.0 MB -- so no byte ceiling is proposed for the
first landing; it is registered for large-KKT consumers (section 7).

### 2.6 Accelerate

Nothing in the design is platform-specific: the swap and the refactorization go through `SymmetricFactor`'s published
two-phase contract, which the Accelerate adapter implements as a symbolic `SparseFactor(LDLTTPP, structure)` once per
analyze and a numeric `SparseFactor(symbolic, matrix)` per factorize (`accelerate_session.cpp`) -- a numeric
factorization built **fresh** from the symbolic every time (no `SparseRefactor`), so a retained session's numeric
factorization is constructed exactly as a fresh analysis's first one would be, given an equal symbolic. Whether
Accelerate's symbolic phase is deterministic for a pattern (so that the retained symbolic equals a fresh one) is
**UNOBSERVED**: no Mac run was made. The prototype's verify mode (section 4) is backend-neutral and is the instrument a
CI or Mac session would run; until then the Accelerate identity claim is recorded as UNOBSERVED, not as true. Accelerate
reports no perturbed-pivot count, so the platform-solve response arms on every platform factorization there; that path
is unchanged by the swap.

### 2.7 Thread safety

No shared mutable state is introduced: the retained list is a member of a `KktFactor`, owned by exactly the object that
owns that factor today (a per-solve local, an `SsnEngine`, a `BorderState`). The one object that can be shared between
engines is a `BorderState` (through `HotState`), under the existing sequential-use contract (walk_engine.h THREAD
SAFETY, the `use_count()` detach rule); its retained list travels with it under the same contract and is never reachable
from anything but its own factor. No `share()` handle is ever emitted from a `KktFactor`'s factor (no call site), so a
retained session has no co-owner. No static, no thread_local, no process cache. Concurrent solves on different solver
instances touch disjoint `KktFactor`s, so `docs/concurrency.md`'s contract is unchanged.

### 2.8 Counters

- `QpCounters::symbolic_analyses` / `SqpCounters::symbolic_analyses` keep their meaning: they count the border/rebuild
  path's **analysis decisions** (`analysis_decision().needed`), taken before the lookup. They do not move.
- PF-4's `SqpCounters::backend_symbolic_analyses` (AnalysisTally, counted in the adapters after each completed platform
  analysis) **drops by the hit count**, and `backend_symbolic_repeats` drops with it -- the declared mover of a PF-5 gate.
- New: `QpCounters::symbolic_analysis_reuses` and its `SqpCounters` sum -- retained sessions swapped in (counted inside
  `KktFactor`, folded into the engine counters through a tally scope like `RetakeTallyScope`, so the per-call temporaries
  are covered by the same construction rule). `backend_symbolic_analyses + symbolic_analysis_reuses` then equals the
  pre-PF-5 `backend_symbolic_analyses` on a trajectory that does not move, which is the checkable identity.
- `SymmetricFactor::Counters` (per instance) are unchanged; `dense_response_tally()` carries across swaps (H2).
- trace v1 fixtures: unchanged unless the new counter is added to the trace (then a declared fixture move).

### 2.9 The bit-identity argument

The claim to establish: for every K, (refactorize into a retained session analyzed on K's pattern) produces bitwise
the same numerics -- inertia, perturbed pivots, every solve -- as (fresh analysis of K, then factorize). It rests on:

1. **Equal symbolic.** The platform session analyzes with matching and scaling off, so phase 11's ordering (METIS,
   `iparm[1]`), supernode partition and pivot structure are functions of the pattern and the session's configuration
   alone; two phase-11 calls on one pattern yield the same symbolic if phase 11 is deterministic (single-threaded, fixed
   seed). Measured directly by the verify mode (section 4) -- not presumed.
2. **Stateless numeric phase.** Phase 22 recomputes L D L^T from the values passed and the symbolic; nothing from a
   previous phase 22 on the session is an input (no `iparm[3]` preconditioned-CGS reuse, no `iparm[5]` in-place, no
   matching). The session re-copies the values into its CSR copy before phase 22.
3. **Address sensitivity.** CLAUDE.md section 7 records that MKL kernels are address-sensitive. A retained session's
   buffers sit at whatever addresses they were first allocated at, a fresh session's at new ones. This is the point
   the measurement had to settle, per factorization and end to end.

Then end to end: equal numerics at every factorization => equal solves => the same trajectory, counters and residuals.

Measured (section 4): items 1-3 hold on MKL Pardiso at `MKL_NUM_THREADS=1` for every one of 19 195 sparse-arm hits
(equal inertia and perturbed pivots, bitwise-equal solve against a fresh analysis + factorization in the same process),
and end to end on 146 NLP rows. The address-sensitivity point is settled for phase 22 at least as far as these cells
reach. The claim is per backend and per thread count: at `threads > 1` (Pardiso's parallel factorization) it is not
measured and is not claimed. Accelerate is UNOBSERVED.

## 3. Prototype

Scratch branch `pf5-proto` (local, never pushed): cb08a6d1f (the design, sections 2.1-2.4) + e401b64fd (a probe arm that
keeps K0's list across QP solves). Full diff: `prototype.diff`. Prototype-only switches, all environment variables read
by the library (a landing has none of them): `HVEN_PF5_RETAIN=N` (cap; unset = the original code path, byte for byte),
`HVEN_PF5_PROBE` (per-role fresh/hit counts and the LRU-distance histogram), `HVEN_PF5_VERIFY` (on every hit, a
throwaway `SymmetricFactor` analyzes and factorizes the same K; status, arm, inertia (+/-) and perturbed pivots are
compared and one solve through each is compared bitwise), `HVEN_PF5_KEEP` (skip the release at solve exit).

What it implements: the MRU list and swap (`pf5_install`), fresh analysis on a fresh factor committed only after
success, the discard-on-failed-refactorization guard (`Pf5SwapGuard`, both `factorize_checked` overloads), the dense
counter carry (H2), release at the walk's solve exit (`BorderRetakeExit`). What it does not: the new counter, a byte
ceiling, a test (no ctest was run on the prototype: it is a measurement instrument, not a candidate).

Instruments (all in `tools/`): lane binary = PF-4's `lane_build.sh` re-pointed (private hven-bench clone at `e03cbad`,
pin = the prototype commit; sha256 `92bfc41b...` for cb08a6d1f, `7f988b29...` for e401b64fd); `corun.sh` = one cell
process per pinned physical core of a `box-lane` lane (`BOX_LANE_PHYS`, SMT siblings idle), `MKL_NUM_THREADS=1`, `perf stat
-e instructions:u,cycles:u` per process through PF-4's `cell.sh` (core pin re-pointed); rows compared by PF-4's
`nlp_check.py`/`train_check.py` (exact row equality), instructions by `instr_compare.py`. **Co-run protocol** (CLAUDE.md
section 7): width 4, one process per physical core of the lane, `MKL_NUM_THREADS=1`; only per-process `instructions:u`
and deterministic rows are asserted; no wall-clock is quoted (the `wall=` in the logs is informational). No row was
CAPPED. Arms A (unset) and B (`HVEN_PF5_RETAIN=8`) are the SAME binary. Replay = `hven_sqp_corpus` built from
cb08a6d1f in this worktree (`tools/build.sh`, Release, SAFER_FAST, clang), serial per mode, `scripts/compare_replay.py`.

## 4. Identity

**Per factorization (the direct test of H4).** `HVEN_PF5_VERIFY` on cap 8 (`cells1/V`, `cells3/KV`, `*.pf5`):

| cell | hits verified | identical | differ | inertia/pivots differ |
|---|---:|---:|---:|---:|
| ACOPP118 | 7 411 | 7 411 | 0 | 0 |
| YORKNET | 7 688 | 7 688 | 0 | 0 |
| MESH | 2 271 | 2 271 | 0 | 0 |
| STEENBRC | 727 | 727 | 0 | 0 |
| CRESC100 | 457 | 457 | 0 | 0 |
| MSS1 | 267 | 267 | 0 | 0 |
| CRESC50 | 258 | 258 | 0 | 0 |
| SPANHYD | 116 | 116 | 0 | 0 |
| DALLASM | 0 (no repeats) | -- | -- | -- |

(`cells3/KV` runs with K0's list kept across solves, so its hits include K0's; `cells1/V` gives the same ACOPP118/
YORKNET counts without that.) Every hit sits on Pardiso's sparse arm. These cells' KKTs are all above the dense
threshold, so no dense-arm hit was verified. The dense arm's reuse saves only an allocation and is identity-safe by
construction (LAPACK over the same values into a re-zeroed buffer), but that is **argued, not measured** (section 7).
So the PF-4 worry, "phase 12 vs phase 22 with matching/scaling", does not arise. The platform session runs with
matching and scaling off, and phase 11 reproduces the same symbolic: the retained session's phase-22 numerics equal a
fresh 11+22's bit for bit.

**End to end.** Exact row equality (status, iterations, every counter, objective, residuals) A vs B:

| population | rows | movers |
|---|---:|---:|
| profile cells, cap 8 (`cells1`) | 4 | 0 |
| nlp-35 hven-sqp `default` (PF-3/PF-4's 15, `nlp35/check.txt`) | 15 | 0 |
| TRAIN hven-sqp `default` (DN-3 roster, 119 sqp rows; HOLDOUT never run; `train/check.txt`) | 119 | 0 |
| 8 repeat-heavy cells, cap 8 and cap 8 + keep (`cells3/check-*.txt`) | 8 + 8 | 0 + 0 |

(`check.txt` prints `ROW_COUNT` failures only because PF-4's checker expects 15 rows; the mover count is the result.)

**Replay** (27 cells x walk/ssn/ipm, `replay/compare.txt`): 0 differences in all three modes, cap 8 vs unset, including
the residual-class columns. **Caveat**: the replay barely exercises the path. The probe run (`replay/probe-hits.txt`)
shows 3 hits in the whole walk replay and none in ssn/ipm: the corpus cells' QP solves do not revisit faces. The
replay is therefore evidence that nothing else moved (the swap machinery, the carry, the release), not evidence of
reuse identity. That evidence is the verify mode plus the NLP populations above.

## 5. Savings

`instructions:u`, cap 8, A vs B, same binary (`*/instr*.txt`):

| population / cell | A | B | B/A |
|---|---:|---:|---:|
| ACOPP118 | 2.0635e11 | 1.7372e11 | 0.842 |
| YORKNET | 2.0349e11 | 1.8340e11 | 0.901 |
| MSS1 | 9.936e10 | 9.885e10 | 0.995 |
| STEENBRC | 3.4190e11 | 3.4102e11 | 0.997 |
| CRESC100 | 3.780e11 | 3.777e11 | 0.999 |
| DALLASM | 1.1796e10 | 1.1792e10 | 1.000 |
| **nlp-35 total (15)** | 2.0212e12 | 1.9651e12 | **0.972** |
| **TRAIN hven-sqp D total (119)** | 1.1411e12 | 1.1398e12 | **0.9989** |

These match PF-4's estimates (~16 % ACOPP118, ~10 % YORKNET). Keeping K0's list across QP solves (`cells3/instr-K.txt`)
adds almost nothing: MESH 0.9987 -> 0.9966, every other cell within 0.0003. So the release at solve exit stays.

**Cost side.** The largest TRAIN increases are +0.2 to +0.4 % on cells of 7e6 to 3e8 instructions (HS83 +0.39 %, about
1e5 instructions). This is prototype overhead per analysis: `std::getenv` on every analysis for the probe switch, and a
`SymmetricFactor` constructed per miss instead of re-analyzing in place. A landing has no getenv, and on a miss it can
re-analyze the outgoing factor in place when the list is full (or before anything is retained). The expected landed
overhead is one `std::vector` scan of at most 8 `uint64_t` per analysis. The landing gate asserts TRAIN total B/A
<= 1.000 and per-cell increases within the noise band PF-4 used.

## 6. Recommendation

**Build it**, as section 2 specifies, in one landing after pf4 merges (it edits `src/kkt/kkt_factor.cpp` and
`kkt_factor.h`, which pf4 also touches):

1. `KktFactor`: the retained list (cap `kRetainedAnalyses = 8`, MRU), stored Options, the dense counter carry,
   `dense_response_tally()` reading through it, `release_retained_analyses()`.
2. `analyze_if_needed`: lookup, then swap; on a miss, a fresh analysis committed only after success (re-analyze in
   place when nothing would be retained).
3. The discard guard in both `factorize_checked` overloads.
4. The release at the walk's solve exit (beside `release_idle_retake_sessions`) and at the SSN engine's.
5. Counter `symbolic_analysis_reuses` (QpCounters / SqpCounters, `dump_counters`, trace if wanted). PF-4's
   `backend_symbolic_analyses` and `_repeats` become declared movers. Expect them to fall by about the hit count.
6. Tests:
   - a fixture whose working set cycles A, B, A, so reuse > 0, the backend analysis count drops, and solves are bitwise
     equal to cap 0;
   - an `HVEN_TESTING` fault seam (the existing analyze/factorize injectors in `symmetric_factor_mkl.cpp`'s adapter)
     failing a swapped-in refactorization: then `EliminatedFace::holds` is false, the hot-start gate refuses, and the
     factor has no session;
   - a dense-tally test across a swap;
   - mutations: drop the discard, drop the carry, an off-by-one in the eviction.
7. Gate:
   - replay identical;
   - nlp-35 and TRAIN 0 movers;
   - `instructions:u` as section 5;
   - CI on four lanes. The macOS lane is the Accelerate evidence; Accelerate identity stays UNOBSERVED until a Mac or
     CI verify run.

Expected: ACOPP118 about -16 %, YORKNET about -10 %, nlp-35 about -3 %, TRAIN about -0.1 %, with no trajectory change.
Peak memory rises by at most 8 retained sessions per long-lived factor for one QP solve, measured at +1 to +5 MB.

Do not build hoisting for this purpose (section 1). Do not build a cross-instance cache.

## 7. Open questions

1. **PF-4's CRESC100/CRESC50 "same instance" counts** (2 842 / 1 421) are not taken by this design: 457 / 258 hits,
   even with K0's list kept across solves. Probe 2 keyed on `SymmetricFactor` instances. Possibly a moved-from or
   reused object, or a factor this probe's roles do not see (a local reused within one call). Worth one probe if
   CRESC-class cells matter; those two cells' analysis share is small (B/A 0.999 either way).
2. **Accelerate identity: UNOBSERVED.** The design's reuse path is backend-neutral, and Accelerate builds every
   numeric factorization fresh from the symbolic. Whether its symbolic phase is deterministic per pattern has not been
   observed. Run the verify instrument on the macOS CI lane, or have the landing's cycle test assert bitwise equality
   there.
3. **Dense-arm hits not measured** (no repeat-heavy cell under n = 105 was found among the measured cells). The landing's
   cycle fixture should include a dense-selected size.
4. **A byte ceiling for large KKTs** (`factor_size_bytes`-based eviction). Not needed at the measured sizes. Should a
   consumer with very large KKT factors get it before landing?
5. **Hash-only key vs exact pattern compare.** This round keeps today's trust level (64-bit hash + n + nnz). Exact
   comparison costs O(nnz) memory per entry, if a reviewer wants it.
6. **The SSN engine's `kkt_` and the predictor's factor** see no hits at the default options. Should the release at
   SSN solve exit still be added for symmetry (the design says yes)?
7. pf4's accuracy diagnosis may change the border path's call pattern (rebuild frequency), and so role 1's share. The
   gate after rebase re-derives the movers rather than carrying these numbers.
