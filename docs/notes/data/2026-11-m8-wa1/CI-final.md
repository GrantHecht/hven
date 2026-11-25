# WA-1 CI on the final code (879d52bd4 on origin/m8 c71c0a7d9; head f4f50dab1 = code + r3 DECLARATION)

| run | linux-clang-release | linux-clang-coverage | macos-clang-release | windows-clang-release |
|---|---|---|---|---|
| 44515953087 | success | success | success | success |
| 44525558146 (repeat, for the Windows read) | failure: `QpPlatformRetake.TheResponseIsTieredAndPerFactorization` (test_qp_platform_retake.cpp:184, perturbed_pivots 1 vs 1), the only failure (3903/3904) | success | success | success |

The repeat's single failure is the known base failure of `origin/m8` c71c0a7d9 (the sparse-equil-v4 platform-retake test; being fixed on
`seq4-winfix`), reported by the coordinator as Windows-only but seen here on the Linux runner at the same line: INHERITED, not WA-1's
(the test exercises the KKT factor's retake response and never reaches the walk). Windows is green on both runs.

Earlier CI (superseded code): 44459486250 (run 1, base 66cc44ccc) green; 44480073900 (run 1 rebased on bc8ed0e12) and 44498064793 (r2)
Windows failure, BRITGAS qp1 drift; diag branch 44502512280 (diagnostic print), 44505376046 (the r3 budget: Windows green).
