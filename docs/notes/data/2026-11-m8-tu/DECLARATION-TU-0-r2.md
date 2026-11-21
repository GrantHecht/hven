# M8 TU-0 r2 -- successor round (DECLARATION)

Committed before the first r2 leg. `DECLARATION-TU-0.md` stands unchanged except for what this file replaces; its round (`gate/`, `orchestrate.out`) is kept as the r1 record.

## 0. Why r2

r1 stopped at `P1-rel-criterion`: the Release ctest ran green ("100% tests passed, 0 tests failed out of **3980**", `gate/suites/ctest-rel.log`) but the declared count was **3982**, which was
`ctest -N`'s total; ctest's own "out of N" excludes the two tests that do not run on this build (`FailByDesignControl...` Skipped, and the Disabled pair counted differently). Nothing but the
declared count was wrong; no other leg ran. The code commit, base, binaries, instruments, mutations and criteria of `DECLARATION-TU-0.md` are unchanged.

## 1. What changes

- **ctest count: 3980** (Release and Debug), the figure ctest prints in "out of N" on the code commit (Release read it in r1's P1; Debug is the same enumeration).
- Round directory `gate-r2/`; the orchestrate line of section 6 of `DECLARATION-TU-0.md` is run with `3980` and round name `gate-r2`, output to `orchestrate-r2.out`.
- Binary shas: corpus A, lane A, lane B as in `DECLARATION-TU-0.md` section 1 (P0 checks them); corpus B is recorded by P0 (addendum to that section).

## 2. Order

```
flock /tmp/box-build.lock setsid -w bash docs/notes/data/2026-11-m8-tu/legs/orchestrate_tu0.sh 0947374fef07c7085f982ecbdcf1aaaa45b6d00f 331b3183747b5c1849651706f648de2c337ddf75 92e3f2939970b372b3400683ddc2b2b66f06cc1346e8ecbc56e892fcd39a563f 2bfd9f6bdc3a12c2864105a689cc78b815b2fdb4e130550516e4eeadebf7e963 9d766ea786fdbffb938120547b629177c5ff8d4309efa144194a5044246c9481 3e14366fa060be9dced8f97ddb078b045551eb10c1be1ec950985efe6fb3d10a 3980 gate-r2 \
  > docs/notes/data/2026-11-m8-tu/orchestrate-r2.out 2>&1
```
A failed leg stops the chain and is answered by `DECLARATION-TU-0-r3.md`.
