// warm_start.cpp -- the interior-point crossover's ingest orchestration,
// `from_interior_point`, carved out of detail/warmstart/warm_start.h.
//
// M3 PHASE-C T7.
//
// ---------------------------------------------------------------------------
// WHAT MOVED, AND WHAT DELIBERATELY DID NOT.
//
// The header carries ONE of the three `template` declarations in the whole SQP
// tree: detail::ip_activity_threshold. IT STAYS IN THE HEADER, and not merely
// because CLAUDE.md 5 exempts templates. It takes its `residual` as a TEMPLATE
// PARAMETER precisely so a caller can hand it an Eigen EXPRESSION (`x - lower`)
// instead of a materialised vector, on a path reachable at n = 1e6 through a
// scale corpus's crossover cells; out-of-lining it would force two n-sized
// temporaries per call, which is the cost the template exists to avoid.
//
// The inlining that matters is preserved BY CONSTRUCTION rather than by
// argument: at the base commit that template emitted no symbol in any of the 69
// objects of a clean Release build -- it was fully inlined into
// from_interior_point at all three of its call sites there -- and since this TU
// includes the header, it is instantiated and inlined in exactly the same place
// now. Nothing about that changed; only where from_interior_point's own body
// is compiled changed.
//
// The StartLevel resolution the T7 plan row also names is NOT here and is not
// missing: it lives inside SqpDriver::solve_impl and was moved to
// src/drivers/sqp_driver.cpp by T5, so there was nothing of it left in this
// header to carve.
//
// ---------------------------------------------------------------------------
// WHY IT COSTS NOTHING, READ OUT OF THE OBJECT FILES.
//
// from_interior_point ran ONCE PER SOLVE at most -- it is the hand-off from an
// interior-point run, not a per-iteration path -- and at the base commit it was
// never inlined anywhere. It emitted a weak copy in exactly four objects
// (bench_corpus, test_corpus_cells, test_warm_start_battery, test_warm_start),
// and every one of its 23 call sites across the build was already a DIRECT
// relocation to that copy. NO src/ object defined it at all: the crossover has
// no caller inside the library, being a public entry point for a consumer. So
// this carve replaces four weak copies with one library symbol and changes no
// call site's shape.
//
// ---------------------------------------------------------------------------
// THE FP, AND WHAT CARRIES THE PROOF.
//
// The activity inference computes with doubles -- the complementarity mean, the
// dual infinity norm, and the two bound-side thresholds -- and the `> 0.0` /
// `>=` comparisons that consume them decide which rows enter the emitted
// working set. Under -ffast-math those are re-associable, so a boundary with
// different flags on its two sides could change an ingested activity pattern
// and therefore a downstream solve.
//
// Since M3 phase-C U0 there is ONE uniform flag regime: this TU, the header,
// and every consumer compile with the same options. The falsifier is the one
// T4, T5 and T6 accepted: the proof is BIT-IDENTITY of every asserted counter
// across the 57-cell walk census and the 17-cell bench. Any counter movement
// means the arithmetic moved, and the carve is reverted rather than
// re-derived.
//
// ---------------------------------------------------------------------------
// The sign conventions, the activity-inference derivation, what the emitted
// object does and does not carry, and every "why" stay in the header. This file
// carries the executable form.

#include <hven/detail/warmstart/warm_start.h>

namespace hven::solvers {

WarmStart from_interior_point(const Vec &x, const Vec &lambda_e, const Vec &lambda_i,
                              const Vec &slack_i, const Vec &z_lower, const Vec &z_upper,
                              const Vec &lower, const Vec &upper, const IpCrossoverOptions &opts) {
    const Index n = x.size();
    const Index mi = lambda_i.size();
    if (slack_i.size() != mi) {
        throw std::invalid_argument(fmt::format(
            "from_interior_point: slack_i has size {}, expected {} (lambda_i's own size -- one "
            "cI value per inequality row)",
            slack_i.size(), mi));
    }
    if (z_lower.size() != n) {
        throw std::invalid_argument(
            fmt::format("from_interior_point: z_lower has size {}, expected {} (x's own size)",
                        z_lower.size(), n));
    }
    if (z_upper.size() != n) {
        throw std::invalid_argument(
            fmt::format("from_interior_point: z_upper has size {}, expected {} (x's own size)",
                        z_upper.size(), n));
    }
    if (lower.size() != n) {
        throw std::invalid_argument(fmt::format(
            "from_interior_point: lower has size {}, expected {} (x's own size)", lower.size(), n));
    }
    if (upper.size() != n) {
        throw std::invalid_argument(fmt::format(
            "from_interior_point: upper has size {}, expected {} (x's own size)", upper.size(), n));
    }

    WarmStart out;
    out.x = x;
    out.lambda_e = lambda_e;
    out.lambda_i = lambda_i;
    // This project's single signed convention (this header's SIGN
    // CONVENTIONS note): z >= 0 at an active lower bound (z_lower live,
    // z_upper == 0), z <= 0 at an active upper bound (mirrored).
    out.z = z_lower - z_upper;

    out.ineq_active.assign(static_cast<std::size_t>(mi), 0);
    out.qp_working_set = WorkingSet(n, mi);
    // ONE threshold for the whole row population, computed BEFORE the loop:
    // the rule is relative to the hand-off's own dual scale, which is a
    // property of the vector, not of a row (this header's ACTIVITY INFERENCE
    // note).
    const double row_threshold =
        detail::ip_activity_threshold(lambda_i, slack_i, opts.activity_rel_tol);
    for (Index j = 0; j < mi; ++j) {
        const bool active = lambda_i(j) > opts.dual_tol && lambda_i(j) >= row_threshold;
        if (active) {
            out.ineq_active[static_cast<std::size_t>(j)] = 1;
            out.qp_working_set.add_ineq(j);
        }
    }

    out.bound_active.assign(static_cast<std::size_t>(n), 0);
    std::vector<BoundState> &states = out.qp_working_set.bound_state();
    // The two bound sides are SEPARATE dual populations (a variable can be
    // priced at one side only), so each sets its own scale. A caller that
    // prices an UNBOUNDED side -- z_lower(i) > 0 against lower(i) = -1e20 --
    // poisons its side's mu_hat with a ~1e20 product and drives that side's
    // whole verdict to FREE; that is the safe direction this header always
    // takes on a malformed hand-off, and such an input is a contradiction the
    // caller must not produce in the first place (nlp_model.h's +/-1e20
    // convention marks a side nothing can be active at).
    const double lower_threshold =
        detail::ip_activity_threshold(z_lower, x - lower, opts.activity_rel_tol);
    const double upper_threshold =
        detail::ip_activity_threshold(z_upper, upper - x, opts.activity_rel_tol);
    for (Index i = 0; i < n; ++i) {
        if (lower(i) == upper(i)) {
            // kFixed: sits at both bounds at once, not sign-constrained --
            // the same arbitrary-but-consistent +1 WarmStart::bound_active's
            // own note picks for a solve-derived object.
            out.bound_active[static_cast<std::size_t>(i)] = 1;
            states[static_cast<std::size_t>(i)] = BoundState::kFixed;
            continue;
        }
        const bool active_lower = z_lower(i) > opts.dual_tol && z_lower(i) >= lower_threshold;
        const bool active_upper = z_upper(i) > opts.dual_tol && z_upper(i) >= upper_threshold;
        if (active_lower && !active_upper) {
            out.bound_active[static_cast<std::size_t>(i)] = -1;
            states[static_cast<std::size_t>(i)] = BoundState::kAtLower;
        } else if (active_upper && !active_lower) {
            out.bound_active[static_cast<std::size_t>(i)] = 1;
            states[static_cast<std::size_t>(i)] = BoundState::kAtUpper;
        } else {
            // FREE: both the AMBIGUOUS (neither fires) and the CONTRADICTORY
            // (both fire) cases resolve the same safe way. The second is a
            // hand-off pricing BOTH sides of a variable that is not fixed --
            // impossible on a converged central path, where one side's price
            // is always at the barrier noise floor, and a statement the
            // inference declines to arbitrate when it does arrive. (Phase-7
            // Task 0 note: the pre-repair rule made this unreachable "under
            // any sane tolerance" by ALSO requiring both gaps to be small,
            // which cannot hold for lower(i) != upper(i). The repaired rule
            // reads duals only, so the case is reachable and is spelled out
            // rather than assumed away.)
            out.bound_active[static_cast<std::size_t>(i)] = 0;
            states[static_cast<std::size_t>(i)] = BoundState::kFree;
        }
    }

    out.funnel_width = -1.0;
    out.tr_radius = -1.0;
    out.primal_delta = -1.0;
    out.dual_mu = -1.0;
    // BY DESIGN, and NOT the condition Phase-5 Task 0 repaired in the driver:
    // there is no model here to hash in the first place, so the hash is
    // UNKNOWN rather than merely uncomputed. See WarmStart::structure_hash.
    out.structure_hash = 0;
    out.hot = nullptr; // never hot: no factorization exists to offer.
    out.valid = true;
    return out;
}

} // namespace hven::solvers
