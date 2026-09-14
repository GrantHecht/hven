# M6 W5 — the API break window: acceptance evidence (T9)

Produced 2026-09-13 on branch `m6`; **code head `369e3146`** (the three T9
commits on top of it are docs, so that is the tree the numbers describe).
**Read `PROVENANCE.txt` first** — it records the commits, how the W5 BASE was
verified, the toolchain, the hardware, the declared serial protocol and the
comparator's pin, and it governs every number below. Every value here was
measured in this task, except `psym-record.md`, which is explicitly a record of
earlier tasks and says so.

> **Corrected 2026-09-13 (T9 fix round 3).** "The three T9 commits" above
> UNDERCOUNTS: T9 landed five docs commits on `369e3146`, and this fix round
> adds two more. The sentence is kept as first written and is not deleted —
> every one of those commits is docs-only, so the code head the numbers describe
> is unchanged, which is what the sentence was asserting. `PROVENANCE.txt` lists
> them all and records this correction, with the two others of this round, in
> its dated block at the end.

Everything is **MKL on Linux**. Apple/Accelerate and Windows are **UNOBSERVED**
and nothing is inferred from this run for either.

```
docs/notes/data/2026-09-m6-w5-acceptance/
  PROVENANCE.txt                    the stamp, the BASE verification, the serial rule
  suites.txt                        both suites, the long test, the smoke, the export
                                      contract, the golden rig, the wrapper fold proofs
  psym-record.md                    the per-task P-SYM record -- NOT re-run at T9
  replay/base-{walk,ssn,ipm}.csv    U0 27 cells at the W5 BASE accbff0e
  replay/head-{walk,ssn,ipm}.csv    U0 27 cells at HEAD 369e3146
  replay/comparisons.txt            the five 0-difference lines, with their header
  leg/head-interior-capture{1,2}.csv  the top-level interior-point leg, twice
  leg/comparisons.txt               43/30/0 twice, and the byte comparison
```

The 27-cell list is **not duplicated here**: it is
`docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt`, unchanged since
W1, and both replay arms were driven from that file. The comparator is
**not duplicated here** either: it is
`docs/notes/data/2026-09-m6-w4-acceptance/probes/compare_replay.py`, unchanged,
and `PROVENANCE.txt` carries its sha256.

Every CSV carries its own `#`-prefixed hven-native provenance header, the
convention the corpus CSVs use. Strip the `#` lines and what remains is the
artifact.

---

## 1. The claim this artifact settles

W5 made two claims. This directory is the evidence for the first, and the index
to the evidence for the second.

**(1) TRAJECTORY NEUTRALITY, across the whole window.** The API break changed
fifteen behaviours, retired two enums and a wrapper class, renamed 16
identifiers, 29 enumerators and 27 paths, removed four aliases and folded one
field out of a struct. Across all of it the solver's trajectory did not move:

| comparison | cells | columns | differences |
|---|---|---|---|
| W5 BASE `accbff0e` walk → HEAD `369e3146` walk | 27 | 75 | **0** |
| W5 BASE `accbff0e` ssn → HEAD `369e3146` ssn | 27 | 75 | **0** |
| W5 BASE `accbff0e` ipm → HEAD `369e3146` ipm | 27 | 75 | **0** |
| committed t10b IPM baseline vs HEAD-ipm | 27 | 75 | **0** |
| committed t10b IPM baseline vs BASE-ipm — **the control** | 27 | 75 | **0** |
| committed interior baseline vs capture 1 | 43 | 30 | **0** |
| committed interior baseline vs capture 2 | 43 | 30 | **0** |

Both interior captures are additionally **byte-identical** to the committed
baseline's 43 data rows outside `wall_s`. The control line is what makes the
fourth a statement about the tree rather than about this run's conditions: the
BASE binary reproduces the committed t10b baseline too.

This is a stronger reading than any single task's. Each W5 task proved 0/75
against ITS OWN base; the window's neutrality is the composition of twenty-odd
such steps, and a composition can hide a pair of cancelling errors that neither
step showed. Measured end to end, there is nothing to cancel.

**(2) INSTRUCTION NEUTRALITY where claimed** is per task and is not re-run here.
`psym-record.md` is the index: for each of the twenty-three W5 tasks it names
the ledger close line that recorded its P-SYM result, quotes what that line
found, and says which retained transcript it was written from. It also records, per
task, where the result was a DECLARED DIFFERENCE rather than identity: a task
that deletes a translation unit, moves a member, or shrinks a struct cannot be
instruction-neutral, and each such task named its differing set BEFORE its arms
were built and argued it in its own close line. The ledger records an explicit
declared-FAIL verdict at T8.6, T8.9 and T8.10's stage B.

## 2. The suites, and the golden rig inside them

| leg | result | note |
|---|---|---|
| Debug, long test excluded | **2541 / 2541** passed, 0 failed | `-E ^Continuation\.ProbeBudgetBoundsAFailingProposal$` |
| Debug, the long test ALONE | **1 / 1** passed, 671.87 s | detached under nohup, alone on the lock, pids recorded |
| Release, whole suite | **2542 / 2542** passed, 0 failed | 2541 + 1 = 2542 |
| `ctest -N`, both configs | **2544 / 2544** | three tests do not run in either config, the same three |

**The golden rig runs inside the suites** — 48 `Arms/SqpTrace` and
`Arms/InteriorPointTrace` arms, `GoldenRigAudit.StaticScanSelfTest`, and the
`FailByDesignControl` skip — and **`tests/golden_rig/expected*` is byte-unchanged
since the W5 BASE.** So is the whole directory:

```
$ git diff --stat accbff0e HEAD -- tests/golden_rig/
(no output)
```

That is the rig's own statement about the window: the OLD-SEAM adapters and
every expected table they compare against are the bytes they were before the
break, and they still pass against a tree whose public API is entirely renamed.

## 3. The installed consumer

| check | result |
|---|---|
| `scripts/check_install_smoke.sh` | **PASSED** — 18 standalone-include TUs, both solvers converged through ONE generic consumer against a fresh install prefix |
| `scripts/check_export_contract.sh` | **PASSED** — platform macros, `EIGEN_MAX_ALIGN_BYTES` exported=32 computed=32, no `INTERFACE_COMPILE_OPTIONS`, include dirs exactly once |

The smoke is the sharpest gate this window has, because T8.9 made its consumer
GENERIC: one `run_once<Solver, Model>` instantiated for BOTH engines. A shape
that drifted apart on one engine would fail to compile there, against an
installed prefix, read by the system default front end rather than the project's
pinned clang. `suites.txt` carries the full transcript.

## 4. What this artifact does NOT claim

* **No wall-clock number here is asserted.** Every duration is informational
  (CLAUDE.md §7). The window's one wall-asserting measurement is T8.9r, whose
  own artifact is `docs/notes/data/2026-09-m6-w5-t8-runtime/` — read its
  `PROVENANCE.txt` first, and read `docs/migration/2026-09-m6-api-break.md` §6
  for the three-sentence version with the owner's KEEP.
* **No Apple or Windows value.** UNOBSERVED, not estimated.
* **No P-SYM run.** `psym-record.md` says why, and what stands instead.
* **No statement about tycho.** The owner ruled on 2026-09-04 that the window
  does not wait for it; tycho consumes on its resume. The brief and the roadmap
  carry that ruling as dated amendment blocks, and
  `docs/migration/2026-09-m6-api-break.md` is what tycho reads when it does.

## 5. Where to go next

| you want | read |
|---|---|
| to migrate a consumer across the break | `docs/migration/2026-09-m6-api-break.md` |
| the reasoning behind any one break | `docs/notes/2026-09-m6-w5-migration-guide.md` |
| what closed each task, and on what evidence | `docs/notes/2026-08-m6-ledger.md`, the W5 section |
| the window's one runtime reading | `docs/notes/data/2026-09-m6-w5-t8-runtime/` |
