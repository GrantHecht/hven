# M6 CLOSE GATE — evidence (frozen head `15f97cae`, code head `d25a4a6c`)

**Read `PROVENANCE.txt` first** — it records the frozen head, the toolchain, the
hardware, the corpus binary's stamp, and the protocol each leg ran under
(CLAUDE.md §7). Everything here is **MKL on Linux**; Apple/Accelerate and
Windows are **UNOBSERVED** and nothing is inferred for either.

Produced 2026-09-14 by the M6 close-gate implementer. **No commit landed during
the gate**, nothing was pushed, and nothing under `docs/notes/data/**` was
written by it: the tree was clean at `15f97cae` before the first leg and clean
after the last.

**ALL NINE LEGS RAN, IN THE BRIEF'S ORDER, AND ALL NINE PASSED — the gate is
GREEN.** Two results are not all-green by design and were expected to be so: the
golden rig's one designed failure, and the walk engine's score gates (reported,
not gated — and byte-identical to their pinned M3 reading).

## The verdict table

| # | leg | result | log |
|---|---|---|---|
| 0 | wrapper fold proofs | **PASS** — 12 wrappers each propagated a stubbed failure, + `run_leg.sh`'s own failure-propagation proof as its own leg (13); re-run whole after the one wrapper patch | `suites/00-fold-proof.log`, `suites/00b-fold-proof-recheck.log` |
| 1 | builds — Release + Debug from EMPTY | **PASS** — 4/4 steps rc=0; 0 `error:`/`FAILED` lines in either build log; stamp `15f97cae51ff` CLEAN in both trees | `suites/01-builds.log` |
| 2 | suite — Release, whole | **PASS — 2591 / 2591, 0 failed** (inventory 2593) | `suites/02-suite-release.log` |
| 3 | suite — Debug, long test excluded | **PASS — 2590 / 2590, 0 failed** (inventory 2593) | `suites/03-suite-debug.log` |
| 4 | suite — Debug, the >600 s test ALONE | **PASS — 1 / 1**, 669.33 s, detached under nohup, alone on the lock, pids retained | `suites/04-long-test.log`, `suites/04-long-test-pid.txt` |
| 5 | install smoke + export contract | **PASS** — 18 standalone-include TUs, both solvers converged through ONE generic consumer against a fresh install prefix; export contract `exported=32 computed=32` | `smoke.txt`, `export.txt` |
| 6 | golden rig — three seams | **PASS — exactly the one designed failure**, 3/3 controls green; both seam pins verified | `rig/` |
| 7 | 27-cell three-arm replay + t10b control | **PASS — 27 / 75 / 0** on walk, ssn and ipm, and 0 on both control lines | `replay/` |
| 8 | 57-cell walk census — T1 44 / T2 3 / T3 10 | **PASS — `57 baseline cells, 57 fresh cells, 0 mismatches`**, byte-exact on all 13 asserted columns; `serial_confirm_list.txt` EMPTY (no candidate, no solo re-run owed); all 13 formerly-discharged cells match | `census/` |

The three tests that do not run in either config are the same three W2's close
named, and they are unchanged here: `FailByDesignControl.ControlsArePresentFor…`
(Skipped with no OLD-SEAM adapter in the build — it RUNS and passes in the
three-seam tree), and `EqpRefinementAb.FootprintRuleProbe` /
`EqpRefinementAb.FullBattery` (Disabled).

## Layout

```
PROVENANCE.txt              the stamp, the frozen head, the protocol of every leg
README.md                   this file — the verdict table
smoke.txt                   the install smoke leg (LTO OFF, the script's default arm)
export.txt                  the export contract, align read from THIS build's configure log
smoke-full-transcript.txt   the smoke script's own full output
smoke-configure.log         the smoke's configure log (the align line's source)
stamp-{release,debug}.csv   the corpus binary's provenance header from each tree
suites/                     both configs' ctest logs, the long test's log with its pids,
                            the build/configure logs, the fold proofs
rig/                        the three-seam configure and build, both ctest runs, both
                            inventories, README.md (the pins, the failure, the controls)
replay/                     the HEAD arm's three CSVs, the comparisons, README.md
census/                     the runner's whole output dir + README.md
wrappers/                   every wrapper this gate ran, as it ran them
```
