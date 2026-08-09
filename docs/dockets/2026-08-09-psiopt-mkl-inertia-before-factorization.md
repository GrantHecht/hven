# The interior-point seam has no pre-factorization inertia state at all (MKL)

**Status: CONFIRMED.** Observed on Linux/MKL, hven commit `ca2744c1ba41`,
against the live `tycho` checkout. Machine `fedora-x86_64-linux`, Intel
oneAPI MKL 2026.1 (Product Build 20260612), thread pin `process-global=1`.

**Trace: P5. Arm: `psiopt-old@mkl`. This trace PASSES on that arm — and
the pass is the finding.** That needs saying up front, because every other
record here is written from a failure.

P5 passes on this arm because the rig's adapter answers `kUnavailable`.
The adapter answers `kUnavailable` because the seam has no answer to
report faithfully — reading it would be reading uninitialized memory. So
the trace is green on the strength of the *adapter's* honesty, not the
seam's, and if the adapter had reported what the seam actually holds, the
result would have been a value that is not merely wrong but different on
every run. A trace cannot fail-by-design against undefined behaviour; it
can only decline to look. This record is what "declined to look" is
written down as.

## What the old seam does

The interior-point seam's inertia counts are plain `int` members
(`psiopt/include/tycho/detail/solvers/linear/pardiso_interface.h:230-231`):

```cpp
int neigs_; // iparm[22]
int peigs_; // iparm[21]
```

No initializer, and the constructor does not give them one — it zeroes the
Pardiso parameter array and nothing else. They are assigned only inside
the factorization paths (`pardiso_interface.h:250-251, 274-275, 335-336,
353-354`), each of which runs after a real backend phase.

So before any factorization, `peigs()` and `neigs()` return the contents
of uninitialized storage. This is a different defect from the SQP seam's:
that one has a defined and misleading answer, this one has no defined
answer. Both are unrepresentable states wearing the costume of a reading,
but only this one is undefined behaviour.

The seam also has no way to say so. There is no state, no flag, and no
factorization counter on the object that a caller could consult first —
which is why the rig's adapter has to keep its own
(`tests/golden_rig/seam_psiopt.cpp` counts factorizations itself and
guards on that).

`ppivs()` on the MKL side is a real Pardiso counter and is not implicated;
see the Accelerate record for the branch where it is a hardcoded literal.

## The trace that proves it

P5 on `psiopt-old@mkl`, passing. The derived expected table records what
the arm answered (`tests/golden_rig/expected/P5.csv`):

```
psiopt-old@mkl,before_factorize_inertia_state,state,kUnavailable
psiopt-old@mkl,before_factorize_n_pos,counter,-1
psiopt-old@mkl,before_factorize_n_neg,counter,-1
psiopt-old@mkl,before_factorize_n_zero,counter,-1
psiopt-old@mkl,before_factorize_zero_is_derived,bool,false
psiopt-old@mkl,before_factorize_perturbed_pivots_presence,presence,absent
```

Read those rows for what they are: they are the *adapter's* answer, not
the seam's. The adapter's own comment at the site says as much — it
substitutes `kUnavailable` because the alternative is an indeterminate
read, and it flags the asymmetry with its Apple branch (which has no
guard, because there the members ARE defined and the seam's real answer is
a fabrication worth surfacing).

The honest summary of what was observed on this arm is therefore: **the
seam was never asked.** No number in the P5 row set for this arm came out
of `tycho`'s own code.

## What hven answers instead

`hven::linear::SymmetricFactor` has the pre-factorization case as a state
in the type — `InertiaEvidence::State::kUnavailable`, counts left at their
invalid sentinel — and the object tracks its own lifecycle, so the state
is decided by the factorization's own bookkeeping rather than by a caller
remembering to check first.

The difference that matters here is not the value but the location: on
hven the question "has anything been factorized?" is answered by the
factorization object, which is the only thing that knows. On the old seam
that knowledge lives nowhere, which is why the rig's adapter had to
reconstruct it externally, and why any consumer wanting the same safety
would have to reconstruct it too.

## Migration consequence

**Owner: the interior-point engine retarget (M2).**

1. **The retarget removes an undefined-behaviour path rather than changing
   a value.** No consumer behaviour needs to be preserved here, because
   there is no defined behaviour to preserve. That makes this record
   unusual: it is a migration *benefit* to be claimed, not a compatibility
   obligation to be met.
2. **Search for pre-factorization `peigs()`/`neigs()` reads before the
   retarget, not after.** If any exist today they are latent UB and are
   worth knowing about independent of hven — a read that happens to return
   plausible numbers on this build is exactly the kind of thing that
   changes with a compiler bump. The rig cannot find them: it drives the
   seam through one adapter, and the adapter guards. This is a source
   audit of `tycho`'s own call sites, and it is not covered by the
   consumed-surface audit either, since that instrument watches backend
   touchpoints rather than seam-internal reads.
3. **The rig's external factorization counter goes away with the
   adapter.** `seam_psiopt.cpp` is deleted when M2 closes; nothing needs
   to inherit its counter, because on hven the state is intrinsic.
4. **This record does not license the Apple branch's behaviour.** The same
   members on the Accelerate side are zero-initialized, which makes their
   pre-factorization answer defined, real-looking and wrong — a genuinely
   worse position than this one, filed separately as
   [psiopt-accelerate-inertia-zero-fill](2026-08-09-psiopt-accelerate-inertia-zero-fill.md).
