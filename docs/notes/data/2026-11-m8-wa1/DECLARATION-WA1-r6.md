# M8 WA-1 r6 -- P3 re-taken against r5's leg rows (DECLARATION, successor of DECLARATION-WA1-r5.md)

Committed before the first r6 leg; never edited afterwards. `DECLARATION-WA1-r5.md` and every earlier output are kept.

## 0. Why

r5's chain (`r5/orchestrate.out`) passed P1 (ctest Release and Debug 3922/3922) and P2 (replay 3 x walk/ssn/ipm, 0 differences
vs both dev references) and STOPPED at P3: 476 rows, 0 missing, the mover set exactly the declared nine, no WORSE mover -- but
one row, `sd-hven-sqp-SPANHYD.K` (numerical_error on both arms), differed from its B dev row: iterations 29 -> 23,
factorizations 42085 -> 40039, qp minors 21410 -> 20306 (`r5/train/train_check.txt`).

Re-run ALONE under the serial rule (CLAUDE.md section 7), the same frozen lane binaries, five times each
(`r6/spanhyd-solo.txt`): arm A 23 iterations / 20309 minors 5 of 5; arm B 23 / 20306 5 of 5 -- the leg's row. The B dev row (29 /
21410) came from the dev enumeration, as did the earlier rung-variant measurement on a different binary of the same code
(`.scratch/meas/train-N-rung`); every solo run and the leg read 23. SPANHYD.K's B row is therefore not reproducible across
processes (bimodal; numerical_error either way, a NEUTRAL mover either way: B differs from A in minors, polish steps and inertia
reads in both modes). A dev reference that is one draw of a bimodal row is an instrument defect, not a code result.

## 1. Code and binaries

Unchanged from `DECLARATION-WA1-r5.md`: code commit `0811f4cf6d1e9d017927e55878e5006308b0665c` on `origin/m8` `b02e88eb4`, the same
`build-wa/{rel,dbg}` trees and frozen binaries (corpus A `48f8e5e2...2ee8`, B `f158012c...eb53`; lane A `5f89a12e...bc51`, B
`555d49dd...0f08`, full hashes in r5 section 5). P1 and P2 stand from r5.

## 2. Legs (fixed here, before data), outputs under `r6/`

| leg | criterion |
|---|---|
| P3 | TRAIN 238 x D/K serial: `I4_PASS` with the row reference = **r5's leg rows** (`r5/train/B`, hashes in `r5/MANIFEST-dropped.txt`) and the mover set `r5/movers-train.txt`; no WORSE mover |
| P4 | `runtime/run_walk.sh` into `r6/runtime/`: THROTTLE 0; counter identity; filtered B/A in [0.995, 1.005] |
| P5 | `legs/mutations_r5.sh` into `r6/mutation/`: control clean; N1-N16 each KILLED; tree restored |
| P6 | `legs/install_smoke.sh`: rc 0, both front ends |

HOLDOUT is never run. `legs/orchestrate_r6.sh` = `orchestrate_r5.sh` with outputs under `r6/`, P3's DEV reference `r5/train/B`, and
P1/P2 not re-run. STOP at the first failed leg; a successor `DECLARATION-WA1-r7.md` after a fix.

```
flock /tmp/box-build.lock setsid -w bash docs/notes/data/2026-11-m8-wa1/legs/orchestrate_r6.sh 0811f4cf6d1e9d017927e55878e5006308b0665c 48f8e5e246d0b5cf359d27488f73dc849c98b65e71c91f24e1916d2a031a2ee8 f158012ca08b481ab6759c185144c97006c4d0e3d22b2c64155e1cd493b6eb53 5f89a12e505cd92aaf6f2d0ad7a9b377aaa8894573234a253cfe19adad9cbc51 555d49dd78eb397aed0c25e00d301c6af39bfd17c3fd939347ff2fff1fed0f08 3922 > docs/notes/data/2026-11-m8-wa1/r6/orchestrate.out 2>&1
```

## 3. Corrections to r5's text (r5 itself is not edited)

r5 section 3 says the rewritten test file holds 14 tests (9 synthetic); it holds 13 (3 helper, 8 synthetic, 2 qp26). CI run
44579321329 on the code commit finished green on all four lanes.
