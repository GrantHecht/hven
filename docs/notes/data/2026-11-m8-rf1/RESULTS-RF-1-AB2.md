# RESULTS-RF-1-AB2 -- gate A+B2 read-out (DECLARATION-RF-1-AB2.md, -r1, -r2)

Arms: C = `c721dc32d`, AB2 = `7b80afca7` (code). Evidence in `gate-AB2/`. Counters only; no wall-clock claim.

## Comparator ruling (B2-1)

`TheReductionResumeTakesTheNoClaimPointAndCountsIt`: h_out 10.000015 (at `1bb03fe46`) -> 1.52447e-05 at AB2, the reduction
resume fires at k = 0.5 (1 resume). Mutations M14 (comparator reverted to the shared order) and M16 (start incumbent
dropped) are killed by it; M15 (phase-feasibility conjunct dropped) and R1 (non-strict) by `RestorationBestOrder.*`.

## Leg T -- TRAIN 238 x {D, K} (`gate-AB2/train/ab2_check.txt`)

476 rows, 0 missing. hven-ipm unchanged (224 optimal, 225 optimal+acceptable, factorizations and iterations identical).
hven-sqp: optimal 210 -> 208, optimal+acceptable 210 -> 208; factorizations 200718 -> 179402 (-21316); iterations
4480 -> 4078 (-402). Six status movers:

| cell | C -> AB2 | objective-quality read |
|---|---|---|
| sd-hven-sqp-CRESC100.D | optimal -> stalled | **LOST SOLVE**, worse: f 0.5676 -> 9.680, h 2.5e-9 -> 724. One restoration at major 1 (h_in 1.67e5), the sub-solve ends kWorkLimit at the PHASE BUDGET (5000 + F_entry) after 9 restoration majors with h_out 7.03e4 (0.42 h_in: no reduction resume) -> kStalled / kRestorationWorkLimit. At C the phase ran 34 majors and the solve converged (16943 factorizations). |
| sd-hven-sqp-CRESC100.K | optimal -> stalled | **LOST SOLVE**, worse: same mechanism (f 0.5676 -> 13.19, h 1.6e-11 -> 987) |
| sd-hven-sqp-CRESC50.D | max_iter -> infeasible | worse in kind: a CERTIFIED local infeasibility (restoration kOptimal at h 5.27, feas_out 0.557) on a problem C was approaching feasibly (h 2.9e-4); 3 restoration entries (C: 2) |
| sd-hven-sqp-HS99EXP.D, .K | max_iter -> stalled | neither solved; both infeasible points (h ~4e5); the phase budget ends the restoration (kRestorationWorkLimit) |
| sd-hven-sqp-MSS1.D | numerical_error -> stalled | neither solved; phase budget again (restoration h 2.58 -> 0.31, short of the 0.1 resume); h at the returned point 0.092 (C 1.3e-7) |

Open items for the settler: the two lost solves (the default phase budget binds on CRESC100), the CRESC50 certificate.

## Leg P -- the pilot's 62 failing cells (`gate-AB2/pilot-r1/report.txt`; the first run is void, `-r1`)

Solved (optimal or acceptable): C 12 -> AB2 19; no C-solved cell lost. Rescued: ACOPR118 x5 (004-008, kOptimal at
f = 129661 with the rungs and the elastic route), ACOPR57 013 (qn, adrm 1e-10: optimal) and 018 (acceptable), ACOPP118
001/002 acceptable -> optimal. SMBANK (10 cells): acceptable in both, now through the trust-region floor exit, 192-224
majors (C 154-285). BRITGAS (9): still unsolved; the phase budget now ends the 84k-factorization phases (027:
84250 -> 25373 factorizations), status max_iter/numerical_error -> stalled, at lower f. MSS2 (7): still failing
(numerical_error, 7 majors; 034 stalled). YORKNET (13): still unsolved; several stalled -> numerical_error or infeasible
(054: certified infeasible after 102 majors), 055-057 now stop at major 1 on the phase budget (f 119037 vs C 14.3e3).
DALLASM (5): unchanged.

## Leg M -- mutations (`gate-AB2/mutations.out`, `-r2` for M7)

Control clean. Killed: M1-M6, M7 (at -r2), M8-M12, M14-M18, R1. **SURVIVED: M13** (the sub-driver's `return_best`
reverted: no fixture tells the best-h point from the last one) and **M19** (the reduction-resume extension to kStalled /
the phase budget's kWorkLimit reverted: no fixture reaches it). M7's first run did not compile (instrument defect, -r2).

## Legs C, S, CI

ctest rel and dbg at `aa3460b78`: 4068/4068 each. Install smoke and export contract: rc 0 (default, clang). CI run
44991274789 at `aa3460b78`: linux release and coverage green; macOS and Windows red on three RF-1 pins that read
differently off the reference build (recorded, not guessed: HS15 warm 21/39 and held-off 39 on both, ProbeBudget arm A
686 factorizations on Accelerate); pinned in `82c37da99`, CI run 44995693469.

## Leg W -- walk census: NOT COMPLETED (UNOBSERVED)

Started 01:35 (AB2) and 01:51 (C), stopped by this agent at ~02:50 on the settler's note that m8 moved to `692727dc8`
(integ2: pf2d + pf5b) and the gate must run against rf1 rebased onto it. AB2 had finished tier 1 (42 rows), C 4 rows; no
comparison was made and no partial rows are evidence. The census is owed by the rebased gate.

## Reference used, and what the m8 move means for this read-out

Every leg above ran against the UN-rebased branch: AB2 = `7b80afca7` on pf2d `84a6e3373`, reference C = `c721dc32d` (its
committed gate-C rows for TRAIN; rebuilt pilot-C binary for P). m8 is now `692727dc8`, which adds pf5b's symbolic-analysis
reuse; rf1 is not yet rebased. These readings are therefore the pre-rebase gate's, to be re-derived on the rebased head
against a rebased C (successor declaration).
