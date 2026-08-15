# `pattern_hash` and its relationship to the sibling SQP driver's hash

`hven::pattern_hash` (`include/hven/core/pattern_hash.h`,
`src/core/pattern_hash.cpp`) is the library-wide structural key for a
sparse matrix: a 64-bit FNV-1a hash over a matrix's shape and sparsity
pattern only (its dimensions plus its index arrays) -- never its stored
values. The linear-algebra layer uses this to key a cached factorization
(does the matrix's structure still match what it was factorized from?),
and it is the same key the warm-start currency will use later to decide
whether a hand-off's own structural fingerprint still matches the model
being solved.

## Relationship to the sibling project's own hash

A sibling project (an SQP driver whose engine originated outside this
repository) already has a hash exactly like this one -- an FNV-1a
fingerprint over a sparse matrix's dimensions and index arrays, computed
with the same offset basis and prime, in the same ingredient order (rows,
then cols, then nnz, then the outer index array, then the inner index
array). That code is expected to migrate into this repository eventually,
and its own warm-start object already carries a hash produced this way, so
this primitive is written to stay comparable with it rather than reinvent
the scheme:

- same offset basis (`14695981039346656037`) and prime (`1099511628211`);
- same per-matrix ingredient order: rows, cols, nnz, outer index array,
  inner index array;
- the sibling project actually hashes three matrices (an objective
  Hessian and two constraint Jacobians) into one combined key, one after
  another, each with its own leading rows/cols/nnz triple and no separator
  beyond that. This header provides the single-matrix primitive with the
  same per-array feeding discipline; nothing here special-cases "three".

### The combined key over several matrices is NOT provided, and is not a fold

An earlier version of this page said a caller could build the sibling's
combined key by folding several `pattern_hash` results together, "or
equivalently" by extending one running `Fnv1a` accumulator across
matrices. The two are not equivalent, and neither reproduces the sibling's
key:

- **`pattern_hash` always starts from the offset basis.** It constructs a
  fresh `Fnv1a` per call (`src/core/pattern_hash.cpp`). There is no
  overload taking an accumulator to continue, and `Fnv1a`'s state cannot
  be seeded from a digest -- the struct exposes `feed`/`feed_index` and
  `value()`, no inverse. So "extending one running accumulator across
  matrices" is not something this public surface can do: it describes
  code a caller would have to write against `Fnv1a` directly, feeding
  each matrix's ingredients itself and never calling `pattern_hash` at
  all.
- **Folding digests is a different function from continued
  accumulation.** Feeding the eight bytes of a completed 64-bit digest
  into an accumulator mixes eight bytes; continuing the accumulation
  across the second matrix mixes that matrix's dimensions and every
  index-array entry. Same primitive, different byte streams, different
  results. A fold is a perfectly good combined key -- it is just not
  *this* one, and it is not the sibling's.

What IS true, and all this page claims: the single-matrix digest is
comparable with the sibling's single-matrix digest, ingredient for
ingredient, at a fixed width and byte order (see the section below).

**Which combined-key recipe hven adopts is deliberately left open, and is
settled at the warm-start migration** -- the point where the combined key
first has a consumer and the sibling's warm-start objects first have to be
read. The choice is between an append-style extension of this primitive
(reproducing the sibling's continued accumulation exactly, at the cost of
a new public entry point) and a stated fold over per-matrix digests
(nothing new to expose, no bit-for-bit compatibility with the sibling's
existing keys). Deciding it here, before either constraint is real, would
be guessing.

## One deliberate generalization, and why it is width-stable rather than a loosening

The sibling project's version feeds each index array as one contiguous
block of raw bytes, at whatever machine width Eigen's sparse storage index
happens to be on that build (typically a 32-bit `int`). This header
instead feeds each outer/inner index-array entry element-wise, each entry
widened to 64 bits (`std::int64_t`, `Fnv1a::feed_index`'s own parameter
type -- the hash's width contract is stated in fixed-width terms so it does
not depend on what `hven::Index` is aliased to), through `Fnv1a::feed_index`.

This does not weaken comparability -- it removes a build-dependent
variable from the same algorithm. `Fnv1a::feed_index` extracts the eight
bytes of the widened value by explicit shift (least-significant byte
first), rather than by reading the object representation, so the result
is additionally independent of host byte order. Scoped precisely: the
resulting hash is stable across `StorageIndex` widths and (because of the
shift-based extraction) across byte order; it is not claimed to be
"portable" in any broader sense, and it is not claimed to reproduce the
sibling project's own hash bit-for-bit on every build of that project --
only that the same algorithm, evaluated at a fixed 64-bit width and a
fixed byte order, produces one comparable value regardless of which
platform or `StorageIndex` type happens to be in play on either side.

## What this does not cover

This page documents this repository's own primitive and its relationship
to the *one* sibling hash it was built to track. Auditing every structural
hash the sibling project holds, and prescribing how the eventual migration
should reconcile them, is scoped to the migration plan itself, not to this
page.
