# M7 W2 NC1 (the declined ride) — gate evidence: DECLARED vs MEASURED

Declaration (written and committed **before** the chain was detached, nothing
in it adjusted after): `../wrappers-gates/psym-derivation-nc1.txt`, plus
`../wrappers-gates/preflight.txt` and `../wrappers-gates/rehearsal/`. Base
`82303972` (W2 R1, the restoration-landing diagnosis), range
`82303972..5732a1f3` for every source/header derivation (`5732a1f3` is NC1,
the declined ride: a drop onto a negative-curvature face never falls through
to a step that re-enters the released constraint — the cherry-pick of
`232853d9`, tree identical). Declaration commit `9d0fb512` adds no source,
header or test (docs/notes only). Chain: HEAD at start `9d0fb512`, 0 dirty
files (`chain-nc1.txt:2`), started `CHAIN_START 2026-09-25T18:20:43-04:00` /
`GATES_START 2026-09-25T18:20:43-04:00` (`chain-nc1.txt:1`;
`gates-nc1.txt:1`), ended `CHAIN_END 2026-09-25T19:19:07-04:00`
(`chain-nc1.txt:26`). Runner transcript: `gates-nc1.txt` (copied verbatim
from `.scratch/m7w2nc1gates/logs/gates-nc1.log`). Chain wrapper transcript:
`chain-nc1.txt` (copied verbatim from
`.scratch/m7w2nc1gates/logs/chain-nc1.log`). Full test and build
transcripts, copied verbatim from `.scratch/m7w2nc1gates/gates/`:
`suite-debug.txt`, `suite-release.txt`, `suite-debug-long.txt`,
`install-smoke.txt`, `export-contract.txt`, `fold-proofs.txt`, `psym-*.txt`
(from `gates/psym/`: `classification-{Release,Debug}.txt`,
`compare-{Release,Debug}.txt`, `libobjs-{Release,Debug}.txt`,
`libobjs-both-{Release,Debug}.txt`, `sections-{Release,Debug}.txt`),
`replay/` (the `run-*.log` files renamed `.txt`, `head-*.csv` and
`comparison.txt` kept as-is). The build trees and object directories
(`gates-dbg/`, `gates-rel/`, `psym-build/`, `psym/{before,after}{,-obj}-*`,
`smoke-work/`) and the build/configure logs are not copied.

Rehearsal, pre-flight and offline transcripts, measured before the
declaration and already committed alongside it under `../wrappers-gates/`
and `../wrappers-gates/rehearsal/`.

Every MEASURED cell below cites the file and line it was read from, in the
raw transcript copied alongside this README (all 25 copies are verbatim `cp`;
`cmp` against each source is clean — see **Copy verification** below).

## G1 — Debug suite (long test excluded)

| | DECLARED (`psym-derivation-nc1.txt` §3) | MEASURED |
|---|---|---|
| tests | 2853 | 2853 — `gates-nc1.txt:39`; `suite-debug.txt:5713` |
| failed | 0 | 0 — `gates-nc1.txt:38`; `suite-debug.txt:5713` |
| by name | baseline 2849 started names − 0 REMOVED + 6 ADDED | asserted — `gates-nc1.txt:40` (2855 started names == 2849 − 0 + 6) |

## G2 — the long test, alone

| | DECLARED | MEASURED |
|---|---|---|
| tests | 1 | 1 — `gates-nc1.txt:53`; `suite-debug-long.txt:5` |
| failed | 0 | 0 — `gates-nc1.txt:52`; `suite-debug-long.txt:5` |

`Continuation.ProbeBudgetBoundsAFailingProposal`, run alone
(`gates-nc1.txt:44-45`; `suite-debug-long.txt:2-3`).

## G3 — Release suite

| | DECLARED (§3) | MEASURED |
|---|---|---|
| tests | 2854 | 2854 — `gates-nc1.txt:65`; `suite-release.txt:5715` |
| failed | 0 | 0 — `gates-nc1.txt:64`; `suite-release.txt:5715` |
| by name | baseline 2850 started names − 0 REMOVED + 6 ADDED | asserted — `gates-nc1.txt:66` (2856 started names == 2850 − 0 + 6) |

By name against the derived baseline (the previous round's gate record,
`docs/notes/data/2026-09-m7-w2-e2-working-set/gates-e2-fix6/suite-{debug,
release}.txt`, 2849/2850 started names, ran at `63c687f7`; no test source
differs between `63c687f7` and `82303972`, checked in the pre-flight —
`gates-nc1.txt:18`): six additions, no removals, net +6 per config —

- ADDED `QpE2WorkingSet.ADeclinedRideIsExchangedOnSteenbrbRestorationQp3`,
  `QpE2WorkingSet.ADeclinedRideIsExchangedOnSteenbrbRestorationQp4`,
  `QpE2WorkingSet.ADeclinedRideIsExchangedOnSteenbrbRestorationQp5`,
  `QpE2WorkingSet.ADeclinedRideIsExchangedOnSteenbrbRestorationQp6`,
  `QpE2WorkingSet.ADependentRideBlockerIsExchangedAndTheRideRetaken`,
  `QpE2WorkingSet.ANullSpaceVetoedRideNeverStepsBackIntoTheReleasedBound`;
- REMOVED (none).

Four ADDED non-`.cpp` test files (`tests/qp/fixtures/e2-d4/
Cs_STEENBRB_qp{3,4,5,6}.txt`, the R1 dumps); `tests/CMakeLists.txt`
unchanged (`gates-nc1.txt:19`). Two sources MODIFIED OUTSIDE the P-SYM
universe (`tests/qp/test_qp_anti_cycling_arm.cpp`,
`tests/qp/test_qp_walk_seam.cpp`, both `hven_qp_walk_seam_tests`-only,
a `HVEN_TESTING` recompile target, not a `PSYM_GROUPS` entry) —
`gates-nc1.txt:10-11`.

## G4 — the U0 replay (behavioural gate)

| | DECLARED (§5) | MEASURED |
|---|---|---|
| cells × columns, 3 engines | 27 × 75, 0 differences under the 1e-5 residual gate | 27 × 75, 0 differences on walk, ssn and ipm — `gates-nc1.txt:72,81,83` (CMP_RC=0 at `:80,82,84`; asserted `:87-88`); `replay/comparison.txt:1,9,10` |
| within-gate walk residual moves | 7, on f7_n800_path_warm / f7_n1000_path_warm, recorded not asserted | 7, all on those two cells — `gates-nc1.txt:73-79`; `replay/comparison.txt:2-8` |

ssn and ipm show 0 within-gate moves (`gates-nc1.txt:81,83`;
`replay/comparison.txt:9,10`). No counter, status or objective column moved
on any cell. §5 of the derivation records this same result confirmed
offline first, against both the NC1 scratch build (the rel-deps Release
build of `5732a1f3`) and exactly against the fix6 gate record's head CSVs
(`gates-e2-fix6/replay/`), before the chain ran.

## G5 — P-SYM (both configurations)

| | DECLARED (§2) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| DIFFERING objects | 38 = 2 A + 36 B | 38 — `gates-nc1.txt:111`, asserted `:122` | 38 — `gates-nc1.txt:161`, asserted `:172` |
| class A (source changed) | 2 | 2 — `gates-nc1.txt:112`, asserted `:123` | 2 — `gates-nc1.txt:162`, asserted `:173` |
| class B (includer, source untouched) | 36 | 36 — `gates-nc1.txt:113`, asserted `:124` | 36 — `gates-nc1.txt:163`, asserted `:174` |
| UNEXPLAINED | 0 | 0 — `gates-nc1.txt:114`, asserted `:120` | 0 — `gates-nc1.txt:164`, asserted `:170` |
| NOISE-ONLY | 2 Release (0 lib) / 37 Debug (6 lib) | 2 (0 in the library) — `psym-classification-Release.txt:52` | 37 (6 in the library, all explained) — `psym-classification-Debug.txt:52` |
| negative control | 106 / 71 | 106 — `gates-nc1.txt:115,121`; `psym-classification-Release.txt:56` | 71 — `gates-nc1.txt:165,171`; `psym-classification-Debug.txt:91` |
| compare rc | 1 | 1 — `gates-nc1.txt:100,116` | 1 — `gates-nc1.txt:150,166` |
| capture pair | after = before = 151 | 151 — `gates-nc1.txt:93-95` | 151 — `gates-nc1.txt:143-145` |
| outside-universe objects (`test_qp_anti_cycling_arm.cpp.o`, `test_qp_walk_seam.cpp.o`) | built, 0 in the manifest | `gates-nc1.txt:96-98` | `gates-nc1.txt:146-148` |
| ONLY-AFTER | `[]` | `gates-nc1.txt:101` | `gates-nc1.txt:151` |
| library ADDED | none | `[]` — `gates-nc1.txt:125` | `[]` — `gates-nc1.txt:175` |
| class B symmetry | EQUAL (36 both configs) | asserted — `gates-nc1.txt:197` ("class B sets equal (36 object(s))") | same assert (cross-config) |
| class A cross-config | Release 2 / Debug 2, identical sets (the Debug NOISE-ONLY class A slot is EMPTY this round) | asserted — `gates-nc1.txt:200-202` | same |
| class B by name | matches the declared 36-name list | `gates-nc1.txt:206` | `gates-nc1.txt:207` |
| class A by source | matches the declared source list | `gates-nc1.txt:208` (2 files) | `gates-nc1.txt:209` (2 files) |

The header derivation (`gates-nc1.txt:103-109,153-159`) accepted exactly the
three declared headers (`solver_counters.h`, `qp_engine.h`,
`qp_walk_trace.h`), identically in both configurations. Class A (2, both
configurations) is the 2 in-universe MODIFIED `.cpp` — `src/qp/qp_engine.cpp`
(the NC1 rule) and `tests/sqp/test_hs_sweeps.cpp` (the 3371 → 3365 re-pin) —
neither NOISE-ONLY in either configuration this round: unlike fix6's re-pin,
`test_hs_sweeps.cpp`'s re-pin is not NOISE-ONLY in Debug, so the Debug
NOISE-ONLY class A slot is EMPTY (`EXPECT_CLASS_A_NOISE_ONLY_DEBUG ""`).
Class B is symmetric — Release and Debug sets equal, 36 objects, declared by
name in the runner (`gates-nc1.txt:17`). The library objects among them are
`src/core/ledger.cpp.o`, `src/drivers/sqp_solver.cpp.o`,
`src/globalization/sqp/soc_elastic_restoration.cpp.o`,
`src/qp/ipqp_engine.cpp.o` (`psym-classification-Release.txt:14-17`). The
cause for all 36: each holds, copies or destroys a `QpCounters` by value, and
`QpCounters` grew by three `Index` fields (24 bytes) with one field renamed
in place (`dep_ride_declines` → `dep_ride_blockers`) — sizes, member
offsets, copy/move/zeroing and the inlined destructors move; none of the 36
names the renamed field (only `qp_engine.cpp` and the header do). The
asymmetry admission list is EMPTY this round (the two class B sets are
equal). The mover lists are in `psym-classification-{Release,Debug}.txt`,
the full byte-difference dumps in `psym-compare-{Release,Debug}.txt`, the
object inventories in `psym-libobjs{,-both}-{Release,Debug}.txt`.

**Unexplained movers: 0 in both configurations** (measured and asserted,
cited above — no discrepancy from the declared 0).

## G5b — sections (library objects present on both sides)

| | DECLARED (§4) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| non-debug-mover set | 5 = {qp_engine.cpp.o} (derived, class A) + {ledger.cpp.o, sqp_solver.cpp.o, soc_elastic_restoration.cpp.o, ipqp_engine.cpp.o} (declared, class B) | exactly that, 5 — `gates-nc1.txt:127-131,138`, asserted `:139` | exactly that, 5 — `gates-nc1.txt:177,181,183-185`, asserted `:195` |
| objects compared | 44 | 44 — `gates-nc1.txt:132`; `psym-sections-Release.txt:11` | 44 — `gates-nc1.txt:188`; `psym-sections-Debug.txt:17` |
| byte-identical | 39 Release / 33 Debug | 39 — `gates-nc1.txt:133` | 33 — `gates-nc1.txt:189` |
| DEBUG_ONLY | Release 0 / Debug 6 | 0 — `gates-nc1.txt:134`, asserted `:140` | 6 (`console_trace_sink.cpp.o`, `ipm_solver.cpp.o`, `sqp_print.cpp.o`, `trace_writer.cpp.o`, `ssn_engine.cpp.o`, `continuation.cpp.o`) — `gates-nc1.txt:178-180,182,186-187`, asserted `:196` |
| REFUSED | 0 | 0 — `gates-nc1.txt:136` | 0 — `gates-nc1.txt:192` |

## G6 — install smoke, export contract, format/diff cleanliness

| | DECLARED (§6) | MEASURED |
|---|---|---|
| install smoke | PASSED | PASSED, rc 0 — `gates-nc1.txt:213,215`; `install-smoke.txt:1192` |
| export contract | OK, align 32 | OK, align 32 — `gates-nc1.txt:218,220,223`; `export-contract.txt:3,6` |
| clang-format | rc 0 on every changed .h/.cpp (7 files) | rc 0 over the derived files — `gates-nc1.txt:210,225` |
| git diff --check | clean | rc 0 — `gates-nc1.txt:226` |
| dirty files | 0 | 0 — `gates-nc1.txt:227` |

## Fold proofs

| | DECLARED | MEASURED |
|---|---|---|
| total / failed | 46 / 0 | 46 / 0 — `chain-nc1.txt:24`; `fold-proofs.txt` (final line, `FOLD_PROOFS_TOTAL=46  FOLD_PROOFS_FAILED=0`) |

## Box protocol

Every leg ran inside the box lock with `PGREP_EMPTY` recorded before its
foreground start (`gates-nc1.txt:29,42,55,68,90,212`; `chain-nc1.txt:17`).

## ASSERT OK count

58 `ASSERT OK` lines in `gates-nc1.txt`, 0 assert failures (every `^ASSERT `
line in the transcript reads `ASSERT OK`; no `ASSERT FAIL` or refusal
appears outside the pre-flight's declared negative-control refusals, which
live in `../wrappers-gates/preflight.txt`, not in this chain).

## GATES_SUMMARY

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0` (`gates-nc1.txt:231`);
`GATES_AGGREGATE rc=0 over [0 0 0 0 0 0]` (`gates-nc1.txt:233`);
`CHAIN_SUMMARY gates=0 fold_proofs=0` (`chain-nc1.txt:27`).

No DECLARED FINDING this round: every declared value in
`psym-derivation-nc1.txt` (§§1-8) matched the measured chain exactly, on the
first leg. The rehearsal's hypothesis refusal (a deliberately wrong 2/2
class-A-only hypothesis, rc 4, recorded in `../wrappers-gates/rehearsal/`)
predates the declaration and is not a chain leg — the declaration itself
(38 = 2 A + 36 B, both configurations) was written from that rehearsal's
offline re-classification (`rehearsal-classification-{Release,Debug}.txt`)
and matched this chain's G5 exactly, with no adjustment.

## Copy verification

All 25 files under this directory (plus `replay/`) were produced with `cp`
from the sources listed above and then `cmp`-checked against those sources.
Every comparison reported no differences (`cmp` exit 0, no output) —
`chain-nc1.txt`, `gates-nc1.txt`, `suite-debug.txt`, `suite-release.txt`,
`suite-debug-long.txt`, `install-smoke.txt`, `export-contract.txt`,
`fold-proofs.txt`, the ten `psym-*.txt` files, and the seven `replay/` files
(the three `run-*.txt` compared against their source `run-*.log`,
byte-for-byte, only the filename changed).
