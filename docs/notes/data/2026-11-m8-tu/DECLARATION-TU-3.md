# DECLARATION-TU-3: the cluster-label read of the I19-2a store

Task: M8 TU-3 (brief `docs/notes/2026-11-m8-brief.md`, section "TU"). Settler ruling C527 (2026-11-24).
This file is the pre-registration of a declared READ. It is committed on branch `tu3` BEFORE any I19-2a
result was read: the author read only DECLARATION-I19-2a.md (arm set, corpora, R1 and V1 definitions),
DECLARATION-TU-1.md section 6, hven-bench `tu1:tu/clusters.py` and `tu/tu1_spec.json`, and the C527 ruling. No file
under `docs/notes/data/2026-11-m8-i19-2a/gate-I19-2a/` and nothing of the hven-bench i19 store was opened. It is
never edited afterwards. A correction is a successor `DECLARATION-TU-3-r2.md` naming what it changes and
committed before the rows it governs. Results go to a separate file (`RESULTS-TU-3.md`).

## 0. Why a read, not a run (C527)

The clusters K1-K5 were declared in DECLARATION-TU-1 section 6 (hven `tu1` 9edda3432, 2026-11-21) before any
TU-2 row. The pre-check on stored TU-2 rows found only mechanical passes on 1-3 overlapping STEENBR/hs011
cells at the broken pin fd1e318b5, on arms TU-2's ruling declined; IPM B1 was blocked. C527: no label from the
TU-2 rows. TU-3 reads the I19-2a store (repaired pin, extended scoring, both modes). No solver is run by TU-3.

## 1. Baseline

The SQP default after the I19-2a decision. Both cases are named now:

* **Case H0 (nothing ships from I19-2a):** the baseline is H0, the unchanged defaults (kRatio, SOC on, every
  lever off), exactly I19-2a's scoring baseline.
* **Case X (a candidate ships from I19-2a, default flip):** the baseline is that shipped arm X's own rows on the
  same cells. A candidate is evaluated against X only on cells where both X and the candidate have stored rows;
  a cell missing either row is NOT SCORED and is listed. No row is derived, interpolated or substituted.

The case is read from the I19-2a decision record, not from a row. The case chosen and the baseline arm id are the
first lines of RESULTS-TU-3.md.

## 2. Candidate configurations

I19-2a's stage-1 arms, with their literal labels as stored: **G1-G7** (G1 `trust_region_growth=kRatioByStepType`,
G2 `trust_region_growth_after_soc=false`, G3 `trust_region_rejection_memory=3`, G4 `trust_region_growth_ratio_max=3`,
G5 `trust_region_shrink_to_step=true`, G6 `trust_region_growth_on_step_limit=false`, G7
`trust_region_growth_funnel_guard=true`), **C1** (= TU-2 A4: SOC off + `kAnyAccepted`), **C2** (= TU-2 A1: SOC
off), and **K** (the composite as I19-2a defined it from the passing G-arms). The placebo P is not a candidate;
it is read as a control (section 6). K-i leave-one-outs are not candidates (I19-2a R3). In Case X the baseline
arm is removed from the candidate list. An arm that did not run a cluster's cells (for example a G6 arm run only
on CAP corpora) contributes no rows to that cluster and is reported as not covered. No other arm is added.

## 3. Clusters

TU-1 section 6, unchanged, members verbatim (TRAIN members):

| cluster | predicate | members |
|---|---|---|
| K1 network | C = N | BIGBANK DALLASM SMBANK STEENBRB STEENBRC STEENBRD STEENBRE STEENBRG |
| K2 quadratic objective, general constraints | O = Q and C = O | ACOPP118 ACOPP57 ACOPR118 ACOPR57 SAWPATH |
| K3 constraint-dominant | m > n | ACOPP118 ACOPP57 ACOPR118 ACOPR57 NET3 SAWPATH TAX213322 TAX53322 |
| K4 large | n >= 500 | BIGBANK MSS2 MSS3 READING8 SAWPATH STEENBRC STEENBRE STEENBRG TAX213322 |
| K5 sum-of-squares objective | O = S | YORKNET hs011 hs014 hs025 hs027 hs028 |

**Corpus cells.** A cell belongs to a cluster iff its problem name is a member. Both base labels (`default`,
kExact; `sqp-qn`, kLbfgsDamped) are taken, so a member contributes two cells per corpus it is in. The corpora are:

* **NLP-46** (cutest-nlp-35's 23 TRAIN problems x 2 labels): every CUTEst member of K1-K5, 2 cells each. This is
  the SQP-CUTEst family of TU-1 section 2 (46 cells).
* **TRAIN-238** (22 hs + 97 sd problems x 2 labels): the HS members (K5: hs011 hs014 hs025 hs027 hs028), 2 cells
  each, from the hs-x and hs-q families (the SQP-HS family, 44 cells). A CUTEst member also present in the sd roster
  would be a cell of the same cluster; the membership test is by name, the roster is not re-cut, and any such
  cell is listed in the results with its corpus.

Cluster cell counts at most: K1 16 (NLP-46), K2 10, K3 16, K4 18, K5 12 (2 NLP-46 + 10 TRAIN-238 hs). The
realized counts are reported. The CAP corpora, SENS-P and PILOT-62 are not cluster cells and are not scored for a
label; they enter only as the guard families of section 4 (rule item 3).

## 4. The rule

All of it applies; it is fixed here and is applied mechanically by the TU-3 reader (section 7).

1. **Cell rule (TU-1 section 4 item 1, verbatim).** The candidate WINS a cell if it reaches and the baseline does
   not, or both reach and the candidate's primary counter is at least 10% lower (`10 * lower <= 9 * higher`,
   integers); LOSES symmetrically; otherwise TIE. "Reaches" = `outcome` optimal (acceptable bucketed with optimal
   as in TU-1; the I19-2a strict-optimal reading is reported alongside, not decisive). A documented alternate local
   optimum (hs025, hs033) counts as reaching. Primary counter: `factorizations` (SQP). Wall-clock is never read.
2. **Label rule (TU-1 section 6).** A label for cluster K ships for candidate X only if K's cells (both corpora,
   both base labels) as ONE family have sign(wins - losses) = +1 against the baseline, AND X has sign not -1 on the
   SQP-HS family (TRAIN-238 hs cells, 44) and not -1 on the SQP-CUTEst family (NLP-46, 46), each pooled across both
   base labels. Families are read on the whole family, not the cluster remainder.
3. **I19-2a guards on the cluster cells.** (a) **R1 tail guard (V4):** on every cluster cell both arms reach, X
   must not have majors, `eval_f + eval_jac`, `rejected_steps` or `reject_funnel_ceiling` >= max(H0 + 20, 3 H0)
   (each column separately, per cell; in Case X "H0" is read as the baseline arm's value for the same cell, and the
   H0 value is also reported). Confounded cells (BRITGAS, DALLASM, YORKNET) are not exempt. One breach vetoes the
   label for that cluster and that candidate. (b) **V1:** on every cluster cell, X's strict-optimal count >= the
   baseline's AND its optimal+acceptable count >= the baseline's, per cluster per base label; every lost cell is
   listed. V1 for a label is evaluated on the cluster's cells only (I19-2a's family-level V1 is a different object
   and is not re-scored). A V1 loss is a veto.
4. **Evidence floor.** At least **3 net cluster wins** (wins minus losses over the cluster's cells, both
   currencies of item 1 pooled as cells), and the net wins must come from at least **2 distinct problem names**
   (not all from one problem). A base-label pair of one problem counts as one name.
5. **Placebo control.** P (the radius perturbed in the last digits) is run through items 1-4 on each cluster. A
   cluster where P itself earns a +1 sign and 3 net wins is declared noise-limited and ships no label.
6. **Both modes.** The rule is the same for each candidate; a candidate must pass in the pooled label view. The
   per-base-label split is reported but the sign in item 2 is pooled over both base labels, as TU-1 section 2 pools.

A label is a curated setting of existing fields (the candidate's keys); it is a label for a cluster, not a
default change.

## 5. Overlap handling

A label is per cluster. A problem in several clusters (BIGBANK, SAWPATH, ACOP*, TAX* and STEENBR* appear in more
than one) counts in each cluster it is a member of; the wins are scored independently per cluster. The K1/K4
overlap (BIGBANK STEENBRC STEENBRE STEENBRG) is reported explicitly: its cells, and the K1-only and K4-only remainders'
signs. If K1 and K4 ship labels for different candidates, the shared members' disagreement is reported and does not
by itself void either label. A cluster's net wins are never double-counted across a candidate's two clusters in
the floor of item 4 (each cluster is read on its own cells).

## 6. Scope limits and outcome

* **No IPM label.** I19-2a has no IPM candidate and the IPM B1 lane is blocked at the TU-2 pin. No IPM row is read.
* **If nothing separates:** no label ships, and the M7 close section 3 guidance stays. That is a result and is
  recorded as such. Nothing is re-cut: clusters, arms and the rule are not changed after rows are read.
* **A label that passes** is a candidate to the settler, not a merge: it needs its own census read and ordinary
  gates, and any default-affecting reading is a declared event. Sub-rule: if the 57-cell walk census veto of TU-1
  section 4 item 5 has not been read for the arm, no label for that arm moves (TU-1 item 5, unchanged).
* The informational columns (I19-2a's own S1-S3 currencies, floor hits, restoration entries) are reported per
  cluster and read by no decision here.

## 7. Computation

A reader in hven-bench, `tu/tu3_read.py`, with `tests/test_tu3.py` (synthetic rows), on a branch `tu3` off `i19`.
DESCRIBED here; NOT RUN or written before this declaration is committed. Spec:

* **Inputs:** the I19-2a store's row files, read-only, by corpus (TRAIN-238, NLP-46) and arm label (H0, P, G1-G7,
  C1, C2, K, and the shipped arm in Case X); the cluster membership from `tu/clusters.py` `members(spec)`
  (`--verify` must pass before any row is read); `clusters.py` and the declared member lists of section 3 must agree.
* **Selection:** rows keyed (set, problem, label); a cluster cell = (name in cluster, corpus in {TRAIN-238 hs rows,
  NLP-46}, base label in {default, sqp-qn}). Coverage check first: every cluster cell has an H0 row (or a baseline
  row in Case X); a missing baseline row is a stop (section 8); a missing candidate row is NOT SCORED and listed.
* **Rule:** `tu/rule.py` cell rule (the TU-1 implementation, unmodified) on `outcome` and `factorizations`;
  family sign on the cluster; HS and CUTEst family signs over the full family; R1 and V1 per section 4 item 3
  using the integer thresholds `max(H0 + 20, 3 * H0)`; evidence floor of item 4 (net wins and distinct names).
* **Outputs:** `RESULTS-TU-3.md` and `tu3_cells.csv` (one row per candidate x cluster x cell: counters, outcome,
  win/tie/loss, R1 columns) with a provenance stamp (harness sha, store sha256s, date; the stamp names the store's
  pin and its binaries.txt; no wall-clock quoted); per candidate x cluster: wins, losses, net, distinct names, family
  signs, R1 breaches, V1 losses, K1/K4 overlap block, P control, decision. A per-cluster verdict line: LABEL
  (candidate, keys) or NONE.
* **Determinism:** pure function of the store; byte-identical on re-run. No cell is re-measured by TU-3. A cell
  needing a serial re-run (V1 lost cell) is listed for the settler; it is not run here.

## 8. Stops

TU-3 stops (a successor declaration is the only exit) on: a baseline row missing for a cluster cell, a store whose
binaries or pin disagree with the I19-2a declaration's, `clusters.py --verify` failing or disagreeing with section 3,
any attempt to read HOLDOUT, a spec/declaration disagreement between `tu/tu3_read.py` and this file, a candidate arm
whose label does not match the I19-2a declaration's literal label, or a re-run of the reader that is not
byte-identical.

## 9. Provenance

Ruling C527, DECLARATION-TU-1 section 6 (hven `tu1` 9edda3432, cherry-picked unchanged onto this branch),
DECLARATION-I19-2a (frozen on origin/i19-2a). Committed before any I19-2a result was read.
