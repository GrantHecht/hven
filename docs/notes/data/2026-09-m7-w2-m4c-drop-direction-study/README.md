# M7 W2 M4c — the drop/direction trace study (READ-ONLY, POST-HOC, nothing ships)

**Result, in plain language.** In the shipped hven-sqp failures, the QP walk drops a
bound whose multiplier says "leave", and the direction computed after dropping it
re-enters the same bound at zero step. Over 100,051 such drop→re-add events on 52
recurrent solves (ten M3c cells, DEFAULT options, traces M4/M4b already pinned):

| mechanism (OBSERVED co-occurrence; causation NOT tested) | share of core events | cells |
|---|---|---|
| (b) the working set is provably linearly DEPENDENT — `me + |W| > n` by up to +106 | 68.5 % | ACOPP118, ACOPR57, DALLASM, STEENBRB/C/G, SAWPATH, half of ACOPP57 |
| (e) the direction leaves the released variable in place | 20.5 % | half of ACOPP57, most of BRITGAS |
| (c) indefinite reduced Hessian | 8.4 % | BRITGAS |
| (a) a FALSE DROP of a noise-level multiplier (8e-4 ulps of the gradient scale) because `drop_worst` tests `opt_tol` ABSOLUTELY while `free_block_stationarity` scales the same residual class | 2.5 % | all of YORKNET |
| (d) `p[released]` ≈ 0 with a noise sign | 0 on the core subset | — |

The engine's own contract text admits both halves in advance
(`include/hven/detail/qp/qp_engine.h` ~:333 "Linearly dependent working sets do not
break the loop … No anti-cycling rule is implemented"; ~:1505 the multiplier/direction
disagreement on a degenerate working set). There is no linear-independence test on
the add path. INFERRED: the active-set invariant (an independent working set) is not
maintained; that is why a step-length rule (M4b, EXPAND-style) could not help — the
state is a function of the working set.

**Provenance.** Read from commit `6af1d43e` (line numbers cited there; a mapping to
`0292bfd5` is in REPORT.md §0). Settler verified: `qp_walk_ws_size` excludes equality
rows (`qp_walk_trace.h:283`), so `me + ws_size > n` is an integer proof of dependence;
the two header passages are as quoted; the absolute `opt_tol` tests are at
`src/qp/qp_engine.cpp` ~:2321/:2337 at that commit.

**Files.** `REPORT.md` — the study (SIGNOFF `M7-W2-M4C-TRACESTUDY-REPORT`), incl. §6
"what the records cannot decide" (rank never measured — (b) is an UNDERcount; post-drop
inertia deliberately not recorded; causation not established) and §9. `wrappers/` — the
two derivation scripts; `tables/tables.md` and `tables/tails.json` — their outputs.
Inputs: the committed slices under `2026-09-m7-w2-m3c-default-census/traces-p2b/`,
`2026-09-m7-w2-m4-anticycling-arm/traces-m4/`, `2026-09-m7-w2-m4b-expand-on-the-ten/traces-m4b/`
and the uncommitted-but-sha-pinned slices named in `…-m4b/traces-m4b/SLICES.txt`.

**What it licenses.** Nothing under `src/` or `include/`. It names the pre-registered
experiment for the next task (M5): (a) a scaled drop tolerance on YORKNET alone
(prediction: all 2,538 core units vanish); (b) an independence-preserving add rule on
the seven class-(b) cells. Papers only: GMW 1981 ch. 5; N&W 2006 §16.5; Fletcher LAA
106 (1988), Ann. OR 46/47 (1993); Gill & Wong MPC 7 (2015).
