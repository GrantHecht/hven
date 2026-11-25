# OWNER-REPLAY-REQUEST (I19-2a, V6)

**Status: NOT REQUIRED. There is no candidate.** No G-arm and no C-arm passed stage 1 (`S1/stage1.txt`), no composite K
exists, and the stage-2 arm set was H0, P and C1 only (`S2/final-tables.txt`). Nothing ships, so no default flip is on the
table and the V6 gate (declaration 5.3) is not triggered. The text below is prepared as asked so that a replay can be run
without further work if the settler wants one regardless (for example to read the two nearest arms); a replay under this
file is informational and cannot ship anything under the frozen rule (no fall-through; a successor round would carry it).

## The arms (label and exact option settings)

An arm is the default options plus the keys below; every key is a `SqpAdvancedOptions` field (research tier), all other
options at their defaults. The base label for an exact-Hessian solve is `default`; for the L-BFGS (QN) mode it is
`sqp-qn` (`hessian_mode = kLbfgsDamped`).

| arm | keys | status |
|---|---|---|
| H0 (baseline for V6) | none | required in any replay |
| G3 (nearest: fails only the V4 tail guard, one cell) | `advanced.trust_region_rejection_memory = 3` | stage 1 fail (d) |
| G6 (nearest on the cap corpora) | `advanced.trust_region_growth_on_step_limit = false` | stage 1 fail (b), (d) |
| G1 | `advanced.trust_region_growth = kRatioByStepType` | stage 1 fail |
| G2 | `advanced.trust_region_growth_after_soc = false` | stage 1 fail |
| G4 | `advanced.trust_region_growth_ratio_max = 3` | stage 1 fail, VACUOUS V3' |
| G5 | `advanced.trust_region_shrink_to_step = true` | stage 1 fail |
| G7 | `advanced.trust_region_growth_funnel_guard = true` | stage 1 fail |

Build: the owner's shim must link hven at or after code head `bae35b20480aa2be43e9438dbd9b9e2b4eea4e2e` (the I19-2a code
head; the levers exist only there). The six new counters (`trust_region_growths`, `trust_region_growth_refusals`,
`trust_region_shrinks_to_step`, `trust_region_grown_step_rejections`, `trust_region_unbound_rejections`,
`step_limit_binding_majors`) are in `SqpCounters`.

## The four issue-#19 variants

Free or DRM-fixed LOI-cutoff true anomaly, each with `caller_step_limit` 0.1 and 1.0: Free TA / 0.1, Free TA / 1.0,
Fixed TA / 0.1, Fixed TA / 1.0; first planning update; the owner's shim, the owner's box and serial rule (issue-19.md,
"Four variants of one dispersed injection/OD estimate").

## V6 input-file shape (harness `tu/i19_spec.json` `owner_replay`; read by `i19_rule.read_v6`)

A JSON file supplied by the settler:

```json
{
  "variants": ["Free TA / 0.1", "Free TA / 1.0", "Fixed TA / 0.1", "Fixed TA / 1.0"],
  "arms": {
    "H0": { "<variant>": { "info": 1, "majors": 0, "reject_funnel_ceiling": 0 } },
    "<arm>": { "<variant>": { "info": 1, "majors": 0, "reject_funnel_ceiling": 0 } }
  }
}
```

`variants` must list exactly four names and every arm (H0 and the arm under test) must carry exactly those four.
Per variant: `info` = the solver's INFO code (1 = strict optimal), `majors` = major iterations, `reject_funnel_ceiling` =
rejected steps at the funnel ceiling. V6 reads PASS iff, for the arm, every variant has `info == 1`, the sum of `majors`
is not above H0's, and the sum of `reject_funnel_ceiling` is strictly below H0's (`i19_rule.v6_state`). A missing arm or
a missing H0 reads PENDING; a variant set that is not exactly the four reads FAIL.
