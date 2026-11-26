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
11. **Power outage 2026-11-26 ~10:04 EST (declared interruption, not a protocol change).** The box lost power and rebooted (boot 10:03:35); every
    process died, including driverX (pid 715440) and driverY (pid 773052). Recovered by a recovery agent at 10:12-10:20; the code (f0ba4e101), the frozen
    declaration (ff371b35e) and the harness were not touched. Rule applied: a complete leg is kept; a partial leg re-runs whole with its partial outputs
    moved (never deleted) to `.scratch/qpcrun/run1-outage/` (dev-store stubs under `dev-store/`, the stale `Y.forked` sentinel under `markers/`, copies of
    the pre-outage driver logs and pid files under `logs/` and `pids/`).
    **Leg-by-leg state at the outage** (no E-A row of any arm existed; no ensemble-store row of an arm existed):
    | leg | state | disposition |
    |---|---|---|
    | B (three installs, lanes, four grids) | complete 03:24 (`B.done`, "LEG B done") | kept; the three lane sha256 re-verified against BINARIES.txt (QPC 24033940.., QPC-C1 163fbd1a.., QPC-C2 a4de64d0..) |
    | BINARIES / cells commit | complete (bab732cfa, `BIN.done`) | kept |
    | E-R (census copy verified, certificates, tiers, freezes) | complete (9e78f4a37, `ER.done`) | kept; it ran before any arm row, and still does |
    | N0 x3 (SENS-Q neutrality) | complete: 67 cells, differences=0, committed 9c5ca8398 (`N0.done`); run dirs byte-identical to the committed copies | kept |
    | Q x3 (V-QP, V-HONEST) | complete: failures=0, exceptions=0, same commit | kept |
    | member-0 development runs x3 (DEV store, driverX) | NOT STARTED: each of the three arm processes had planned its `hs` leg and was queued on box-lane (log: only the "LEG ... 22 cells" line, no lane acquired); the only residue was three `leg.json` stubs in the DEV store | stubs (`qpc-m0dev-store/TRAIN-238/QPC`, `QPC-C1`, `QPC-C2`) moved aside; leg runs whole. `diff -rq` of the DEV store against the ensemble store is now empty, as at E-R |
    | N2, P-GATES | not started | planned afresh |
    | driverY: C (ctest Rel+Dbg) | NOT STARTED (queued on box-lane since 03:24, lane never acquired; `Y-C.log` empty; no `gate-QPC/C`, no `R-src`) | empty log moved aside; leg runs whole |
    | driverY: N1, N3H/N3R/N3, M, M1R, SmG, SmC | not started | planned afresh |
    | driverEA x3 (E-A) | not started (no `EA-*.forked`, no marker) | forked by driverX after P-GATES as designed |
    | driverD (V-PDIV) | not started (no `D.launch` sentinel) | launched last by the last finisher, as designed |
    **A defect shared with RST-B, found at the inventory and repaired before relaunch.** `hven-bench-qpc` had no `sets/cutest/sif/` (the CUTEst instances
    are fetched and git-ignored, never committed); every TRAIN-238 `sd` leg (member-0 development runs, E-A) would have crashed on
    `FileNotFoundError .../sets/cutest/sif/ACOPP30.SIF`, as the RST-B round's legs did on 2026-11-25. `sets/cutest/sif/` copied from `hven-bench-ens` (same pin)
    and proven by `scripts/fetch_cutest_sif.py --verify` ("rule re-applied at ebbc673b87f5 -- 35 problems, matching the pin; 0 fetched, 35 already the pinned
    bytes"). No code or row-path change. The leg B grids/installs/N0/Q legs did not need CUTEst.
    **Relaunch.** The stale `Y.forked` sentinel (which would have made driverX skip forking driverY) was moved to `run1-outage/markers/`; the markers
    `B.done BIN.done ER.done N0.done` were left. `driverX.sh` relaunched unchanged at 10:16 (setsid, log `.scratch/qpcrun/logs/driverX.log`); it forked
    driverY.sh itself at once and started the three member-0 development runs. Real pids (argv `bash .../driverX.sh` / `driverY.sh`): driverX 72984,
    driverY 72991 (`driverX.pid`, `driverY.pid`). QPC queues on box-lane behind the RST-B round's relaunched steps and other agents' lane jobs, as before.
12. **Two environment stops after the 2026-11-26 outage relaunch (11:44 and 13:07); diagnosed, environment repaired, frozen code and declaration untouched.**
    **Leg C passed.** `Y-C.log` (11:22:55 to 11:44:34, lane 1, -j4, head 65de1e2de): Release ctest 100% passed, 0 failed, 4288/4288; Debug ctest 100% passed, 0 failed,
    4288/4288. This equals the pre-freeze reading (4288/4288 both, DECLARATION section 7) exactly. `QpCertificateData` 27/27, `QpReleaseReadmission` 3/3,
    `EqpConvergedRefinement` 4/4 green in both builds. Skips/disabled are the known ones (FailByDesignControl, six YORKNET/ACOPP118 skips in Debug, two disabled
    EqpRefinementAb). Evidence: `gate-QPC/C/`, `gate-QPC/leg-C.log`.
    **Stop 1 (driverX, 11:44, member-0 development runs rc=3).** All three arms refused at `run_serial` ("the harness tree is DIRTY ... ?? scripts/qpc_pilots.py"). Cause:
    `hven-bench-qpc/scripts/qpc_pilots.py` is the ported P glue of item 9 (ruling C584(2)), created 01:01, untracked by design at the time (identical to the committed
    copy `gate-QPC/chain/qpc_pilots.py`; diff empty), and the harness refuses any row run from a dirty tree. The refusal fired before any cell and before any row: the only
    residue was three `corun-2e049a41.leg.json` plan stubs under `qpc-m0dev-store/TRAIN-238/<arm>/`, moved to `.scratch/qpcrun/run1-outage/stop-2026-11-26/dev-store-TRAIN-238/`
    (a partial leg re-runs whole). Fix: the glue is the harness's own script, so it was committed in `hven-bench-qpc` (explicit pathspec, no trailer):
    **harness head 27c02687 -> e806548** (also branch `qpc-harness-glue` there so the commit is not orphaned), the only change `scripts/qpc_pilots.py` (added). The declaration pins
    harness `27c02687`; this is a mechanical amendment of the pin, and it is within the harness's own rule: `ens.py check_harness_shas` (I-a) lets shas differ iff no
    ROW_PRODUCING_PATHS (`lanes/ runners/ schema/ tu/ens_members.py`) differs, and `git diff --name-only 27c02687 e806548 -- lanes/ runners/ schema/ tu/ens_members.py`
    is empty (0 paths). Arm rows from here on stamp `harness.sha e806548`; the census R rows and the N0/Q-era work stamp `27c02687`; the reader's merge accepts both.
    Legs already run stay valid: N0 x3 and Q x3 (9c5ca8398) are `legN0.sh`/`legQ.sh`, which call the grid binaries and `legs/` scripts only, never `scripts/qpc_pilots.py`
    and never `run_serial`/`i19_run` (nothing of the harness tree is read by them beyond what is unchanged); leg B built from `census_build.sh`/`lanes/`, unchanged.
    The E-R freeze files are ensemble-class files from census rows stamped 27c02687 and are unaffected.
    **Stop 2 (driverY, 13:07, leg N1 BASE_CONFIGURE_FAIL, base 99712ec32).** Real cause (`gate-QPC/N1-stop-configure/base-configure.log`): `CMake Error at CMakeLists.txt:215:
    Bundled eigen not found at dep/eigen/`. driverY creates the base tree `.scratch/qpcrun/R-src` with `git worktree add --detach` at 11:44, after leg C; a fresh worktree has
    EMPTY submodule directories (`dep/eigen`, `dep/fmt`), and N1 configures the base tree in place (the leg-B trees got theirs when they were built, 03:17). Not an outage
    artifact; the pre-reboot plan would have hit it too (R-src did not exist at 03:24 either). Fix (environment only): `dep/eigen` and `dep/fmt` of R-src populated from
    the qpc worktree's checked-out submodules (same pins: gitlinks `bc3b3987` eigen, `407c905e` fmt, equal in the tree of 99712ec32); their `.git` files re-pointed at the
    qpc worktree's module dirs, as the B trees do; `git -C R-src submodule status` clean, `git status` clean, no frozen file touched. The leg N1 failure commit
    (584695130) stands as the record of the stop. The run1 stop evidence (stop markers, logs, pids) is in `.scratch/qpcrun/run1-outage/stop-2026-11-26/`.
    **Relaunch.** Moved aside: `STOP.txt`, `Y-N1.failed`, `dev-m0-*.rc`, the stale `driverX.pid`/`driverY.pid`, and `Y.forked` (so driverX forks driverY itself, as in
    item 11); kept: `B.done BIN.done ER.done N0.done Y-C.done`. driverX/driverY relaunched unchanged; real pids are in the commit that follows (`driverX.pid`, `driverY.pid`).
