# M8 PF-5 r2 -- the successor declaration after r1's P3 (DECLARATION)

Committed on `pf5b` before the first r2 leg; never edited afterwards. `DECLARATION-PF-5.md` and `-r1.md` stay frozen.
Run 1's evidence is `gate-run1/` (`c5fb93753`); r1's is `gate-r1/` plus `gate/` (`679e5219b`).

## 1. What failed, and why

r1's P3 reported 476/476 rows `NO_ROW_OR_CAPPED` with 0 reuses. It was not a solver or binary result. The job files
`legs/jobs-train.txt` and `jobs-nlp.txt` hardcode their output prefix `.../2026-11-m8-pf5/gate/...`. r1 set
`GATE_DIR=gate-r1`, which redirected the orchestrator's checker to `gate-r1/train/` but not the cells, so all 952 cell
processes ran (binaries, shas and paths were fine; rc=0 on all but the hs025/hs033 cells, which fail alike on both arms)
into `gate/` and the checker read an empty directory. Disclosure: the `gate/` rows were not checked by any leg.

## 2. The fix (legs only; no code, binary or tree moves)

`legs/orchestrate.sh` derives per-run copies of the job files into `$GATE_DIR/` (sed of the output prefix), hands those
to corun.sh, aborts if the rewrite did not take, and fails P3 before the checker unless 476 `.result` files exist under
`$GATE_DIR/train/A/cells`. Nothing else changes.

## 3. What carries, what re-runs

- Code commit `01e84165d7ca200483c5ccedf0dac05608893360`, the four frozen binaries and sha256, and the criteria and
  protocols of P3-P7 are `DECLARATION-PF-5.md`'s (and r1's fixed checker), unchanged. P1 and P2 of run 1 stand (the
  orchestrator re-checks that the code paths of HEAD equal the code commit and the binary shas).
- r2 re-runs P3, P4, P5, P6, P7 from scratch into `gate-r2/` (none of r1's `gate/` rows is reused):

```
GATE_DIR=gate-r2 bash docs/notes/data/2026-11-m8-pf5/legs/orchestrate.sh 01e84165d7ca200483c5ccedf0dac05608893360 f4c987298a21420543e7e90a708bebdf4b29aa109febb48ee492b0fdcc364f92 5d7c634138dd007e8f96c60bb20d1def1f68491309203b083e2f32da5d5667ec 13e7eaebd0b3708349fbb3549be86591ab3d52dd4084f188d3bb37a0ef73d7ed 912f2f1d6a6cd01b1a46871ac24c4949e835c76fc058de2e4c2521816370795f 4049 P3 > docs/notes/data/2026-11-m8-pf5/gate-r2/orchestrate.out 2>&1
```
