# M6 W4 — machine-trace schema v0 acceptance evidence (T5)

Produced 2026-09-05 on branch `m6`; **code head `1df1ae8`** (everything on top of
it is docs, so that is the tree the numbers describe).
**Read `PROVENANCE.txt` first** — it records the commits, the toolchain, the
hardware, the declared serial protocol and the cpu10 idle snapshot, and it
governs every number below. Every value here was measured in this task.

The schema these artifacts instantiate is **`docs/trace-schema-v0.md`**, which is
the document of record. This directory is what v0 actually looks like when a
real solve writes it.

Everything is **MKL on Linux**. Apple/Accelerate values are **UNOBSERVED** and
none is inferred.

```
docs/notes/data/2026-09-m6-w4-acceptance/
  PROVENANCE.txt                        the stamp, the serial rule, the cpu10 snapshot
  suites.txt                            the four ctest legs, both configs, and the count arithmetic
  identities.txt                        the five per-site qp.mode identities, recomputed over 81 cells
  exemplars/hs24-walk.jsonl             HS24 at kWalk            — the baseline shape
  exemplars/hs33-ssn.jsonl              HS33 at kSsn
  exemplars/hs38-ipqp.jsonl             HS38 at kIpm             — the tier, an escape, a fallback verdict
  exemplars/hs38-interior-point.jsonl   HS38 on the INTERIOR-POINT driver
  telemetry-census-rows.csv             770 rows: the six W4 T3 activity fields, per major
  telemetry-census-cells.csv            81 cells: the four folds and the per-site qp.mode counts
  replay/base-{walk,ssn,ipm}.csv        U0 27 cells at fb46802 (the W4 T5 BASE)
  replay/head-{walk,ssn,ipm}.csv        U0 27 cells at 6d40940
  replay/comparisons.txt                the five 0-difference lines
  probes/trace-exemplar-probe.cpp       the exemplar/census instrument's own source
  probes/compare_replay.py              the comparator (recomputes comparisons.txt)
```

The 27-cell list is **not duplicated here**: it is
`docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt`, unchanged, and
both replay arms were driven from that file.

**Every file carries a `#`-prefixed hven-native provenance header** — the same
convention the corpus CSVs use. Strip the `#` lines and what remains is the
artifact: JSON lines for an exemplar, a CSV with its own header row otherwise.
The header is not part of the schema; a v0 reader strips it, exactly as it
strips the corpus CSVs'.

---

## 1. The four exemplar streams

The cells were named in the T5 brief before the run, so the provenance was
decided before the numbers were.

| file | cell | lines | what it shows |
|---|---|---|---|
| `hs24-walk.jsonl` | HS24, `qp_mode = kWalk` | 9 | the minimum vocabulary: `sqp.solve` begin/end, one `sqp.major` per row, one `qp.mode` at `site` `dispatch` per major |
| `hs33-ssn.jsonl` | HS33, `qp_mode = kSsn` | 12 | the same shape through the SSN arm — `"mode":"ssn"` on every dispatch line |
| `hs38-ipqp.jsonl` | HS38, `qp_mode = kIpm` | 950 | the whole tier alphabet |
| `hs38-interior-point.jsonl` | HS38 on `InteriorPointSolver` | 39 | `ipm.solve` begin/end and 37 `ipm.iter` lines |

`hs38-ipqp.jsonl`'s event census, which is what makes it the interesting one:

```
  482  ipqp.iter        == ipqp_iters
  218  ipqp.reg         == ipqp_reg_increases + ipqp_reg_decreases
   49  ipqp.restart
   49  ipqp.route
   48  ipqp.certify     (one fewer than route: one subproblem took no certification read)
   50  qp.mode          49 at site `dispatch`, 1 at site `fallback_rung_b`
    1  ipqp.escape      == ipqp_escapes
    1  fallback.verdict
   50  sqp.major        == history.size()
    1  sqp.solve.begin
    1  sqp.solve.end
```

**The `fallback_rung_b` line is W4 T5's own contribution to this stream.** Before
T5 the fallback's cold walk ran and wrote nothing — a registered gap in the
stream, pinned as a gap at W4 T2. It is now present and TAGGED, while the
`dispatch` site still reads 0 walk lines: the escape was serviced inside the
kIpm arm, not by a second dispatch. That distinction is what the `site` key
exists to carry.

**Restoration is NOT exercised by any of the four.** The brief expected HS38 at
kIpm to carry restoration lines; measured, it does not — `restoration_iters == 0`
and **zero depth-1 lines on all 81 census cells** (`identities.txt`). So the
nesting rule (`docs/trace-schema-v0.md` §7 rule 2: a depth-0 bracket may contain
a whole depth-1 solve, and the requesting row's line FOLLOWS it) is **not**
exemplified here. It is pinned instead, on a fixture built for it, at
`tests/sqp/test_trace_writer.cpp`'s
`TheRequestingRowIsWrittenAFTERTheNestedSolveItAskedFor` and
`TheNestedRestorationSolveIsItsOwnPairAtDepthOne`. Recorded as an absence rather
than manufactured.

**The interior-point stream** shows both `null` conventions live on its first
line: `prox_reg_primal` and `prox_reg_dual` read `null` (classic mode, so
proximal regularization is off) while `first_rejection_iter` and
`theta_at_first_rejection` read `null` for a different reason (no rejection
recorded). `p_pivots` reads a real `0` here — MKL observed zero perturbed pivots.
On Accelerate that key would read `null`; **UNOBSERVED**, and the pin that
asserts it is written to run there.

## 2. The telemetry census — the first per-family reading M7 starts from

`telemetry-census-rows.csv` (770 rows, one per major) and
`telemetry-census-cells.csv` (81 cells: the 27 shipped HS models × three QP
modes). The row file carries the six W4 T3 activity fields verbatim; the cell
file carries the four `SqpCounters` folds beside the per-site `qp.mode` counts
and the counters they are checked against.

| reading | over the 81 cells |
|---|---|
| rows with `active_set_delta > 0` | 73 of 770 |
| `active_set_delta` summed | 96 |
| largest `active_set_delta_peak` | 3 |
| rows with `weak_active_rows > 0` | **34** |
| largest `weak_active_peak` | 2 |
| rows with `near_active_rows > 0` | **0** |
| largest `near_active_peak` | 0 |

**The folds reproduce the rows exactly**: recomputing each cell's
`active_set_delta_total` (sum), `active_set_delta_peak`, `weak_active_peak` and
`near_active_peak` (max) from the row file agrees with the counters on all 81
cells, **0 mismatches**.

Two findings worth carrying into M7, stated as measurements rather than
conclusions:

* **`near_active_rows` is zero everywhere on this battery.** Not one inactive
  inequality row on any HS cell at any mode has a QP slack inside the `1e-6`
  RELATIVE margin. The field is not dead — its arithmetic is pinned directly on
  controlled fixtures at `tests/sqp/test_activity_telemetry.cpp` — but the HS
  battery contains no near-active row at this margin. A heuristic that expects
  to read one will need a different family, or a different margin.
* **`weak_active_rows` fires on 34 rows** and peaks at 2. Weak activity in the
  strict-complementarity sense is real and common here, near-activity is not,
  and the two are separate readings — which is the distinction
  `docs/trace-schema-v0.md` §6 (b) exists to state.

Active-set churn is **small** on this battery: 73 rows of 770 move the set at
all, and the largest single-major change is 3. That is a statement about HS, not
about the instrument; the F7 corpus is where a bulk-flip family would live.

## 3. The per-site `qp.mode` identities — recomputed over 81 cells

`identities.txt`. Each `site`'s line count equals the counter that prices those
invocations; **0 failures on all 81 cells.**

| `site` | lines over the battery | checked against |
|---|---|---|
| `dispatch` | 725 | bounded, not equal: ≥ one per major that solved a QP, ≤ two (the handing arm's plus the walk's) |
| `ssn_warm_grade` | 13 | `ipqp_to_ssn` = 13 |
| `fallback_rung_b` | 3 | unfired fallback entries + `ipqp_fallback_rung_b` (= 0 here, so all three are unfired entries) |
| `elastic_rung` | **246** | `elastic_activations` 37 + `elastic_escalations` 209 |
| `soc_resolve` | 6 | `soc_steps` = 6 |

**`elastic_rung` is why the `site` key exists.** 246 walk invocations against 725
dispatch lines and 770 majors: a ladder's climb is unbounded per major, and no
per-major reading can reconcile the walk invocations these solves made. Before
T5, 268 of the 993 kernel invocations on this battery wrote no line at all.

## 4. The U0 27-cell three-arm replay — the instrumentation invariant

`replay/`. Six legs (3 arms × {BASE, HEAD}), each solo, one at a time, under the
box lock, `taskset -c 2`, `MKL_NUM_THREADS=1 OMP_NUM_THREADS=1`.

```
provenance:  # binary: fb4680226f6e   (BASE, the W4 T5 brief's declared base)
             # binary: 1df1ae8c4124   (HEAD)

=== base-walk vs head-walk ===                    27 cells  75 columns  0 differences
=== base-ssn  vs head-ssn  ===                    27 cells  75 columns  0 differences
=== base-ipm  vs head-ipm  ===                    27 cells  75 columns  0 differences
=== committed t10b ipm baseline vs head-ipm ===   27 cells  75 columns  0 differences
=== committed t10b ipm baseline vs base-ipm ===   27 cells  75 columns  0 differences
```

T5 touches `src/drivers/sqp_driver.cpp` at seven places — four new emit sites,
two threaded sink parameters and one hoisted boolean — and moves nothing on the
corpus. The fifth line is the control: the BASE binary also reproduces the
committed t10b baseline, so the fourth is a statement about the tree rather than
about this run's conditions.

**The corpus attaches no sink**, so the replay proves the no-sink path unchanged
rather than the stream unchanged. The stream side is asserted by the pins: the
null-sink tests compare every counter field-by-field through the three X-macro
tables between an attached and an unattached solve of the same cell, which is
the claim the replay cannot make.

## 5. Rebuilding the exemplars and the census

`probes/trace-exemplar-probe.cpp` is **not a CMake target** — it is a scratch
instrument committed so the numbers above are re-derivable, on the W2 acceptance
directory's own precedent. Build it against a **Release** tree's `libhven.a`
with that tree's own flags:

```
clang++ -DMKL_LP64 -m64 -O3 -DNDEBUG -std=c++20 -fPIE -Wabsolute-value -pthread \
        -mllvm -inline-threshold=225 -fomit-frame-pointer -fno-stack-protector \
        -fno-asynchronous-unwind-tables -falign-loops=32 -march=native -mtune=native \
        -ffast-math -fno-finite-math-only -fopenmp=libiomp5 \
        -DEIGEN_DONT_PARALLELIZE -DEIGEN_INITIALIZE_MATRICES_BY_ZERO \
        -DEIGEN_MAX_ALIGN_BYTES=32 -DFMT_HEADER_ONLY -DFMT_USE_LOCALE=0 \
        -DHVEN_DEFAULT_QP_THREADS=8 \
        -I<repo>/tests/sqp -I<repo>/include -I<repo>/src/interior/utils \
        -I/opt/intel/oneapi/mkl/latest/include \
        -isystem <repo>/dep/eigen -isystem <repo>/dep/fmt/include \
        probes/trace-exemplar-probe.cpp <build>/libhven.a \
        -L/opt/intel/oneapi/compiler/latest/lib \
        -Wl,-rpath,/opt/intel/oneapi/mkl/latest/lib:/opt/intel/oneapi/compiler/latest/lib \
        -Wl,--start-group /opt/intel/oneapi/mkl/latest/lib/libmkl_intel_lp64.a \
        /opt/intel/oneapi/mkl/latest/lib/libmkl_intel_thread.a \
        /opt/intel/oneapi/mkl/latest/lib/libmkl_core.a \
        /opt/intel/oneapi/compiler/latest/lib/libiomp5.so -Wl,--end-group -ldl -lm \
        -o trace-exemplar-probe
```

Run it as `MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 ./trace-exemplar-probe
<this directory> <a provenance header file>`; it writes the four exemplars and
the two census CSVs, each behind the header it is handed.

The probe includes `tests/sqp/support/hs_problems.h` rather than copying the
fixtures. The interior-point cell is the exception it cannot avoid: `NLPProblem`
and `NlpModel` are different contracts with no adapter between them, so HS38 is
hand-transcribed for the interior-point driver — and the probe **verifies that
transcription against `Hs38Model` before it runs**, on f and grad f at three
points and on the declared box, aborting on any disagreement. An exemplar that
described a different problem would be worse than no exemplar.

**Re-run from the committed source after the files were written, solo and
pinned:** five of the six artifacts reproduce **byte-for-byte** (the header's own
`date-utc` line aside). The sixth differs on exactly ONE line and exactly the
fields the schema says are not reproducible — `hs38-interior-point.jsonl`'s
final `ipm.solve.end`, whose seven `_s` values are wall-clock. Every other byte
of that file, its 37 `ipm.iter` lines included, is identical. That is the
INFORMATIONAL/asserted split of CLAUDE.md §7 showing up as a diff, and it is the
reason no pin reads an `_s` field.
