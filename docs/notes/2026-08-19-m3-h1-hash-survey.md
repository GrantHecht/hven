# Phase C / H1 — the structure-hash survey

**Pinned commit:** `8400424262e575722a5a394defe4aee9c29d19d4` (`8400424`,
branch `m3`, local HEAD — *not* origin/m3, per the dispatch amendment).
Working tree clean of tracked modifications at read time; every line
citation below is a tracked file at that commit.

**Method:** read-only. Grep and Read only — no build, no test run, no
benchmark, per the co-run constraint (a counter-asserting census owns the
machine). Nothing in this note required execution to establish; where a
question would have needed one, it is recorded as a question in §8.

**Task:** plan §7 H1 (`docs/notes/2026-08-15-m3-phase-c-plan.md:1222-1255`),
which is plan §5 item 1.

---

## 1. Headline

| | |
|---|---|
| Hash-computation sites, pattern family | **3** (S1, S3, S4) |
| Hash-computation sites, other families | **3** (S2 `values_hash`; two bench fingerprints, §6) |
| Production consumer sites of a pattern-hash value | **34** |
| Production consumer sites of `values_hash` | **7** |
| **A pin asserting a hash VALUE?** | **YES — exactly one.** `tests/core/test_pattern_hash.cpp:130` pins `hven::pattern_hash`. See §5. |

The VALUE-pin verdict is the gate-relevant one and it is **not** the "none
found" the plan's phrasing anticipated. It does not block H2/H3, but it
**changes H2's license** — §5 states how.

---

## 2. Site inventory, confirmed and completed

The plan's starting table (§7 H1, four rows) is **confirmed accurate in
substance and incomplete in one respect**: it does not record the *widths*
each site feeds at, and those differ site-to-site inside a single site's own
feed. Widths are the load-bearing detail for H2, so they are given in full
here. Line numbers below are at `8400424` and have drifted from the plan's
(S2b/S2c/T5–T8 moved them); the plan's `qp_engine.h:2015`, `:2025`,
`ssn_engine.h:2961` read `:2015`, `:2025`, `:2963` today.

### S1 — `hven::solvers::detail::structural_hash` (`include/hven/detail/qp/qp_engine.h:2015`)

The QP engine's hot-start reuse fingerprint, condition (a).

Seed: `kFnvOffsetBasis = 14695981039346656037ULL` (`qp_engine.h:2008`), one
accumulator threaded across all three matrices (continued accumulation, not a
fold). Mixing step `fnv1a_mix` (`:1943-1950`), prime `1099511628211ULL`.

Feed order — `mix_pattern(h, qp.H)`, then `qp.Ae`, then `qp.Ai`
(`:2017-2019`), no separator between matrices beyond each one's own leading
triple. Per matrix (`mix_pattern`, `:1958-1976`):

| # | Ingredient | Width fed | Mechanism |
|---|---|---|---|
| 1 | `m.rows()` | **8 bytes** (`hven::Index`) | raw object representation, `fnv1a_mix(h, &rows, sizeof(rows))` |
| 2 | `m.cols()` | **8 bytes** | raw object representation |
| 3 | `m.nonZeros()` | **8 bytes** | raw object representation |
| 4 | `outerIndexPtr()[0..rows]` | **4 bytes/entry** (`SpMatRM::StorageIndex` = `int`) | one raw block, `sizeof(StorageIndex) * (rows+1)` |
| 5 | `innerIndexPtr()[0..nnz)` | **4 bytes/entry** | one raw block, `sizeof(StorageIndex) * nnz` |

Compression assumption: **tolerant, by paying**. `mix_pattern` takes a
compressed **copy** when `!m_in.isCompressed()` (`:1961-1965`) — an O(nnz)
copy plus an allocation. The comment at `:1952-1957` states the reason:
`QpProblem`'s matrices are caller-supplied. This is the "compressed-copy cost
site" the plan's H2 design constraint names.

### S2 — `hven::solvers::detail::values_hash` (`qp_engine.h:2025`)

**NOT A PATTERN HASH. OUT OF RE-KEY SCOPE.** Stating this explicitly is
H1's assigned job (plan §7 H1 table, row 2; plan §5 item 1), and it is
restated as a standalone scope statement in §4 below.

Seed and threading identical to S1. Per matrix (`mix_values`, `:1990-2006`):
rows, cols, nnz at 8 bytes each (raw), then `valuePtr()[0..nnz)` as one raw
block of `sizeof(double) * nnz`. The leading rows/cols/nnz triple is a
deliberate *shape separator*, not shared code with S1 — `:1978-1989` gives
the collision class it hardens against (mi=2 `[1,0],[-1,0]` vs mi=1
`[1,-1]`, identical value bytes, different shape). Same compressed-copy
tolerance as S1 (`:1993-1997`).

H3 leaves this untouched (plan §7 H3, "`values_hash` untouched").

### S3 — `SsnEngine::…::structure_hash` (`include/hven/detail/qp/ssn_engine.h:2963`)

The SSN pattern-rebuild gate's key. A **member function**, not a free
function — it reads `bound_rows_` off the enclosing object, which is why its
signature takes only `(qp, n, me, mi, mb)`.

Seed: `kOffsetBasis = 14695981039346656037ULL` (local `constexpr`, `:2964`);
prime `kFnvPrime` (`:2965`); local `mix` lambda (`:2967-2973`), local
`mix_index` = `mix(&v, sizeof(v))` on an `Index` (`:2974`) → **8 bytes, raw
object representation**.

Feed order (`:2999-3029`):

| # | Ingredient | Width fed | Mechanism |
|---|---|---|---|
| 1–4 | `n`, `me`, `mi`, `mb` | 8 bytes each | `mix_index` |
| 5 | `qp.H` pattern | see below | local `mix_pattern` (`:2988-2998`) |
| 6 | `qp.Ae` pattern | " | " |
| 7 | `qp.Ai` pattern | " | " |
| 8 | per `br` in `bound_rows_`: `br.var` | 8 bytes | `mix_index` |
| 9 | per `br`: `br.sign` | **8 bytes, raw `double`** | `mix(&sign, sizeof(sign))` |

The local `mix_pattern` (`:2988-2998`) feeds, per matrix: `rows`, `cols`,
`nonZeros()` (8 bytes each), then **for every stored entry, the PAIR
`(r, it.col())` at 8 bytes each** — walked with `SpMatRM::InnerIterator`.

Compression assumption: **tolerant, by construction, at zero copy.** The
rationale comment is `:2975-2987` and is worth quoting in H2's design record
because it is the argument, not just an assertion: for an uncompressed matrix
`innerIndexPtr()` holds per-vector gaps that `nonZeros()` does not describe,
so reading `nnz` entries off it hashes "a slice that is neither the pattern
nor a stable function of it". Iterating is the same O(nnz) and exact in both
storage states, and it keeps the key agreeing with `for_each_entry`
(`:2926-2959`) about what "the pattern" is — the emission walks these
matrices with `InnerIterator` too.

**S3 feeds no outer array at all.** It reconstructs (row, col) pairs. This is
the single largest structural difference between the three sites and §7's
first surprise.

### S4 — `hven::pattern_hash` (`include/hven/core/pattern_hash.h:93`, `src/core/pattern_hash.cpp:9-37`)

The library-wide primitive; the only one on a public `core/` surface, the only
one with its own doc page (`docs/pattern-hash.md`), and the only one with a
`Fnv1a` accumulator type behind it (`pattern_hash.h:26-77`).

Seed: `Fnv1a::kOffsetBasis` (`:27`), same constant. Feed order
(`pattern_hash.cpp:22-34`): `rows`, `cols`, `nnz`, `outer[0..rows]`,
`inner[0..nnz)` — **every one of them through `feed_index`, uniformly 8
bytes, extracted LSB-first by explicit shift** (`pattern_hash.h:65-71`), so
the digest is fixed against host byte order *and* against `StorageIndex`
width.

Compression assumption: **intolerant.** `pattern_hash.cpp:10-15` **throws
`std::invalid_argument`** on `!A.isCompressed()`. This is the "throw site" of
the plan's H2 design constraint.

---

## 3. The premise correction (plan §12 Q4), recorded

Plan §5's framing — "**three** mutually incompatible structure hashes … one
omitting `cols`" — is **STALE, and this survey records it as corrected**, as
plan §7 H1 requires and §12 Q4 ratifies.

Confirmed against source at `8400424`:

1. **`kkt_system.h::hash_pattern` and its Accelerate twin no longer exist.**
   A tree-wide grep for `hash_pattern` over `include/`, `src/`, `tests/`,
   `bench/` returns **no code hit** — the only surviving occurrences are
   prose: `qp_engine.h:1941-1942` ("identical constants to **the dissolved
   seam's** `hash_pattern`") and `docs/audit/static-scan-2026-08-09.csv:177`,
   a frozen audit row naming the pre-migration file. Phase B dissolved both.
2. **Neither surviving pattern site omits `cols`.** S1 mixes it at
   `qp_engine.h:1968/1971`; S3 mixes it at `ssn_engine.h:2990`; S4 mixes it at
   `pattern_hash.cpp:18/23`. The `cols`-omitting variant left with the
   dissolved pair.
3. Therefore the re-key's scope is **two SQP sites (S1, S3) plus one new core
   entry point (H2's multi-matrix surface)** — and the "removes the
   `cols`-omitting variant" benefit **was already collected by phase B** and
   must not be re-claimed in H3's commit message or the Gate C package.

---

## 4. Scope statement: `values_hash` is not in the re-key

`hven::solvers::detail::values_hash` (S2) hashes **stored numeric values**,
never a sparsity pattern. It answers reuse condition (c) — "have H/Ae/Ai's
values changed?" — which is a different question from condition (a), and the
two are checked as separate conjuncts of the same gate
(`qp_engine.h:3069-3070`). A structural re-key cannot subsume it: two QPs
with identical patterns and different values must reuse the *symbolic*
analysis and must not reuse the *numeric* factorization, and `values_hash` is
what draws that line.

**It is out of scope for H2 and H3, its digest must not move, and its seven
consumer sites (§5, table row S2) must be untouched.** H3's scope line
already says "`values_hash` untouched"; this survey confirms there is no
hidden coupling that would drag it in — S1 and S2 share only the `fnv1a_mix`
primitive and the offset-basis constant, not a feed path.

---

## 5. The VALUE-pin confirmation — **ONE PIN FOUND**

### 5.1 The finding

```
tests/core/test_pattern_hash.cpp:130
    EXPECT_EQ(pattern_hash(A), 14789870936883269507ULL);
```

`TEST(PatternHash, LiteralValuePinnedForFixedFixture)` (`:128-131`), on the
fixture `make_matrix(3, 4, {{0,1,5.0}, {1,3,-2.0}, {2,0,7.0}})`. The test's
own header comment (`:120-127`) states the intent plainly: *"the algorithm is
frozen … so an exact pin is safe"*, and records that the pin already survived
one rewrite (the `feed_index` byte-cast → shift-based change) unchanged.

It is backed by an **independent in-repo derivation** at `:138-168`
(`CrossCheckAgainstIndependentFnv1aReference`), which re-implements the
constants and the feed order without calling `hven::Fnv1a`, so the pair
together pin both the value and the recipe.

### 5.2 What it means for H2 — the license change

Plan §7 H1 says a found VALUE pin "is a declared re-derivation, never
silent", and plan §9 row 3's license is conditional on this finding. The
honest reading here is **narrower and easier than a re-derivation**, and H2
should not treat it as one:

- The pin is on **S4, `hven::pattern_hash`'s single-matrix digest.**
- **H2 is not specified to change that digest.** H2's scope (plan
  §7:1259-1274) adds a *new multi-matrix entry point* and an
  *iteration-based path that contractually produces the same digest as the
  compressed path*. Both are additive. The single-matrix compressed digest is
  the thing everything else is defined to agree with.
- So the correct handling is: **the pin is a CONSTRAINT on H2, not a casualty
  of it.** If H2 lands and `test_pattern_hash.cpp:130` still passes
  unmodified, that is positive evidence the refactor preserved the primitive.
  **If H2 finds itself editing that literal, something has gone wrong and it
  must stop and escalate** — a moved literal there is exactly the
  "silent pin mutation" CLAUDE.md §7 forbids.
- **Bonus for H2:** the pin plus the independent derivation at `:138-168` is
  a ready-made oracle for H2's required compressed/uncompressed equality
  test. H2 can build the same fixture uncompressed and assert the iteration
  path reproduces `14789870936883269507ULL` — a *value* assertion, stronger
  than an equality-between-two-computations assertion, and free.

### 5.3 What it means for H3 — nothing

H3 re-keys S1 and S3. **No pin anywhere asserts an S1 or S3 value.** Every
S1/S3 assertion in the suite is either an equality/inequality *between two
computations*, or a comparison against the literal `0` — and `0` is
`WarmStart::structure_hash`'s **sentinel** ("no model was seen / no claim
made", `warm_start.h:317` with its note at `:308-317`), not a computed digest.
So H3's digests may change value freely, exactly as plan §7 H3 assumes, and
its proof obligation stays what plan §12 Q5 ratified: a per-backend no-op over
reuse *decisions*.

### 5.4 The sweep, enumerated so the confirmation is auditable

Seven independent searches, all over tracked files at `8400424`:

| # | Pattern | Scope | Result |
|---|---|---|---|
| P1 | the literal `14789870936883269507` | whole repo | **1 hit** — `tests/core/test_pattern_hash.cpp:130` |
| P2 | `(EXPECT\|ASSERT)_(EQ\|NE)\(…(pattern_hash\|structure_hash\|structural_hash\|values_hash)…\)` filtered to those containing a ≥4-digit or ≥4-hex-digit literal | `tests/`, `bench/` | 14 hits; **all but P1 compare against `0u`** (the sentinel) or against another hash call |
| P3 | `0x[0-9a-fA-F]{12,16}` and `[0-9]{15,20}(ULL\|ull)`, excluding the FNV basis/prime | `tests/`, `bench/` | P1; `budget_table_hash`'s pin (§6); two PRNG seeds (`recipes.cpp:22,27`, `test_funnel.cpp:620`); six baseline-CSV `budget_table_hash` header lines |
| P4 | `hash` anywhere in a tracked `*.csv` | all CSVs (`bench/baselines/`, `docs/notes/data/`, `docs/audit/`) | only `budget_table_hash:` provenance headers and one frozen audit row. **No CSV carries a pattern-hash column at all** |
| P5 | `pattern_hash\|structure_hash\|structural_hash` | `scripts/` | **none** — no comparison script reads a hash |
| P6 | `pattern_hash` | `tests/golden_rig/` | 2 hits, both declaration/override (`seam.h:164`, `seam_native.cpp:55`). **No rig expected-table asserts a hash value**, and the old-seam adapters report `share_handle == false` so no cross-project value comparison exists (§7, surprise 6) |
| P7 | a `*_hash` token followed within 40 chars by a ≥6-hex-digit or ≥10-digit literal | `docs/` | **none** |

Directories covered by the union of P1–P7: `include/`, `src/`, `tests/`
(incl. `tests/golden_rig/`), `bench/` (incl. `bench/baselines/`), `docs/`
(incl. `docs/notes/data/` and `docs/audit/`), `scripts/`. Untracked working-tree
scratch was excluded deliberately, per the dispatch.

**Verdict: exactly one VALUE pin, on S4, and it is a constraint H2 should
satisfy rather than a re-derivation H2 must declare.**

---

## 6. Two non-pattern hashes, recorded so they are not swept in

Neither is a pattern hash; neither is in re-key scope. Both are recorded
because a future reader grepping for "FNV" in this repo will hit them.

- **`budget_table_hash`** (`bench/corpus_cells.h:1043-1055`, exported at
  `:1142`) — FNV-1a over the *decimal text* of five wall-budget constants, a
  provenance fingerprint stamped into every corpus CSV
  (`bench/bench_corpus.cpp:523`). Its value **is pinned**, at
  `tests/sqp/test_corpus_cells.cpp:752` (`0x357aee91dee27391`) and in six
  tracked baseline CSVs. Out of scope; **but see the next bullet, which is
  why it is worth a paragraph.**
- **`ssn_safeguard_probe`'s `sig`** (`bench/ssn_safeguard_probe.cpp:832`) — a
  running signature over probe counters.

**Both seed from `1469598103934665603ULL`, which is NOT the FNV-1a 64-bit
offset basis.** The real basis is `14695981039346656037` (20 digits); these
use a 19-digit value — a dropped digit somewhere in this constant's history.
It is harmless (any fixed seed produces a usable fingerprint) and it is
**load-bearing precisely because it is wrong**: `budget_table_hash`'s pinned
`0x357aee91dee27391` was computed with it. "Correcting" the constant would
silently break a pin in a test and six baseline artifacts. **Do not fix it as
a drive-by.** If it is ever changed it is a declared re-derivation under
CLAUDE.md §7 like any other. Flagged here rather than filed as a defect
because it is not one.

---

## 7. Things that would surprise H2 and H3

**1. The three pattern sites feed three genuinely different byte streams —
S3 does not feed an outer array at all.** S1 feeds the outer and inner arrays
as raw 4-byte-per-entry blocks; S4 feeds the same two arrays element-wise at 8
bytes LSB-first; **S3 feeds neither, emitting a `(row, col)` pair per stored
entry instead.** Plan §7 H2 reasons that "the outer offsets are derivable
while iterating" — true, and that is the right design, but it means the
iteration path H2 must write **does not exist anywhere today**. S3 is not a
prior art for it. Re-keying S3 onto H2's surface is a full stream change with
a different *shape* of ingredient, not a re-spelling of the same one.

**2. Widths are inconsistent *inside* S1's own feed.** S1 mixes `rows`,
`cols`, `nnz` at 8 bytes (`hven::Index`) and then the index arrays at 4 bytes
(`SpMatRM::StorageIndex`, which `core/types.h:73-78` documents as
`int` **"deliberately and load-bearingly"**, and calls "the width that
matters", explicitly contrasting it with `Index`). So S1's digest is
`StorageIndex`-width-dependent. S4's is not. H2's element-wise `feed_index`
removes this variable, which is the stated point.

**3. S1 and S3 read object representations; S4 does not — so today's two SQP
digests are host-endian-dependent and the core one is not.** S1's `fnv1a_mix`
and S3's `mix` both take `(const void*, len)` over an lvalue's bytes
(`qp_engine.h:1970-1972`, `ssn_engine.h:2974`). `Fnv1a::feed_index`
(`pattern_hash.h:65-71`) extracts by shift. The re-key therefore *adds* a
byte-order-stability property to the SQP side. No pin depends on either state
(all three sites are compared only against themselves, same process, same
build), so this is a property gain with no artifact cost — but H3's commit
message should claim it rather than leave it as a silent side effect.

**4. `feed_index`'s parameter is literally `std::int64_t`, not `hven::Index`,
and that is a contract.** `pattern_hash.h:59-64` states it in capitals: *"the
hash's stability claim is 'evaluated at a fixed 64-bit width', so it must not
ride an alias that a future retypedef could move."* This is S2c's residue —
S2c redefined `hven::Index` onto `Eigen::Index` and retired the last alias, and
this parameter was deliberately left *not* following it. **H2 must not
"tidy" `feed_index(std::int64_t)` into `feed_index(Index)`** when it writes
the multi-matrix surface. `core/types.h:42-61` backs the width with two
`static_assert`s, so the conversion is exact on every supported target and no
digest depends on which spelling a caller uses.

**5. The offset-basis constant is written out four times.** `Fnv1a::kOffsetBasis`
(`pattern_hash.h:27`), `detail::kFnvOffsetBasis` (`qp_engine.h:2008`), a local
`constexpr` inside S3 (`ssn_engine.h:2964`), and twice more in
`tests/core/test_pattern_hash.cpp` (`:95`, `:139` — the latter deliberately,
as the independent cross-check). H3 collapses two of these. The test-side
duplicates must **stay** duplicated: `:139`'s whole value is that it does not
share a definition with the implementation.

**6. The golden rig exposes `pattern_hash()` on `SeamHandle` and asserts
nothing with it.** `tests/golden_rig/seam.h:164` declares it pure-virtual;
`seam_native.cpp:55` is the only override; no expected table or comparison
consumes it, and the OLD-SEAM adapters report `Capabilities::share_handle ==
false` and never produce a handle. So **there is no cross-project hash-value
comparison anywhere in the rig**, and H2/H3 cannot break one. (This was the
most plausible place for a hidden cross-implementation VALUE pin to be
hiding, which is why it is called out rather than merely absent from §5.4.)

**7. `0` is a live sentinel in the same value space as an S1 digest, with no
guard.** `WarmStart::structure_hash == 0` means "no claim made"
(`warm_start.h:317`; producers at `mesh_transfer.h:681` and
`src/warmstart/warm_start.cpp:179` set it *by construction*; the ingest gate
reads it at `src/drivers/sqp_driver.cpp:374`). A genuine `structural_hash`
result of exactly `0` would alias the sentinel and silently downgrade a warm
object to `kSeeded`. This is a pre-existing 2⁻⁶⁴ exposure, **not introduced
by the re-key** — but H3 changes the digest, which moves *which* preimage
hits it. Failure mode is conservative (an unnecessary cold-ish start, never a
wrong reuse), so this is a note, not a defect. No guard exists and none is
proposed here.

**8. S3's collision guard is real but knowingly untestable, and H3 must not
disturb it.** `ssn_engine.h:2902-2925` records that this file *used to claim*
the key was "not the only guard" and that the claim was **false** (Fable
kernel review, M-1). The second guard is now the refresh path's exact
map-consumption check (`t == value_pos_.size()`, thrown at `:3044-3049`).
Reaching it requires forging an FNV-1a collision, so **a mutation removing it
survives the sweep** — recorded there as a knowingly-unkillable line. H3
changes the key that feeds this guard; it must leave the guard itself
byte-identical, or the M-1 finding reopens.

**9. `br.sign` is fed as eight raw `double` bytes, the only non-integer
ingredient in any of the three sites** (`ssn_engine.h:3027-3028`). H3's
composite key must keep it (plan §7 H3 makes preserving it a
*behavior-preservation* obligation, since dropping it would still be
correct). Note the mechanism mismatch: if H3 routes the structural part
through `feed_index` and leaves `sign` on a raw-bytes `feed`, the composite
becomes half byte-order-stable — worth deciding explicitly rather than by
default. Feeding `sign` as a comparison conjunct rather than a hash
ingredient (plan §7 H3 permits either) sidesteps it entirely.

---

## 8. Consumer inventory, at `8400424`

### S1 — `detail::structural_hash`, 15 production consumer sites

| Site | Role |
|---|---|
| `include/hven/detail/qp/qp_engine.h:3023` | computed once per `run()` into `current_structural_hash` (`:3015-3019` explains the once-only discipline) |
| `qp_engine.h:3069` | reuse gate, conjunct (a) of `reuse_eligible` |
| `qp_engine.h:3514` | exit-commit into `border_structural_hash_`, `kOptimal` only |
| `qp_engine.h:5125` | the `border_structural_hash_` member |
| `qp_engine.h:2825` | `prev_border_structural_hash` snapshot |
| `qp_engine.h:2251` | `HotState::structural_hash` field |
| `qp_engine.h:2380` | emitted into a `HotState` by `hot_state()` |
| `qp_engine.h:2804` | adopted from a `HotState` |
| `include/hven/detail/warmstart/warm_start.h:317` | `WarmStart::structure_hash` — the carried value, and the sentinel |
| `src/drivers/sqp_driver.cpp:374` | ingest gate, `warm.structure_hash != 0` |
| `src/drivers/sqp_driver.cpp:398` | **the values-only probe** — the match that resolves `kWarm`/`kHot` |
| `src/drivers/sqp_driver.cpp:2400` | exit-commit, `qp_built` path |
| `src/drivers/sqp_driver.cpp:2404` | exit-commit, zero-major probe path |
| `include/hven/detail/warmstart/mesh_transfer.h:681` | producer that writes the sentinel by construction |
| `src/warmstart/warm_start.cpp:179` | `from_interior_point`, writes the sentinel by construction |

`predictor.h` carries the field forward unchanged without reading it
(`:812`, `:1392`) — a pass-through, not a consumer.

**Consumer-location note (T7/T5 relocations, already landed).** Two of the
above moved in commits that are *already in* this survey's pinned HEAD, so
they are recorded at their **current, final** locations and need no
"will relocate" annotation:
`src/drivers/sqp_driver.cpp` is T5's TU (`de97f5d`), carrying the ingest gate
and the probe out of `include/hven/drivers/sqp_driver.h`; and
`src/warmstart/warm_start.cpp` is T7's TU (`a4db5f0`), carrying the
interior-point crossover's ingest out of
`include/hven/detail/warmstart/warm_start.h`. T8 (`8400424`) added
`src/warmstart/continuation.cpp`, which consumes no hash.

### S2 — `detail::values_hash`, 7 production consumer sites (out of scope, listed for the untouched-check)

`qp_engine.h:2252` (HotState field), `:2805` (adopt), `:2826` (snapshot),
`:3024` (compute), `:3070` (gate conjunct (c)), `:3515` (exit-commit),
`:5126` (member).

### S3 — `SsnEngine::structure_hash`, 3 production consumer sites, engine-internal only

`ssn_engine.h:3036` (computed in `sync_matrix`), `:3038` (the rebuild gate,
`key == structure_key_`), `:3099` (committed into `structure_key_`; member at
`:3581`, with `has_structure_` at `:3582`). **The value never leaves the
engine** — no field, no hand-off, no test reads it directly.

Two call sites reach it, both through `sync_matrix`: `ssn_engine.h:1688`
(once per SSN solve, setting `pattern_rebuilds`) and `:3261`
(`set_prox_sigma`, once per proximal-ladder rung, return deliberately
discarded). This matches B1's measured ≈ 0.15 calls per SSN major
(plan §7 H3:1311-1314). Since it is unconditional in `sync_matrix` (`:3036`,
before the gate), **every prox rung pays a full O(nnz) iteration pass** —
H3 should price its re-key against that, not against the gate's hit rate.

### S4 — `hven::pattern_hash`, 16 production consumer sites

KKT tier: `src/kkt/kkt_calls.cpp:16` (`analysis_decision`, the only hash the
short-circuit skips — `:10-15`), `:30` (`factorize_checked` records the
decision's hash rather than recomputing, `:26-29`), and the
`KktFactor::analyzed_pattern` field (`include/hven/detail/kkt/kkt_calls.h:52`).

Linear tier, MKL: `src/linear/symmetric_factor_mkl.cpp:623` (`analyze()`
stamps `pattern_hash_`), `:642-648` (`factorize()`'s **throwing** guard),
`:766` (handle construction), `:800` (adopt), `:874` (accessor).
Accelerate twin, same five roles: `src/linear/symmetric_factor_accelerate.cpp:570`,
`:589-595`, `:707`, `:735`, `:809`.
Declarations: `include/hven/linear/symmetric_factor.h:893` (`SymmetricFactor::pattern_hash_`),
`:952` (accessor), `:971` (`Factorization::pattern_hash_`).

Identity context: `pattern_hash` is one leg of the naming triple
`(pattern_hash, session_id, epoch)` — `symmetric_factor.h:222`,
`include/hven/detail/linear/session_id.h:6-18`. **H2 must not move S4's
digest** without treating that triple as re-derived; §5.2 argues it should not
need to.

Test-tree readers (not pins, listed for H2's blast radius):
`tests/sqp/test_kkt_calls.cpp:70,127`;
`tests/linear/test_symmetric_factor.cpp:883,1082`;
`tests/core/test_pattern_hash.cpp` throughout;
`tests/golden_rig/seam.h:164` + `seam_native.cpp:55` (unasserted, §7.6).
Note `tests/CMakeLists.txt:204` recompiles `core/pattern_hash.cpp` into the
`HVEN_TESTING` executable, so a change to the primitive lands in two binaries.

---

## 9. The design constraint H1 hands to H2 (plan §12 Q3), with its three pieces of evidence

The three sites disagree about compression, and the disagreement is
*principled at each site*, which is why no naive unification works:

1. **The throw site.** `src/core/pattern_hash.cpp:10-15` — `hven::pattern_hash`
   throws `std::invalid_argument` on an uncompressed matrix. Justified at
   `pattern_hash.h:90-92`: an uncompressed matrix's index arrays "do not
   describe its pattern on their own". Its callers are all internal
   (`kkt_calls.cpp`, the two `SymmetricFactor` backends) and all hand it a
   matrix the library assembled and compressed itself, so the throw has never
   fired in production.
2. **The `InnerIterator` rationale.** `ssn_engine.h:2975-2987` — S3 iterates
   *because* `qp.H/Ae/Ai` are caller-supplied and `QpProblem` imposes no
   compression requirement, and because reading `nnz` entries off an
   uncompressed `innerIndexPtr()` would hash a gap-contaminated slice that "is
   neither the pattern nor a stable function of it, so two genuinely different
   structures could collide and be wrongly reused". Iteration is the same
   O(nnz) and exact in both storage states.
3. **The compressed-copy cost site.** `qp_engine.h:1952-1957` and
   `:1961-1965` — S1 resolves the same problem the third way, by taking a
   compressed copy when and only when the input needs one. The comment notes
   the common case (already compressed) binds straight through, so the copy is
   a tail cost, not a per-call one.

**The constraint:** a re-key that routes S1 and S3 through `hven::pattern_hash`
as it stands would either **throw on legal input** (S3's matrices) or **force
an O(nnz) compressed copy plus an allocation per SSN major** — and B1 priced
exactly that unit at **≈ 7.3 % of SSN-major wall on the SSN-heavy families**
for one added O(nnz) pass (plan §7 H3:1304-1310;
`docs/notes/data/2026-08-15-m3-b1-hash-cost/`). So it is a measured cost, not
a theoretical one.

H2's answer, per its own scope: an **iteration-based, uncompressed-tolerant
path that contractually produces the same digest as the compressed path**, with
the equality pinned by a test. The outer offsets are derivable while iterating
(a running count of entries emitted per outer vector reproduces
`outer[0..rows]` exactly), so the contract is achievable — but §7.1 is the
warning: nothing in the tree does this today, and the failure mode plan §7 H2
names is a digest that agrees on dense-ish fixtures and disagrees on a
**gapped** matrix (an outer vector with zero stored entries, or an
uncompressed matrix with reserved-but-unused slots). **The equality test must
include an empty-row fixture and a genuinely uncompressed fixture**, or it
proves nothing. `tests/core/test_pattern_hash.cpp:66-87` already has
zero-by-zero, no-nonzeros, and uncompressed fixtures to build on.

Plan §7 H2's escalation trigger applies: if that equality does not fall out of
the first design, escalate to Fable — the failure class is wrong-reuse
(silent data corruption), not a test failure.

---

## 10. Concerns and open questions

Recorded rather than resolved, since resolving any of them would have needed
a build or a run, which this dispatch forbids.

1. **The one VALUE pin needs an explicit disposition in H2's plan of record.**
   §5.2 argues it is a constraint H2 satisfies, not a re-derivation H2
   declares. That reading should be ratified before H2 starts, because the two
   readings license opposite actions on `test_pattern_hash.cpp:130`.
2. **Unverified by execution: that `test_pattern_hash.cpp:130` currently
   passes.** I read it; I did not run it. The claim "the pin holds at
   `8400424`" is inferred from the file being tracked in a suite the phase's
   own gates run, not observed. H2 should confirm on its first build.
3. **Unverified by execution: S3's per-major call count.** The ≈ 0.15
   calls/major figure is B1's, quoted from the plan, not re-measured here. The
   *structural* claim in §8 (unconditional in `sync_matrix`, two call sites)
   is read directly from source and is solid.
4. **`docs/pattern-hash.md:63-71` still says the combined-key recipe is
   "deliberately left open, and is settled at the warm-start migration".**
   H2 closes it (append-style continuation, per plan §7 H2:1261-1264). That
   paragraph, and the "What this does not cover" section at `:96-102`, need
   rewriting in H2's commit, not a later sweep — the page currently documents
   a decision H2 will have made.
5. **`docs/pattern-hash.md` and several headers describe S1/S3 as belonging to
   "a sibling project" that "is expected to migrate into this repository
   eventually"** (`pattern-hash.md:13-32`, `pattern_hash.h:11-14`). That
   migration has happened — S1 and S3 are in this tree. The prose is stale
   against CLAUDE.md §1's origin-neutrality rule and reads oddly now that both
   hashes are local. Not H1's to fix and not blocking; flagged for S4b's
   deferred reference sweep, or for H2 to fold in while it is editing that
   page anyway.
6. **The 19-digit FNV seed in `bench/` (§6) is a trap for a future tidy-up
   pass.** It is not in H2/H3's path, but the pinned `0x357aee91dee27391`
   depends on it. Suggest a one-line comment at `bench/corpus_cells.h:1047`
   recording that the constant is intentionally left as-is — a separate,
   trivial commit, not H2's or H3's.

---

**Post-survey pointer (added at commit time):** concern 1 (§10) — the two
readings of the VALUE-pin clause — was RULED 2026-08-19: the §5.2 reading
is adopted with three binding riders on H2 (literal frozen; literal-arm
oracle REQUIRED in the compressed/uncompressed equality test; H2's first
build observes the pin at its base). The byte-order claim and the
FNV-seed guard-comment dispositions were also ruled there. See
`tycho_sqp:docs/notes/2026-08-19-m3-h1-ruling.md` (SIGNOFF
H1-RULING-FINAL).
