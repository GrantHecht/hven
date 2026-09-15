# W5 T8.9r-attrib3 — Step B: what `9cebbbe` does, and what four experiments say

Step A (`steps.md`) put the interior leg's step at **one commit**, `9cebbbe`
(*IpmResult; `solve(model, x0, budget)` with phases; the model borrowed per
call; the five entries, the mutable accessors and `ConvergenceFlags` removed*),
median per-row wall step **1.0324**, 11 of 11 scored rows above 1.01, 36.5× the
floor two byte-identical control pairs measure.

This file is what that commit's diff says, what the machine says, and what
**four** experiments — each a scratch build of a patched `git archive`
extraction, none of them committed and none of them a proposed fix — say when
the suspected mechanism is removed or reproduced.

**The honest summary first: no experiment recovers 80 % of the step.** The
largest recovers **32.8 %** and is the fourth. Three hypotheses are REFUTED by
measurement, one is CONFIRMED as a partial contributor with an exactly-measured
instrument, and **about two thirds of the step remains unexplained**. Section E
says what that leaves for the settler.

---

## A. What the diff actually changes, and what it does not

### A.1 The hot loop's source is a rename, almost line for line

`src/drivers/interior_point_solver.cpp` changes by 851 lines at this commit, and
`alg_impl` — the iteration loop — is 372 diff lines of it. Filtering the hunks
whose every changed line matches the commit's rename set (`ConvergenceFlags` →
`SolveStatus`, `result_.foo_` → `result_.foo`) leaves **31 hunks of 70**, and
**none of the survivors is inside the iteration**. What survives inside
`alg_impl` is:

* `src/drivers/interior_point_solver.cpp:2032` — `SolverContext ctx{this->nlp_, …}`
  where the parent wrote `this->nlp_.get()`. A raw pointer load replacing a
  `shared_ptr`'s pointer load: the same one instruction. **Candidate (i) of the
  dispatch — "the model borrowed per call adds an indirection in the hot loop" —
  is refuted by reading: there is no added indirection, the member simply stopped
  carrying a control block.**
* `:2074` / `:2097` — `iters.reserve(this->effective_max_iters_)` and
  `for (; i < this->effective_max_iters_; i++)`, reading a solver member where
  the parent read `opts_.max_iters`. One member load either way.
* `:3465` — **the one genuinely new statement inside the function**,
  `this->exit_grad_lag_ = v_rhs.prim_grad();`, and it is **after** the loop:
  once per phase, one vector copy.

`converge_check`, `fill_residual_info`, `eval_nlp` and `factor_impl` have **no
diff hunks at all** at this commit. `eval_nlp`'s compiled size is identical to
the byte (`0x2999` both sides).

### A.2 The dispatch's other named candidates, each answered

* **(ii) `kkt_pattern_is_analyzed(model)` / a per-solve pattern re-check** — it
  exists and it is new, but it runs **once per call** (`:888`, with the
  per-factorization `kkt_pattern_check()` seam at `:909` unchanged from the parent), not per iteration or per KKT solve.
* **(iii) the `wall_seconds` boundary** — one `Timer` start at `:4271` and one
  stop at the return. Two clock reads per call, not per iteration.
* **(iv) per-iteration allocation or residual copies** — there are none new
  inside the loop; every `resize`/`Zero(`/block assignment the commit adds is at
  the exit seam, below the loop (`:3465`–`:3500`; the declared-width `z` is
  built at `:3492`).
* **`compute_declared_diagnostics`** — new per-call work, wired here (it was
  *added* at `8ae1618` and *called* from this commit), called once per solve from the exit seam at `:5011` and measured by §10's leg at
  ≈342 500 instructions at n = 1000 and ≈6.7 M at n = 20000 against the ≈10.6 e9
  an `f7_n20000` row executes: **0.06 % of a row whose wall moved 2.9 %.**

### A.3 What the commit unambiguously does change

* **The solver object grows.** `sizeof(InteriorPointSolver)` **2056 → 2440**,
  because `result_`'s type grows **336 → 640** bytes. Every member declared after
  it moves up by exactly **320 bytes** — `nlp_` 992→1320, `acceptance_`
  1008→1328, `primal_vars_` 1056→1376, `stli_scratch_` 1080→1400, `bounds_`
  1360→1680, `bound_duals_` 1368→1688. `layout.txt` is the table, cut from
  clang's own `-fdump-record-layouts`.
* **The IPM object file grows and reshuffles.** `interior_point_solver.cpp.o`
  .text **285 330 → 295 042**; `alg_impl` +259 bytes, `factor_impl` +513 with no
  source change at all, `run_phase_sequence` +7 353. Most functions move
  *earlier* in the object even though the object is bigger.
* **The result core changes lifetime.** At the parent the result lived on the
  solver and was read through an accessor —
  `include/hven/drivers/interior_point_solver.h:511`,
  `const SolveResult &result() const { return result_; }` — with
  `reset_accumulators()` (`:360`) resetting its scalars and **leaving its vectors'
  storage alone across calls**. At the culprit the result is **returned by value**:
  `src/drivers/interior_point_solver.cpp:4263`, `this->result_ = IpmResult{};`
  at the public entry of every call, and `:5060`, `return std::move(this->result_);`
  at the exit. Every buffer the result owns is therefore **freed at entry and
  allocated afresh during each solve**, where the parent reused them.

### A.4 And the machine says the extra cycles are everywhere

`perf record -e cycles:u -F 997`, three rounds each, alternating, on the wall
pin's own four-cell invocation (`mechanism-tables.txt` §B2; the reports are
retained under `perf/rec/`). The largest single share delta is
**`difftime` +1.24 points** — that, with `mkl_serv_get_clocks_frequency`,
`__vdso_time` and the two `mkl_serv_cpuis*` probes, is MKL's one-time TSC
frequency calibration, which busy-waits for a `time()` boundary; it runs inside
the **first** solve of the process, which is the row the R3 rule excludes, and
its cost scatters by tens of milliseconds between processes.

With that family removed and the shares renormalised, **no group moves by more
than 0.41 points**:

| group | parent % | culprit % | delta |
|---|---|---|---|
| MKL kernels (in-binary) | 66.78 | 66.37 | −0.40 |
| Eigen | 20.46 | 20.60 | +0.14 |
| `hven::` | 2.23 | 2.65 | +0.41 |
| libiomp5 | 3.22 | 3.21 | −0.02 |
| libm | 1.82 | 1.85 | +0.03 |

**Two thirds of the extra cycles are spent in MKL's own Pardiso kernels — code
whose source did not change by one byte — in the same proportion as before.**
No function got slower relative to the others; everything got slower together.
The per-row table says the same thing from the other side: the step is
**proportional to each row's own wall**, +0.65 ms on a 32 ms row and +36 ms on a
1 101 ms row, so it is not a fixed per-call cost (`mechanism-tables.txt` §B0, which
prints all nineteen rows — the tiny `hs071` rows at 0.13 ms show a *larger*
ratio, 1.07–1.11, and 0.01 ms of *absolute* delta; a fixed per-call cost would
look like the opposite).

And the whole-process instruction and cycle counters **return no verdict**, for
a reason this leg measures rather than infers: the `b01 → b02` control pair is
the **same executable bytes**, and it reads instructions **+1.02 %** and cycles
**+1.81 %** (`mechanism-tables.txt` §B1) where the pair under test reads −0.13 %
and +0.71 %. The floor is larger than the step. That reproduces §11's finding on
a control stronger than T8.1's.

---

## B. Experiment 1 — put the hot members back where they were

**Hypothesis (dispatch candidate (v)).** The step is the solver object's growth:
`result_` gained 304 bytes and pushed `nlp_`, the five globalization pointers,
the six dimension scalars, all fourteen scratch vectors, `bounds_` and
`bound_duals_` up by 320 bytes.

**The edit** (`experiment-1.patch`, against `9cebbbe`, 2 hunks, 2 lines): move the
`IpmResult result_;` declaration to the end of the class and leave
`char hven_t89ra3_layout_pad_[320];` where it stood. Sized so that **every**
member from `acceptance_` down lands at the **parent's exact byte offset** —
`layout.txt` prints the four columns side by side, and they match to the byte.
Nothing executed changes: all 19 CSV columns on all 19 rows are identical to the
unpatched culprit's (361 cells, 0 mismatches, `experiments.out`).

**The result.** Measured against the parent and the culprit in the **same five
rounds**, arm order rotated. **All five batches meet R2''s FRACTION; on fix3's
EVIDENCE accounting four are PROVEN and `xwall-r2` is UNPROVEN-EVIDENCE** — a
foreign `R` at its `open` snapshot with no re-snapshot after it
(`../../../logs/IDLE-PROOF.md`; `logs/IDLE-PROOF-X.md` is the superseded
`user`-only proof):

| | scored corpus (s) | ratio to parent |
|---|---|---|
| parent `8ae1618` | 5.474975 | 1.0000 |
| culprit `9cebbbe` | 5.644315 | **1.0309** |
| culprit + experiment 1 | 5.615536 | 1.0257 |

**RECOVERY 16.8 %** of the corpus step; per-row median recovery **0.08**;
**0 of 11** rows at or above 0.80. **REFUTED as the mechanism.** The recovery is
small but it is outside the arms' round-to-round spread (0.18–0.50 %), so the
data offsets are worth something — they are not worth the step.

## C. Experiment 2 — move the parent's hot members to where the culprit put them

**The same hypothesis, from the sufficiency side**, which is the cleaner test:
the patched parent executes *literally* the parent's code.

**The edit** (`experiment-2.patch`, against `8ae1618`, 1 hunk, **1 line**):
`char hven_t89ra3_layout_pad_[320];` immediately before `nlp_`. Every member from
`acceptance_` down lands at the **culprit's exact byte offset**. All 19 columns
on all 19 rows identical to the unpatched parent's.

**The result**, same five rounds: **5.482352 s, ratio to parent 1.0013**.
**REPRODUCTION 4.4 %** of the step; per-row median **0.00**; **0 of 11** rows at
or above 0.80. **REFUTED, in the direction where a positive would have been
decisive.**

A fact worth recording beside it, because it bears on how much anyone should
have expected: 320 is **five whole cache lines**, so no member changed its
position within its line and no two members changed their distance apart.

## D. Experiment 3 — move the code instead, three distances

**Hypothesis (dispatch candidate (vi), and §11's second clause).** The step is
where the *code* sits: the IPM object grew 9 712 bytes and reshuffled, so every
function in it and every object linked after it is at a different address.

**The edit** (`experiment-3-{4096,9712,16384}.patch`, against `8ae1618`, 1 hunk
each): N bytes of unreachable `.text` at the head of
`src/drivers/interior_point_solver.cpp`, emitted by a module-level `__asm__`
block, so every function below it and every object after it moves. **9 712 is
the exact amount the culprit grew that object by** (the padded object measures
295 058 against the culprit's 295 042); 4 096 and 16 384 bracket it.

**The result**, five rounds, five arms in one batch set. **THREE OF THE FIVE
BATCHES FAIL R2''s FRACTION** — `ywall-r2`, `-r3` and `-r5` read 0.5291 %,
0.8065 % and 0.5293 % on the SMT sibling against a 0.5 % bar, the pinned core
itself staying under 0.16 % throughout — **and on fix3's EVIDENCE accounting
`ywall-r1` and `ywall-r5` are UNPROVEN-EVIDENCE as well** (a foreign `R` with no
re-snapshot), which leaves ONE of the five fully proven
(`../../../logs/IDLE-PROOF.md`; `logs/IDLE-PROOF-Y.md` is the superseded
`user`-only proof). **These are the weakest numbers in this file**, and they are
used only for a NEGATIVE claim — that this box is not generically
placement-sensitive at this scale — which sibling contention could only push the
other way for. They are flagged, not re-measured:

| arm | scored corpus (s) | ratio to parent | reproduction of the step |
|---|---|---|---|
| parent `8ae1618` | 5.479950 | 1.0000 | — |
| culprit `9cebbbe` | 5.648247 | **1.0307** | 100 % |
| parent + 4 096 B | 5.482519 | 1.0005 | **1.5 %** |
| parent + 9 712 B | 5.490612 | 1.0019 | **6.4 %** |
| parent + 16 384 B | 5.480450 | 1.0001 | **0.3 %** |

**REFUTED.** None of the three padded arms puts a single scored row above 1.01,
and the three are within 0.2 % of each other — **this box is not generically
sensitive to code placement at this scale**, which also disposes of the
comfortable reading that the step is unattributable placement noise. It is not
noise: it is reproducible to 1.0307 / 1.0309 / 1.0324 across three independent
round sets.

## E. Experiment 4 — the page-fault churn, and the third of the step it carries

**This is one experiment more than the brief's cap of three, and it is declared
as a deviation** (report §Deviations). It was run because the machine handed
over a signal that none of the three had touched, measured on the most
reproducible counter in this artifact.

**The observation.** Minor page faults, whole process, with a **byte-identical
control pair**, six rounds (`mechanism-tables.txt` §B3a):

| arm | minor faults | median | sys (s) |
|---|---|---|---|
| control A `510a4bb` | 308 104 … 308 107 | 308 105 | 0.6223 |
| control B `8ae1618` | 308 104 … 308 108 | 308 106 | 0.6213 |
| culprit `9cebbbe` | 361 254 … 361 286 | 361 263 | 0.7368 |

The control step is **×1.000003** — 1 count in 308 106, the tightest instrument
anywhere in this artifact — and the culprit's is **×1.1725, +53 156 faults**,
with **+0.1155 s of kernel time** beside it.

**And it is present where the wall is measured, not only under `perf`.** The
`perf stat` event list changes the measured child's argv+environ footprint, and
`attribution.md` §6 records that these cells are bimodal in exactly that; so the
count was re-taken with **no perf at all**, the wall pin's invocation byte for
byte, reading `/usr/bin/time`'s `%R` (whose format string is `/usr/bin/time`'s
argv, not the child's). Five rounds, alternating, **all five batches PROVEN on
the fix2 R2' re-audit** (`../../../logs/IDLE-PROOF.md`; `logs/IDLE-PROOF-F.md`
is the superseded `user`-only proof) — `§B3b`:

| arm | minor faults | median | sys (s) | user (s) |
|---|---|---|---|---|
| parent `8ae1618` | 308 269 … 308 331 | 308 272 | 0.590 | 5.680 |
| culprit `9cebbbe` | 361 415 … 361 424 | 361 421 | 0.680 | 5.870 |

**+53 149 first-touched 4 KiB pages per process = 207.6 MiB**, `sys` **+0.090 s**,
`user` **+0.190 s**.

**The edit.** The intervention is applied **identically to both arms** and
touches no source: glibc is told to stop handing large blocks back to the kernel
(`MALLOC_MMAP_THRESHOLD_`, `MALLOC_TRIM_THRESHOLD_`, `MALLOC_TOP_PAD_`), so the
allocator cannot un-map and re-fault what a previous solve released. If the step
is carried by re-faulting freshly-mapped pages it must collapse; if it is not,
both arms simply get faster together and the step stands. Five rounds, four arms
(two modes × two commits) in one batch set, `logs/A*-alloc-r*.log`, all five batches
PINNED-CLEAN (`logs/IDLE-PROOF-A.md`).

**The result** (`mechanism-tables.txt` §B4):

| arm | scored corpus (s) | minor faults | sys (s) |
|---|---|---|---|
| default / parent | 5.473976 | 308 271 | 0.600 |
| default / culprit | 5.638122 | 361 420 | 0.680 |
| bigthresh / parent | 5.130077 | 66 400 | 0.140 |
| bigthresh / culprit | 5.233006 | 66 554 | 0.140 |

* the intervention removes **99.7 %** of the extra faults (+53 149 → +154);
* the step falls from **+2.999 %** to **+2.006 %** — **32.8 % of it removed**;
* and it makes *both* arms 6–7 % faster (parent ×0.9372, culprit ×0.9281), which is a separate finding and is not
  this leg's to dispose of.

**CONFIRMED as a partial mechanism, at about a third of the step.** The change
that provokes the faults is §A.3's last bullet: the result core is
**default-constructed at every entry and moved out at every exit**
(`interior_point_solver.cpp:4263` and `:5060`), so the buffers it owns are freed
and re-allocated on every solve where `result()`-by-reference reused them
(`interior_point_solver.h:511`, `:360`). Freeing them returns memory to the
allocator's top and to the kernel between solves, and the next solve's large
buffers — the KKT assembly and the Pardiso workspace among them — come back on
freshly-mapped pages that must be faulted in and are cold in cache and TLB when
the kernels touch them. **That last sentence is the reading of the evidence, not
a pinned measurement**: what is pinned is the fault count, its floor, the kernel
time, and the 32.8 %.

---

## F. What is left, and what it is not

| hypothesis | experiment | recovered / reproduced |
|---|---|---|
| the solver object's members moved (removal) | 1 | **16.8 %** |
| the solver object's members moved (reproduction) | 2 | **4.4 %** |
| the library's code moved (three distances) | 3 | **0.3 / 6.4 / 1.5 %** |
| the result core's per-call allocate/free churn | 4 | **32.8 %** |

**About two thirds of `9cebbbe`'s +3.0…3.2 % is not accounted for by any of them**,
and what remains sits in user time (+0.190 s against +0.090 s of kernel time),
spread across MKL's Pardiso kernels and Eigen's assembly in unchanged
proportion, at an instruction count the instrument cannot resolve.

Four things it is **not**, each refuted here by measurement rather than by
argument:

1. **not added work** — twelve counter columns identical end to end (§5), all
   nineteen columns identical across every experiment pair, `eval_nlp` compiled
   to the byte;
2. **not `compute_declared_diagnostics`** — 0.06 % of a row;
3. **not the new object in the archive** — `8ae1618` adds it and the executable
   is byte-identical;
4. **not generic placement sensitivity** — three code shifts and two data shifts
   move the corpus by 0.01–0.5 %, and two later commits that rewrote the same
   files cost 0.3 % and −0.5 %.

**No disposition is offered — §11.1 and the owner have it.** No committed source
changed; the experiment patches are evidence, not a fix.

APPLE / ACCELERATE: UNOBSERVED.  WINDOWS: UNOBSERVED.
