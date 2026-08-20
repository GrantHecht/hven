# M3 phase-C T6+T7+T8 — the battery, in numbers

The last three carves of the phase-C T-series, landed as three commits sharing
one census window per the owner's 2026-08-19 amendment. This note is the
tracked record of what the proof battery measured; the reasoning behind each
carve lives in its commit message and in the TU banners.

| | |
|---|---|
| base | `6be0dbb` |
| instrument maintenance | `58f97d8` — `psym_compare.sh` |
| T6 elastic/SOC/restoration | `0a76fe0` — `src/globalization/sqp/soc_elastic_restoration.cpp` |
| T7 warm-start ingest | `a4db5f0` — `src/warmstart/warm_start.cpp` |
| T8 continuation driver | `8400424` — `src/warmstart/continuation.cpp` |
| census baseline of record | `bench/baselines/2026-08-16-u0-corpus/walk_baseline.csv` |
| toolchain / box | clang 22.1.8, AMD Ryzen 7 5800X3D, Fedora, Release, `HVEN_FP_MODE=SAFER_FAST` |

## P-CENSUS (shared, at the combined head)

Launched 2026-08-19T22:07:35-04:00 (02:07:35Z) from the combined head's own
corpus binary, provenance stamp `8400424262e5`, against the baseline of record.
Ratified `parallel-tiered v1`: T1 = 44 cells 6-wide pinned, T2 = 3 SOLO,
T3 = 10 5-wide at full budget. ALONE on the machine for the whole window; load
average held at exactly 1.00 through the solo tier.

```
57 baseline cells, 57 fresh cells, 0 mismatches
serial-confirm list: 0 cell(s)
=== census done (merge_rc=0 compare_rc=0 rows=57/57) ===
```

| tier | window (UTC) | duration |
|---|---|---|
| T1, 44 cells 6-wide | 02:07:35 → 02:44:05 | 36.5 min |
| T2, 3 cells SOLO | 02:44:05 → 05:26:17 | 2 h 42 min |
| T3, 10 cells 5-wide | 05:26:17 → 08:06:44 | 2 h 40 min |
| **total** | | **5 h 59 min** |

**ZERO mismatches, zero cells needing serial confirmation.** This is the sixth
consecutive full census since U0 to find no counter movement, and the first
taken under the owner's 2026-08-19 amendment — one run covering three
boundaries. No bisection was needed and no extra census was run.

*One wall-clock observation, recorded with its exoneration.*
`f7_n5000_path_warm` ran 4542 s against a frozen-baseline wall of 2188 s while
returning the baseline's own `NumericalError`. T5's census on this tree recorded
**4497 s / NumericalError** on the same cell and passed with zero mismatches, so
the 2× wall is a pre-existing property of the cell in the post-U0 tree,
reproduced to within 1 %, and not something these commits introduced. Wall is
informational under CLAUDE.md §7 in any case. Related and worth recording where
someone will find it: an `f7_n5000` cell's 3600 s deadline **restarts when the
child signals setup-complete** (`bench_corpus.cpp:1019-1020`), so such a cell is
bounded by ~2 × 3600 s, which is why a 4542 s cell is bounded rather than
runaway.

## P-BENCH (base vs combined head)

**Counters: 36 asserted columns × 17 cells × 8 reps — ALL IDENTICAL.** That is
the unconditional bar the FP-carrying carve rules set, and it is met for all
three boundaries at once. `bench_scale --self-check` passed on both arms, which
matters here because T8 put `bench_scale.cpp.o` inside a P-SYM radius.

Wall-clock, judged under the OWNER RULING as amended 2026-08-18:

| limb | cells | old (s) | new (s) | Δ | worst cell | gate |
|---|---|---|---|---|---|---|
| sub-second | 10 | 3.07259 | 3.07098 | −0.052 % | +0.43 % | +1.5 % one-sided ✓ |
| engine-heavy | 7 | 31.93933 | 31.97030 | +0.097 % | +0.32 % | +0.5 % ✓ |
| pooled | 17 | 35.01191 | 35.04128 | +0.084 % | — | — |

No cell fails either gate. §5's revert clause is not triggered.

## P-BUILD (base vs combined head) — a COST, recorded as one

| arm | median wall (`-j16`, `CCACHE_DISABLE=1`, 3 runs) | peak RSS |
|---|---|---|
| `6be0dbb` | 67.17 s | 668688 KiB |
| `8400424` | 68.47 s | 668488 KiB |

**+1.30 s (+1.9 %), ranges disjoint by 0.48 s; RSS flat.** The three commits add
15.06 s of serial compile work (the three TUs' no-PCH cost) and remove much less
than that: T5's build win came from deleting ~2 330 lines of body from a header
that 20 objects instantiated, whereas these three delete ~370 / ~115 / ~290
lines from headers whose bodies only 3 / 4 / 5 objects ever emitted. The
build-side case for these boundaries is therefore **not** wall-clock; it is
header hygiene and ~840 KB of duplicated object code removed. Recorded as a
cost rather than explained away.

## P-SYM (per commit; radius declared in its own file before each compare)

| commit | objects | byte-identical | in-radius differing | noise-class | archive |
|---|---|---|---|---|---|
| T6 | 68 | 63 | 3 | 1 (`bench_corpus` stamp, CHANGED 0) | `libhven.a` |
| T7 | 69 | 64 | 4 | 0 (stamp rode inside a radius row) | `libhven.a` |
| T8 | 70 | 63 | 5 | 1 (`bench_corpus` stamp, CHANGED 0) | `libhven.a` |

Zero class-(a) (`__LINE__`) rows in all three. Every in-radius object shrank;
none was flat. Nothing outside any declared radius differed, and no exception
was claimed beyond the two pre-declared stamp rows.

## The relocation proof, both arms, per commit

This is the leg the T5 review added (F3: prove it on **both** sides), with its
section set stated explicitly (F1: `.text` alone undercounts).

| commit | before | after |
|---|---|---|
| T6 | every moved definition already an out-of-line direct call out of `solve_impl`; 742 sites / 105 distinct | **identical callee-for-callee**, 742 / 105, same two symbol ranges, same byte extents |
| T7 | `from_interior_point` never inlined; 23 direct sites across 4 weak copies | **23 direct sites**, now to one library symbol |
| T8 | `run_continuation` 2/1/28/1/5, its closure 3 per object, `validate` 1 per object | caller counts **unchanged**; 129 → 126 sites accounted to the digit |

T8's 129 → 126 is the whole of the difference in `run_continuation`'s own
histogram, and it decomposes exactly: the three calls to its `record` closure
stop needing relocations (the closure became a local symbol in the same section
as its caller), and two constant-pool labels renumbered. Every other callee and
count is identical.

## P-PCH (T9, per commit)

Source count 25 → 26 → 27 → 28, one per commit. Controls reproduced across all
three measurement sessions to within 0.08 s.

| TU | no-PCH | with-PCH | Δ | object | joins list? |
|---|---|---|---|---|---|
| `globalization/sqp/soc_elastic_restoration.cpp` | 5.58 | 3.70 | −1.87 | differs | no |
| `warmstart/warm_start.cpp` | 3.25 | 1.75 | −1.50 | differs | no |
| `warmstart/continuation.cpp` | 6.23 | 4.35 | −1.87 | differs | no |

Sixth, seventh and eighth consecutive SQP-side TUs in the "faster but not
byte-identical" group. **The T-series closes with a clean sweep**: all seven TUs
it added compile faster with the PCH and not one produces a byte-identical
object. Admitting them would mean relaxing bar 2, which `docs/build.md` is
explicit is a numerics-governance decision; eight measurements are now the
evidence for that conversation.

## P-SUITE (per commit)

Nine full runs, three per commit, **0 failures** throughout: Release 1153/1153,
Debug 1153/1153 (Eigen asserts live), Release-without-SNOPT 1149/1149. No test
added or retired at any step.

## One declared deviation from a plan row

`set_elastic_penalty` — named in the T6 row as one of the elastic tier's "four
functions" — **stayed inline in `elastic.h`**. It emits no symbol in any of the
68 objects and has zero direct call sites anywhere, which is T4's exact test for
full inlining at every use; and it is the one function in that row for which the
row's own justification ("the elastic builders allocate, so call overhead is
already dominated") is false, since it allocates nothing. Moving it would have
been the single edit capable of falsifying the premise the rest of the carve
rests on. The header carries the measurement as its reason.

## P-SUITE and the install lane

Nine full suite runs, three per commit, **0 failures** throughout: Release
1153/1153, Debug 1153/1153 (Eigen's asserts live), Release-without-SNOPT
1149/1149. No test added or retired at any step.
`scripts/check_install_smoke.sh` at the combined head: **PASSED**.

## What this unit is, in one line each

- **T6** moved 23 definitions out of three headers into one TU, consolidated
  `RestorationModel`'s vtable/typeinfo to a single emission, and left
  `set_elastic_penalty` alone on measured evidence.
- **T7** moved the crossover's ingest and deliberately left the tree's template
  where the n = 1e6 path needs it.
- **T8** moved the continuation driver, its validator and its closure, and
  accounted for every one of the three relocation counts that changed.
- The unit costs **+1.9 % of build wall-clock** and buys header hygiene plus
  ~840 KB of de-duplicated object code. Runtime is neutral to the limits of the
  instruments: counters bit-identical across 57 census cells and 17 bench cells
  × 8 reps, and both wall-clock limbs inside their thresholds.
