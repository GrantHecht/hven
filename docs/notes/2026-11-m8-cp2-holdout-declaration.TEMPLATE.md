# CP-2 HOLDOUT DECLARATION -- TEMPLATE (UNFILLED; NOT APPROVED; NOT A DECLARATION)

This file is a template. It is not a declaration until every `<<UNFILLED: ...>>` field is replaced,
it is committed, and the owner has approved it (G-HOLDOUT, a per-declaration approval; brief Q3,
`docs/notes/2026-11-m8-brief.md:513-516`). It must be copied to
`docs/notes/2026-11-m8-cp2-holdout-declaration.md` and filled there; this template stays as is.
No HOLDOUT cell has been built, decoded, run or read for this template.

Authority: brief §2 CP-2 `:435-439`, §3 exit criterion 10 `:484`, §0 TRAIN/HOLDOUT `:25-29`,
CP-3 `:446` (CP-3 reuses these rows; no second HOLDOUT run); roadmap §M8 exit criteria
(`:291-297`). Comparison rule: `docs/notes/2026-11-m8-cp1-accuracy-rule.md` (`cp1-v2`).

## 1. Machine-checked header (the CP-3 generator and the HOLDOUT guard read these lines)

```
declaration.id:            cp2-holdout-read
declaration.status:        <<UNFILLED: DRAFT | COMMITTED>>
declaration.commit:        <<UNFILLED: the hven commit that contains this filled file, set when committed>>
approval.status:           <<UNFILLED: APPROVED | REFUSED>>
approval.by:               <<UNFILLED: owner>>
approval.date_utc:         <<UNFILLED: YYYY-MM-DD>>
approval.scope:            this declaration only; not a pre-authorization (brief Q3)
accuracy_rule:             cp1-v2
run.count_authorized:      1
```

A file with any `<<UNFILLED` left, or `approval.status` other than `APPROVED`, is refused by every
script that could read a HOLDOUT row.

## 2. What is read, and at which pin

| field | value |
|---|---|
| hven pin (the TU-4 freeze) | <<UNFILLED: sha, from the TU-4 declaration>> |
| TU-4 declaration | <<UNFILLED: path + sha256>> |
| harness commit and pin file | <<UNFILLED: hven-bench sha; cmake/hven_pin.cmake sha256>> |
| toolchain / host / date | <<UNFILLED: stamp, CLAUDE.md §7>> |
| lanes | hven-ipm, hven-sqp, ipopt, snopt (all four, one campaign, section 7) |
| options labels | <<UNFILLED: the labelled configs as frozen by TU-3/TU-4, per lane; options-dump digests>> |
| budgets | <<UNFILLED: wall, iteration ceilings; must equal the TRAIN comparison's>> |
| MKL | `MKL_NUM_THREADS=1`; serial, one solve alone on the machine (§7) |
| sets | HS-27 v1 HOLDOUT (6 problems) and CUTEst-NLP-35 v1 HOLDOUT (12 problems), as listed by `sets/split-v1.json` at <<UNFILLED: sha256 of the split pin>> |
| cell list | <<UNFILLED: the 18 problems x (4 lanes + 2 pre-tuning arms) = 108 cells, by name, copied from the split pin, never by hand; 72 comparison cells + 36 pre-tuning-arm cells; see section 7 item 2>> |
| pre-tuning defaults (comparison arm) | the shipped defaults at the TU-1 declaration's pin: post-PF, post-AP-2, pre-TU-2 (brief `:435-439`): <<UNFILLED: sha and the arm's options digest>> |

## 3. One-time terms

- The HOLDOUT is read ONCE. A failed, killed or partial run is not retaken silently: it is reported,
  and any second read is a new declaration with a new owner approval (brief Q3).
- Nothing is re-tuned, re-labelled or re-defaulted on what this read shows. Either verdict is reported.
- The rows are committed as frozen evidence beside their provenance stamp; CP-3 reuses them and takes
  no second HOLDOUT run.
- The run takes the box lock and runs serially (CLAUDE.md §7). Owner of the run: <<UNFILLED>>.
- Deviations (a lane unavailable, a cell not runnable) are listed in the report as named gaps; an
  absent value is `UNOBSERVED`, never filled.

## 4. The pre-registered reading (from the brief; do not edit on filling)

A tuned default **generalizes** if, on HOLDOUT, BOTH hold:

1. it does not lose a cell that the pre-tuning defaults (section 2, last row) solve; and
2. it does not worsen the HOLDOUT shifted geometric mean of the primary counter by more than 10 %.

Otherwise it does not generalize. Either way the result is reported and nothing is re-tuned on it.

Fixed by this declaration before any row exists (fill, do not change after approval):

| parameter | value |
|---|---|
| primary counter | <<UNFILLED: e.g. iterations or factorizations, per the TU-1 declaration>> |
| shift | <<UNFILLED: declared shift of the primary counter; for the CP-3 wall currency the shift is 0.1 s (1 s sensitivity), fixed by `cp1-v2` section 2.6>> |
| DNF penalty | <<UNFILLED: ONE absolute value per metric, declared here; not the largest budget among lanes or configs (section 7 item 3)>> |
| "solves a cell" | `outcome == optimal` under each arm's own natural tolerance (`cp1-v2` reading A); the referee reading B is printed beside it and a standing is MET only under both |
| comparison | tuned arm vs pre-tuning arm, same lane, same cells; ratio = tuned SGM / pre-tuning SGM; worsens if ratio > 1.10 |
| lanes the reading applies to | <<UNFILLED: the hven lane(s) whose default was tuned>> |
| per-default or whole-config? | <<UNFILLED: reading per tuned default, or on the adopted config as one>> |

## 4b. Memo items (decision memo 2026-11-24, Q6; owner ruling C528)

G-HOLDOUT is signed only with all of these filled. They are fixed before any HOLDOUT row exists.

1. **Prior rulings, cited by version.** The comparison rule id (`cp1-v2`), the speed band (K = 2.0 against
   IPOPT, coverage >= IPOPT, full set and commonly solved subset), the classes (the TU-1 family table), the
   currency (serial solve-call wall time, evaluations as companion), the shift (0.1 s, 1 s sensitivity), and
   the penalty: <<UNFILLED: each cited as rule id + the sha of the file that holds it>>.
2. **Cell count, including the pre-tuning arms.** Eighteen problems x four comparison lanes = 72 cells, plus
   the pre-tuning arm on BOTH hven lanes (18 x 2 = 36 cells): **108 cells** in all. The pre-registered
   reading of section 4 needs the pre-tuning arm; it is part of the read and is counted in it.
3. **One uniform declared penalty.** Every failed cell in every lane and every summary is charged one declared
   absolute value (the wall currency: 600 s, `cp1-v2` section 2.6; for the counter reading of section 4: the
   value in the table above). It is never "the maximum budget over configs": that rule lets the lane with the
   largest budget decide the order.
4. **One campaign.** "One run" means ONE CAMPAIGN: all 108 cells, under one pin, one harness commit, one wall
   ceiling and one box, started and completed as a single declared unit. The campaign is: <<UNFILLED: runner
   command, cell order, the wall ceiling, and the box lane protocol>>.
5. **Abort protocol.** An infrastructure failure before any result row has been viewed (the box dies, a
   process is killed by the host, a disk fills) is not a read, and the campaign may restart from its first
   cell once. Anything else is a read: once a single result row has been viewed by anyone, the campaign
   cannot be restarted or repeated under this declaration, and a new read needs a new declaration and a new
   approval. The viewing log is: <<UNFILLED: who may open the output directory, and how a first view is
   recorded>>.
6. **Leakage disclosure.** The artifact states, beside the held-out numbers: the hash split puts sibling
   problems on both sides (STEENBRF against STEENBR*, TAXR* against TAX*, ACOP* variants, ZAMB2-n, READING7,
   DALLASL, NET2); the HS models are copied from hven's own test battery, so the six HS HOLDOUT problems are
   ones hven was developed against; M7 targeted TRAIN cells by name while the competitors run at defaults.
   HOLDOUT is a held-out set of instances, not independent instances. <<UNFILLED: any further leakage the
   declarer knows of>>
7. **Clean state, single pin.** The harness tree is clean, the hven pin is one sha for all four lanes (the
   TU-4 freeze), and `allow_mixed_pins` is false in the store manifest. All four lanes, and both pre-tuning
   arms, run in the same campaign under the same wall ceiling. The CP-3 generator in FINAL mode refuses a
   dirty harness, a mixed-pin store, and a declaration not committed at its stated sha.
8. **Scope of the verdict.** The whole adopted configuration, one verdict per hven lane. Per-default
   readings would need reads that are not authorised.
9. **Consequence.** If the verdict is "does not generalize": the tuned defaults ship, the verdict is printed
   in the comparison artifact, no generalization claim is made, and nothing is re-tuned (owner ruling C528,
   `cp1-v2` section 7). If it is "generalizes", the artifact says so under item 8's scope.

## 5. Outcome table (filled by the verdict script after the read, not by hand)

| lane | cells lost vs pre-tuning | cells gained | SGM pre | SGM tuned | ratio | verdict |
|---|---|---|---|---|---|---|
| hven-ipm | | | | | | |
| hven-sqp | | | | | | |

## 6. Sign-off

```
declared_by:      <<UNFILLED>>
reviewed_by:      <<UNFILLED>>
owner_approval:   <<UNFILLED: quote or ledger line reference>>
```
