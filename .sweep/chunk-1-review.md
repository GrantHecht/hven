# Chunk 1 comment-sweep review — include/hven/detail/globalization (f0fadf2 over 1e3b64e)

Scope: comment content only (the comment-stripped gate is already green). All 28 files
read new-vs-old; every surviving/new docstring checked against the code that implements
it (`src/drivers/interior_point_solver_globalization.cpp`, `src/globalization/sqp/funnel.cpp`,
`src/globalization/sqp/soc_elastic_restoration.cpp`, `src/drivers/interior_point_solver.cpp`).
Line numbers are at f0fadf2 unless stated.

**Verdict: APPROVED WITH ITEMS** — 2 blocking, 7 should-fix, 8 nits.

The rewrite is substantively faithful. The three docstrings the report flagged as
written-under-uncertainty were checked first: watchdog.h's rung-carrying mechanics and
feasibility_switch_recovery.h's notification-timing consequence are both TRUE of the code;
barrier_governor.h's Fiacco–McCormick gate wording is true, but its sibling in
monitored_governor.h introduced a false condition (finding 2).

---

## Blocking

### 1. filter_acceptance.h:94-97 — the barrier-objective ceiling formula is false for |phi_ref| <= 10

New text:

```
///      (i) barrier-objective CEILING — reject when
///          log10(phi_trial - phi_ref) > kFilterObjMaxInc + log10(|phi_ref|)
///          (only evaluated when phi_trial > phi_ref): a LOG10-scaled growth
///          test, deliberately NOT a ratio.
```

The code (`FilterAcceptance::is_acceptable_to_current`, interior_point_solver_globalization.cpp:2042-2054) is:

```cpp
double basval = 1.0;
if (std::abs(phi_current) > 10.0)
    basval = std::log10(std::abs(phi_current));
if (std::log10(phi_trial - phi_current) > kFilterObjMaxInc + basval)
    return false;
```

The threshold is `kFilterObjMaxInc + basval`, and `basval` is **1.0** unless `|phi_ref| > 10`.
As written the docstring is wrong on the whole `|phi_ref| <= 10` branch (where `log10(|phi_ref|)`
is <= 1 and can be negative or `-inf` at `phi_ref == 0`) — i.e. it understates the ceiling
exactly in the regime most solves spend their time in. The pre-image (old file lines 49-58)
stated it correctly, verbatim with the `basval` default.

Correct text: `... > kFilterObjMaxInc + basval, where basval = log10(|phi_ref|) when
|phi_ref| > 10 and 1.0 otherwise (only evaluated when phi_trial > phi_ref)`.

### 2. monitored_governor.h:70-72 — re-entry documents a "no tiny step" condition this engine does not implement

New text:

```
/// (5) Re-entry monotone -> free: the sufficient-progress test is re-evaluated
///     every monotone iteration against the frozen band; passing it (with no
///     tiny step) re-enters free mode.
```

`MonitoredBarrierGovernor::decide` (interior_point_solver_globalization.cpp, monotone branch)
re-enters free mode on `check_sufficient_progress(curr_error)` alone; there is no tiny-step
predicate anywhere in the governor, in `update_barrier`, or in IterateInfo
(`grep -i tiny src/` returns nothing). The pre-image (old file:110-114) attributed the
condition to **Ipopt**, not to this code: "*if it passes (and no tiny step) **Ipopt** calls
SetFreeMuMode(true)*". The condensation dropped the subject and turned a reference-behavior
note into a false statement about this implementation — precisely the class of claim a
reader would later "restore" as a missing guard.

Correct text: `... passing it re-enters free mode. (The reference additionally requires that
the previous step was not a tiny step; this engine tracks no tiny-step state and does not
apply that extra condition.)`

---

## Should-fix

### 3. sqp/globalization.h:18-19 — surviving text promises tags that were deleted

```
/// ... Where this file's predecessor port
// (the IPM barrier-context funnel, now funnel_acceptance.h) and [KLV]
// disagree, [KLV] WINS; the resolutions are tagged below.
```

Nothing is tagged below: the whole `DIVERGENCES FROM [TYC], EACH RESOLVED TOWARD [KLV]`
block (old sqp/globalization.h:128-196, six numbered items) was removed. Two consequences:
the sentence is now false about its own file, and `src/globalization/sqp/funnel.cpp:60-62`
("the six recorded divergences from the interior-point port ... live in globalization.h and
stay there") is left dangling at a file the chunk did not touch.

At minimum drop the clause. Better: items (4) ("theta_min/theta_max machinery NOT PORTED")
and (5) (KLV Table 1 values vs Uno's shipped defaults, with the beta 0.99-vs-0.9999
contraction consequence) are deliberate deviations a reader comparing the two funnels would
plausibly "fix", so they qualify under rule 3 as important-fact callouts and should survive
in one or two condensed lines.

### 4. Report — the six-divergence block is missing from the rationale-candidates list

`.sweep/chunk-1-report.md` lists the kRestoreMinRejections history for sqp/globalization.h
(:271-277, :305-309 — both verified faithful) but not old lines 128-196, the single largest
deleted rationale block in the chunk. The controller cannot move to docs/notes what the
report does not name. Add it.

### 5. Report — the "total lines" column is wrong in all 28 rows

Every row's `total lines` after-value repeats that row's `comments` before-value. Measured:

| file | report says | actual |
|---|---|---|
| filter_acceptance.h | comments 353→144, lines 462→353 | comments 357→145, lines 462→**261** |
| acceptance_strategy.h | comments 150→76, lines 215→150 | comments 151→77, lines 215→**140** |
| sqp/globalization.h | comments 647→351, lines 733→647 | comments 656→357, lines 733→**440** |

Totals are correspondingly wrong (`6555 → 4429` reported; the after-total is ~2900).
The counts are the report's only quantitative claim about the chunk; regenerate the table.

### 6. sqp/globalization.h:213-223 — one comment block, two `@brief`

```
/// @brief Generalization note for h. KLV Sec. 2.4.1 defines h(x) = ||c(x)||_1
...
/// @brief Interface the driver holds.
class GlobalizationStrategy {
```

Doxygen keeps one brief per entity; the second silently wins or warns depending on config.
Make the generalization note the detailed description under a single `@brief Interface the
driver holds.`, or move it to the constant/struct it actually describes.

### 7. Floating `///` blocks that Doxygen will attach to the wrong declaration

- sqp/globalization.h:36-71 (`@brief THE ALGORITHM (KLV Algorithm 2 ...)`) and :73-74
  (`KLV parameters: every value is Table 1 ...`) are file-level narration separated by
  blank lines; the next declaration is `inline constexpr double kFunnelTauBar` (:77), which
  therefore inherits three stacked doc blocks and two `@brief`s.
- inertia_regularization.h:11-41 (`@brief Proximal primal-dual KKT regularization ...`,
  including the three numerical caveats) attaches the same way to `kProxRegFloor` (:46),
  which carries its own `@brief`.

Both blocks are legitimate content (rule 3 caveats), but they document a *file*, not a
constant. Use plain `//` for them, exactly as the same files' header paragraphs already do
(sqp/globalization.h:6-23, trust_region.h:8-13).

### 8. monitored_governor.h:141-145 — BarrierDecision fields lost a real semantic fact

```
    struct BarrierDecision {
        double mu = 0.0;
        bool mu_event = false;
        bool monotone = false;
    };
```

Old: `double mu = 0.0;      // barrier parameter to use (meaningful iff monotone).` That
qualifier is load-bearing — `decide()` returns `d.mu = mu_in` untouched on every free-mode
path, so a caller reading `d.mu` without checking `d.monotone` gets a stale value. Restore
at least the `mu` field's `///<` (the other two are self-evident).

### 9. sqp/elastic.h:203-206 — the multiplier-contamination derivation no longer follows

New: `stationarity in s_j reads rho - sigma*lambda_j - z_j = 0, so |lambda_j| = rho`.
The condensation dropped the premise that carried the conclusion — old text: `... - z_j = 0
**with z_j = 0 when s_j is off its bound**, so |lambda_j| = rho`. As written the step is a
non-sequitur. Restore the four-word clause.

---

## Nits

10. **Ownership sentences dropped in three files.** filter_acceptance.h (old:264-267) and
    funnel_acceptance.h (old:176-179) both lost *"no SolverContext reference and no NLP eval
    — a pure function of its ProgressMeasures arguments plus <state>"*; l1_restoration.h
    lost old:178-183 (*"No SolverContext reference or NLP handle is retained across calls"*).
    switching_acceptance.h:84-86 and restoration.h:29-33 kept theirs, so the surviving rule
    is discoverable one level up — but these are rule-3 ownership statements and the sweep
    kept the sibling ones, so the deletion is inconsistent rather than principled.

11. **Trailing whitespace left where inline comments were stripped** — 20 lines:
    filter_acceptance.h:237-239, monitored_governor.h:23-28 and 142-144,
    switching_acceptance.h:29-32 and 164, sqp/globalization.h:162, 194, 197. Byte-preserving
    was the right call for the gate, but the tree ships trailing whitespace it did not have
    before; strip in a follow-up formatting pass (still code-token-neutral).

12. **Missing `@throws` on declarations that do throw.** filter_acceptance.h:185, 188;
    funnel_acceptance.h:115, 118; modern_merit.h:141, 144 (all `std::logic_error` on a
    mis-wired phase transition); modern_merit.h:111 (`logic_error` on an unknown
    MeritPenaltyRules); sqp/globalization.h:383 (`judge` throws `logic_error` before the
    first `reset`). The rewrite added `@throws` diligently elsewhere
    (acceptance_strategy.h:115, merit_acceptance.h:40, sqp/globalization.h:376, 405, 413),
    so these read as omissions rather than a style choice.

13. **acceptance_strategy.h:78-84, 86-90** — `@param current` documents a parameter that is
    unnamed in the declaration (`virtual void notify_switch_to_feasibility(const
    ProgressMeasures &)`). Doxygen warns; either name the parameter or fold the sentence
    into the description.

14. **l1_restoration.h:31-33** — the docstring *"Closed-form elastic slack initialization
    for one constraint row; exposed as a free function so it is directly unit-testable ..."*
    is attached to `struct ElasticSlackInit` (the result aggregate) but describes
    `l1_elastic_slack_init` (:44), which has no `@brief`. Swap them.

15. **acceptance_strategy.h:22-28** — kKappaResto lost its citation (Ipopt option
    `required_infeasibility_reduction`, shipped default 0.9) while every sibling constant in
    the chunk kept its option-name citation, including the report's own stated convention
    ("replaced by option-name citations at the constants").

16. **Grouped constant docstrings attach to the first constant only** —
    monitored_governor.h:19-28 (one block naming six Ipopt options over six constants; the
    mapping is now positional) and recovery_chain.h:21-33 (one block over four depth
    constants). Correct as prose, mis-attached as Doxygen.

17. **barrier_governor.h:98-100** — `update_barrier_monotone`'s docstring documents `mu_in`
    and `mu_event` but not the `barr_obj` out-parameter, which the body writes (the sibling
    `update_barrier` at :62 does document it).

18. **switching_acceptance.h:28-33** — the four `RejectionCause` enumerators lost their
    per-value mapping (`kCeiling // θ(trial) > θ_max (Eq. 21)` etc.). The class's ACCEPTANCE
    ORDER paragraph still carries the mapping in aggregate, so this is recoverable, but a
    one-word `///<` each would cost nothing.

---

## Verified true (no action)

- filter_acceptance.h: the two-condition margin (Eqs. 18a/18b), augmentation-on-H-type-only,
  the once-per-accept reset heuristic and its per-phase cap, the whole entry/exit/stash
  contract, and the mu-event reset invariant — all match the implementation.
- funnel_acceptance.h: Eq. (9) init, (2a)/(2b) verdicts, the update-strategy-1 width rule and
  its unconditional-contraction argument, the +inf-width FP-inertness of the exit re-base.
- modern_merit.h: the WMNO tau threshold, the nu / pi_u / pi_l update rules (including
  `min{pi_u, pi_l + max{0.1(r-pi_l), eps_l}}`), the ignored `reference` argument and the
  stashed-tracker read.
- monitored_governor.h items (1)-(4) and (6), including the `floor = min(bar_tol,kkt_tol)/
  (kBarrierTolFactor+1)` mapping. Only (5) is wrong — finding 2.
- watchdog.h: the ExtendedBacktrack seed/carry mechanics (`scale = alpha`, then
  `scale = alpha_result * scale`) and the whole WatchdogState machine.
- feasibility_stall.h, proximal_restoration.h, l1_restoration.h, soc.h, sqp/soc.h,
  sqp/restoration.h, sqp/trust_region.h, sqp/globalization.h's five kRestore conjuncts and
  full-step mode, merit_acceptance.h, restoration.h, recovery_chain.h, solver_context.h,
  progress_measures.h, noop_recovery.h, globalization_mechanism.h,
  backtracking_line_search.h, classic_adaptive_governor.h, switching_acceptance.h.
- Rationale-candidate list spot-checks: globalization_mechanism.h:4-10, restoration.h:8-19,
  feasibility_stall.h:26-58, monitored_governor.h:14-45, modern_merit.h:85-98,
  sqp/globalization.h:271-277 & 305-309, sqp/elastic.h:166-171 — all faithful to the deleted
  text (line ranges and summaries both).
- No residual narration, history, or plan/task/milestone labels found in any of the 28
  files (`Task N`, `phase-C`, `T6`, `fix round`, `M3`, `E2`, `G7` all gone). Kept
  measurements that read like provenance (sqp/elastic.h's "emits NO symbol in a clean
  Release build", sqp/globalization.h's unpruned-include note) are engineering facts about
  the current tree, not history — correctly retained.
