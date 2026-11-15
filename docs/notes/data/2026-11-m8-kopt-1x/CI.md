# kopt-1x CI record (workflow `ci`, branch `kopt-1x`, dispatched one at a time)

| run | head | result | linux-clang-release runner |
|---|---|---|---|
| 43754680347 | `86314dcd3` (the change; before the census test) | success: windows, macos, linux-coverage, linux-release all green | AuthenticAMD, avx512f=0, `mkl_auto_branch=2`, REFERENCE route pins asserted; 100% of 3722 |
| 43761003402 | `34ee68ebc` (final code + declaration) | success, all four jobs | AuthenticAMD, avx512f=1, `mkl_auto_branch=2`, OFF-REFERENCE (portable arms); 100% of 3722 |
| 43765482443 | `34ee68ebc` | success, all four jobs | AuthenticAMD, avx512f=1, `mkl_auto_branch=2`, OFF-REFERENCE; 100% of 3722 |

No Intel AVX-512 (`mkl_auto_branch=14`) runner was drawn: the YORKNET qp5 off-reference arm at 1x is NOT observed on its target hardware
by this record (UNOBSERVED, not zero-filled). Evidence for the 1x contract on the off-reference arm here is the portable arm on AMD AVX-512
(runs 2 and 3), where `tests/qp/test_qp_rank_repair.cpp`'s `eq_row_contract` now asserts ratio <= 1 (was 10x) and passed.
