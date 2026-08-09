# The SQP seam answers an inertia before anything is factorized

**Status: CONFIRMED.** Observed on Linux/MKL, hven commit `ca2744c1ba41`,
against `tycho_sqp` at tag `phase-7-close`
(`4faa1df116da53c9dc68f36635c118f52d39d2b9`, tree state verified at
configure). Machine `fedora-x86_64-linux`, Intel oneAPI MKL 2026.1
(Product Build 20260612), thread pin `process-global=1`.

**Trace: P5. Arm: `sqp-old@mkl`. This is the one failure the Linux
three-seam run is supposed to have.**

**Amended 2026-08-09.** Two source-read claims in the original record —
the release-path re-zeroing and the pre-factorization
`supports_partial_solve()` read — were misreads, caught by the SQP
instance's verification against the pinned tree (its sign-off note,
`tycho_sqp/docs/notes/2026-08-09-hven-m1-close-signoff-sqp.md` §5) and
re-verified independently on this side before amending. The CONFIRMED
half — the P5 trace, the quoted failure, and the constructor-zeroed
unguarded accessors it proves — is unchanged.

## What the old seam does

`tycho_sqp::KktSystem` reports inertia by reading three entries of the
Pardiso parameter array (`include/tycho_sqp/kkt_system.h:351-355`):

```cpp
inline Index KktSystem::num_pos_eigs() const { return static_cast<Index>(iparm_[21]); }
inline Index KktSystem::num_neg_eigs() const { return static_cast<Index>(iparm_[22]); }
inline Index KktSystem::num_perturbed_pivots() const { return static_cast<Index>(iparm_[13]); }
```

Three plain reads, with no guard of any kind. The array they read is
zero-filled by the constructor (`kkt_system.h:129`, `iparm_.fill(0)`), and
once more per object lifetime in `analyze()`'s `!initialized_` first-init
branch (`kkt_system.h:277`, immediately before `pardisoinit`). Nothing
else ever clears it — in particular `release()` (`kkt_system.h:205-240`)
never touches `iparm_`.

The consequence is not a crash and not a garbage value — it is worse than
either. Asking a freshly-constructed `KktSystem` for its inertia returns
`(0, 0, 0)`, and returns it through the same code path, with the same
types, as a genuine post-factorization reading. There is no second channel
— no status, no flag, no exception — by which a caller could tell the two
apart. A zero triple from a matrix that really has no positive, negative
or zero eigenvalues is impossible; a zero triple from a matrix that was
never factorized is what you always get. The seam offers no way to know
which one you are holding.

After `release()` the defect changes shape and gets worse. `release()`
clears `active_` but leaves `iparm_` alone, so a seam that HAS
factorized, and then released, keeps reporting the *previous
factorization's* triple — real-looking numbers describing numerics that
no longer exist. The pre-factorization zero triple is at least
implausible for most matrices; the stale triple is a plausible reading
of the wrong factorization, and no zero-plausibility check can catch it.

Note what this is not. It is not an uninitialized read (the array is
deliberately zeroed), and it is not a bug in the arithmetic. It is a
missing state: the type can represent "the inertia is (0,0,0)" but cannot
represent "there is no inertia to report", so the first stands in for the
second.

## The trace that proves it

`P5_InertiaBeforeFactorizationIsAnExplicitState` constructs the seam,
asks for an inertia with nothing factorized, and asserts the answer is an
explicit non-answer. On `sqp-old@mkl` it fails, verbatim:

```
tests/golden_rig/traces_psiopt.cpp:265: Failure
Expected: (e.state) != (hl::InertiaEvidence::State::kObserved), actual: 4-byte object <00-00 00-00> vs 4-byte object <00-00 00-00>
nothing has been factorized, so there is nothing to have observed

tests/golden_rig/traces_psiopt.cpp:267: Failure
Value of: e.n_pos == 0 && e.n_neg == 0 && e.n_zero == 0
  Actual: true
Expected: false
counts with nothing behind them must stay invalid, never be zero-filled -- a zero-filled triple reads exactly like a real one
```

The derived expected table records what the seam answered, in its own
row set (`tests/golden_rig/expected/P5.csv`):

```
sqp-old@mkl,before_factorize_inertia_state,state,kObserved
sqp-old@mkl,before_factorize_n_pos,counter,0
sqp-old@mkl,before_factorize_n_neg,counter,0
sqp-old@mkl,before_factorize_n_zero,counter,0
sqp-old@mkl,before_factorize_zero_is_derived,bool,true
sqp-old@mkl,before_factorize_perturbed_pivots_presence,presence,present
sqp-old@mkl,before_factorize_perturbed_pivots,counter,0
```

`kObserved` is the whole finding in one word. The seam is not reporting
zeros while admitting it has nothing — it is reporting zeros as an
observation.

**The rig's adapter is deliberately not protecting anyone from this.**
`tests/golden_rig/seam_sqp.cpp`'s `evidence()` has no pre-factorization
guard, and says at the site that adding one would report the NEW surface's
honest answer in place of the OLD seam's real one and make this failure
silently never occur. That is also why a control test
(`FailByDesignControl.SqpSeamStillZeroFillsItsPreFactorizationInertia`)
asserts the opposite: if the adapter ever regresses to smoothing, P5 goes
green and the control goes red, so the finding cannot be lost quietly in
either direction.

## What hven answers instead

`hven::linear::InertiaEvidence` carries an explicit `State`, and the
"nothing to report" case is a value of it rather than a convention about
the counts. Before any numeric factorization the state is `kUnavailable`
and the three counts stay at their invalid sentinel — they are never set
to zero, precisely because zero is a value a real reading can take.

That makes the two situations different objects rather than identical
ones, which is the property the SQP seam cannot express at all. It also
generalizes: the same `State` carries `kQueryFailed` for the Accelerate
case where the query runs and fails, which the seams handle by
zero-filling too.

## Migration consequence

**Owner: the SQP engine retarget (M3).**

1. **Every `num_pos_eigs()` / `num_neg_eigs()` call site has to decide
   what it does when the answer is unavailable.** Today they cannot ask,
   so none of them decides — they read three ints and proceed. After the
   retarget the state is in the type and the compiler will not let the
   question be skipped. Each site gets classified: some genuinely cannot
   be reached before a factorization (those assert the state), and any
   that can must route the unavailable case rather than treat it as
   `(0,0,0)`.
2. **Audit for inertia reads on the release path specifically.** A call
   site that reads inertia from a released system gets the *stale previous
   factorization's* triple — not zeros — because `release()` never touches
   `iparm_`. Unlike the pre-factorization case this one happens
   mid-lifecycle, and the stale values are plausible, so no
   zero-plausibility argument can catch it. This is the likeliest place
   for a real latent misread and should be searched for by hand, not
   assumed absent. Owner: the SQP instance self-assigned this audit, with
   the corrected stale-not-zero semantics, as an M3 carry (its seam; its
   sign-off note §5).
3. **`zero_is_derived` stays true for this backend and is not affected.**
   MKL reports only the positive and negative counts, so the zero class is
   computed as `dim - p - n` on both the old seam and hven. The finding
   here is about the state, not about how the zero class is obtained.
4. **The perturbed-pivot row moves too.** `iparm[13]` is read by the same
   unguarded mechanism, so `num_perturbed_pivots()` reports a present zero
   before any factorization. On MKL the counter is real once a
   factorization has happened, so hven keeps it present — but the
   pre-factorization present-zero is the same missing-state defect.
   `supports_partial_solve()` (`kkt_system.h:78`) is built on that read
   behind an `active_ &&` guard, which short-circuits before any analysis
   and after `release()` (which clears `active_`) — in those windows the
   gate answers `false` correctly from the lifecycle flag and never reads
   `iparm_[13]`. The real gap is the post-`analyze()`/pre-`factorize()`
   window: `analyze()` sets `active_ = true` immediately after phase 11
   (`kkt_system.h:285`), before any numeric factorization has written the
   count, so the gate there consults a perturbed-pivot count no
   factorization produced — zero on a first lifetime, stale on a
   re-analyze.
