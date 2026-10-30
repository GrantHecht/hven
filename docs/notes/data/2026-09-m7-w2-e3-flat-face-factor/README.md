# M7 W2 E3 — the flat-face factor: the regularization's centre on the EQP side

Base `c864b8a6` (sources `0fa59cdc`, NC2 + fix1). Origin: NC2 fix1's surviving finiteness gap — the
ordinary step on a flat face ascends `f` (fix1 §A–B; `REVIEW-fix1-recheck-fable.md` M26) — and RETAKE8
D9 (hven-bench `runs/2026-09-m7-w2-retake8/README.md`), which measured `eq_drift_violations` over 6900
passes on 9 of 15 TRAIN cells. Brief: `.superpowers/m7-w2-e3-brief.md` (rulings E3-1–E3-7, settler
adjudication E3-8–E3-11). Implementer hand-back: `.superpowers/m7-w2-e3-report.md`
(SIGNOFF: M7-W2-E3-REPORT), reproduced verbatim as `REPORT.md` behind a settler preamble.

This was a measurement-first round in the EQP layer, and its measurements corrected two things and
identified a third. E3-1 measured, before any design: (a) the EQP's ordinary step on a flat face
ascends `f` on every pass of the `Cs_STEENBRB` cycles, by a positive fraction of the absolute
centring's own term — the cycle's energy ledger closes to `8e-19`; (b) the one refinement step
contracts the EQP answer's own residual 33×/2.3×/3.2× on three cells, correcting fix1's inferred
"at best 2×–3.5×" (a different object — `stat_free_inf` at the iterate, not the EQP answer's own
residual); (c) the equality drift is the FACTOR'S SOLVE, not the regularized model — the exact
regularized answer misses its own rows by `mu*|lambda_e|` (8e-8, not the `~7e10` the brief's
alternative needed), while the engine's factored answer at that pass leaves a residual of `2.57e+02`
against its own system on `cond(K) ~ 1e11` with a 17-dimensional flat nullspace.

E3-2 built a test-only PROXIMAL-centring arm (the primal regularization on the step from the current
iterate rather than centred at 0) and measured it (E3-3) against the full battery: corpus, census, HS
pins, U0, Debug + Release suites. **The arm does NOT ship** (E3-4, settler ruling E3-9). It converts
all six `Cs_STEENBRB` cells to the Clarabel optimum and zeroes the equality drift on every cell that
had it — the strongest single lever measured in W2 — but it loses four certified `ACOPP57_def` cells,
produces two NaN objectives, fails the HS corpus, and fails 52/50 suite tests. The mechanism (measured,
not inferred): the proximal step is `p = -(1/delta) P_Z g`, `1/delta`-sized at a converged point, and
two things in the walk are written in the wrong currency for that — the step-size termination test
(`||p||inf <= feas_tol*max(1,||x||inf)`, unreachable once `p` carries `1/delta`) and the runaway guard
(written on `|x_i|`, which never moves while `p` blows past `0.1/delta`). The absolute centring's bound
is needed for the three runaway-detection fixtures, in some currency. The δ-free re-price fallback
(E3-4) is also unavailable: the unregularized system is exactly singular on these flat faces.

No production commit resulted. The one commit, labelled `arm(m7): W2 E3 ...`, sits on branch `e3` of
worktree `agent-a69f46e11796560b9` at `cce88386` (`arm.diff.txt` here, labelled NOT MERGEABLE — it
reads `getenv` from library code, so the object code is not byte-identical to the option-off baseline
even though the measured behaviour is). Registered (E3-11): `QpWalkAction::kUnset` on the base walk
(3/4680 + 1/283 passes, pre-existing, not the arm's); the `unrefined_out` seam column, worth keeping;
mode 1 (proximal rhs, unregularized refinement) deserves the full battery once the termination test is
repaired; the two NaN cells need a pass-0 state dump to separate the factor failure from the `1/delta`
amplification. Next: E4 (`.superpowers/m7-w2-e4-brief.md`) — the proximal pair with the walk's
termination and runaway tests moved into the right currency, plus the EQP-site residual check — opened
with priority over NC3 (the saddle exit) and the landing round, because E4's cells are the SNOPT-set
cells (STEENBR, YORKNET).
