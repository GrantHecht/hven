# RESULTS-AP4 -- groups G1 (options registry) and G2 (generated options reference)

Criteria: `DECLARATION-AP4.md` (committed first, unchanged). Base `0cecdc6d0`; G1 code `e884f8f57`; G2 code `dfbab4137`. Instruments: `legs/` (as run; paths are this worktree's). Evidence: `gate/g1/`, `gate/g2/`, `gate/replay-A/` (the base arm, taken once, serves both groups).

| leg | G1 | G2 |
|---|---|---|
| ctest Release | 3990 run, 100% passed | 4001 run, 100% passed (+11 `OptionsReference.*`) |
| ctest Debug | 3990 run, 100% passed | 4001 run, 100% passed |
| dump bytes vs base (`OptionsDumpFixtures.*`, 6 tests) | identical: `IpmOptions{}`, the five `ipm_preset`s, `SqpOptions{}`, one all-fields-perturbed value of each | unchanged |
| replay vs base, 27 cells x walk / ssn / ipm | `differences: 0` x3 | `differences: 0` x3 |
| install smoke + export contract, default (GCC) and clang | rc 0, rc 0 | rc 0, rc 0 (the smoke consumer now also calls the new public header) |

Findings:
- The header-default rider found one stale header comment, `IpmLinearOptions::solve_parallelism` ("Default `kAdaptive`"; the enumerator is `kAdaptivePartitioning`); fixed in `ipm_options.h` (comment only). It compared 101 stated defaults and could not read 10 phrases (`2^26 = ...`-style); two per-engine statements (`common.max_iterations`, `common.threads`) and one multiple of epsilon are listed exceptions in the test.
- Every interval range the tables state is enforced by `validate()`: all just-outside, NaN and (where finite) +inf probes threw (IPM 100+ probes, SQP 120+), every enum field refused an out-of-enum value, and the defaults validate. No `validate()` gap was found; no validation changed.
- Layering: the design put every enum name table in one `src/solver/` TU; that would have needed three new `tests/core/test_layering.cpp` allow-list entries (a solver-layer file naming the ipm and sqp enums). The tables and name tables sit at the layer of the struct they list instead, so the allow-list is untouched. The one core test that moved is `CoreLayering.TheScanActuallyReadsTheCoreHeaders` (one new `detail/core` header, the registry vocabulary: 6 -> 7 internal, 17 -> 18 total).
- Source count 60 -> 64 (all four new TUs OUT of the PCH, unmeasured default); no PCH-membership change.
- Disclosed, not measured: the dump now allocates one `std::string` per line (the old printer did too, via `fmt::format`); `dump_options` is a once-per-run diagnostic, not on any solve path, and no solver TU is touched.
