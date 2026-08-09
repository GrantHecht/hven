# The migration docket

A **docket record** is what a fail-by-design trace produces when it fires.

The golden rig runs the frozen `hven::linear` honesty rules against the two
seams hven replaces. Most of those rules the old seams already satisfy, and
the trace passes on every arm. A few they do not — and those traces are
written to FAIL on the old-seam arm rather than to skip it, because the
failure is the deliverable. A record here is that failure written down: the
behaviour the old seam actually has, the trace that demonstrates it, what
the new surface answers instead, and what a consumer has to change when the
engine is retargeted.

The rig is not the only thing this protects. A behaviour that no longer
exists after a migration cannot be re-discovered from the code, so a record
written now is the only surviving statement of what the engines were relying
on before. `seam_psiopt.cpp` and `seam_sqp.cpp` are deleted when the two
migrations close; these files are not.

## Reading a record

Every record has the same four sections, in the same order:

| Section | What belongs in it |
| --- | --- |
| **What the old seam does** | The behaviour, with the file and line it comes from and the mechanism that produces it. Not a paraphrase of the trace — the seam's own code. |
| **The trace that proves it** | Which trace, on which arm, what it asserts, and the observed values that make it fail. Copied from a run, never described from memory. |
| **What hven answers instead** | The frozen semantics that replace the behaviour, and why they are the honest answer rather than merely a different one. |
| **Migration consequence** | What breaks, what has to change, and who owns it, when the engine is retargeted onto `hven::linear`. |

## Status vocabulary

A record's status line says how much of it has been observed, and nothing
in a record may be more confident than that line:

- **CONFIRMED** — the failure has been observed on real hardware, on a
  named commit, in a run whose output is quoted in the record.
- **AWAITING-MAC-RUN** — the behaviour has been read out of the old seam's
  source and the trace that will catch it exists, but the arm has never
  executed: this project has no Apple hardware with both sibling
  checkouts on it. Nothing in such a record is an observation. The values
  it predicts are predictions, and they are labelled as such.

An AWAITING-MAC-RUN record becomes CONFIRMED by someone running the Mac
three-seam leg and pasting what the run actually printed — not by
agreeing that the prediction looks right.

## Index

| Record | Trace | Seam | Status |
| --- | --- | --- | --- |
| [sqp-inertia-before-factorization](2026-08-09-sqp-inertia-before-factorization.md) | P5 | `sqp-old@mkl` | **CONFIRMED** |
| [psiopt-mkl-inertia-before-factorization](2026-08-09-psiopt-mkl-inertia-before-factorization.md) | P5 (passes) | `psiopt-old@mkl` | **CONFIRMED** |
| [psiopt-accelerate-perturbed-pivots](2026-08-09-psiopt-accelerate-perturbed-pivots.md) | P4 | `psiopt-old@accelerate` | AWAITING-MAC-RUN |
| [psiopt-accelerate-inertia-zero-fill](2026-08-09-psiopt-accelerate-inertia-zero-fill.md) | P5 | `psiopt-old@accelerate` | AWAITING-MAC-RUN |

The second row is the odd one: a CONFIRMED record whose trace **passes**.
It is here because the finding is real and the passing trace is what
records it — see that record's own opening for why a pass, in that one
case, is the finding rather than the absence of one.
