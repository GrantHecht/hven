# M6 W6 T2 — the coverage close read (4e31bd16)

**Read `PROVENANCE.txt` first.** Everything here comes from one instrumented run
of the whole suite on the `linux-clang-coverage` tree, taken after T2's tests
landed and with no library source changed since the open read. Coverage flags
change codegen: **nothing in this directory is a pin, a baseline or a timing**,
and nothing here is asserted except as a report of what that run measured.
Apple/Accelerate: **UNOBSERVED** — this read is MKL-on-Linux only.

| file | what it is |
|---|---|
| `PROVENANCE.txt` | commit, toolchain, hardware, the three departures from `scripts/run_coverage.sh`, the suite's status, the exclusion list **re-derived**, the mismatched-data re-count |
| `coverage-summary.txt` | `llvm-cov report`'s per-file table, verbatim |
| `coverage-areas.txt` | the per-directory rows — all four metrics, **lines** the asserted one |
| `zero-headers.txt` | every library file at 0.00 % lines, each with a judgement |
| `parity.txt` | parity **restated** on this read, the caveat beside it, and the twenty regions T0 named with a status apiece |
| `delta.txt` | T0 → close, per driver file; and what each partly-covered region still needs |
| `pch-experiment.txt` | the PCH-off experiment — an **experiment**, reported separately and comparable only to itself |

## 1. The headline

```
                    OPEN (T0, 7da77b5d)        CLOSE (T2, 4e31bd16)
src/drivers/        88.63 %  (8586/9687)       93.17 %  (9025/9687)     +4.53
library total       87.47 % (22259/25448)      89.31 % (22729/25449)    +1.84
                    ---------------------      ---------------------
gap                 +1.17 points               +3.85 points, in src/drivers/'s favour
```

**Parity holds, and it is wider.** It held at T0 already — the mandate's item was
discharged there by the artifact — so T2's job was never the number: it was the
paths T0 NAMED. The number moved as a consequence, which is the right direction
of causation.

> The +3.85 is an **upper bound** on the lead, for T0's corrected reason: the
> under-read lands on HEADERS and the `src/drivers/` row is fourteen `.cpp` TUs.
> Filling every line of the four still-under-read headers (63 lines) would move
> the library to 89.56 % against `src/drivers/`'s 93.17 %; parity still holds.
> `parity.txt` carries the arithmetic.

## 2. What the tests actually bought

Three driver TUs went to 100 % (`solve_result.cpp` 66.67 → 100, `trace.cpp`
75 → 100, `sqp_print.cpp` 82.86 → 100). The two IPM TUs that held two thirds of
every uncovered line in `src/drivers/` moved by +20.79 and +2.24 points and now
hold 386 of the 662 that remain. And the ONE header T0 judged genuinely cold,
`include/hven/detail/globalization/soc.h`, went **0/28 → 29/29**.

Of the twenty uncovered regions T0 published: **six COVERED outright, five
partly, nine untouched** — 444 uncovered lines then, 127 now. The nine untouched
are the ones T0 itself called "single-branch work" and named no item for. The
shape changed more than the totals say: T0's biggest uncovered run in
`src/drivers/` was 104 lines; the biggest here is 15.

`delta.txt` names what is left in each partly-covered region, why it is left,
and what would reach it. **Nothing needs a seam** — no item was classified
unreachable-without-one, no `HVEN_TESTING` point was added (plan J.4), and the
`KktFactor` construction observer stays deferred to M7 (J.10, A7).

## 3. The 0 % list is one file shorter, and the rest are the same rows for the same reasons

T0 listed eight; this read lists seven. `soc.h` left, as above. The other seven
are unchanged, and `zero-headers.txt` gives each one the judgement T0 gave it —
four **under-read**, one dead, one compile-time only, one Eigen-dispatch only.

The under-read finding is now much harder to argue with. `eval_error_log.h`
still reads **0/9** while T2's own tests assert the count and the message its
`record()` / `record_unknown()` produced. A row that stays at zero while a test
asserts the value its function computed is a report defect, not a coverage gap.

## 4. And the experiment T0 designed says where the defect comes from

With the library's precompiled header **off** — CMake's own
`CMAKE_DISABLE_PRECOMPILE_HEADERS`, no source or CMake change needed — all four
under-read rows fill in, and the mismatched-function count over that object set
falls from 126 to 92.

That **settles more than T0 expected and corrects its framing**: T0 restated the
correlation as "two of the four are in `src/hven_pch.h` and two are not, so the
experiment can test the PCH contribution only". It moves all four, because the
effect follows *"is this header's inline body compiled inside a TU that consumes
the PCH"*, not *"is it listed in the PCH"* — and `src/drivers/ipm_solver.cpp` is
one of the four opt-in sources and includes the other two. The count not falling
to zero is the other half: the PCH is a large part of the cause, not all of it.

**It is not a reason to change the build.** The membership list is evidence-gated
on compile time and byte-identical objects, and this is a reporting defect in an
instrument tree. What it buys is knowing which rows to stop chasing.
`pch-experiment.txt` has the table, the proof of reach in both directions, and
the declared limits.

## 5. What did NOT change here

No library source: `git diff --stat 51a00906..HEAD -- src/ include/` is empty.
The only non-test change in the window is one emitted string in
`scripts/run_coverage.sh` (78c46593), and `PROVENANCE.txt` records both why it
changed and why this read could not emit it.
