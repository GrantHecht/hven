# RESULTS-I19-2a ADDENDUM

Corrects the I19-2a record per settler ruling C539 (adoption of the independent review of the I19-2a results). **This addendum supersedes the
specific statements listed in section 0.** `DECLARATION-I19-2a.md` (frozen at `b6c5fe9cbe72ba4af0765dc6872ba2c464555bc4`) and `RESULTS-I19-2a.md` are
not edited; where they disagree with this file on a listed statement, this file governs. Nothing here changes the decision: **nothing ships**
(no arm passed stage 1; no candidate; no composite K).

Every number below was re-derived by the scripts in `addendum/` from the rows in the read-only stores
`hven-bench-i19/.scratch/i19-2a/store-{H,R,Hbase,alone}` (harness checkout branch `i19`, head `56fa03fdf40acb38275f0a46058ddbd2da925f0e`, its own
`i19_run` / `i19_rule` loaders; nothing in that checkout was modified). The one loader deviation is the one RESULTS Deviation 4 already discloses
(a row with no counters side row, i.e. the ten NLP-46 `dnf_wall` cells, is read as not-ended). Numbers were not copied from the review; where mine differ
from the review's, section 6 says so. Raw outputs: `addendum/*.out`.

## 0. Statements superseded

| statement (where) | superseded by |
|---|---|
| "the placebo ... moves 198 live rows, so the instrument's noise floor is as large as the largest arm effects" (RESULTS Verdict); "198" in C537 | 198 is TRAIN-238 only (of 238 cells) and 161 of the 198 are refinement-counter bookkeeping; outcome-level movement is 37 TRAIN cells. Section 1. |
| "C1 20 lost / C2 10 lost -> current defaults robust" (C537) | C1 and C2 are worse than the defaults; the defaults are not perturbation-robust. Section 4a. |
| Tail-guard lists "TAIL ..." as arm harms (RESULTS lost/tail lists, "Why each arm fails") | Most tail lines are on cells H0 does not solve (gained or both-failed). Section 2b. |
| Lost cells reported without gained cells (RESULTS) | Section 2a lists gained cells beside lost cells. |
| "P loses three H0-optimal cells" stated as a finding (RESULTS NLP-46 paragraph, C537) | Candidates until re-run alone; the alone re-run is reported in section 4b. |
| "Closest arms ... G3 ... and G6" (RESULTS, "The candidate and the decision"); M9 register "G3/G6" | Section 5: all seven arms are re-read against the ensemble rule, G5 first. |
| Declared objective differences "flagged when |f_c - f_b| > 1e-6 max(1, |f_b|)": no list in RESULTS | Section 3. |

## 1. Sensitivity restated (placebo P versus H0)

Definitions (harness code, not mine): a row **moves** when the harness `i19_rule.moved()` test fires (status, or any deterministic column outside the
new counters, residual-class floats only outside the near-ulp gate). A moved row is **outcome-level** when its status, majors, factorizations, eval_f or
eval_jac differs; otherwise it is **bookkeeping-only** (refinement / polish / backend-solve ledger counters and a few small side columns). The script
asserts that every outcome-level row is also a harness-moved row.

### 1a. By corpus

| corpus | cells | moved | bookkeeping-only | outcome-level | outcome-level share | status changes | majors changed, both optimal |
|---|---|---|---|---|---|---|---|
| TRAIN-238 | 238 | 198 | 161 | **37** | 15.5% | 2 | 8 of 210 |
| CAP-238-1 | 238 | 193 | 158 | **35** | 14.7% | 1 | 8 of 209 |
| CAP-238-01 | 238 | 120 | 106 | **14** | 5.9% | 3 | 4 of 209 |
| NLP-46 | 46 | 36 | 9 | **27** | 58.7% | 4 | 8 of 21 |
| PILOT-62 | 62 | 60 | not separable | 60 (see below) | 96.8% | 17 | n/a |

The "198 moved rows" is TRAIN-238 only, 83% of its cells (not of the 714 TRAIN+CAP cells; the headline never stated its denominator). Across the three
TRAIN/CAP corpora, 86 of 714 cells (12.0%) move at outcome level, on **26 of 119 problem names (21.8%)**.

Bookkeeping-only rows differ in `eqp_polish_steps` (151 of 161 on TRAIN), `backend_solves_polish` (130), `border_refine_steps` (75) and the matching backend
ledger columns. Ten of the 161 TRAIN bookkeeping-only rows (11 of 158 on CAP-1, 12 of 106 on CAP-01, 4 of 9 on NLP-46) also differ in a small
non-refinement side column (for example `restoration_h_out`, `qp_minor_iters`, `accepted_f_type`); none of them changes status, majors, factorizations or
evaluation counts. Over all 198 moved TRAIN rows the three refinement columns differ in 188 / 163 / 109 rows (`eqp_polish_steps` / `backend_solves_polish` /
`border_refine_steps`).

PILOT-62 rows record only problem, label, status, outcome, factorizations and certified, so bookkeeping and outcome movement cannot be split there. 60 of 62
cells differ in status or factorizations; 17 change status; the outcome class (optimal / error / dnf_budget) changes in 11 (optimal 20 -> 21, error 32 -> 25,
dnf_budget 10 -> 16). Its solved counts are single draws and carry no conclusion.

### 1b. Status changes, H0 to P

- TRAIN-238: CRESC50/sqp-qn max_iter to numerical_error; LAKES/default numerical_error to stalled (both failed cells).
- CAP-238-1: CRESC50/sqp-qn max_iter to numerical_error.
- CAP-238-01: CRESC50/sqp-qn numerical_error to optimal; DEMBO7/sqp-qn stalled to optimal; LAKES/default numerical_error to stalled.
- NLP-46: ACOPR118/default optimal to acceptable; ACOPR118/sqp-qn stalled to dnf_wall; BRITGAS/default optimal to max_iter; SAWPATH/sqp-qn optimal to numerical_error.

### 1c. Outcome-level movers by name

- TRAIN-238 (23): ACOPP30, BT7, BURKEHAN, CRESC100, CRESC4, CRESC50, DEMBO7, HS101, HS108, HS117, HS22, HS56, HS88, HS99EXP, LAKES, MESH, MISTAKE, MSS1, NET1, OPTPRLOC, SPANHYD, WATER, hs022.
- CAP-238-1 (22): ACOPP30, BT7, CRESC100, CRESC4, CRESC50, DEMBO7, HS101, HS108, HS22, HS56, HS74, HS88, HS99EXP, LAKES, MESH, MISTAKE, MSS1, NET1, OPTPRLOC, SPANHYD, WATER, hs022.
- CAP-238-01 (10): CRESC100, CRESC4, CRESC50, DECONVC, DEMBO7, LAKES, MESH, NET1, SPANHYD, hs010.
- NLP-46 (16): ACOPP118, ACOPR118, ACOPR57, BIGBANK, BRITGAS, DALLASM, MSS2, NET3, SAWPATH, SMBANK, STEENBRB, STEENBRC, STEENBRD, STEENBRE, STEENBRG, YORKNET.
- Union over TRAIN, CAP-1, CAP-01 (26): ACOPP30, BT7, BURKEHAN, CRESC100, CRESC4, CRESC50, DECONVC, DEMBO7, HS101, HS108, HS117, HS22, HS56, HS74, HS88, HS99EXP, LAKES, MESH, MISTAKE, MSS1, NET1, OPTPRLOC, SPANHYD, WATER, hs010, hs022.

Where the movement is real it is large: BT7/default 97 to 17 majors, a different local minimum (f 360.38 under H0, 306.50 under P); CRESC100/default 211 to 40
majors; NLP-46 ACOPP118/default 34,235 to 1,733 factorizations (C1: 6,392); BRITGAS/default optimal to max_iter; SAWPATH/sqp-qn optimal to
numerical_error; STEENBRD 107 to 47 majors; STEENBRE 82 to 117.

### 1d. H0-counter association (H0 rows; "other" = bookkeeping-only plus unmoved)

TRAIN-238 (37 outcome-level, 201 other):

| H0 event | outcome-level with event | share | other with event | share | P(outcome-level given event) | given no event |
|---|---|---|---|---|---|---|
| restoration entered | 24 | 65% | 14 | 7% | 24/38 | 13/200 |
| qp_numerical_exits > 0 | 14 | 38% | 4 | 2% | 14/18 | 23/220 |
| eqp_residual_refusals > 0 | 14 | 38% | 3 | 1% | 14/17 | 23/221 |
| suspect_escalations > 0 | 8 | 22% | 2 | 1% | 8/10 | 29/228 |
| reject_funnel_ceiling > 0 | 18 | 49% | 15 | 7% | 18/33 | 19/205 |

Median H0 majors: outcome-level 40, other 9. The same table for the other corpora (and TRAIN+CAP pooled: restoration 58 of 86 outcome-level cells versus 55 of
628 others; P(outcome-level | restoration) = 58/113 against 28/601 otherwise) is in `addendum/s1_sensitivity.out`. On NLP-46 every outcome-level mover with
any of these events is an event cell (restoration 19/19, qp_numerical_exits 18/18, eqp_residual_refusals 12/12, suspect_escalations 15/15), but 8 of the
27 outcome-level movers there have no restoration entry, so the association is strong, not exclusive.

**Corrected statement.** Under a last-digit perturbation of the initial radius, about 15% of TRAIN cells (12% of TRAIN+CAP cells, 22% of problem names)
and 59% of NLP-46 cells change outcome or work, concentrated where the solve passes through restoration, QP numerical exits, EQP residual refusals and
funnel-ceiling rejections. The remaining moved rows are inner-loop refinement bookkeeping and are not a defect.

## 2. Gained cells and tail-line reclassification

### 2a. Lost and gained cells per arm (stage-1 corpora including the SENS-P guard cells)

| arm | lost (strict) | lost count in RESULTS | gained (strict) | gained at acceptable level only |
|---|---|---|---|---|
| P | 0 | 0 | 2 | 0 |
| G1 | 2 | 2 | 0 | 0 |
| G2 | 1 | 1 | 2 | 2 |
| G3 | 0 | 0 | 0 | 1 |
| G4 | 2 | 2 | 0 | 2 |
| G5 | 3 | 3 | 2 | 1 |
| G6 | 0 | 0 | 2 | 0 |
| G7 | 2 | 2 | 0 | 0 |
| C1 | 20 | 20 | 3 | 0 |
| C2 | 10 | 10 | 3 | 0 |

The lost counts reproduce RESULTS exactly. The lost lists are in RESULTS and `addendum/s2_s3_s5_arms.out`; the gained cells are:

- P (placebo): CAP-238-01 CRESC50/sqp-qn (numerical_error to optimal), CAP-238-01 DEMBO7/sqp-qn (stalled to optimal). The placebo itself gains two cells, so a gained cell
  is not by itself evidence for a lever.
- G2: CAP-238-01 CRESC100/default (numerical_error, 2 majors, to optimal, f 18.54 to 0.5676), CAP-238-01 CRESC50/sqp-qn; acceptable-only: CRESC50/sqp-qn on TRAIN and CAP-1.
- G3: none strict; CAP-238-01 CRESC50/sqp-qn to acceptable.
- G4: none strict; CRESC50/sqp-qn to acceptable on TRAIN and CAP-1.
- G5: CAP-238-01 CRESC100/default (to optimal in 40 majors, f 0.5676), SENS-P F3-n50-p0.4/sqp-qn (max_iter to optimal); CAP-238-01 CRESC50/sqp-qn to acceptable.
- G6: CAP-238-01 DEMBO7/sqp-qn (stalled to optimal), SENS-P F3-n50-p0.4/sqp-qn (max_iter to optimal).
- C1: CAP-238-01 CRESC100/default; MSS1/sqp-qn on TRAIN and CAP-1 (numerical_error to optimal).
- C2: the same three cells as C1.
- G1, G7: none.

NLP-46 (informational; only P and C1 have rows): P loses ACOPR118/default (to acceptable), BRITGAS/default, SAWPATH/sqp-qn and gains none (strict solves 24 to 21);
C1 loses BRITGAS/default and gains none (24 to 23).

### 2b. Tail lines reclassified by the baseline status of the cell

Harness `tail_guard` over the same stage-1 cells, each line classed by H0: **baseline-solved** (H0 strict optimal, a real tail hit), **gained-cell** (H0 not
strict optimal, arm strict optimal), **both-failed** (neither strict optimal). Line counts reproduce RESULTS.

| arm | tail lines (cells) | baseline-solved (real) lines/cells | gained-cell | both-failed |
|---|---|---|---|---|
| P | 0 | 0 | 0 | 0 |
| G1 | 5 (3) | 5 / 3 | 0 | 0 |
| G2 | 2 (1) | 0 | 2 / 1 | 0 |
| G3 | 1 (1) | 1 / 1 | 0 | 0 |
| G4 | 0 | 0 | 0 | 0 |
| G5 | 2 (1) | 0 | 2 / 1 | 0 |
| G6 | 3 (2) | 0 | 0 | 3 / 2 |
| G7 | 3 (3) | 0 | 0 | 3 / 3 |
| C1 | 50 (15) | 47 / 14 | 3 / 1 | 0 |
| C2 | 38 (12) | 35 / 10 | 2 / 1 | 1 / 1 |

By arm:

- G1: real: DEMBO7/sqp-qn CAP-1 rejected_steps 1 to 28; HS91/default (TRAIN and CAP-1) rejected_steps 11 to 66 and reject_funnel_ceiling 0 to 54.
- G2: both lines are CAP-238-01 CRESC100/default, which H0 fails (numerical_error after 2 majors) and G2 solves in 46 majors: gained-cell, not a harm.
- G3: real: TRAIN HS102/sqp-qn rejected_steps 9 to 35 (EV 84 to 114 on the cost tally).
- G5: both lines are the same CRESC100/default gained cell (G5 optimal in 40 majors).
- G6: MESH/default CAP-01 (numerical_error to max_iter) and CRESC100/default CAP-01 (numerical_error under both): both-failed.
- G7: CRESC100/default CAP-01, HS99EXP/sqp-qn (max_iter under both) and LAKES/sqp-qn (stalled under both): both-failed.
- C1: real lines on 14 cells, the distinct problems being CRESC4, F-CAP, F-SOC, HS104, HS89, HS92, OPTPRLOC; one gained cell (CRESC100/default CAP-01). C2: real lines on 10
  cells (F-CAP, F-SOC, OPTPRLOC), one gained, one both-failed.

NLP-46 tail lines (informational): P has one, MSS2/sqp-qn majors 7 to 37, which is a both-failed cell (numerical_error under both). C1 has four on ACOPR118/default (real:
H0 and C1 both optimal; majors 22 to 192) and YORKNET/default majors 12 to 55 (both-failed).

So of the V4 failures in RESULTS, only **G1 (HS91/default, DEMBO7/sqp-qn) and G3 (HS102/sqp-qn) are real tail hits**; G2's, G5's, G6's and G7's tail lines are gained or
both-failed cells and were wrongly counted as harms. Two caveats that keep the verdict honest: the tail rule as frozen was applied as declared, and G2, G5 and G7
still fail V1 for other reasons (section 2a/5): G2 loses CRESC4/default on CAP-1, G5 loses CRESC100/default on TRAIN and CAP-1 and DECONVC/sqp-qn on CAP-01, G7 loses
CRESC100 and CRESC4. Those V1 losses are all on placebo-unstable problems (section 5), so they are not clean harms either, but "G2 fails (d) only because it solved a cell"
is not right for G2: G2 also fails (a).

## 3. Objective differences the declaration promised

Cells where both the arm and H0 are strict optimal and |f_arm - f_H0| > 1e-6 max(1, |f_H0|). The cell is a different solution; a cost win or loss on it is
not a like-for-like comparison. Arms with no such cell: G2, G3, G4.

| arm | cell | f_H0 | f_arm | direction |
|---|---|---|---|---|
| P | TRAIN-238 BT7/default | 360.3797672 | 306.5 | lower |
| P | CAP-238-1 BT7/default | 360.3797672 | 306.5 | lower |
| P | NLP-46 STEENBRC/default | 27504.93682 | 27510.74447 | higher |
| P | NLP-46 STEENBRD/default | 9074.194267 | 9030.081867 | lower |
| P | NLP-46 STEENBRE/default | 27464.97096 | 27459.16331 | lower |
| G1 | TRAIN-238 and CAP-238-1 HS81/default | 0.4388512199 | 0.05394984777 | lower |
| G5 | CAP-238-1 BT7/default | 360.3797672 | 306.4999999 | lower |
| G5 | TRAIN-238 DECONVC/default | 2.078853039e-11 | 3.881608105e-04 | higher |
| G6 | CAP-238-1 HS61/sqp-qn | -143.6461422 | -81.91909609 | higher |
| G7 | TRAIN-238 and CAP-238-1 HS81/default | 0.4388512199 | 0.05394984777 | lower |
| C1 | CAP-238-1 BT7/default | 360.3797672 | 306.5 | lower |
| C1 | TRAIN-238 and CAP-238-1 DECONVC/default | 2.078853039e-11 | 3.734328221e-03 | higher |
| C1 | NLP-46 STEENBRD/default | 9074.194267 | 9030.081867 | lower |
| C1 | NLP-46 STEENBRE/default | 27464.97096 | 27459.16331 | lower |
| C2 | TRAIN-238 and CAP-238-1 DECONVC/default | 2.078853039e-11 | 7.689781625e-03 | higher |
| C2 | TRAIN-238 and CAP-238-1 HS81/default | 0.4388512199 | 0.05394984758 | lower |

The union of problem names is BT7, DECONVC, HS61, HS81, STEENBRC, STEENBRD, STEENBRE. (The review named BT7 under P and BT7 and DECONVC under G5; the HS81 (G1, G7,
C2), HS61 (G6), STEENBR* (P, C1) and DECONVC (C1, C2) entries are additional.) "Lower" is a lower reported objective; each such pair is two optimal points that
differ, which the harness counted as a cost win or loss.

## 4. Wording corrections

### 4a. "Current defaults robust"

C1 (20 lost) and C2 (10 lost) are **worse than the defaults**. That is all the comparison shows. The perturbation placebo shows the defaults are **not
perturbation-robust**: P moves 12% of TRAIN+CAP cells and 59% of NLP-46 cells at outcome level (section 1) and, on NLP-46, three H0-optimal solves flip (4b). C1's
verdict survives on placebo-stable cells (HS90, HS91, HS104 lost on TRAIN; HS75, HS89, HS92 and others on CAP) and C2's mainly through the F-CAP and F-SOC fixtures and
HS75 (section 5 table).

### 4b. P's three NLP-46 losses are candidates until re-run alone

RESULTS Deviation 8 did not re-run P's NLP-46 cells alone, and C537 states the three losses as findings. Under CLAUDE.md section 7 a deviation observed in a
co-run tier is a candidate until reproduced alone. The cells and their original tiers: ACOPR118/default (co-run), BRITGAS/default (exclusive), SAWPATH/sqp-qn (exclusive).
Here each cell was re-run alone for **both H0 and P**, one process at a time, serially, each under `box-lane --exclusive --`, with the frozen H lane binary (sha
`617241550fc016e235e95a46667f3273aee66fba6f1af8841cb7af8f633bc72b`, checked equal to `BINARIES.txt` before the first run), the same labels, wall cap 300 s.

| cell | arm | store row (co-run or exclusive tier, as measured) | alone re-run | harness moved test, store vs alone |
|---|---|---|---|---|
| ACOPR118/default | H0 | optimal, 22 majors, 14,874 LA, 47 EV | identical | no difference |
| ACOPR118/default | P | acceptable, 40 majors, 21,957 LA, 77 EV | identical | no difference |
| BRITGAS/default | H0 | optimal, 50 majors, 74,805 LA, 133 EV | identical | no difference |
| BRITGAS/default | P | max_iter, 1 major, 28,400 LA, 304 EV | identical | no difference |
| SAWPATH/sqp-qn | H0 | optimal, 44 majors, 47,406 LA, 115 EV | identical | no difference |
| SAWPATH/sqp-qn | P | numerical_error, 17 majors, 24,108 LA, 55 EV | identical | no difference |

**All three losses are reproduced alone**, for H0 and for P: status, majors, factorizations, evaluations and objective are identical to the stored rows, and the harness moved test
(which also compares every side column, residual columns through the near-ulp gate) reports no difference on any of the six cells. They are therefore findings, not co-run candidates: P
alone turns three H0-optimal NLP-46 cells into acceptable, max_iter and numerical_error. (BRITGAS/default under P ends after one major, 28,400 factorizations, f 7.879 against H0's optimal f 0.) Raw rows: `addendum/alone-rerun/`, log `addendum/alone-rerun.log`, comparison `addendum/alone_rerun_compare.out`. Each run acquired `box-lane --exclusive` on its own
(2026-11-25 00:01 to 01:53 local), alone on the box under the serial rule, one process at a time.

### 4c. ACOPR118/sqp-qn (`dnf_wall`)

`addendum/a4_acopr118.out`: H0 is `stalled` (outcome error, 154 majors, 5,463 factorizations) at 178.6 s on the exclusive tier (178.3 s on the R lane). P is `dnf_wall`
at 300.1 s in **both** tiers (the co-run row is superseded by the exclusive row; both are in the store), and C1 is `dnf_wall` at 300.1 s / 300.0 s in both tiers too. NLP-46
`dnf_wall` rows (no status): H0 10, P 11, C1 12. The extra P row is ACOPR118/sqp-qn; the extra C1 rows are ACOPR118/sqp-qn and YORKNET/sqp-qn. This is a wall-dependent outcome on
a cell neither arm solves; it is not scored by any rule here and no decision depends on it. (RESULTS Deviation 4's list of ten wall-cap cells was H0's; P and C1 add the cells above.)

## 5. M9 shortlist correction: all seven arms, G5 first

The register entry "re-read I19-2a G3/G6 against an ensemble floor" is replaced by **"re-read all seven arms (G1-G7) against the ensemble rule, G5 first"** (C539 B3).

What can and cannot be said now. The ensemble rule (C539 B4: paired 6-member ensembles for baseline and arms, baseline-frozen STABLE / COST-UNSTABLE / STATUS-UNSTABLE classes,
a null from baseline member pairs, 5-of-6 seed votes, one vote per problem name, same-solution condition) **cannot be applied to the I19-2a store**: it holds one H0 draw, one
arm draw per arm, and a single placebo draw (P). No ensemble class, null floor or seed vote exists for any cell. The table below is therefore **exploratory**: it re-reads each arm's
single draw on the problems on which the single placebo P did not move (93 of 119 names; 26 are placebo-unstable). It is not a re-verdict and fixes no threshold.

### 5a. EXPLORATORY: wins-losses (10% integer cell rule, both strict optimal) on placebo-stable problems

| arm | corpus | stable cells | LA W-L | EV W-L | LA W-L excl. different-solution cells | EV W-L excl. different-solution cells |
|---|---|---|---|---|---|---|
| G1 | TRAIN-238 | 186 | 13-12 | 11-12 | 13-11 | 11-11 |
| G1 | CAP-238-1 | 186 | 13-14 | 9-10 | 13-13 | 9-9 |
| G1 | CAP-238-01 | 186 | 3-3 | 4-5 | 3-3 | 4-5 |
| G2 | TRAIN, CAP-1, CAP-01 | 186 each | 0-0 | 0-0 | 0-0 | 0-0 |
| G3 | TRAIN-238 | 186 | 2-0 | 1-1 | 2-0 | 1-1 |
| G3 | CAP-238-1 | 186 | 2-0 | 1-0 | 2-0 | 1-0 |
| G3 | CAP-238-01 | 186 | 0-0 | 0-0 | 0-0 | 0-0 |
| G4 | TRAIN-238 | 186 | 2-6 | 1-3 | 2-6 | 1-3 |
| G4 | CAP-238-1 | 186 | 1-6 | 0-3 | 1-6 | 0-3 |
| G4 | CAP-238-01 | 186 | 0-1 | 0-0 | 0-1 | 0-0 |
| **G5** | **TRAIN-238** | 186 | **5-1** | **9-1** | 5-1 | 9-1 |
| **G5** | **CAP-238-1** | 186 | **6-1** | **11-1** | 6-1 | 11-1 |
| **G5** | **CAP-238-01** | 186 | **4-1** | **9-1** | 4-1 | 9-1 |
| G6 (CAP live set) | CAP-238-1 | 186 | 2-2 | 2-0 | 2-1 | 2-0 |
| G6 (CAP live set) | CAP-238-01 | 186 | 5-2 | 6-0 | 5-2 | 6-0 |
| G7 | TRAIN-238 | 186 | 2-5 | 2-5 | 2-4 | 2-4 |
| G7 | CAP-238-1 | 186 | 2-5 | 3-3 | 2-4 | 3-2 |
| G7 | CAP-238-01 | 186 | 0-1 | 0-0 | 0-1 | 0-0 |
| C1 | TRAIN-238 | 186 | 20-11 | 21-10 | 20-11 | 21-10 |
| C2 | TRAIN-238 | 186 | 6-1 | 5-0 | 6-1 | 5-0 |

The three corpora share one problem set, so a single problem appears in each corpus (and twice per corpus, once per base label); the rows are not independent votes (the B4
rule scores one vote per problem name). The `hs025` zero-factorization-tie variant is identical to the full column here (`addendum/s2_s3_s5_arms.out`, which also carries C1 and C2 on
CAP-1 and CAP-01). The G6 TRAIN row is omitted: G6's live set is the CAP corpora.

### 5b. EXPLORATORY: lost, gained and real-tail cells by placebo stability of the problem

| arm | lost cells on stable problems | lost on unstable problems | gained (stable / unstable / SENS-P) | real tail lines (stable / unstable) |
|---|---|---|---|---|
| G1 | 0 | 2 (ACOPP30, DEMBO7) | 0 / 0 / 0 | HS91/default x4 lines on 2 cells; DEMBO7/sqp-qn x1 on unstable |
| G2 | 0 | 1 (CRESC4) | 0 / 2 / 0 | 0 |
| G3 | 0 | 0 | 0 / 0 / 0 | HS102/sqp-qn x1 (stable) |
| G4 | 0 | 2 (CRESC100 x2) | 0 / 0 / 0 | 0 |
| G5 | 0 | 3 (CRESC100 x2, DECONVC) | 0 / 1 / 1 | 0 |
| G6 | 0 | 0 | 0 / 1 / 1 | 0 |
| G7 | 0 | 2 (CRESC100, CRESC4) | 0 / 0 / 0 | 0 |

Every V1 loss of G1, G2, G4, G5 and G7 is on a placebo-unstable problem (CRESC100/CRESC4/CRESC50 family, DECONVC, ACOPP30, DEMBO7), and one problem name carries one vote under the B4
rule: G4 loses only CRESC100, G5 loses CRESC100 and DECONVC, G7 loses CRESC100 and CRESC4. Under P, CRESC100/default itself swings 211 to 40 majors on TRAIN.

Stable-problem cost losses of the closest-read arms (cells H0 and the arm both solve): G5: HS89/sqp-qn CAP-01 (LA 52 to 160, EV 91 to 168), HS68/sqp-qn on TRAIN and CAP-1 (LA 20 to 25, EV 42 to 51); G3:
HS102/sqp-qn TRAIN (EV 84 to 114); G6: HIMMELP6/sqp-qn and HS84/sqp-qn CAP-01 (LA 15 to 17, 27 to 30), HS61 on CAP-1 (LA 9 to 11, 10 to 12).

### 5c. Reading (exploratory, one placebo draw)

- **G5 (shrink-to-step) has the only consistent positive signal** on placebo-stable cells in all three corpora (LA 5-1 / 6-1 / 4-1, EV 9-1 / 11-1 / 9-1), no lost cell on a stable problem, no real tail hit; its V1
  losses are all on placebo-unstable problems (CRESC100, DECONVC), and its RESULTS tail lines are a gained cell. It also carries different-solution cells (DECONVC/default on TRAIN: f 2.1e-11 to 3.9e-4, a worse optimum for a
  minimiser; BT7/default on CAP-1 lower) that the same-solution condition would remove from the cost tally and score on their own.
- **G3 is near-inert** on stable cells (2-0 / 1-1) and its only real stable-cell effect is a harm (HS102/sqp-qn).
- **G6** (CAP live set) is positive on CAP-01 (5-2, 6-0) and flat on CAP-1 (2-2, 2-0), loses nothing, has no real tail hit, and lands HS61/sqp-qn on CAP-1 at a higher objective (-143.6 to -81.9).
- **G1** nets to nothing on stable cells (13-12 TRAIN) and has real tail hits on HS91/default (stable) and DEMBO7/sqp-qn (unstable); **G4** and **G7** are net negative on stable cells; **G2** moves nothing on stable cells.
- None of this is a ship or reject; the ensemble read decides, and RST-B and QPC come first (C539 I2).

## 6. Where this addendum differs from the review

1. **Bookkeeping count.** The review says 164 of P's 198 TRAIN movers have identical status, majors, factorizations, eval_f and eval_jac. Re-derived: **161**. The review's own outcome-level count (37) implies
   161 (198 - 37), and the 37 reproduces. Everything else in review B1's table (CAP-1 35, CAP-01 14, 8 of 210 / 8 of 209 / 4 of 209, 2 / 1 / 3 status changes, 26 of 119 names, the association
   percentages, 24/38 and 13/200, median majors 40 and 9, 188 / 163 / 109) reproduced exactly.
2. **Cell naming.** The review writes "NLP-46 ACOPR118/default: 34,235 to 1,733 factorizations"; those numbers belong to **ACOPP118**/default. ACOPR118/default is 14,874 to 21,957 (optimal to acceptable).
3. **Gained cells.** The review lists G2, G5 and G6 gains but not (a) the SENS-P gain F3-n50-p0.4/sqp-qn under G5 and G6, (b) the C1/C2 gains (CRESC100/default CAP-01, MSS1/sqp-qn x2), or (c) that **P itself gains two cells**
   (CAP-01 CRESC50/sqp-qn, DEMBO7/sqp-qn). (c) matters for how a gained cell is weighed.
4. **G2.** The review says G2 "fails (d) only because it solved a cell H0 fails". Re-derived: the tail lines are indeed that gained cell, but G2 also fails (a) (CRESC4/default CAP-1 lost, a placebo-unstable problem) and (b).
5. **Objective differences.** The review names BT7 (P) and DECONVC and BT7 (G5). The full list adds HS81 (G1, G7, C2), HS61 (G6), STEENBRC/D/E (P, C1) and DECONVC (C1, C2) (section 3).
6. **A4 (ACOPR118/sqp-qn).** The review states the cell hits the wall under P in both tiers; confirmed, and **C1 does the same** (so NLP-46 dnf_wall rows are H0 10, P 11, C1 12, not only P 11).
7. **Section 5 tables** (G5 TRAIN LA 5-1 / EV 9-1, CAP-1 EV 11-1; G3 2-0 / 1-1; G6 CAP-01 5-2 / 6-0; G1 TRAIN LA 13-12; HS89 / HS68 / HS102 cost cells; real tail hits G3 HS102 and G1 HS91) reproduced exactly.
8. **B4's pre-registrable rule is a proposal in the review and a binding rule in C539; this addendum does not apply it** (no ensemble exists at this pin) and says so in section 5.

## 7. Provenance

Scripts and raw output: `addendum/common.py` (loader), `s1_sensitivity.py` (section 1), `s2_s3_s5_arms.py` (sections 2, 3, 5), `nlp_cells.py`, `a4_acopr118.py` (4c), `alone_rerun.py` and `run_alone_rerun.sh` (4b); outputs `*.out`,
`alone-rerun.log`, `alone-rerun/` (the re-run rows). Toolchain, hardware and dates as in `RESULTS-I19-2a.md` (clang 22.1.8, Release, `HVEN_FP_MODE=SAFER_FAST`, sequential MKL, the shared 8-physical-core box);
the section 4b re-run was taken on 2026-11-25 under `box-lane --exclusive`. Analysis reads are counter/status/objective only; wall-clock is informational and is not asserted anywhere in this addendum.
