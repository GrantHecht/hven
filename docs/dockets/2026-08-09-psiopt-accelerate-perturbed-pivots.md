# The interior-point seam reports a hardcoded zero perturbed-pivot count on Accelerate

**Status: AWAITING-MAC-RUN.** Nothing in this record has been observed.
The behaviour below is read out of the old seam's source; the trace that
will catch it exists and has never executed, because no machine in this
project has Apple hardware with both sibling checkouts on it. The Apple
branch of the rig's interior-point adapter has never been compiled either
(it is not reachable by the Linux Accelerate syntax lane — that lane's
stub is far smaller than this seam's header needs), so first contact may
also surface compile errors that have nothing to do with this finding.

**Trace: P4. Arm: `psiopt-old@accelerate`.** Every P4 and P5 row for that
arm in `tests/golden_rig/expected/` reads `UNOBSERVED` and stays that way
until a run fills it.

## What the old seam does

`psiopt/include/tycho/detail/solvers/linear/accelerate_interface.h:414-418`:

```cpp
inline int ppivs() const {
    // Accelerate doesn't provide direct pivot perturbation count
    // Return 0 as a safe default for now
    return 0;
}
```

The comment states the problem accurately and then does the one thing
that hides it. Accelerate has no perturbed-pivot counter; there is no
number to return. Returning `0` does not communicate that — it
communicates "this factorization perturbed no pivots", which is a
specific, strong, and unearned claim about the numerics.

It is also the *most dangerous* value available, because zero is the
value that unlocks things. `tycho_sqp`'s sibling gate is written
`active_ && num_perturbed_pivots() == 0` — a perturbed-pivot count of
zero is what says a factorization is clean enough for its partial solves
to compose. A backend that cannot count perturbations therefore reports,
on every factorization it ever performs, exactly the value that means
"safe". Had the placeholder been any nonzero number the failure mode
would be conservative; at zero it is permissive.

`p_pivots_` is consumed on the interior-point side too
(`psiopt/src/psiopt.cpp:878`, `iter.p_pivots_ = this->kkt_sol_.ppivs()`),
which is what carries the fabricated value into the solver's own
iteration record.

## The trace that will prove it

`P4_PerturbationEvidencePresenceIsBackendHonest` factorizes a barrier-KKT
fixture and asserts, on Apple:

```cpp
EXPECT_FALSE(e.perturbed_pivots.has_value())
    << "this backend has no perturbed-pivot counter, so the evidence must be ABSENT; a zero "
       "would be a fabricated reading";
```

The rig's adapter carries `ppivs()` through verbatim on its Apple branch
(`tests/golden_rig/seam_psiopt.cpp`, `e.perturbed_pivots =
static_cast<Index>(solver_.ppivs())`, with the reasoning stated at the
site), so the optional arrives PRESENT with value `0` and the assertion
fails. That is the intended outcome and the reason the adapter does not
translate.

**Predicted, not observed** — the row this is expected to produce, marked
as a prediction so it is never mistaken for a reading:

```
psiopt-old@accelerate,perturbed_pivots_presence,presence,present   [PREDICTED]
psiopt-old@accelerate,perturbed_pivots,counter,0                   [PREDICTED]
```

Whoever runs the Mac leg replaces these with what the run printed, moves
the status line to CONFIRMED, and records the machine, the macOS version
and the commit.

## What hven answers instead

`hven::linear::InertiaEvidence::perturbed_pivots` is an optional, and the
frozen per-backend semantics table makes its absence on Accelerate
normative rather than incidental: *"absent (`nullopt`) — Accelerate has no
counter; absence is the honest state."*

The rule the table states around it is the general form of this finding:
a Mac reading may be **less informative** than an MKL one, never
**differently-valued**. A missing counter degrades to `nullopt`. It does
not degrade to a number, because a number is not less information than a
number — it is different information, and the consumer cannot tell.

**That half is now OBSERVED on hardware.** The macOS CI lane's report-mode
artifacts (runs 31295310213 and 31295501823 at commit `48414157cee0`,
`macos-26-arm64`, byte-identical) record, for `native@accelerate` on this
trace's own fixture:

```
native@accelerate,perturbed_pivots_presence,presence,absent
```

So the replacement behaviour is confirmed, not merely specified: hven
answers **absent** where the old seam answers a hardcoded zero. What
remains AWAITING-MAC-RUN is the old seam's half — that arm needs a Mac with
the sibling checkout on it, which no runner has.

## Migration consequence

**Owner: the interior-point engine retarget (M2), with an M3 obligation
already recorded against the SQP side.**

1. **Every consumer of `ppivs()` has to handle absence.** On hven the
   value is an optional; a consumer that wants a number must say what it
   does when there isn't one. The interesting cases are the gates: a
   predicate built as `count == 0` has to become something explicit about
   the unknown case, and the conservative reading (unknown means do not
   trust composition) is the one that matches what the counter was being
   used to decide.
2. **`supports_partial_solve()` is the concrete instance.** hven carries
   the predicate rather than the raw count for exactly this reason — the
   frozen surface's T2b trace asserts that a factorization with perturbed
   pivots reports `supports_partial_solve() == false`, and its Mac arm
   asserts the conservative rung: the predicate is false under ABSENT
   perturbation evidence, never a fabricated true.
3. **The SQP side has a named, separate obligation here.** The freeze
   review recorded that the SQP Accelerate shim maps Apple's ZERO-pivot
   count into `num_perturbed_pivots()` — a differently-valued reading,
   which the degradation rule newly forbids. Both verdict copies
   (`detail::inertia_verdict` and `detail::ssn_inertia_verdict`, which
   test perturbed pivots first) are rewritten at the M3 retarget to
   consume the optional's absence. That is a different defect from this
   one (a wrong number rather than a placeholder number) and it is carried
   by name in the M3 plan; it does not get closed by this record.
4. **`iter.p_pivots_` needs a decision, not a default.** The
   interior-point iteration record currently stores an int. Whatever it
   becomes, the choice should be made where the field is defined rather
   than by whichever call site converts the optional first.
