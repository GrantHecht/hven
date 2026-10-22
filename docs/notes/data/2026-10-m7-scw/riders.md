# SCW review riders -- re-measurement (2026-10-22)

Base eb6d4509. Riders: carried-factor range check and non-finite-scaled-point fallback,
payload/crossover doc sentences, the hidden-gradient objective-factor test, ceiling
spelling, scale-aware SSN band test, restoration seed count under the default plus the
probe, header narrative trims, carried-spread documentation, stale option wording.

## Mutants (apply, rebuild, run the targeted tests, revert) -- all killed
- carried-factor range check removed (`carried_column_factors_usable`): killed by
  ColumnScalingWarmChain.AnUnscaledDecisionIsNotReReadAtTheWarmPoint (2^-1074 / 2^1000 records).
- `obj` dropped from the ceiling divisor `s / (obj * d(i))`: killed by
  ScaledConvergenceGuard.TheCeilingDividesByTheObjectiveFactorOnTheDefaultPath.
- SSN band scale `max(1, ||z||inf)` dropped: killed by
  SsnEngineLocal.TheAbsentPriceBandScalesWithTheSeedsOwnMagnitude.

## Suites (MKL_NUM_THREADS=1, ctest -j4, long test included)
- Release: 3204/3204 pass.  Debug: 3204/3204 pass.

## U0 exact replay vs docs/notes/data/2026-10-m7-rst-gates/gates/replay
- walk, ssn, ipm: 27 cells, 75 columns each, 0 differences.

## Why the 46 cells are not re-run
The rider diff changes no code on a well-formed path. The only behavioural edits are
(a) `carried_column_factors_usable` gains a range test and the carried record is refused
when the scaled x0 / warm.x / warm.z would be non-finite: both only reject records a
rule-produced decision never is (rule factors lie in [1/limit, limit] by construction and
finite caller vectors scale finitely), so every solve whose carried record was valid takes
the identical path; (b) test-only and comment/doc-only changes elsewhere, plus a constant
comment. The U0 replay above (cold and warm cells, 0 differences) confirms it.
