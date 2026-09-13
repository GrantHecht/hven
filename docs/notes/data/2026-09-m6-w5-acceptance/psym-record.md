# M6 W5 T9 — the per-symbol (P-SYM) record

**P-SYM WAS NOT RE-RUN AT T9, AND NOTHING HERE IS A NEW MEASUREMENT.** Each W5
task ran its own per-symbol comparison against its own BASE, declared its
expected differing set before the run, and had that result reviewed and recorded
in its ledger close line. A whole-window P-SYM (the W4 close head against HEAD)
would be a comparison across 168 commits, three renamed TUs, a deleted TU, a new
TU and a struct-layout change: its differing set is the UNION of every task's,
and it would assert nothing that the per-task comparisons have not already
asserted under review. This file is therefore a **record**, not a run.

**What the ledger is, and what the transcripts are.** The ledger close line is
the RESULT — the reviewed statement of what P-SYM found at that task, with the
declared-versus-observed accounting. The retained transcript is the raw
material: it holds the intermediate probe runs, the calibrations and the fix
rounds as well as the final arms, so a bare grep of its `P-SYM:` lines shows
FAIL verdicts that a reader would misread out of context. **Read the ledger line
first; the transcript is what the ledger line was written from.**

Ledger line numbers are as of HEAD `369e3146` (they shift when the W5 CLOSE
entry lands); the date and the task name in each close line are the stable
handle.

| task | ledger | what the ledger records P-SYM found | retained on disk |
|---|---|---|---|
| **T0** | 2026-09-05, `:2703` | No comparison owed: no library code changed at any T0 commit (`git diff d0c7aa0..d9243e6 -- src include` empty). Calibration (a) proves the library's objects byte-identical to the pinned parent's (`129 objects — 129 byte-identical … 0 missing / 125145 matched, 0 only-before, 0 only-after, 0 collisions, 0 rank-tagged`); calibration (b) the one-symbol rename, FAIL without the map and PASS with it. **Tool pin `1aa67ea85b8c…`** | `.superpowers/w5-t0-report.md` + `w5-t0-fix{1,2,3}-report.md`; scratch `w5t0` GONE |
| **T1** | 2026-09-05, `:2791` | Six comparisons over seven arms (A = `5213d3a` … G = fix head), **all PASS**, `0 stale exceptions / 0 stale map entries`; the moved bodies MAPPED MATCHES, `run_nlp_solver` 150/150 identical under one `[SECTION]` exception; the −8 member-displacement class binned in full (OTHER 0 / UNCLAIMED 0 / VPTR 0). **Close pin `f86a7f0ba101…`** | `.superpowers/w5-t1-fix1-psym-transcripts.md` (577 K, 14 verdict/count lines, pin `f86a7f0ba101`); scratch `w5t1`, `w5t1f2-psym` GONE |
| **T2** | 2026-09-05, `:2894` | ONE comparison `ecef96c → 6dfeb0a`: `129 objects — 121 byte-identical, 3 noise-only, 0 unclassified; 60271 matched (3 through the symbol map), 0 only-*, 167 excepted (184 object-qualified entries, 0 wildcards), 0 stale`. One pair OUTSIDE the claim reported rather than excused (`init_impl`, alignment padding). **Tool `092ee5f2…`** | `.superpowers/w5-t2-psym-transcripts.md` (875 K, 38 lines, pins `092ee5f2bd94`, `f86a7f0ba101`); scratch `w5t2` GONE |
| **T4** | 2026-09-06, `:2963` | ONE comparison `eb22025 → 6d74c5d`, NO exception file, a GENERATED 15-line symbol map and ONE object-map line: `129 objects — 112 byte-identical, 12 noise-only, 0 unclassified; 60420 matched (17 through the symbol map), 0 only-*, 0 excepted, 0 stale; relocations 151685 (140 through the map)`. The one byte change the tool does not read (`_ZTS` typeinfo-NAME content) named in advance. **Tool `092ee5f2…`** | `.superpowers/w5-t4-psym-transcripts.md` (35 K, 7 lines, pin `092ee5f2bd94`); scratch `w5t4` GONE |
| **T5** | 2026-09-06, `:3012` | Test-only: every `hven.dir` object and `libhven.a` byte-identical, so no replay owed; 122/129 byte-identical, 3 bench stamp objects `CHANGED 0`; symbol map GENERATED from `nm` (64 through the map; 53 object-qualified exceptions; 0 stale); 63 mapped pairs disassembled beyond what the tool asserts, 56 identical and 7 explained. **Tool `092ee5f2…`** | `.superpowers/w5-t5-psym-transcripts.md` (82 K, 2 lines, pin `092ee5f2bd94`); scratch `w5t5` GONE |
| **T3** | 2026-09-06, `:3060` | ONE comparison, CLAIM before the run: `129 objects, 104 byte-identical, 0 unclassified, 0 only-before/after, 0 stale`; 35 exceptions = 34 migrated functions named by site + 1 layout-only neighbour, that neighbour independently disassembled by the lane and by Codex. **Tool `092ee5f2…`** | `.superpowers/w5-t3-psym-transcripts.md` (35 K, 4 lines, pin `092ee5f2bd94`); scratch `w5t3` GONE |
| **T6 commit 0** | 2026-09-06, `:3141` | Not a comparison: the P-SYM tool protocol for TU splits, with its own pins. Re-closed twice as declared events (T6.a `:3217` fix2, T6.b `:3287` fix3). | scratch `.scratch/w5t6-0/` PRESENT |
| **T6.a** | 2026-09-06, `:3217` | Both arms from EMPTY dirs at the final tool: `129 objects, 124 byte-identical, 0 unclassified, 0 only-before, 0 stale`. **Tool `a545438fcc71…`** | `.superpowers/w5-t6-a-psym-transcripts.md` (23 K, 4 lines, pin `a545438fcc71`); scratch `w5t6a` PRESENT (no compare summary retained in it) |
| **T6.b** | 2026-09-06, `:3287` | At the fix3 tool from EMPTY dirs: `129 objects, 124 byte-identical, 0 unclassified, 0 only-before, 7 exceptions consumed, 0 stale`. **Tool `167ecd8f…`** | `.superpowers/w5-t6-b-psym-transcripts.md` (23 K, 6 lines, pins `167ecd8ff918`, `6ea1fb342073`); scratch `w5t6b`, `w5t6b-fix1` PRESENT |
| **T6.c** | 2026-09-07, `:3387` | `base→c1 PASS on 19 object-, kind- and claim-qualified exceptions, 0 stale`. **Tool pin commit-0 fix4 `6ea1fb34…` / `dd507880…`** | `.superpowers/w5-t6-c-psym-transcripts.md` (76 K, 10 lines, pin `6ea1fb342073`); scratch `w5t6c` PRESENT |
| **T6.d** | 2026-09-07, `:3464` | The ABANDONED cut, and then its REVERT: against `14cf243` from empty directories at the pinned tool, EMPTY exception set, rc=0 — `126 objects byte-identical, 0 unclassified, 0 stale`, `libhven.a` byte-identical, **no kernels object**, all four test objects byte-identical. | `.superpowers/w5-t6-d-psym-transcripts.md` (40 K, 18 lines); scratch `w5t6d` PRESENT |
| **T7** | 2026-09-08, `:3565` (+ follow-up `:3627`) | Comment-only: `libhven.a` byte-identical (`735eea1d…`, all 41 library objects), P-SYM `CHANGED 0` instructions, 165 objects rolled — 159 identical, 3 stamp-only (11 bytes), 3 belonging to T8.1's interleaved commit. **Tool `6ea1fb34…`** | `.superpowers/w5-t7-psym-transcripts.md` (25 K, 2 lines, pin `6ea1fb342073`); scratch `w5t7` PRESENT |
| **T8.1** | 2026-09-08, `:3665` | From EMPTY build dirs, no maps, no exceptions: `hven.dir` **41/41 byte-identical**, `libhven.a` identical (`735eea1d…`), the differing set EXACTLY the three pre-declared bench/test objects. | `.superpowers/w5-t8-1-psym-transcripts.md` (16 K, 2 lines, pin `6ea1fb342073`); scratch `w5t81` PRESENT |
| **T8.2** | 2026-09-08, `:3729` | From EMPTY dirs on `git archive` extractions, the set DECLARED before either arm = the set observed EXACTLY; at fix2 `alg_impl` the ONLY differing symbol of the IPM's main TU (247 identical). | 3 transcripts (51 K, 6 lines, pin `6ea1fb342073`); scratch `w5t82` PRESENT |
| **T8.3** | 2026-09-08, `:3839` | From EMPTY dirs under `.scratch/`, **declared = observed**: 9 library objects + the rename pair + the test/bench `sizeof` class. The 43 unique ONLY-BEFORE names accounted: 27 enum-signature rename pairs, 16 not renames. | 2 transcripts (28 K, 4 lines); scratch `w5t83` PRESENT |
| **T8.4** | 2026-09-08, `:3932` | **Declared = observed** — 13 of 43 library objects at the task, 7 of 43 at fix1; **EVERY QP kernel byte-identical at both**. | 2 transcripts (22 K, 4 lines); scratch `w5t84` PRESENT |
| **T8.5** | 2026-09-09, `:4036` | From EMPTY dirs; fix1 re-ran `ee35257 → d60b49b` naming all TEN library objects explicitly (the `warm_start` entry the re-mangle of five implicit special members, corrected from the report's first reading). | 2 transcripts (30 K, 6 lines); scratch `w5t85` PRESENT |
| **T8.6** | 2026-09-09, `:4111` | At the pin from EMPTY dirs — the task 42 differing all named in advance, fix1 40 differing all named in advance (`hven.dir` 3/43 size-only noise), 0 only-before, **every QP kernel and `finish` byte-identical**, the verdict **FAIL as declared**. | 2 transcripts (24 K, 4 lines, pin `6ea1fb342073`); scratch `w5t86` PRESENT |
| **T8.7** | 2026-09-10, `:4166` | The task 54 differing (the declaration under-named one object, corrected in the transcripts); fix1 45 differing over both commits, all inside the declaration + its dated addendum; `hven.dir` 3/44, 0 only-in-either; the final arms show **all five QP kernels IDENTICAL** after the accumulator moved off `KktFactorization`. | 2 transcripts (27 K, 4 lines); scratch `w5t87` PRESENT |
| **T8.7b** | 2026-09-10, `:4223` | The task 53 differing, all inside the named set, with exactly one ONLY-IN-BEFORE (the deleted TU) — and one object **predicted identical but observed differing**, recorded as declared ≠ observed rather than absorbed. | 2 transcripts (25 K, 4 lines); scratch `w5t87b` PRESENT |
| **T8.8** | 2026-09-10, `:4270` | **The first W5 task on which QP kernel objects move BY CONSTRUCTION**; the declared set per commit, the lane's roll exactly the six, every `interior/`, `model/`, `core/` and both IPM objects byte-identical; fix1's raw roll exactly the three declared. | 2 transcripts (26 K, 5 lines); scratch `w5t88` PRESENT |
| **T8.9** | 2026-09-11, `:4334` | Verdict **FAIL DECLARED**, with the complete differing set named in §0 before any arm was built — `hven.dir` exactly 2 of 42 plus one ONLY-IN-BEFORE (the deleted wrapper TU); every QP kernel, every `linear/`, and both driver TUs byte-identical BY NAME. | 2 transcripts (19 K, 2 lines); scratch `w5t89` PRESENT |
| **T8.10** | 2026-09-13, `:4537` | **Three stages, three different answers, stated as three** rather than as one verdict. (A) the renames alone: 132 objects, 71 byte-identical, 26 in the `__LINE__`/`__FILE__` noise class, FIVE differing symbols, every one a string materializer. (B) + the four alias removals and the field fold: 42 objects, **563 differing symbols — NOT instruction-neutral, DECLARED**, a struct-layout change (`sizeof(SqpOptions)` 384 → 376); the census attributes 535 to the fold's own mechanism and leaves 28 UNATTRIBUTED, reported as a FAILED claim rather than absorbed, with the settler's ruling and the reviewer's classifier caveat both on the record — and the question the census served closed **at the SOURCE**, the stage-B minus stage-A delta being exactly the four alias declarations, the field deletion, the `.start_level` rewrites and two test repairs. (C) + the 57-file whitespace reflow: **PASS**, 122 byte-identical, ten test objects differing only in `__LINE__`, every library and bench object byte-identical. **Tool `6ea1fb34…` / `dd507880…`** | 2 transcripts (30 K, 8 lines); scratch `w5t810`, `w5t810-fix1` (incl. `psym-A-vs-base.txt`, `psym-B-vs-A.txt`, `psym-C-vs-B.txt`), `w5t810-fix2` ALL PRESENT |

## What no longer exists on disk

The scratch directories of **T0, T1, T2, T3, T4 and T5** have been swept
(`.scratch/w5t0`, `w5t1`, `w5t1f2-psym`, `w5t2`, `w5t3`, `w5t4`, `w5t5`). Their
evidence survives in two places that were never scratch: the ledger close line
named in the table above, and the retained
`.superpowers/w5-t*-psym-transcripts.md` file, which for every one of those six
tasks is present. **No task's P-SYM evidence is lost.** `.superpowers/` and
`.scratch/` are both git-ignored, so neither is part of this artifact; the table
names them so a reader on this machine can find them and a reader elsewhere
knows what was and was not retained.

`.scratch/w5t6a` is present but retains no compare summary of its own; T6.a's
result is the ledger line and `.superpowers/w5-t6-a-psym-transcripts.md`.

## The tool, and the one thing to know about comparing these numbers

The P-SYM tool was **re-pinned six times across W5**, each time as a declared
event with its reason in the ledger: `1aa67ea8…` (T0) → `f86a7f0b…` (T1) →
`092ee5f2…` (T2 commit 0) → `cb85572d…` (T6 commit 0) → `a545438f…` (T6.a fix2)
→ `167ecd8f…` (T6.b fix3) → `6ea1fb34…` (T6.c commit-0 fix4), which is the pin
every T7-and-later arm was captured with. **Object counts are not comparable
across those pins**: the capture set grew from 117 to 129 at T0 and to 132 by
T8.10 as new test and gate binaries entered it, and what a verdict ASSERTS
changed three times at T0 (declared in that close line). Each row above is
internally consistent with its own task's arms and is not a series.

*Nothing in this file was measured by T9. Every figure is quoted from the ledger
close line named beside it.*
