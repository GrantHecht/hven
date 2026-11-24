# CP-2 HOLDOUT DECLARATION -- TEMPLATE (UNFILLED; NOT APPROVED; NOT A DECLARATION)

This file is a template. It is not a declaration until every `<<UNFILLED: ...>>` field is replaced,
it is committed, and the owner has approved it (G-HOLDOUT, a per-declaration approval; brief Q3,
`docs/notes/2026-11-m8-brief.md:513-516`). It must be copied to
`docs/notes/2026-11-m8-cp2-holdout-declaration.md` and filled there; this template stays as is.
No HOLDOUT cell has been built, decoded, run or read for this template.

Authority: brief §2 CP-2 `:435-439`, §3 exit criterion 10 `:484`, §0 TRAIN/HOLDOUT `:25-29`,
CP-3 `:446` (CP-3 reuses these rows; no second HOLDOUT run); roadmap §M8 exit criteria
(`:291-297`). Accuracy bar: `docs/notes/2026-11-m8-cp1-accuracy-rule.md` (`cp1-v1`).

## 1. Machine-checked header (the CP-3 generator and the HOLDOUT guard read these lines)

```
declaration.id:            cp2-holdout-read
declaration.status:        <<UNFILLED: DRAFT | COMMITTED>>
declaration.commit:        <<UNFILLED: the hven commit that contains this filled file, set when committed>>
approval.status:           <<UNFILLED: APPROVED | REFUSED>>
approval.by:               <<UNFILLED: owner>>
approval.date_utc:         <<UNFILLED: YYYY-MM-DD>>
approval.scope:            this declaration only; not a pre-authorization (brief Q3)
accuracy_rule:             cp1-v1
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
| lanes | hven-ipm, hven-sqp, ipopt, snopt (all four, one run) |
| options labels | <<UNFILLED: the labelled configs as frozen by TU-3/TU-4, per lane; options-dump digests>> |
| budgets | <<UNFILLED: wall, iteration ceilings; must equal the TRAIN comparison's>> |
| MKL | `MKL_NUM_THREADS=1`; serial, one solve alone on the machine (§7) |
| sets | HS-27 v1 HOLDOUT (6 problems) and CUTEst-NLP-35 v1 HOLDOUT (12 problems), as listed by `sets/split-v1.json` at <<UNFILLED: sha256 of the split pin>> |
| cell list | <<UNFILLED: the 18 problems x 4 lanes = 72 cells, by name, copied from the split pin, never by hand>> |
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
| shift | <<UNFILLED: HB-1 shift; harness default 10, declared>> |
| DNF penalty | <<UNFILLED: HB-1 penalty rule; harness default factor 1 x max budget>> |
| "solves a cell" | `outcome == optimal` under each arm's own natural tolerance (CP-1); no tol re-grade |
| comparison | tuned arm vs pre-tuning arm, same lane, same cells; ratio = tuned SGM / pre-tuning SGM; worsens if ratio > 1.10 |
| lanes the reading applies to | <<UNFILLED: the hven lane(s) whose default was tuned>> |
| per-default or whole-config? | <<UNFILLED: reading per tuned default, or on the adopted config as one>> |

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
