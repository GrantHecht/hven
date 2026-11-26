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
9. **NOT RUN, no script exists (needs a ruling; nothing was improvised):**
   * **N3** (CENSUS-57 H vs the baseline, CEN-R3): no leg script in `legs/`. `scripts/run_walk_census.sh` + `census_compare.py` exist, but the
     declaration's baseline `bench/baselines/2026-10-m7-close-corpus/` predates R (99712ec32); whether R itself still matches it is not stated.
   * **P** (PILOT-62 under each arm): the pinned harness's `i19_run.py pilot` and `scripts/census_pilots.py` plan the pilot through `plan_pilot`
     without registering declared binary arms (`STOP: no arm QPCT in the spec` reproduced with a dry stand-in), so they cannot run QPC / QPC-C1 / QPC-C2.
     RST-B's `rstb_pilots.py` (harness branch rstb-harness 6ff0dd4, not 27c02687) registers them. Running it means using a script from another harness commit.
   * **D** (V-PDIV): needs the local `qpc-pdiv` build and the pdiv worktree's tools; no script in `legs/`.
   * **CI**: triggered separately (see STATE.md).
   CENSUS-57 P-POWER depends on N3 and is therefore not computed.
