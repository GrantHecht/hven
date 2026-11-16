# M8 sparse-equil -- DECLARATION-r2 (successor to DECLARATION.md)

Committed before its first leg; never edited afterwards. `DECLARATION.md` (r1) stays frozen; its code commit, binaries, mover rule,
criteria, mutation list and protocols stand unchanged and are not restated.

## 0. Why r2

r1's chain ran P1 (Release, Debug) and P2 (replay) and **both met their criteria** (`r1/orchestrate.out`, `r1/suites/`, `r1/replay/`: 3782
of 3782 in both builds; walk / ssn / ipm each B x3 vs arm A and vs the dev row, 0 differences, `REPLAY_AS_DECLARED`). It STOPPED at
P3: the private lane clone had no decoded CUTEst sources for the `sd` population (the sources `hven_bench_cell` reads from
`.scratch/h3/decoded/<problem>` -- a git-ignored directory the `sets/cutest` copy did not bring), so 388 of 476 rows per arm had no row
(`r1/train/train_check.txt`: `FAIL MISSING` x388; the 88 `hs` rows ran; one of them, in arm B, carried an equilibrated refactor). That
is an instrument fault of the leg's setup, not a code result; no criterion is changed. I disclosed the same fault in kopt-1x and did not
pre-check the `sd` population here -- the pre-check ran `hs` cells only.

Fix: `.scratch/h3` copied from the local hven-bench-dn1 clone's git-ignored tree (the same decoded sources, pure decode) into the private
lane clone. Pre-check, before this file: all 97 `sd` roster problems have a decoded directory; one `hven-ipm` and two `hven-sqp` `sd`/`hs`
cells (`HS100`, `DIXCHLNG`) x `never-default` x both lane binaries: rc 0, status optimal.

## 1. What carries and what runs

- **Code and binaries: unchanged** (`35ab829899399d9d807982b818f15e22100d4dd5`; corpus A/B and lane A/B sha256 as in r1's section 1; the
  private clone and the lane binaries are not rebuilt).
- **P1 and P2: carried** from r1, whose criteria were met on this very code and these binaries.
- **Run now: P3, P4, P5** with r1's criteria verbatim (section 4 rows P3, P4, P5, including the TRAIN mover rule of section 3: a row of
  arm B may differ from arm A's only if B's own row has `side.se_sparse_equilibrated_refactors > 0`; every other row identical; 0 worse
  statuses; no missing or capped row), by `legs/orchestrate-r2.sh`. P5 modifies and restores the tree and rebuilds `build-se/rel`; it runs
  last.
- The Mac CI row is separate (`CI.md`); the in-flight dispatch on the code commit is the one it reads.

## 2. Order

```
flock /tmp/box-build.lock setsid -w bash docs/notes/data/2026-11-m8-sparse-equil/legs/orchestrate-r2.sh 35ab829899399d9d807982b818f15e22100d4dd5 3782 07e3e178bf9d2c84569a528eecc23928bf39f116f6a8a9892c013ef8dcd1f2e3 d38d461819b4336eb83e87a8248b1cdf380ef1239cfc09a79fe703ee2f3d6e72 d375ea46d365660dabf8c969fc278d9e90dcf14ae909270bf0c654cbeab71719 fb99a0f55c3ea7f1aca019d413364f5a296da1734ddfbfe24941bedc266ae77a > docs/notes/data/2026-11-m8-sparse-equil/r2-orchestrate.out 2>&1
```

Stop at the first failed leg; a successor `DECLARATION-r3.md` after a fix. Raw cell dumps under `.scratch/`, not committed; summaries and a
MANIFEST are.
