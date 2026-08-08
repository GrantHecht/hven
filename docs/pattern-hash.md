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
  same per-array feeding discipline; a caller that needs a combined key
  over several matrices gets it by calling `pattern_hash` matrix by
  matrix and folding the results together, or equivalently by extending
  one running `Fnv1a` accumulator across matrices -- nothing here
  special-cases "three".

## One deliberate generalization, and why it is width-stable rather than a loosening

The sibling project's version feeds each index array as one contiguous
block of raw bytes, at whatever machine width Eigen's sparse storage index
happens to be on that build (typically a 32-bit `int`). This header
instead feeds each outer/inner index-array entry element-wise, each entry
widened to the portable 64-bit `Index` alias (`types.h`), through
`Fnv1a::feed_index`.

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
