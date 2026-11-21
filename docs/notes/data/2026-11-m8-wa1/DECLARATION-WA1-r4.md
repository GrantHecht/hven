# M8 WA-1 r4 -- the mutation instrument re-pointed (DECLARATION, successor of DECLARATION-WA1-r3.md)

Committed before the first r4 leg; never edited afterwards. Every earlier declaration and output is kept.

## 0. Why

r3's chain (`r3/orchestrate.out`) passed P1-P4 and STOPPED at P5: `mutate.py`'s M5 anchor `d.move = -a_merit;` stopped existing in r2's
code (`d.move = -a_stop;`), so M5 did not apply (`MUTATION_DOES_NOT_APPLY`) and the leg read it as a survivor. An instrument defect, not
a code result: control was clean and M1, M2, M3, M4, M8 were killed. The only change here is `legs/mutate.py`'s M5 anchor (now
`d.move = -a_stop;` -> `d.move = a_stop;`, the move along +p); every mutation was pre-flighted to apply and restore before this file.

## 1. Code and binaries

Unchanged from `DECLARATION-WA1-r3.md`: code commit `879d52bd43e13da863f402a47c0c78bba1f6e38a` on `origin/m8` `c71c0a7d9`, the same
`build-wa/rel` tree and frozen binaries. P1-P4 stand from r3: ctest Release and Debug 3916/3916; replay 3 x walk/ssn/ipm 0 differences;
TRAIN I4_PASS, 8 NEUTRAL movers as declared; runtime filtered B/A 1.000002, counter identity 81/81.

## 2. CI (r3's code, already read)

Run 44515953087 green on all four lanes; a second run (44525558146) dispatched for the Windows repeat.

## 3. Legs (fixed here, before data), outputs under `r4/`

| leg | criterion |
|---|---|
| P5 | `legs/mutations.sh`: control clean; M1, M2, M3, M4, M5, M8 each KILLED; M6, M7 recorded; tree restored (MUTATIONS_AS_DECLARED) |
| P6 | `legs/install_smoke.sh`: install smoke and export contract rc 0, default (GCC) and clang++ front ends |

```
flock /tmp/box-build.lock setsid -w bash docs/notes/data/2026-11-m8-wa1/legs/orchestrate_r4.sh 879d52bd43e13da863f402a47c0c78bba1f6e38a > docs/notes/data/2026-11-m8-wa1/r4/orchestrate.out 2>&1
```

STOP and ON FAIL as before; a successor `DECLARATION-WA1-r5.md` after a fix.
