# DECLARATION-QPC-r2 -- successor to DECLARATION-QPC, answering the 2026-11-27 STOPs

> Successor to DECLARATION-QPC (frozen at ff371b35e), committed before any row it governs, answering the two STOPs of
> 2026-11-27 (leg M1R rc=2 at 14:45; N2 QPC at 16:04) as diagnosed and accepted by settler ruling C617. **Everything in
> DECLARATION-QPC stands except the items below.** NO criterion, bar, ship rule (section 5), corpus, cell list, binary,
> arm, freeze file or mover rule changes. Criteria are never changed after data.

## What the two STOPs were

* **STOP 1 (leg M1R).** `legs/legM1R.sh` carries a literal harness-sha pin (`27c02687`) that the harness-head amendment of
  `gate-QPC/DEVIATIONS.md` item 12 (harness 27c02687 -> e806548) did not reach. M1R refused at its first check and did no work.
* **STOP 2 (N2).** `hven-bench-qpc` never had `.scratch/h3/decoded/` (the decoded CUTEst instances are fetched/decoded,
  git-ignored, never committed). All 628 CUTEst cells of each arm's member-0 development run were `error` rows (exception
  `cutest: cannot enter .scratch/h3/decoded/<P>`) with no side counters; the 132 hs cells per arm are bit-identical to H0.
  `i19_run` exits 0 even at 100% exceptions, so the driver did not stop until N2 compared the rows. N2's verdict (a difference
  outside the declared-mover rule) was correct; the run-1 N2 files stay as the stop record.

## r2-1 Harness pin

The harness is hven-bench branch `qpc-harness-glue` at `e80654847013a53f5d5676b03a495811e2cc2907` = ens `27c02687` plus
`scripts/qpc_pilots.py` (the ported pilot wrapper, settler C584). Proof: `git diff --name-only 27c02687 e806548 -- lanes/
runners/ schema/ tu/` is empty (the whole diff is the one added file `scripts/qpc_pilots.py`), and `qpc_pilots.py` is
referenced by nothing in the tree. DECLARATION-QPC's lane, lanes-tree and census statements are unchanged; leg B and the
census H0 rows (stamped 27c02687) stand.

## r2-2 Decoded instances

`hven-bench-ens/.scratch/h3/decoded` (read-only source) is copied byte-for-byte (`cp -a`) to
`hven-bench-qpc/.scratch/h3/decoded`. The sha256 manifests (every file, sorted) of both trees compare equal (800 files);
the manifest is committed as `cells/decoded-copy.sha256`. The holdout guard (`schema/holdout_guard.py`) returns rc 0 over every
CUTEst problem name of the corpora and pilots (`cells/decoded-required.txt`, 120 names, derived from the H0 rows of the
member-0 store). Nothing is re-decoded; nothing enters any repository except the manifest.

## r2-3 Mechanical bars (new STOP conditions, added because a 100%-exception run exited 0)

(a) Before any harness-driven leg, every CUTEst problem of TRAIN-238, CAP-238-1, CAP-238-01, NLP-46 and PILOT-62 (the list
`cells/decoded-required.txt`) has `decoded/<P>/libpb.so`, or STOP.
(b) After each member-0 development run and after each E-A leg, the arm's rows in the store hold zero `solver_exceptions.csv`
data rows (every such file under the arm's directories), or STOP.
(c) Leg M1R's TRAIN-238 part produces 238 cell results, rc 0 on all but at most the 4 pre-existing hs025/hs033
`--assert-reference` cells, or STOP.

## r2-4 Leg script

`legs/legM1R-r2.sh` = `legs/legM1R.sh` with the harness check at `e806548` and r2-3(a), (c). `legs/legM1R.sh` stays frozen.
`legs/n2_qpc.py` is unchanged (its verdict was correct; the improvement "report exceptions before counter differences" is
recorded, not adopted).

## r2-5 Run-1 arm directories

The run-1 arm directories in the member-0 development store (TRAIN-238, CAP-238-1, CAP-238-01, NLP-46 x QPC, QPC-C1, QPC-C2)
are MOVED, never deleted, to `.scratch/qpcrun/run1-nodecode/`; `diff -rq $DEV $STORE` is empty again, as before the run. The run-1
N2 evidence stays as the stop record; the run-1 hs cells are reported as a reproducibility reference only.

## r2-6 Rerun, whole

M1R (r2); the member-0 development runs x3; N2 x3; then, as planned: P-GATES; E-A x3 (including PILOT-62 and the certificate
leg; the E-A arm legs run from the harness checkout that now has `decoded/`, and r2-3(b) applies there); the readers; Sm gcc and
clang; D; the CI reading.

## r2-7 Carried over unchanged

Leg B and `BINARIES.txt`; E-R (census copy, `cells/H0-copy.sha256`, baseline certificates, tiers, both FREEZE files); N0 x3 and
Q x3; leg C; and from run 1 N1, N3H/N3R/N3 and M (none reads the harness tree or CUTEst; their evidence is committed).

## r2-8 Order

Commit this file, the decoded manifest and the run-1 N1/N3/M evidence -> provision and verify decoded -> M1R-r2 in parallel with
the member-0 development runs x3 -> N2 x3 -> P-GATES -> E-A ... per DECLARATION-QPC section 6.
