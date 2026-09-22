# M7 W2 M4c — the drop/direction trace study (READ-ONLY, POST-HOC, nothing ships)

**Result, in plain language.** In the shipped hven-sqp failures, the QP walk drops a
bound whose multiplier says "leave", and the direction computed after dropping it
re-enters the same bound at zero step. Over 100,051 such drop→re-add events on 52
recurrent solves (ten M3c cells, DEFAULT options, traces M4/M4b already pinned):

| mechanism (OBSERVED co-occurrence; causation NOT tested) | share of core events | cells |
|---|---|---|
| (b) the working set is provably linearly DEPENDENT — `me + |W| > n` by up to +106 | 68.5 % | ACOPP118, ACOPR57, DALLASM, STEENBRB/C/G, SAWPATH, half of ACOPP57 |
| (e) residual / unclassified after (a)–(d), often off the retained face — **not a direction mechanism**: all 100,051 core directions cross the released bound (`released_directional > 0`), and it is α = 0 that leaves the variable in place | 20.5 % | half of ACOPP57, most of BRITGAS |
| (c) the engine's negative-inertia-excess signal (`inertia_neg > inertia_expected_neg`; the post-drop reduced Hessian's inertia was NOT recorded) | 8.4 % | BRITGAS |
| (a) a drop of a noise-level multiplier (8e-4 ulps of the gradient scale) where `drop_worst` tests `opt_tol` ABSOLUTELY while `free_block_stationarity` scales the same residual class — a HYPOTHESIS pending arm (a)'s intervention (M5), not an established false drop | 2.5 % | all of YORKNET |
| (d) `p[released]` ≈ 0 with a noise sign | 0 on the core subset | — |

The engine's own contract text admits both halves in advance
(`include/hven/detail/qp/qp_engine.h` ~:333 "Linearly dependent working sets do not
break the loop … No anti-cycling rule is implemented"; ~:1505 the multiplier/direction
disagreement on a degenerate working set). There is no linear-independence test on
the add path. INFERRED: the active-set invariant (an independent working set) is not
maintained. That a step-length rule (M4b, EXPAND-style) could not help *because of
that* is a further INFERENCE **these records do not test** (REPORT.md §6 items 7–8):
what M4b observed is that arm E converted nothing and that its floor displacement is
erased one pass later on the KKT-snap branch. Closing the gap between the
co-occurrence and the cause needs the intervention M5 runs, not a trace.

**Provenance.** Read from commit `6af1d43e` (line numbers cited there; a mapping to
`0292bfd5` is in REPORT.md §0). Settler verified: `qp_walk_ws_size` excludes equality
rows (`qp_walk_trace.h:283`), so `me + ws_size > n` is an integer proof of dependence;
the two header passages are as quoted; the absolute `opt_tol` tests are at
`src/qp/qp_engine.cpp` ~:2321/:2337 at that commit.

**Files.** `REPORT.md` — the study (SIGNOFF `M7-W2-M4C-TRACESTUDY-REPORT`), incl. §6
"what the records cannot decide" (rank never measured — (b) is an UNDERcount; post-drop
inertia deliberately not recorded; causation not established) and §9. `wrappers/` — the
two derivation scripts; `tables/tables.md` and `tables/tails.json` — their outputs.

**Inputs, and how to reproduce the two tables.** `wrappers/m4c_tails.py` reads the TEN
OFF **flat** traces `off-<CELL>.csv` — M4's own, uncommitted (457 MB for the twenty)
but sha-pinned in
`…-m4b/traces-m4b/SHA256-parent-traces-m4b.txt` and re-verified in
`…-m4b/logs/pin-off-traces.txt`; they live at `.scratch/m7w2m4/arm/traces/` on the box
they were taken on. It imports `classify_cause_fix1.py` **unmodified** from the
committed `…-m4b/wrappers/`. The per-solve slices committed under
`2026-09-m7-w2-m3c-default-census/traces-p2b/`,
`2026-09-m7-w2-m4-anticycling-arm/traces-m4/` and
`…-m4b/traces-m4b/` (plus the uncommitted, sha-pinned ones named in
`…-m4b/traces-m4b/SLICES.txt`) are the committed *excerpts* of those same walks — they
are what a reader checks a row against, not what the script consumes.

```
m4c_tails.py OUTDIR off-ACOPP118.csv off-ACOPP57.csv off-ACOPR57.csv off-BRITGAS.csv \
    off-DALLASM.csv off-SAWPATH.csv off-STEENBRB.csv off-STEENBRC.csv \
    off-STEENBRG.csv off-YORKNET.csv          # -> OUTDIR/tails.json
m4c_table.py OUTDIR/tails.json OUTDIR/tables.md
```

**What it licenses.** Nothing under `src/` or `include/`. It names the pre-registered
experiment for the next task (M5): (a) a scaled drop tolerance on YORKNET alone;
(b) an independence-preserving add rule on the seven class-(b) cells. Arm (a)'s
prediction is split (M5 amendment A6): OFFLINE, replayed over the recorded YORKNET
drops, all 2,538 fail the scaled test; DYNAMICALLY the trajectory diverges after the
first refused drop, so a different surviving tail is **not** a refutation — refutation
means the same candidate identity is still dropped, or the same matched recurrence
persists.

The published remedy, stated as the papers state it. Gill & Wong (*Math. Prog. Comp.*
**7** (2015) 71–112) formulate the working set as linearly INDEPENDENT and give a
KKT-based dependency test with a multiplier-aware exchange — not a "largest
coefficient" rule. Nocedal & Wright (2006) §16.5 concerns the EXACT method: an
independent starting working set plus an exact null-space direction implies a genuine
blocking constraint is automatically independent (Exercise 16.15); it prescribes
nothing for the REGULARIZED case, where a blocker can exist only because `aᵀp` leaked
off zero. Fletcher (LAA **106** (1988) 149–183; *Ann. OR* **46/47** (1993)) supports a
dedicated degeneracy procedure over the tied/dependent constraints. GMW 1981 ch. 5 is
the definitional statement of the independent working set.

M5's arm (b) design was amended accordingly
(`.superpowers/m7-w2-m5-armb-amendments.md` A1–A7): the rank oracle is a
rank-revealing QR of the UNREGULARIZED active rows at a pre-declared
`dependency_tol`, never the regularized KKT factor whose `−μI` block is what makes
dependent rows factorizable (A1); the invariant is `rank(R) = me + |W| ≤ n` after
every mutation, not the row count, which is necessary only (A2); one shared
`admit_if_independent` predicate covers **every** membership-mutating site — ordinary
add, ride add, seed ingestion, shift additions, zero-probe restoration, repair pins —
with a dependent seed repaired at entry and a dependent MANDATORY row refused with a
diagnostic (A3); no exchange is attempted in M5, the intervention is a pass-over of
dependent candidates in ratio order, with Gill & Wong's multiplier-aware exchange
registered as the production design item (A4); the leak is measured with normalized
`aᵀp`, the retained rows' own leaks, the dependency coefficients, and a `μ` sweep
before anything is called `O(μ)` (A5); arm (a)'s prediction is split offline/dynamic
(A6); and the neutrality report adds feasibility and stationarity against each leg's
own tolerances, with no "production-ready" wording (A7).

---

## fix1 — the changes made in fix round 1 (2026-09-22)

Against `.superpowers/m7-w2-m4bc-review-sol.md` (M4c: FIX-ROUND (4) + minors 2–3).
Documentation and derivation scripts only; nothing under `src/`, `include/`, `tests/`;
no solver ran, nothing was built.

| item | change |
|---|---|
| **I1** (Important 1) | `wrappers/m4c_tails.py`'s import of `classify_cause_fix1` was a repository-RELATIVE tail joined onto an ABSOLUTE directory, computing `…/docs/notes/data/docs/notes/data/…`, which exists nowhere. Now RELOCATABLE: resolved from `__file__` (the sibling M4b artifact directory), then from the repository root found by walking up to a `.git` entry — `os.path.exists`, not `isdir`, since that entry is a FILE in a worktree — and REFUSING with the list of places it looked if neither carries the module. Both resolutions and the refusal were exercised. |
| **I1 re-run** | Both tables re-derived from the ten sha-verified OFF parents (all ten match `SHA256-parent-traces-m4b.txt`) and the committed `classify_cause_fix1.py`. **BYTE-IDENTICAL.** `tails.json` `sha256 852154d7b5053a6d2dea60259217f18dd9046ce2be833b4e959dd47c6639edb5` before and after; `tables.md` `sha256 ece73d50c8e7495593a888b974af72b67e10ab5cde8e6082795358dba06dd670` before and after. |
| **minor 2** | The core predicate now asserts `A.action == "add"` explicitly instead of inferring it from the blocker fields. **Delta 0**: core 100,051 → 100,051, and `A.action` is `add` on 100,051 / 100,051 — the review's re-derivation of ACOPP57 and BRITGAS stands. `A.action` is carried beside the unit, not inside it, so the pinned per-unit record in `tails.json` is unchanged. |
| **minor 3** | `c_verdict_wrong` was advertised as a class-(c) predicate but caused no label. It is **removed from the advertised predicates** and documented as a recorded diagnostic that participates in no class, here, in REPORT §4 and in the script. The other option — letting it decide class (c) — was tested against the records and REFUSED: it fires on units with a NEGATIVE inertia excess (ACOPR57, ACOPP57), which REPORT §6 item 4 declines to interpret, and would relabel ACOPP57's 1,777 residual units. Its key keeps the `c_` spelling it was PINNED with in `tails.json`; the prefix is a frozen artifact name, not a claim of class membership. |
| **I2** | Class (e) renamed here, in REPORT §4/§4.2 and in the script: "residual / unclassified after (a)–(d), often off the retained face". It is the unconditional fallback — neither `e_off_face` nor `e_square` is required — and it is NOT a direction mechanism: all 100,051 core directions cross the released bound; α = 0 is what leaves the variable in place. |
| **I3** | Three narrowings, here and in REPORT (each marked `[fix1]` there): "that is why" → an INFERENCE the records do not test (§6); "false drop" → a hypothesis pending arm (a)'s intervention in M5; "indefinite reduced Hessian" → "the engine's negative-inertia-excess signal", since the post-drop reduced Hessian's inertia was never recorded. |
| **I4** | The remedy text above and in REPORT §7–8 corrected: Gill & Wong 2015 = KKT-based dependency test + multiplier-aware exchange (not "largest coefficient"); Nocedal & Wright §16.5 = the exact method (Exercise 16.15), prescribing nothing for the regularized case; Fletcher = a dedicated degeneracy procedure. M5's amended design (A1–A7) summarized above and in REPORT §8. |
| **untouched** | `REPORT.md`'s SIGNOFF line, the two committed tables' bytes, every count, every provenance line, and `.superpowers/m7-w2-m4c-report.md` (REPORT.md is no longer byte-identical to it: the fix1 edits land in the committed REPORT, which is the artifact of record). |
