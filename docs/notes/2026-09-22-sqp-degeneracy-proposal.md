# Why hven's SQP lane loses to SNOPT, and what to build — settler's proposal (2026-09-22)

## The diagnosis, in one paragraph
Twenty of the 23 training cells fail under the SQP lane. Eight of them die because the QP subproblem's active-set walk cycles: it drops a bound, the direction re-enters it at zero step, it re-adds it, thousands of times until the iteration cap. The cycle happens because the walk's working set becomes linearly dependent and the engine keeps going anyway: hven keeps a dependent working set factorizable through its dual regularization, but under dependence the multipliers are not unique, so the drop rule's sign test reads noise. That is a core-algorithm gap in the QP engine's working-set management, not a defect of the second-order information and not a tuning problem. Two step-length anti-cycling rules (EXPAND-style and Bland-style) were built and measured: they cannot help, because the state lives in the working set, not the iterate. The dependence has two distinct sources, established by an offline rank computation of every cell's equality Jacobian.

## The two populations
**A. Structurally rank-deficient equality Jacobians (network models).** SMBANK 1, DALLASM 1, STEENBRB 12, STEENBRC/D/E/G 14, BIGBANK 1 (150 with fixed arcs), MSS2 325, MSS3 946 redundant rows. Network balance: one redundant row per connected network or per period copy. Linear rows, so every subproblem carries it. Every other solver in the comparison copes with this routinely.
**B. Full-rank Jacobians where the working set itself goes dependent (power-flow, gas).** ACOPP57/118, ACOPR57/118, BRITGAS, YORKNET. The engine admits blocking bounds and rows past independence (no independence test at any add site); rows exceed variables by up to +106.
**C. SAWPATH**: full rank but rows scaled across ten orders of magnitude — any relative dependence test needs equilibration.
Separately: NET3, STEENBRD, STEENBRE start at a NaN objective (`project_start_point` is off by default); YORKNET additionally drops on a multiplier eight orders below its scale because the drop test is absolute where the neighbouring stationarity test is scaled.

## What to build (ranked; papers only: Gill, Murray & Wright 1981 ch. 5; Gill & Wong 2015 §4; Fletcher 1988/1993; Nocedal & Wright §16.5)
| item | what | fixes | does not fix | est. cost (this pipeline: implement + gates + census + review) |
|---|---|---|---|---|
| **E1 Independent equality basis at pre-solve** | Detect redundant equality rows once (sparse rank-revealing QR or SVD with row equilibration), remove them from the KKT basis, keep a map for residual checks and reconstructed multipliers | population A: DALLASM, STEENBRB/C/G (+D/E after E5), BIGBANK, MSS2/3 — up to 9 cells | B, C, NaN starts, cost per minor | 3–4 days |
| **E2 Independent working set with a multiplier-aware exchange** | Test each blocking add for dependence against the retained basis; on a dependent blocker, exchange it for a removable member chosen by the dependency coefficients and multiplier signs (Gill & Wong), recompute the step; never a blind full step | population B (6 cells) and the residual cycling in A | NaN starts | 5–7 days; the largest change; needs a cheap dependency test (maintained factor, not a dense QR per pass) and row equilibration (C) |
| **E3 Consistent multipliers under dependence** | Minimum-norm / least-squares multiplier estimate so drop pricing is meaningful when the set is dependent; scaled drop tolerance (YORKNET) | drop-rule noise; supports E1/E2 | nothing alone | 1–2 days |
| **E4 Certificate checks bounds** (running now) | kOptimal requires the returned iterate inside its box | a false optimality certificate (Critical) | — | in flight |
| **E5 `project_start_point` default on** | NaN-start cells | NET3, STEENBRD, STEENBRE | — | 0.5 day + census |
Order: E4 (in flight) → E1 → E3 → E2, with E5 ridden on E1's census re-take. E1+E3 first because they are cheaper, self-contained, and decide population A; E2 is the broad fix and benefits from E1's basis machinery.

## What this does not establish
Causation by intervention on a real cell: no arm converted a cell. E1/E2 are the published remedies matching the measured mechanism; the expected effect per cell is the settler's inference, to be measured by the census after each lands. Cost per minor (the dense Schur rebuild, C1) is a separate ceiling on SNOPT-parity even after the cycling is gone.

## Evidence trail
Census M3c; per-minor trace M3b; arms M4/M4b (`docs/notes/data/2026-09-m7-w2-m4*`); trace study M4c; degeneracy arms M5 (Codex: production-neutral, experimentally UNSUPPORTED; b1 retired); rank check M5r (`…-m5r-equality-rank/tables/rank.md`); ledger `docs/notes/2026-09-m7-ledger.md`.
