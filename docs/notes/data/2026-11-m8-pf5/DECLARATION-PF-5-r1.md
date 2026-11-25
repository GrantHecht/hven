# M8 PF-5 r1 -- the successor declaration after gate run 1's P3 (DECLARATION)

Committed on `pf5b` before the first r1 leg; never edited afterwards. `DECLARATION-PF-5.md` stays frozen; run 1's
evidence is `gate-run1/` (committed as it stood, `c5fb93753`). A failed or void r1 leg is answered by
`DECLARATION-PF-5-r2.md`.

## 1. What failed, and why

Run 1 passed P1 (ctest Release and Debug, 4049/4049 each) and P2 (replay: A vs B x3, walk/ssn/ipm, 0 differences),
and stopped at P3: `pf5_check.py` failed 238 of the 476 TRAIN rows with `B_LACKS_side.symbolic_analysis_reuses`. Those
238 are exactly the hven-ipm rows (119 cells x D/K): the IPM lane reports no `SqpCounters`, so neither arm carries any
of the three PF-5 columns there, and the checker demanded the new one on every row. A checker defect, not a code or
trajectory finding. Every hven-sqp row passed its identities in run 1.

Disclosure: after the failure the fixed checker was run on run 1's rows (diagnosis, not a leg): `ROWS_AS_DECLARED`,
476 rows, 52 with hits, 15 631 reuses (9 691 sparse-arm = platform analyses saved, 5 940 dense-arm). That reading is not
gate evidence; r1 re-runs P3 from scratch.

## 2. The fix (legs only; no code, binary or tree moves)

`legs/pf5_check.py`: a row on which NEITHER arm carries any of the three PF-5 columns compares plainly (every key, the
exit code and the .optdump equal; residual-class within train_check's gate). Rows that carry them keep section 3 of
`DECLARATION-PF-5.md` unchanged. `legs/orchestrate.sh`: the output directory is `GATE_DIR` (default `gate`).

## 3. What carries, what re-runs

- The code commit (`01e84165d7ca200483c5ccedf0dac05608893360`), the four frozen binaries and their sha256, the criteria
  of P3-P7 and the protocols are `DECLARATION-PF-5.md`'s, unchanged.
- P1 and P2 of run 1 STAND: the code paths of the r1 leg HEAD equal the code commit (orchestrate.sh checks it), and
  the binaries are the same files (checked by sha256), so re-running them would measure the same objects.
- r1 runs P3, P4, P5, P6, P7 in that order into `gate-r1/`:

```
GATE_DIR=gate-r1 bash docs/notes/data/2026-11-m8-pf5/legs/orchestrate.sh 01e84165d7ca200483c5ccedf0dac05608893360 f4c987298a21420543e7e90a708bebdf4b29aa109febb48ee492b0fdcc364f92 5d7c634138dd007e8f96c60bb20d1def1f68491309203b083e2f32da5d5667ec 13e7eaebd0b3708349fbb3549be86591ab3d52dd4084f188d3bb37a0ef73d7ed 912f2f1d6a6cd01b1a46871ac24c4949e835c76fc058de2e4c2521816370795f 4049 P3 > docs/notes/data/2026-11-m8-pf5/gate-r1/orchestrate.out 2>&1
```
