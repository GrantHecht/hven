# `pattern_hash` — the structural key, and the combined key over several matrices

`hven::pattern_hash` (`include/hven/core/pattern_hash.h`,
`src/core/pattern_hash.cpp`) is the library-wide structural key for a
sparse matrix: a 64-bit FNV-1a hash over a matrix's shape and sparsity
pattern only (its dimensions plus its index arrays) -- never its stored
values. The linear-algebra layer uses this to key a cached factorization
(does the matrix's structure still match what it was factorized from?),
and it is the same key the warm-start currency will use later to decide
whether a hand-off's own structural fingerprint still matches the model
being solved. That second consumer is not wired up yet; the header says the
same thing in the same tense, and neither should be read as describing
something the tree does today.

## The surface

| entry point | what it is |
|---|---|
| `Fnv1a` | the accumulator: `feed` (raw bytes), `feed_index` (one 64-bit-widened index, LSB first), `value()` (read without consuming) |
| `feed_pattern(h, A)` | feeds ONE matrix's ingredients into a running accumulator. Uncompressed-tolerant. The whole recipe lives here |
| `pattern_hash(A)` | the single-matrix digest: a fresh `Fnv1a`, one `feed_pattern`, `value()`. **Requires a compressed matrix** (see below) |
| `combined_pattern_hash(A, B, ...)` | the combined key over several matrices: one accumulator, `feed_pattern` per matrix, in argument order |

### The ingredient order, stated exactly

Per matrix, in this order, every ingredient through `Fnv1a::feed_index`
(so every one of them is widened to 64 bits and fed least-significant byte
first):

1. `rows`
2. `cols`
3. `nnz`
4. the outer offset array, `rows + 1` values: `0`, then the running total
   of stored entries per row
5. the inner index array, `nnz` values: each row's stored column indices,
   in stored order, rows in order

Two matrices with the same shape and the same sparsity pattern hash
identically, regardless of their stored values; two matrices differing in
shape or pattern are not expected to collide.

## The combined key IS an append-style continuation, and is NOT a fold

`combined_pattern_hash(A, B, C)` threads **one** `Fnv1a` through the three
matrices: `B`'s ingredients are mixed into the state `A` left behind, and
`C`'s into the state `B` left behind. There is no separator between
matrices beyond each one's own leading `rows`/`cols`/`nnz` triple, and
order is significant.

This was, until the entry point existed, deliberately left open on this
page; it is now settled, and the alternative is recorded here because the
two recipes are easy to confuse:

- **A fold over per-matrix digests is a different function.** Feeding the
  eight bytes of a completed 64-bit digest into an accumulator mixes eight
  bytes; continuing the accumulation across the next matrix mixes that
  matrix's dimensions and every index-array entry. Same primitive,
  different byte streams, different results. A fold is a perfectly good
  combined key -- it is simply not this one.
  `tests/core/test_pattern_hash.cpp`'s
  `CombinedKeyIsContinuedAccumulationNotAFoldOverDigests` pins the two
  apart so the distinction cannot be lost by a later "equivalent"
  rewrite.
- **The continuation was chosen because it is the stronger key at the same
  cost.** It mixes every ingredient of every matrix into one 64-bit state;
  a fold discards all but 64 bits per matrix before combining. Both are
  O(total nnz); only one of them is a hash of the whole input.

Two properties follow, and both are pinned by tests:

- `combined_pattern_hash(A) == pattern_hash(A)` for every compressed `A` --
  the one-matrix case of the combined key is the single-matrix digest, not
  a differently-seeded relative of it.
- A caller who needs to interleave ingredients of its own (sizes, flags, a
  bound-row list) with matrix patterns builds its own `Fnv1a` and calls
  `feed_pattern` between its own `feed_index` calls. The combined key is
  the convenience spelling of that, not a separate scheme.

## Uncompressed tolerance is a contract

`feed_pattern` hashes a matrix in Eigen's uncompressed state to the
**bit-identical** digest it would produce for that same matrix after
`makeCompressed()`. Equal structures hash equal whatever storage state
they are in, so a hash-keyed reuse decision cannot be flipped by a
caller's compression bookkeeping. That matters because the SQP engine's
matrices are caller-supplied and `QpProblem` imposes no compression
requirement, while the linear tier's matrices are assembled compressed by
the library itself; a single key has to be right for both without either
side paying for a compressed copy.

The equality is **not** maintained by comparing the two storage states, and
nothing in the implementation branches on the state to make the digests
agree. There is one stream, and both states produce it:

- **The outer offsets are derived, not read.** They are a running total of
  stored entries per row, starting at zero. In the compressed state that
  derivation reproduces `outerIndexPtr()[0..rows]` element for element --
  reproducing it is what "compressed" means. In the uncompressed state it
  reproduces what `makeCompressed()` *would* write there, because Eigen
  builds that array by the same prefix sum.
- **The inner indices are walked with `InnerIterator`**, which is exact in
  both states. Reading `innerIndexPtr()[0..nnz)` as one block is the thing
  the tolerant path must not do: in the uncompressed state that array
  interleaves stored entries with reserved-but-unused slots, so a block
  read hashes a slice that is neither the pattern nor a stable function of
  it -- two genuinely different structures could collide there and be
  wrongly reused.
- The one thing the two states genuinely spell differently is where a row
  keeps its stored count: `innerNonZeroPtr()` when the matrix carries
  per-row free space, the difference of two outer offsets when it does
  not. That is the same pair of accessors Eigen's own `InnerIterator` uses
  to bound a row. It is a storage question, not a digest adjustment -- no
  ingredient's value depends on which arm answers.

The tests that hold this down are the gapped ones, because a dense fixture
proves nothing here: an interior empty row, leading and trailing empty
rows, a no-stored-entries matrix, and every uncompressed fixture built with
reserved-but-unused slack in every row. One arm asserts the pinned literal
value (below) in **both** storage states, so a self-consistent double error
-- one that moved both paths together -- fails too.

### Why `pattern_hash(A)` still requires a compressed matrix

It throws `std::invalid_argument` on `!A.isCompressed()`. That is this
entry point's contract, not the recipe's limitation: `feed_pattern` would
hash the matrix correctly. What the check buys is a boundary guard for the
tier this signature serves -- the KKT and linear layers, whose backends
read a compressed CSR and reject anything else
(`SymmetricFactor::analyze`/`factorize` throw on the same condition) -- so
a caller who forgot `makeCompressed()` fails at the first call that touches
the matrix rather than one call later. Callers who want the tolerance call
`feed_pattern` or `combined_pattern_hash` directly.

## Width and byte order

Every ingredient is widened to `std::int64_t` and fed through
`Fnv1a::feed_index`, which extracts the eight bytes by explicit shift
(least-significant first) rather than by reading the object
representation. Two consequences, and they are the whole of what this hash
claims about portability:

- **Stable across Eigen's sparse storage-index width.** A structure built
  on a 32-bit `StorageIndex` and the same structure built on a 64-bit one
  produce the same digest. `hven::SpMatRM` uses Eigen's default `int`
  today (`core/types.h` explains why that width is load-bearing for the
  backends), so this is a property held in reserve rather than one the
  library exercises -- and it is asserted by test, not merely documented.
- **Stable across host byte order**, by the same shift-based extraction.

`feed_index`'s parameter is spelled `std::int64_t` and not `hven::Index`
deliberately: the stability claim is "evaluated at a fixed 64-bit width", so
it must not ride an alias a future retypedef could move. `Index` converts
to it exactly on every supported target, so no digest depends on which of
the two a caller passes.

Neither property is a claim of portability in any broader sense. The digest
is not a wire format and nothing serializes it across builds.

## The pinned value

`tests/core/test_pattern_hash.cpp` pins one exact digest for one fixed
3x4 fixture, backed by an independent in-repo derivation that
re-implements the constants and the feed order without calling `Fnv1a` at
all. The pair pins both the value and the recipe: a change to either that
kept them consistent with each other would still fail the cross-check.
Every property above is defined to agree with that value -- the tolerant
path and the 64-bit-storage-index fixtures assert the same literal rather
than merely agreeing with another computation of it.

Moving that literal is a declared re-derivation under CLAUDE.md §7, never a
fix-up.

## Relationship to the SQP engine's own structural fingerprints

The SQP engine keeps two structural keys of its own, and since the phase-C
re-key both are built on this surface rather than beside it:

- **The QP engine's hot-start reuse fingerprint**
  (`include/hven/detail/qp/qp_engine.h`, `detail::structural_hash`) is
  `combined_pattern_hash(H, Ae, Ai)` -- the combined key above, nothing
  more.
- **The SSN pattern-rebuild gate's key**
  (`include/hven/detail/qp/ssn_engine.h`) is a composite of two conjuncts:
  a digest (one `Fnv1a` fed `n`, `me`, `mi`, `mb` through `feed_index`,
  then `feed_pattern` over the three matrices -- the interleaving pattern
  this page describes as the caller-side spelling of the combined key),
  and the engine's bound-row `(var, sign)` list, which is deliberately NOT
  hashed: the gate compares it exactly against the copy cached when the
  pattern was built. The list has no counterpart on this page because it
  is not a hash ingredient anywhere.

Both engine keys therefore inherit this surface's properties: uncompressed
tolerance with no compressed copy (the QP side used to pay one; the SSN
side used to iterate on its own), and digests independent of
`StorageIndex` width and host byte order. Their values are compared only
against other computations of themselves in the same process -- nothing
pins either digest's value, and `0` remains the warm-start currency's
"no claim made" sentinel rather than any computed digest.

`values_hash` (`qp_engine.h`) is not a pattern hash at all -- it
fingerprints stored numeric values and answers a different reuse question
-- and is unrelated to everything above; the re-key did not touch it.
