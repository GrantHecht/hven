# RST-B deviations and execution notes (separate from the frozen declaration; ruling C579)

The frozen declaration (`../DECLARATION-RST-B.md`, freeze commit 0bc786791, freeze files bed41d732) is not edited. This file records,
in one place, where execution differs from or fills a gap in its text.

1. **N0r binary.** The declaration names "the P binary before the rider (`5375bfbf5`+naming)"; that is the naming commit
   `70d67680f78de5f00a90f005b037194ae927d324` (declaration line 47). Built from a detached worktree at that commit with
   hven-bench `scripts/census_build.sh` (harness checkout `hven-bench-ens-reader`, lanes tree `4319ab15...`, the P recipe), install
   `.scratch/inst-N0r/install`, lane `.scratch/lane-N0r/lanes/hven/hven_bench_cell`. Declared for the run only in the scratch
   file `hven-bench-rstb/.scratch/rstb-run/BINARIES-n0r.txt` (BINARIES.txt copy plus one `declare N0r options=H0 ...` line);
   `gate-RST-B/BINARIES.txt` is not edited. Arm F+B1+B2 is P's defaults (no `set=`), so N0r runs member 0 with the H0 label
   `default`. **Lane sha256: RECORDED AT BUILD (see RESULTS / the file `.scratch/rstb-run/logs/chainN.out`, line "N0r lane sha").**
   If an option the label needs is absent at that commit the leg STOPS and is reported.
2. **Leg C script.** `legs/ctest.sh` is named by the declaration but does not exist. Leg C is run with the repository's
   `.scratch/build.sh` / `.scratch/ctest.sh` (Release then Debug, `ctest -j4`, one box lane per step, under setsid), at the
   branch head after the mutation leg has restored the tree (`.scratch/rstb-run/legC.sh`; counts in its `legC/ctest-*.log`).
3. **NLP-46 tier assignment.** The declaration says the census's assignment, verbatim: the 10 declared solo problems x 2 labels =
   20 exclusive cells. The harness's own `i19_run.py tiers` derives 12 exclusive cells from the H0 rows (the I19-2a precedent). The
   20-cell assignment was written by hand into the evidence store (`store-rstb/NLP-46/tier-assignment.csv`, ruling C579 item 3);
   the 12 derived cells are all inside it; the derived file is kept at `hven-bench-rstb/.scratch/tier-assignment.derived12.csv`.
4. **Stale text left as-is.** The declaration's title still ends "(DRAFT)" and `BINARIES.txt` line 1 still says "UNFROZEN draft";
   the FROZEN header paragraph governs.
5. **Harness additions (additive, row paths untouched).** hven-bench `rstb-harness` moved from 911d0eb to 6ff0dd4 (descendant;
   `git diff --quiet 27c0268 6ff0dd4 -- lanes/ runners/ schema/ tu/ens_members.py` holds): `tu/rstb_nleg.py` + test (the store-row
   N0-N5 comparator; `compare_replay.py` takes replay CSVs this round does not produce), and `scripts/rstb_pilots.py` (registers the
   declared binary arms before `census_pilots.py` plans PILOT-62; census_pilots alone only knows spec arms). Rows produced before
   this move carry harness.sha 911d0eb or bab378e; both read under the harness-sha rule.
6. **Selection columns not evaluable on some corpora.** PILOT-62 rows carry only status/outcome/factorizations/certified, so N1-N5's
   selections cannot be evaluated there (reported NOT-EVALUABLE; only N0 is read on PILOT rows, where a status-only move is
   listed as a relabel candidate because the refusal counter is not observable). TU2-48 rows lack `nonfinite_step_rollbacks`, so N1
   is not evaluable on TU-2; N2-N5 are. The ensemble corpora (TRAIN, CAP, NLP-46) evaluate every selection.
7. **Certificate leg.** `i19_run.py certs` covers the ensemble corpora and PILOT-62; it has no TU2-48 path. TU-2 false-certificate
   check = the `certified` column of the TU2 rows against status, read by the results step.
8. **Concurrency.** Measured legs run as two driver streams plus a mutation leg and other agents' lane jobs share the box lanes; every
   cell is one pinned physical core per process, MKL_NUM_THREADS=1; wall-clock is informational.
9. **Power outage 2026-11-26 ~10:04 EST (declared interruption, not a protocol change).** The box lost power and rebooted (boot 10:03:35);
   every process died. Recovered by a recovery agent at 10:12-10:20 under the rule: a complete co-run counter leg is kept; a partial
   leg re-runs WHOLE with its partial outputs moved (never deleted) to `hven-bench-rstb/.scratch/rstb-run/run1-outage/` (store partials under
   `run1-outage/store-rstb/`, claim dirs under `claims/`, logs and the old chain scripts/pid files under `logs/` and `chain/`); timing
   (exclusive) legs re-run whole if not complete. Frozen code, declaration and freeze files untouched.
   **A defect pre-dating the outage was found while taking the inventory.** The `rstb-harness` checkout had no `sets/cutest/sif/` (the CUTEst
   instances are fetched, git-ignored, never committed): every `sd` leg of the ensemble corpora crashed at 17:47-18:01 on 2026-11-25
   (`FileNotFoundError .../sets/cutest/sif/ACOPP30.SIF`, rc=3) for `tc-F+B1+B2-0`, `tc-F+B1+B2-1` and `n0r-tc`; the `hs` sub-legs of those steps
   had completed, and streams A, B and N stopped at STREAM_STOP. No ensemble arm row of TRAIN/CAP/NLP-46 had therefore been completed before
   the outage. Repair: `sets/cutest/sif/` copied from `hven-bench-ens` (same pin) and proven by `scripts/fetch_cutest_sif.py --verify`
   ("rule re-applied at ebbc673b87f5 -- 35 problems, matching the pin; 0 fetched, 35 already the pinned bytes"). No code or row-path change.
   **Leg-by-leg state at the outage:**
   | leg | state | disposition |
   |---|---|---|
   | build C', P (installs, lanes) | complete before the leg run | kept (lane shas H0 7d2f9f37, C' 054aac01, P afa3a040 re-verified by sha256) |
   | build N0r (70d67680f) | complete (MKN0R rc=0) | kept; lane sha re-verified `fcce15e129270316c42b2af75a37aab59fb7fd765cc67099d68d53f8be4acb35` (this is the value item 1 left to the build log; BINARIES-n0r.txt carries it) |
   | mutation leg M | complete, committed 9bd763141 (VERDICT MUTATIONS_AS_DECLARED) | kept |
   | leg C (ctest Release + Debug at 077798fe6) | complete (4279/4279 and 4279/4279, rc 0, `legC.out` LEGC_DONE) | kept |
   | co-run T+CAP, NLP-46: `tc-F+B1+B2-0`, `-1` | PARTIAL (hs sub-legs done, sd crashed, see above) | store dirs `TRAIN-238/F+B1+B2` and `F+B1+B2@x1` and both claims moved aside; re-run whole |
   | co-run T+CAP, NLP-46: the other 70 of the 72 steps (48 T+CAP, 24 NLP-46; two are the partials above) | not started | planned afresh |
   | N0r `n0r-tc` | PARTIAL (same crash) | `TRAIN-238/N0r` and its claim moved aside; whole N0r phase re-runs |
   | N0r later steps (nlp, pilot, tu2, exclusive, alone) | not started | planned afresh |
   | pilots P-0..P-6 (28 members: F+B1+B2 x6, F x6, F+B1 x6, F2+B1+B2 x6, F2'+B1+B2 member 0 and x1..x3; every row file verified: 62 data rows, 7 columns, trailing newline; per-step log "pilot rcs ... 0") | complete | KEPT (co-run counter legs; one pinned physical core per process) |
   | pilot P-7 (F2'+B1+B2@x4, @x5, F2, F2@x1) | PARTIAL (24-26 of 62 rows each) | the four `PILOT-62/<arm>` dirs and the claim moved aside; re-runs whole |
   | pilots P-8..P-10, C' pilots, TU-2 (all), H0 TU-2 | not started | planned afresh |
   | exclusive chain X (NLP-46 solo tier, alone-pilot, alone-tu2) | not started (an early 14:49 start was blocked on the box lock and left only a stub log and a stub leg.json, no rows) | planned afresh |
   | certificate chain Z | not started | planned afresh |
   **Relaunch (2026-11-26, original order and scripts' logic, new chain scripts in `rstb-run/`, originals preserved in `run1-outage/chain/`):**
   chainA (corun A, then pilot A, tu2 A), chainB (corun B, pilot B, tu2 B), chainX (waits chainA+chainB pids, then `excl X`), chainZ (waits chainX,
   then `certs Z`), chainN (lane sha check, then `n0r N` with BINARIES-n0r.txt). Same driver.py and claim rule, same box-lane acquisition per
   step (`--exclusive` for the exclusive phases, handled inside i19_run/rstb_pilots). Real pids (argv `bash chain<X>.sh`) are in
   `rstb-run/chain<X>.pid`: A 72506, B 72507, X 72580, Z 72605, N 72630. The relaunched stream log lines carry the new date; the pre-outage
   stream logs are in `run1-outage/logs/`. Co-run width and pinning are unchanged (one process per physical core of a lane, SMT idle,
   MKL_NUM_THREADS=1); wall-clock stays informational.
