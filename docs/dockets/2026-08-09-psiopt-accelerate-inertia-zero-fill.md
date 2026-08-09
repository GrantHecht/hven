# The interior-point seam zero-fills its inertia on Accelerate

**Status: AWAITING-MAC-RUN.** Nothing in this record has been observed.
The behaviour is read out of the old seam's source; the trace that will
catch it exists and has never executed, because no machine in this project
has Apple hardware with both sibling checkouts on it. The Apple branch of
the rig's interior-point adapter has never been compiled either, so first
contact may surface unrelated compile errors before it surfaces this.

**Trace: P5. Arm: `psiopt-old@accelerate`.** Every P5 row for that arm in
`tests/golden_rig/expected/P5.csv` reads `UNOBSERVED` and stays that way
until a run fills it.

## What the old seam does

Two distinct paths reach the same fabricated answer, which is why this is
one record and not two half-records.

**One — the query fails and the counts are set to zero.**
`psiopt/include/tycho/detail/solvers/linear/accelerate_interface.h:443-457`:

```cpp
void cacheInertia() {
    if constexpr (Solver_ == SparseFactorizationLDLTTPP) {
        if (m_numericFactorization) {
            int np = 0, nz = 0, nn = 0;
            if (SparseGetInertia(*m_numericFactorization, &np, &nz, &nn) == 0) {
                peigs_ = np;  neigs_ = nn;  zeigs_ = nz;
            } else {
                peigs_ = 0;   neigs_ = 0;   zeigs_ = 0;
            }
        }
    }
}
```

The `else` is the finding. `SparseGetInertia` reported that it could not
tell you the inertia, and the seam records the answer as `(0, 0, 0)` —
through the same three members, readable by the same three accessors, with
nothing anywhere on the object to distinguish a failed query from a
successful one. The caller is told an inertia. It is not told that the
inertia is fiction.

**Two — nothing has been factorized yet.** The same three members are
zero-initialized at declaration (`accelerate_interface.h:693`, `mutable
int peigs_ = 0;` and its two siblings), and `resetInertia()`
(`accelerate_interface.h:460-464`) sets them back to zero. So a
freshly-constructed seam, and a seam that has been reset, both answer
`(0, 0, 0)` to an inertia query — again indistinguishable from a real
reading.

Note the contrast with the MKL branch of the same seam, filed separately:
there the members are *uninitialized*, which is undefined behaviour but at
least cannot masquerade as a reading with any consistency. Here they are
deliberately zeroed, which makes the fabrication stable, plausible and
repeatable. Defined and wrong is the harder failure to notice.

Zero is once more the maximally misleading choice, for the reason the
frozen clause states: a zero-filled triple is indistinguishable from a
real reading of a matrix with no eigenvalues in any class — and it is a
*specific* claim about the KKT system's curvature, which is exactly what
an interior-point method's inertia check consumes to decide whether the
current system is usable.

## The trace that will prove it

`P5_InertiaBeforeFactorizationIsAnExplicitState` asks for an inertia with
nothing factorized and asserts an explicit non-answer:

```cpp
EXPECT_NE(e.state, hl::InertiaEvidence::State::kObserved)
    << "nothing has been factorized, so there is nothing to have observed";
EXPECT_FALSE(e.n_pos == 0 && e.n_neg == 0 && e.n_zero == 0)
    << "counts with nothing behind them must stay invalid, never be zero-filled -- "
       "a zero-filled triple reads exactly like a real one";
```

The rig's adapter has **no pre-factorization guard on its Apple branch**,
deliberately and with the reasoning stated at the site
(`tests/golden_rig/seam_psiopt.cpp`): the members are defined there, so
the seam's real answer is a zero-filled triple reported as observed, and
substituting `kUnavailable` would report the new surface's honest answer
in place of the old seam's real one — the fail-by-design would silently
never happen.

**Predicted, not observed:**

```
psiopt-old@accelerate,before_factorize_inertia_state,state,kObserved   [PREDICTED]
psiopt-old@accelerate,before_factorize_n_pos,counter,0                 [PREDICTED]
psiopt-old@accelerate,before_factorize_n_neg,counter,0                 [PREDICTED]
psiopt-old@accelerate,before_factorize_n_zero,counter,0                [PREDICTED]
psiopt-old@accelerate,before_factorize_zero_is_derived,bool,false      [PREDICTED]
```

Whoever runs the Mac leg replaces these with what the run printed, moves
the status line to CONFIRMED, and records machine, macOS version and
commit. **The rig will need a fourth fail-by-design control on that leg**
— `fail_by_design_control.cpp` currently carries controls for whichever
old seams the build has, and the Mac three-seam configuration is expected
to run 152 tests with 2 expected failures and 4 controls green.

The query-failure path (path one above) is **not** reachable from any
fixture: no input provokes `SparseGetInertia` to fail independently of a
successful factorization, which is why the library carries a
fault-injection seam at all. Its coverage is
`AccelerateInertiaQueryFaultInjection.QueryFailureReportsKQueryFailedWithInvalidCounts`
in `hven_fault_injection_tests` — which, unlike everything else in this
record, HAS now run on real Apple hardware (CI run 31287205323, macOS
lane, passed). That test proves hven's replacement answers `kQueryFailed`
on that path; it says nothing about what the old seam does, which remains
source-read only.

## What hven answers instead

`InertiaEvidence::State` distinguishes the three situations the old seam
collapses into one:

| Situation | old seam | hven |
| --- | --- | --- |
| never factorized | `(0,0,0)` reported as a reading | `kUnavailable`, counts invalid |
| query ran and failed | `(0,0,0)` reported as a reading | `kQueryFailed`, counts invalid |
| query succeeded | the real counts | `kObserved`, the real counts |

The frozen per-backend semantics table states the middle row as law for
this backend: *"`kQueryFailed` — counts invalid, NEVER zero-filled."*
The counts are not merely reported alongside a flag; they are left at
their invalid sentinel, so a consumer that ignores the state still cannot
read a plausible number out of a failed query.

Accelerate's native three-way `SparseGetInertia` also means
`zero_is_derived` is **false** on this backend when the query succeeds —
the zero class is measured, not computed as `dim - p - n` the way MKL
requires. That is a genuine per-backend difference and is not part of this
finding.

**hven's side of both rows is now OBSERVED on hardware.** The macOS CI
lane's report-mode artifacts (runs 31295310213 and 31295501823 at commit
`48414157cee0`, `macos-26-arm64`, byte-identical) record for
`native@accelerate`:

```
before_factorize_inertia_state = kUnavailable
before_factorize_n_pos / n_neg / n_zero = -1 / -1 / -1
zero_is_derived (after a successful factorization) = false
```

The never-factorized case answers with an explicit state and counts left
invalid — not zero-filled — and the zero class really is measured rather
than derived on this backend. Both are confirmed rather than specified.
The old seam's half stays AWAITING-MAC-RUN: no runner carries the sibling
checkout, so that arm has still never executed.

## Migration consequence

**Owner: the interior-point engine retarget (M2).**

1. **The inertia check is the consumer that matters.** An interior-point
   method reads the KKT inertia to decide whether the current system has
   the right curvature and, if not, how to regularize. Fed `(0, 0, 0)`
   from a failed query, that check does not see a failure — it sees a
   system with no positive eigenvalues, which is a real and specific
   condition with its own routing. Every such site is re-examined at the
   retarget against the state, not the counts.
2. **The rewrite is not a null-check bolted onto today's shape.** The
   engine's inertia consumers currently take three ints. They take a state
   plus three possibly-invalid counts afterwards, and each site says what
   it does in each state — including whether `kQueryFailed` and
   `kUnavailable` route the same way. They probably should not: one means
   "ask again later", the other means "something went wrong now".
3. **The Mac leg is a prerequisite for closing this record, not for
   starting the work.** The behaviour is legible from source and the
   migration obligation is already clear; what the run adds is the
   observed values and the confirmation that the adapter's Apple branch
   compiles and behaves as read. Do not treat AWAITING-MAC-RUN as a reason
   to defer the consumer audit.
4. **Related, separately filed:** the same seam's `ppivs()` hardcoded zero
   on this backend
   ([psiopt-accelerate-perturbed-pivots](2026-08-09-psiopt-accelerate-perturbed-pivots.md)),
   and the MKL branch's uninitialized counts
   ([psiopt-mkl-inertia-before-factorization](2026-08-09-psiopt-mkl-inertia-before-factorization.md)).
