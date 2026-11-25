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
