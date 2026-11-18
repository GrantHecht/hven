# M8 GL-0 -- DECLARATION-r4b (successor to DECLARATION-r4.md, committed before the r4b code and legs; r4 stays as written)

r4's chain (code head `c4d3029c8`) stopped at Release ctest: 3884 of 3885 passed, and `SqpGl0.TheSwitchingRatioRaisesNoFloatingPointFlagAtZeroViolation` FAILED (`fetestexcept(FE_DIVBYZERO | FE_INVALID)` read FE_INVALID) in the Release build only (Debug passed in a pre-declaration run). The cause: with the compiler's default FP-exception assumption (ignore), an optimizer may legally speculate the division into a select, so "the branch skips the division" is not a guarantee in optimized code; the branch form of r4 does not keep the flags clear. Evidence of the failed run: `r4/run1-failed/`.

## r4b change (the only one)
`detail::switching_ratio_value` (include/hven/detail/sqp/sqp_major_row.h) divides on a divisor that is never zero (1.0 stands in for a zero denominator) and selects the result afterwards: +/-inf by the sign of `pred_df` (nonzero), quiet NaN (zero or NaN `pred_df`), otherwise the quotient. A speculated division then divides by a nonzero divisor and cannot raise a flag; a nonzero denominator divides exactly as before (bit-identical). The test is unchanged. Everything else of DECLARATION-r4 (findings 2-4, the tests, the legs, the replay/install/mutation protocol, the not-re-run disclosure) is unchanged and stands; r4's leg list is re-run in full on the new head, including both builds and both ctest runs.

## Instrument
Mutation `n1b` is re-pointed: the helper's guarded divisor `pred_df / (zero_denominator ? 1.0 : denominator)` becomes `pred_df / denominator` (the helper then divides by zero and the flag test must fail). `n1` and `n2` anchors are unchanged. Must-kill count stays 18 (15 + n1, n1b, n2).

## Runtime
The leg is still not re-run: the helper adds one compare-and-select around the existing division (per major, one division already present at this site); disclosed as "unchanged in kind, not re-measured". r3's RT stands for the class.
