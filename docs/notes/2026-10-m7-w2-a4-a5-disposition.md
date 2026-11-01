# M7 W2 A4 → A5 — the A4 verdict read, and A5's disposition

Status: **DRAFT for settler ruling.** Date: 2026-10-28. Base: m7 `073c50ce`.
Authority: `docs/notes/2026-09-m7-brief.md` §1 W2 A4/A5 (`:252–265`) and §2's scope
valve (`:331`: "if A4 shows static selection wins, A5 is dropped by amendment").
Docs only. Nothing was built or run; every claim is a reading of the committed record.

## 0. Sources

- **Harness** (`hven-bench` main, read at `b5423f6`; cited `hb:<path>:<line>` or by commit):
  the A4-0 rule `docs/w2-a4-decision-rule.md` + `scripts/w2_a4_decide.py`; the A4-0 reading
  `docs/w2-a4-sweep.md`; the A4-2 rule `docs/w2-a4-2-decision-rule.md` +
  `scripts/w2_a4_2_decide.py`; the A4-2 reading `docs/w2-a4-2-presets.md`; run dirs
  `runs/2026-09-m7-w2-a4-*`; the latest re-take `runs/2026-10-m7-w2-retake19/four-lane-table.md`.
- **hven**: `docs/notes/2026-09-m7-ledger.md` (`m7:NNN`); settler rulings in
  `.superpowers/m7-w2-c10-report.md` (`C`-numbers; read only).
- **Not found:** the switching-literature note the brief names as A4's reading input
  (`:253–254`). No file under `docs/notes/` or `hb:docs/` holds it. The brief's own scope
  reading calls it "papers only, not a milestone gate" (`:31–34`), so its absence does not
  block this disposition; it is flagged in §5.
- **No prior settler ruling on A4's verdict or on A5** in the C-report: A4 appears only as
  "A4 outcome -> A5 disposition" in the approved remaining plan (C113, `:456`) and as
  "A4 rule pre-registered" (C123, `:478`). The A4 *outcome* was acted on as the IPM default
  event D1 (owner ruling 2026-09-18, `m7:895`), not as an A5 ruling.

## 1. Pre-registration — confirmed by commit order

| round | rule + arithmetic | first rows | rule edits after rows |
|---|---|---|---|
| A4-0 (options-label sweep) | `cbd26ed` 2026-09-16 21:39 (doc + `w2_a4_decide.py`) | first leg 2026-09-16 21:47 (`hb:docs/w2-a4-decision-rule.md:205–207`); rows committed `e614884` 2026-09-17 19:08 | `10bbfc9`: append-only §6, the owner's roster override; §2 arithmetic untouched (`w2_a4_decide.py` has one commit) |
| A4-2 (five IPM presets) | `99d8fe8` 2026-09-17 22:25 (doc + `w2_a4_2_decide.py`) | labels `8406922` 22:25:40; rows `5a40d2c` 2026-09-18 06:34 | none (one commit each) |

**One declared caveat:** A4-0's §3 skip clause was overridden *after* ten labels had run, to
admit `ipm-acc-funnel` / `ipm-acc-filter` as coherent modes (owner ask 2026-09-17; label
commit `aab3fb7`; supersession record `hb:docs/w2-a4-decision-rule.md:176–247`). The
arithmetic did not move; the reading gives both rosters' answers (`hb:docs/w2-a4-sweep.md:15–34`).

## 2. The verdict, per family

**What the rule measures.** Each label against *its own lane's* default, "never against the
other engine" (`hb:docs/w2-a4-decision-rule.md:32–33`); no aggregation across families
(`:138`). **No switching arm and no cross-engine arm was pre-registered or run.**

| family / lane | A4-0, roster as pre-registered | A4-0, roster as owner-extended | A4-2 (presets) |
|---|---|---|---|
| `hs` / ipm | no label wins; default stands | `ipm-acc-filter` WIN (median 10 vs 11) | `filter_l1` WIN (10 vs 11, \|B*\| 22; tie with `soc_recovery_l1` broken on field count) |
| `cutest` / ipm | `ipm-bar-monitored` (118 vs 125.5, \|B\| 14) and `ipm-ls-l1` (130 vs 137, 16 vs 15 optimal) win; tie-break `ipm-bar-monitored` | `ipm-acc-filter` WIN (101.5 vs 125.5) | `filter_l1` WIN (16 vs 15 optimal, 101.5 vs 125.5, \|B*\| 14) |
| `hs` / sqp | default stands (`sqp-ssn`, `sqp-ipm` lose on cost) | same | not measured (IPM-only, `hb:docs/w2-a4-2-decision-rule.md:39–41`) |
| `cutest` / sqp | default stands; lane 2/23 at defaults | same | not measured |

Sources: `hb:docs/w2-a4-sweep.md:141–285`, `:307–357`; `hb:docs/w2-a4-2-presets.md:12–40`, `:104–200`;
`hb:runs/2026-09-m7-w2-a4-2-presets/decisions.txt`; `m7:883`, `m7:891`.

**Static selection won, by the rule as written, on every family it scored.** Under every
roster a fixed configuration either beats the OFF state or the default stands; and under the
extended roster and A4-2 one mode (filter acceptance + monitored governor) wins *both*
families, so the static choice is not even per-family. The settler adopted it as hven's IPM
default with `max_iters` 1000 (owner-approved, D1, `5bf6760c`; `m7:893–899`); the
H-REPIN re-take measured it at 17/23 CUTEst, 22/22 HS (`m7:903`). Known costs carried with it:
`STEENBRD` point quality under the nine-field preset (`hb:docs/w2-a4-2-presets.md:124–161`),
`soc_recovery_l1`'s 18/23 refused by one `NET3` divergence (`:163–177`), `restoration_mode =
l1_nested` registered to M8 (`m7:891`).

**What "wins" cannot mean here.** The rule could not have returned "switching wins": no
switching arm existed (A5 unbuilt) and engines were never compared. So the brief's
conditional (`:257–259`) is satisfied by the letter, not tested against an alternative. That
is a ruling for the settler (§5 R1), not a defect to repair after the fact.

## 3. What is superseded since the A4 rows (named, not re-derived)

All A4 rows are at pin `3904a477` (2026-09-17/18).

1. **The IPM half is realised, not stale.** D1 made the winner the default; at H-REPIN
   (`aeebdccc`) `default` ≡ `ipm-bar-monitored` ≡ `ipm-acc-filter`, and `filter_l1` ≡
   `ipm-rest-l1` (`m7:903`: "the next sweep REBUILDS its roster"). The A4-0/A4-2 tables
   are therefore the record of the event, not a current ranking. **The IPM lane has not been
   re-taken since `aeebdccc`** (every RETAKE14–19 measures `hven-sqp` labels only; the IPM row in
   `hb:runs/2026-10-m7-w2-retake19/four-lane-table.md` is carried `ipm @aeebdccc`), and the
   toolchain moved on 2026-10-27 (C133/C135). No IPM interior/driver source changed in
   `aeebdccc..073c50ce` (`git diff --name-only`), but a shared-code or MKL effect is unmeasured.
2. **The SQP half is wholly superseded.** A4-0 scored a lane that solved 2/23 CUTEst
   ("not a working configuration", `hb:docs/w2-a4-sweep.md:345–346`). Since: E2 (independent
   working set, default ON), NC4/R2, M1/M2 restoration, SCW auto column scaling, RST, the QN
   lane. RETAKE19 (`b5423f6`, pin `6a2a58c7`, MKL 2026.2.0): `hven-sqp` default **16/23**,
   `sqp-qn` 8/23. The `qp_mode` labels (`sqp-ssn`, `sqp-ipm`) were never re-taken at any later
   pin, so A4-0's "default stands on sqp" is a 2/23-era fact. A7 (C134) is the one later
   static-selection read on the SQP lane: `ws_algebra` stays `kFixed`.
3. **A cross-engine picture now exists that A4 never scored.** Read from the committed
   RETAKE19 table, **not** a verdict and not a re-take of A4 (two pins five weeks apart):

   | CUTEst TRAIN (23) | optimal | cells |
   |---|---:|---|
   | ipm @`aeebdccc` | 17 | — |
   | sqp @`6a2a58c7` | 16 | — |
   | ipm only | 3 | BRITGAS, READING8 (−1938.9; SNOPT −2290.5, IPOPT −2179.7), YORKNET |
   | sqp only | 2 | ACOPR57, NET3 |
   | neither | 4 | MSS2, MSS3, TAX213322, TAX53322 |
   | per-cell oracle (union) | 19 | SNOPT 15, IPOPT 14 on the same rows |

   HS: all lanes 22/22 (H-REPIN, `m7:903`), so there is no headroom on `hs`. Where both
   engines are `optimal` on STEENBRC/E/G the SQP lane reaches lower objectives (27504.9 vs
   28428.6 on STEENBRC). **The 19 is a hindsight oracle bound**, not a measured rule: no
   trigger chooses those cells, it prices neither solve's cost, and it is a cold re-solve,
   not A5's export → stage composition.

## 4. A5 — recommended disposition

**DROP A5 from M7 by amendment** (the brief's own valve, `:331`), and **register the
cross-engine question to M8 as a new measured item**, not as A5 carried.

Why drop rather than build:

- The pre-registered rule returned static winners on every scored family; one static mode
  wins both IPM families and is already the shipped default (D1). That is the outcome the
  valve names.
- The only evidence for switching is §3.3's two-cell oracle headroom on one 23-cell family,
  read across two pins, with the IPM lane unmeasured for five weeks. It would need its own
  pre-registered rule, with a switching arm against the best static arm, priced in
  factorizations of *both* solves, at one pin, before any layer is built. That is a
  measurement round plus a build, and C113's approved plan does not budget either before W4.
- A cold fallback (solve with A; on a non-`optimal` outcome, solve with B) is already
  composable by any caller with two solves. The warm form (export → stage, the M5 explicit
  composition) is also already user-reachable (roadmap design constraint,
  `docs/notes/2026-08-26-roadmap-m6-m8-draft.md:190–198`). Dropping A5 forecloses nothing.

Why not "carry A5 to M8" as written: A5's spec presumes A4 called for switching. It did not.
Carrying A5 would carry a build obligation the evidence does not support. Registering the
question keeps it open under its own rule.

**What "configuration guidance" is, if A5 is dropped:**

1. **Shipped:** the D1 default (filter + monitored, `max_iters` 1000). This is the strongest
   form guidance can take, and it is already measured against its OFF state (exit criterion 2).
2. **At W4 close:** a short engine/mode-selection section in the M7 close artifact (or the
   user-facing docs, settler's choice). It contains only row-backed TRAIN statements, each
   with its pin: which engine wins which CUTEst cells, that `hs` is indifferent, that the
   QN lane is a Hessian-free fallback (8/23), `restoration_mode = l1_nested` as the measured
   MSS2/ACOPR57 lever with its STEENBRD cost, `soc_recovery_l1`'s coverage and its NET3
   divergence. The IPM figures come from the W4 suite-v1 end-to-end run, not from
   `aeebdccc`. No HOLDOUT claim.
3. **M8:** the labelled configs absorb (2) as curated toggle settings
   (`roadmap:204–207`: "a labeled config is a curated setting of these same toggles,
   never a separate mechanism").

## 5. Rulings requested

- **R1 — reading of "static selection wins".** Accept that A4's rule, which scored
  within-lane modes against the OFF state and had no switching or cross-engine arm
  (`hb:docs/w2-a4-decision-rule.md:32–33`), satisfies the brief's conditional (`:257–259`).
  Recommended: accept, and say so in the amendment.
- **R2 — which A4 answer is of record.** The pre-registered roster's (hs default; cutest
  `ipm-bar-monitored`), the owner-extended roster's (`ipm-acc-filter` both), or A4-2's
  (`filter_l1` both). The A5 disposition is the same under all three. Recommended: A4-2
  plus the D1 ruling (`m7:891`), with A4-0's two answers cited as its antecedents.
- **R3 — the stale SQP half.** Re-take the `qp_mode` labels at the current pin, or not.
  Recommended: not for A5 (it changes which static SQP mode to name, not whether to build
  switching). Fold it into the W4 suite-v1 run if the guidance section is to name SQP QP kernels.
- **R4 — register A5-R1 to M8:** "cross-engine between-solve selection: CUTEst TRAIN oracle
  union 19 vs best single engine 17 (§3.3). Preconditions: both lanes at one pin, a
  pre-registered rule with a switching arm against the best static arm, cost = both solves'
  factorizations, objective agreement reported per cell."
- **R5 — the missing switching-literature note** (`:253–254`). Waive for M7 (not a gate,
  `:31–34`), and make it the first input of A5-R1. Or require it at W4.
- **R6 — guidance location** (§4 item 2): the M7 close artifact or a user-facing doc.

## 6. Proposed brief amendment text (settler places it)

> **Amendment (2026-10-28, A4 → A5).** A4's decision rule was pre-registered before its TRAIN
> rows (hven-bench `cbd26ed`, `99d8fe8`). Within each lane it found a fixed configuration that
> wins or ties on every family scored. On the interior-point lane one mode (filter acceptance
> with the monitored barrier governor) wins both families; it shipped as the IPM default in
> W2 D1. The rule had no switching arm, so "static selection wins" is read as "a static
> per-family choice wins against the OFF state by the pre-registered rule". **A5 is dropped
> from M7 under §2's scope valve.** The heuristics ship as configuration guidance: the D1
> default, a row-backed engine/mode-selection section at the W4 close, and the M8 labelled
> configs. The explicit export → stage composition (M5) is untouched and remains the
> user-driven switch. The cross-engine between-solve question is registered to M8 as A5-R1
> with its own pre-registered rule. Its only current evidence is an oracle union of 19 vs 17
> on CUTEst TRAIN, read across two pins, and that is not a measurement of switching.
