# DEVIATIONS from DECLARATION-QPC (frozen at ff371b35e; this file is the only place a deviation is recorded)

Written at launch (2026-11-26) by the freeze agent. Each item says what the declaration says, what was done, and whether it blocks.

1. **Runner glue (not declared legs).** The declaration names leg scripts `legs/legB.sh ... legQ.sh`, `n2_qpc.py`, `qpc_ens_extra.py`
   but no script that sequences them or runs the ensemble arms. `gate-QPC/chain/{env.sh,driverX.sh,driverY.sh,driverEA.sh,p_gates.py}`
   are that glue: they call the declared scripts and the harness's `i19_run.py` / `ens.py` with the declared arguments, in the declared
   freeze order, and add only sequencing, sha pre-flights, markers and commits. They contain no bar and compute no verdict.
2. **H sha in BINARIES.** `declare QPC` carries the CODE head f0ba4e101 (the declaration's own wording: "the code head of branch qpc at
   freeze"), not the branch head: `git diff f0ba4e101 ff371b35e -- src include tests bench` is empty. Arms `qpc-c1` 8923ae130 and `qpc-c2`
   82b8c3112 as ruled (C583).
3. **Harness checkout.** The QPC store lives in a NEW worktree `/home/ghecht/Projects/hven-bench-qpc` of hven-bench at `27c02687` (lanes tree
   `4319ab15...` verified), not the census checkout (declaration section 2). Own venv (numpy 2.5.5, scipy 1.18.1, the RST-B venv's versions).
4. **Store copy.** The whole census store (all corpora, PILOT-62, points/, certificates.csv, certs-done.json), not only the H0 member
   directories, was copied with `cp -a` and verified with `diff -rq` and a sha256 manifest (8873 files) before any use; `cells/H0-copy.sha256`
   is the manifest of the H0 member directories. The only later additions to the copy are NLP-46/tier-assignment.csv (from `i19_run.py tiers`,
   as the harness requires; identical to RST-B's derived assignment, 20 exclusive cells = the 10 declared solo problems) and arm rows.
5. **Baseline certificate leg.** The census already ran it (certs-done.json: 287 rows, 286 STATIONARY, 1 APPROXIMATELY STATIONARY, 0 false,
   0 not reproduced). `i19_run.py certs` with no arm names crashes on a store that has a `points/` directory (`ens_members.split_arm` on a point
   file name, `member_stores`); invoked with explicit H0 arm names it is a verified no-op on the copy. The same explicit-name form is used for
   every arm's certificate leg. Harness quirk recorded; no harness change.
6. **Member-0 development store.** Declaration section 4 says P-POWER reads member-0 development rows that are NOT reused as ensemble rows,
   and N2 reads the same rows. They are produced into a separate store `.scratch/qpc-m0dev-store` (a copy of the QPC store taken right after the
   E-R commit); the ensemble legs re-run member 0 afresh into the QPC store.
7. **Leg M1R moved after E-R.** The declared order puts M1R before E-R, but M1R produces TRAIN-238 member-0 rows of an H-equivalent build; the
   task's rule (no arm member-0 row before the E-R freeze file is committed) is stricter, so M1R runs after the E-R commit. Its result is reported only.
8. **P-GATED / SENS-Q P-POWER script.** The declaration freezes the definition (section 4) but names no script for `cells/sens_q_gated.tsv`;
   `gate-QPC/chain/p_gates.py` computes it from leg N0's CSVs using `cmp_sensq.vqp_failures` as the "data-half reading fails" test (that function
   includes the pinned-optimum check, the only data-half reading the declared code has).
9. **Settler rulings C584 on the three gaps (carried out; supersedes the first-draft "NOT RUN" list).**
   * **N3 (CENSUS-57), ruling C584(1).** Run as declared: H against the M7-close baseline `bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv`
     (CEN-R3) with the repo's `scripts/run_walk_census.sh` + `scripts/census_compare.py` (exact 13-column criterion, comparator's own, no residual gate),
     whole leg under `box-lane --exclusive` (deadline-status cells solo, the script's own T1/T2/T3 tiering from the frozen baseline). ALSO run for R
     (99712ec32) on the same baseline. The corpus runner is `hven_sqp_corpus` (the replay/census binary), built from the qpc tree for H and from the N1
     base tree (`.scratch/qpcrun/R-src` at 99712ec32) for R, both Release/SAFER_FAST as leg N1 builds them; the census LANE binary 7d2f9f37 (hven_bench_cell)
     is a different program and cannot run CENSUS-57, so "R" here is the R source's corpus binary (sha256 printed in each leg log). A mismatch that R shows
     too is PRE-EXISTING and listed on its own; only an H-only mismatch counts against N3: on a cell whose own side line (`HVEN_BENCH_QPC_SIDE`) has both
     counters 0 it FAILS the bar (it must still be re-run ALONE, CLAUDE.md section 7, before it is called one), with a non-zero counter it is a declared
     mover. CENSUS-57 P-POWER = cells with converged_refinement_arms > 0 / certificate_gate_refusals > 0 under H (`gate-QPC/chain/n3_report.py`);
     it is NOT in P-GATES.md (that file is committed before the legs that produce it): it is in `gate-QPC/leg-N3.log`. Legs N3H, N3R, N3 in driverY after N1.
   * **P (PILOT-62), ruling C584(2).** `hven-bench-qpc/scripts/qpc_pilots.py` is `rstb_pilots.py` (hven-bench rstb-harness 6ff0dd4) copied with only its
     docstring first line changed (a copy is committed as `gate-QPC/chain/qpc_pilots.py`): it registers the declared binary arms
     (`i19_cells.register_binary_arms`) and then runs `census_pilots.main` unchanged. PROOF: (a) `git -C hven-bench-qpc diff 27c02687 -- tu runners lanes schema
     sets scripts/census_pilots.py` is empty (0 lines; the only new file is scripts/qpc_pilots.py, untracked); (b) `i19_run.plan_pilot` for H0, H0@x1 and H0@t3
     (full plan JSON: cells, labels, argv) is byte-identical with and without the registration (24284 / 32846 / 30986 bytes). The pilot leg per arm (6 members:
     member 0 + x1..x5; at most 4 arms per box-lane acquisition; tag = the arm's own declare line, `--tag "declare <arm> "`), then each member's KILL rows alone,
     then the certificate leg (explicit arm names), all in driverEA before the readers.
   * **D (V-PDIV), ruling C584(3); reported, never a bar, no claim rests on it.** The declaration (section 5.2) names the three commits explicitly:
     `a70c79e34`, `1b3b5e06d`, `c3b4b8f1d` (the pdiv branch has two later QPD-diagnostic commits, 964ea80ec and 10f0b4699, which are NOT meant). The local branch
     `qpc-pdiv` (worktree `.scratch/qpcrun/pdiv-src`, never pushed) = H f0ba4e101 + those three, cherry-picked cleanly (no conflict): 18b6c1c48, 976f7def1, 5eb7eb24d.
     R's side = 99712ec32 + the same three = pdiv commit c3b4b8f1d; its existing install `hven-bench-pdiv/.hven/install-c3b4b8f1ded9` (pin verified) is reused
     read-only. Both lanes are built with this harness's `scripts/i19_build_lane.sh` (ens 27c02687, tags PDQ / PDR3) rather than the pdiv study's i19 harness 56fa03f,
     so the control compares like with like; the sample runs through the pdiv worktree's own `runcells.py` (tasks1.tsv + tasks2.tsv) and its `qpflip.py`, `harm.py`,
     `cmp.py`. The reproduction control (`chain/d_control.py`, the study's own verify.py rule: status, iterations, factorizations, eval_f, eval_jac, objective on every
     H0 task) compares each diagnostic build to its non-diagnostic counterpart's member-0 rows in the QPC store (R: census H0; QPC: the E-A member-0 rows); a failure
     makes the endpoint VOID. D runs LAST (launched by the last finisher of the bar legs; `driverD.sh`). The pdiv tools' hard-coded cwd is hven-bench-pdiv (data lookup);
     they are used unchanged. If any D step cannot run it is recorded NOT RUN with the reason, here and in `gate-QPC/D/`.
10. **CI.** Dispatched once at code f0ba4e101 (run 45765039669); see STATE.md.
