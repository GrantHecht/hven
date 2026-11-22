# DECLARATION-RF-1-AB2-r2 -- mutation M7 of gate A+B2, re-run (successor; -AB2 and -AB2-r1 are kept unedited)

Committed before the re-run, never edited afterwards. Everything of `DECLARATION-RF-1-AB2.md` and `-r1` stands except
mutation M7.

## Why

In leg M of `ab2_chain.sh` (2026-11-22, `gate-AB2/mutations.out`) mutation M7 (row refusals back to the bound rule) did not
compile: its replacement in `mutate_ab2.py` left one parenthesis unbalanced (`M7 BUILD_FAIL`; `gate-AB2/mutation/M7.build.log`).
That is an instrument defect, so M7 has no reading. Every other mutation of that leg stands as run (control clean; M13 and M19
SURVIVED and are reported as such, per the declaration; the leg's printed verdict names only the last survivor).

## The re-run

- `legs/mutate_ab2_r2.py` = `mutate_ab2.py` with M7's replacement balanced (one extra opening parenthesis); no other entry
  changed. `legs/mutations_ab2_r2.sh` = `mutations_ab2.sh` pointed at it.
- Run: control, then M7, on the same filters, after the A+B2 chain has released the tree; evidence in `gate-AB2/mutation-r2/`.
- Bar: control clean; M7 killed, or reported as a survivor by name.
