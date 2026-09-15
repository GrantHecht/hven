# M6 CLOSE GATE — the U0 27-cell three-arm replay, and its control

| comparison | cells | columns | differences |
|---|---|---|---|
| W4 close BASE `accbff0e` walk -> CLOSE HEAD `15f97cae` walk | 27 | 75 | **0** |
| W4 close BASE `accbff0e` ssn  -> CLOSE HEAD `15f97cae` ssn  | 27 | 75 | **0** |
| W4 close BASE `accbff0e` ipm  -> CLOSE HEAD `15f97cae` ipm  | 27 | 75 | **0** |
| committed t10b IPM baseline vs CLOSE HEAD-ipm — **the control** | 27 | 75 | **0** |
| committed t10b IPM baseline vs BASE-ipm — **the control, as W5 T9 ran it** | 27 | 75 | **0** |

Both arms report `# schema: 76` and the same
`# budget_table_hash: 0x357aee91dee27391`; the BASE stamp is `accbff0e60b0` and
the HEAD stamp `15f97cae51ff`, both CLEAN.

## How this was run

* **HEAD arm, measured here.** The Release corpus binary this gate built,
  `--engine {walk,ssn,ipm}` over the 27 cells of
  `docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt` (unchanged since W1),
  SOLO in the foreground of its own lock hold, pinned `taskset -c 2` at
  `MKL_NUM_THREADS=1` and `OMP_NUM_THREADS=1`.  NO co-run protocol; none is declared.
* **BASE arm, NOT rebuilt.**  It is the arm W5 T9 built and retained: a detached
  worktree at `.scratch/w5t9/base` configured from EMPTY at the W4 close
  `accbff0e` (code head `d4d78f9`), target-limited to `hven_sqp_corpus`, stamp
  `accbff0e60b0` CLEAN.  Its three CSVs survive in two places and this gate verified
  by sha256 that they are BYTE-IDENTICAL:
  ```
  base-walk  scratch=c057080401dc13692d401fa3ed92beff
  base-walk  committed=c057080401dc13692d401fa3ed92beff
  base-ssn   scratch=62003479e2d2bb1208f14893deb41ac3
  base-ssn   committed=62003479e2d2bb1208f14893deb41ac3
  base-ipm   scratch=dc4b2e5be32094bcb225c22aea8e784b
  base-ipm   committed=dc4b2e5be32094bcb225c22aea8e784b
  ```
  The COMMITTED copies under `docs/notes/data/2026-09-m6-w5-acceptance/replay/` are
  what the comparisons read, so the BASE side of every line is a frozen, citable
  artifact.  Neither copy was rewritten.
* **Comparator:** `scripts/compare_replay.py` at its BYTE-EXACT default —
  `--residual-gate` NOT passed, as the brief §0 rules for this leg.
  sha256 `4f64af2cae391f868bb7ca8e92ca90cbe2f608b77f4d51fa2903bdfd8ae3183d`.
* **No wall-clock is asserted here.**  `wall_s` is the one column of 76 excluded.

## Files

| file | what |
|---|---|
| `head-{walk,ssn,ipm}.csv` | the HEAD arm, measured in this gate |
| `comparisons.txt` | the five 0-difference lines, with the comparator's sha and the stamps |
| `07a-replay-arm.log` | the arm's box-protocol log |
| `07b-replay-cmp.log` | the comparison's box-protocol log |

The BASE CSVs are NOT duplicated here: they are
`docs/notes/data/2026-09-m6-w5-acceptance/replay/base-{walk,ssn,ipm}.csv`, unchanged.
