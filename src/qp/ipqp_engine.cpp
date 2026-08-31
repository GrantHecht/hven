// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// ipqp_engine.cpp -- the IP-PMM interior-point QP tier's iteration.
//
// The header carries the contracts; this file carries the algorithm. Read
// include/hven/detail/qp/ipqp_engine.h first -- in particular its TASK-4
// BOUNDARY note, which says which parts of
// docs/notes/2026-08-m6-w1-ipqp-spec.md this file implements and which are
// later tasks'.
//
// THE SYSTEM, once, so every sign below can be checked against one statement
// rather than re-derived per block. Slack form, in KKTVector's block order
// [primals(n) | slacks(mi) | eq_lmults(me) | iq_lmults(mi)], with
// s = bi - Ai x >= 0 and lambda_i >= 0; variable bounds are CONDENSED into
// the (1,1) diagonal and add no rows:
//
//     [ H + rho I + Sigma_b     0        Ae'       Ai'    ]
//     [      0            Lam S^-1        0        -I     ]
//     [     Ae                  0     -delta I      0     ]
//     [     Ai                 -I          0     -delta I ]
//
// THE SLACK BLOCK'S UNKNOWN IS -ds, NOT ds, and that is forced by the matrix
// rather than chosen: the (iq, s) coupling is -I, so the inequality row reads
// `Ai dx - v_s - delta dyi`, while the linearized constraint
// `Ai x + s - bi - delta(yi - lambda_est) = 0` reads `Ai dx + ds - delta dyi`.
// Hence v_s = -ds. Stated here because it is the one place a sign error would
// produce a plausible-looking iteration that converges to the wrong point on
// exactly the problems with active inequalities.
//
// THE CONDENSED BOUND ALGEBRA, likewise once. With dL = x - l, dU = u - x and
// per-index complementarity targets (tl, tu),
//
//     dzl_i = [ tl_i - dL_i zl_i - zl_i dx_i ] / dL_i
//     dzu_i = [ tu_i - dU_i zu_i + zu_i dx_i ] / dU_i
//
// so eliminating them from stationarity contributes
// Sigma_b_i = zl_i/dL_i + zu_i/dU_i to the (1,1) diagonal (which is exactly
// ipqp_accumulate_bound_sigma) and moves `tl/dL - tu/dU` to the right-hand
// side. For a UNIFORM target tl = tu = mu that right-hand-side term is
// precisely the NEGATIVE of ipqp_math.h's mu-form bound gradient, which is
// why the RHS below is assembled by calling that kernel rather than by a
// hand-written loop -- and why the AFFINE (mu = 0) right-hand side needs no
// special case: every term of the mu-form gradient carries a factor mu.
//
// ONE DEVIATION FROM "UNIFORM": the Mehrotra corrector's target is
// tl_i = sigma*mu - dx_aff_i * dzl_aff_i, tu_i = sigma*mu + dx_aff_i *
// dzu_aff_i. The uniform part goes through the kernel; the second-order
// correction is a separate, explicitly written term. That is not a duplicate
// of the kernel's loop -- it is a term the kernel does not model.
//
// BOUND DAMPING IS STRUCTURALLY INERT ON THE DRIVER'S PATH, and it is worth
// saying so rather than leaving a reader to wonder whether the tier inherited
// an NLP-shaped bias. ipqp_accumulate_bound_barrier_gradient carries Ipopt's
// one-sided damping term (kIpqpKappaD * mu on a variable bounded on ONE side
// only). Under a FINITE trust-region radius the clamp-centred box makes every
// variable two-sided, so the damping indicator is identically zero and the
// kernel reduces to the exact linearization above. The term survives only for
// a caller who disables the radius entirely, which is the case it exists for.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/Core>
#include <fmt/format.h>

#include <hven/detail/globalization/inertia_regularization.h>
#include <hven/detail/interior/barrier_math.h>
#include <hven/detail/qp/ipqp_engine.h>
#include <hven/detail/qp/ipqp_math.h>
#include <hven/detail/qp/qp_engine.h>
#include <hven/detail/qp/ssn_engine.h>

namespace hven::solvers {

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

/// The four-way inertia reading the tier acts on.
///
/// REFINES `detail::inertia_verdict` (qp_engine.h), which is reused verbatim
/// for the kOk/kWrong decision and is the spec's own named precedent for the
/// perturbed-pivot policy. That helper collapses "the backend perturbed
/// pivots" and "no evidence state was observed" into one `kSuspect`; this tier
/// must keep them apart because spec section 2.2 gives them DIFFERENT
/// remedies -- a perturbed factorization describes a different matrix and is
/// answered by raising `delta`, while an unobservable state is answered by
/// downgrading the certificate and is `ipqp_final_inertia_read == 2`
/// (numerical), not `== 1` (indefinite). Splitting the existing verdict is
/// therefore a refinement of the precedent, not a second copy of it.
enum class InertiaRead { kOk, kWrong, kPerturbed, kUnreadable };

InertiaRead classify_inertia(const hven::linear::InertiaEvidence &e, Index expected_pos,
                             Index expected_neg) {
    if (e.state != hven::linear::InertiaEvidence::State::kObserved) {
        return InertiaRead::kUnreadable;
    }
    if (e.perturbed_pivots.has_value() && *e.perturbed_pivots != 0) {
        return InertiaRead::kPerturbed;
    }
    const detail::InertiaVerdict v = detail::inertia_verdict(e, expected_pos, expected_neg);
    switch (v) {
    case detail::InertiaVerdict::kOk:
        return InertiaRead::kOk;
    case detail::InertiaVerdict::kWrong:
        return InertiaRead::kWrong;
    case detail::InertiaVerdict::kSuspect:
        // The two kSuspect causes above are already handled, so what reaches
        // here is the SHORT-SUM case: n_pos + n_neg != dim, i.e. the
        // factorization reported a zero eigenvalue. That IS a disagreement
        // with the required signature (n + mi, me + mi, 0) taken from a
        // reading that WAS observed, so it is kWrong -- plan section 7 note
        // (h)'s "read and disagreed" -- and the ladder answers it by raising
        // the regularization, which is the one move that can remove a zero.
        return InertiaRead::kWrong;
    }
    return InertiaRead::kUnreadable;
}

/// Ruiz equilibration of a symmetric matrix stored as its UPPER TRIANGLE in
/// row-major CSR (spec 4.3).
///
/// Returns the accumulated diagonal `d` with `D K D` written back into `k`.
/// Both triangles' contributions to a row norm are recovered from the stored
/// half: one pass over the stored entries updates the norm of BOTH the row and
/// the column an entry sits in.
///
/// A SYMMETRIC diagonal scaling PRESERVES INERTIA (Sylvester's law), which is
/// what makes the section 2.2 inertia gate readable off the scaled factor at
/// all. The tier's residuals, multipliers and counters are all stated on the
/// UNSCALED quantities -- the scaling never leaves this function and its
/// inverse never has to be applied to anything the caller sees, because the
/// right-hand side is scaled going in and the solution scaled coming out.
void ruiz_equilibrate(SpMatRM &k, Vec &d, Vec &work) {
    const Index dim = k.rows();
    d.setOnes(dim);
    if (dim == 0) {
        return;
    }
    const int *outer = k.outerIndexPtr();
    const int *inner = k.innerIndexPtr();
    double *vals = k.valuePtr();

    for (Index sweep = 0; sweep < detail::kIpqpRuizSweeps; ++sweep) {
        work.setZero(dim);
        for (Index r = 0; r < dim; ++r) {
            for (int p = outer[r]; p < outer[r + 1]; ++p) {
                const double a = std::abs(vals[p]);
                const Index c = static_cast<Index>(inner[p]);
                if (a > work[r]) {
                    work[r] = a;
                }
                if (a > work[c]) {
                    work[c] = a;
                }
            }
        }
        double dev = 0.0;
        for (Index i = 0; i < dim; ++i) {
            const double m = work[i];
            dev = std::max(dev, std::abs(m - 1.0));
            // An all-zero row (structurally present, numerically empty) is
            // left alone: 1/sqrt(0) is not a scaling, it is a division by
            // zero, and a row with nothing in it has nothing to equilibrate.
            work[i] = (m > 0.0 && std::isfinite(m)) ? 1.0 / std::sqrt(m) : 1.0;
        }
        if (dev <= detail::kIpqpRuizTol) {
            return;
        }
        for (Index r = 0; r < dim; ++r) {
            for (int p = outer[r]; p < outer[r + 1]; ++p) {
                vals[p] *= work[r] * work[static_cast<Index>(inner[p])];
            }
        }
        d.array() *= work.array();
    }
}

/// The walk's own trust-region resolution, unchanged (`qp_types.h`'s
/// SolveOverrides sentinel convention): +inf on the override means "use the
/// engine's own radius".
double resolve_tr_radius(const QpOptions &opts, const SolveOverrides &ov) {
    return std::isinf(ov.tr_radius) && ov.tr_radius > 0.0 ? opts.tr_radius : ov.tr_radius;
}

/// `SsnEngine::validate_overrides`' rule, applied unchanged: a negative or NaN
/// radius would silently cross `lo_eff`/`up_eff` behind an assert a Release
/// build compiles out, and NaN in either regularizer is absorbable by no
/// downstream arithmetic.
void validate_overrides(const SolveOverrides &ov) {
    if (std::isnan(ov.tr_radius) || (ov.tr_radius < 0.0 && !std::isinf(ov.tr_radius))) {
        throw std::invalid_argument(
            fmt::format("IpqpEngine::solve: SolveOverrides::tr_radius ({}) must be the +inf "
                        "sentinel or >= 0",
                        ov.tr_radius));
    }
    if (std::isnan(ov.primal_delta) || std::isnan(ov.dual_mu)) {
        throw std::invalid_argument(
            "IpqpEngine::solve: SolveOverrides::primal_delta/dual_mu must not be NaN");
    }
}

/// The three fraction-to-boundary blocks, through `barrier_math.h`'s kernel
/// (reused verbatim -- spec 3.5's Verbatim row).
double step_to_boundary(Vec &v, Vec &dv, double tau, Index count) {
    if (count <= 0) {
        return 1.0;
    }
    return detail::max_step_to_boundary(v, dv, tau, static_cast<int>(count));
}

} // namespace

double IpqpResiduals::worst() const {
    return std::max(std::max(stationarity, primal_eq), std::max(primal_iq, complementarity));
}

// ---------------------------------------------------------------------------
// T4.a / T4.b -- the box and its domain gate
// ---------------------------------------------------------------------------

IpqpBox make_ipqp_box(const QpProblem &qp, double radius) {
    if (std::isnan(radius) || radius < 0.0) {
        throw std::invalid_argument(
            fmt::format("make_ipqp_box: trust-region radius ({}) must be >= 0 or +inf", radius));
    }
    const Index n = qp.n();
    if (qp.lower.size() != n || qp.upper.size() != n) {
        throw std::invalid_argument(
            fmt::format("make_ipqp_box: lower/upper have sizes {}/{}, expected {}", qp.lower.size(),
                        qp.upper.size(), n));
    }

    IpqpBox box;
    box.radius = radius;
    box.centre.resize(n);
    box.lo_eff.resize(n);
    box.up_eff.resize(n);

    // A radius of +inf disables the window EXACTLY: lo_eff == lower and
    // up_eff == upper, bit for bit, with no arithmetic performed on them.
    // `c - Delta` at Delta == +inf would be -inf rather than `lower`, which is
    // a different (and, on a +/-1e20-sentinel problem, differently
    // classified) value.
    const bool windowed = std::isfinite(radius);

    for (Index i = 0; i < n; ++i) {
        const double lo = qp.lower(i);
        const double up = qp.upper(i);
        if (std::isnan(lo) || std::isnan(up)) {
            throw std::invalid_argument(
                fmt::format("make_ipqp_box: bound {} is NaN (lower={}, upper={})", i, lo, up));
        }
        // THE CENTRE RULE IS refine_on_face's OWN, character for character
        // (src/qp/qp_engine.cpp's gate: `c = min(max(0, lo), up)`). See
        // IpqpBox's doc comment for why sharing it is load-bearing rather
        // than tidy.
        const double c = std::min(std::max(0.0, lo), up);
        box.centre(i) = c;
        box.lo_eff(i) = windowed ? std::max(lo, c - radius) : lo;
        box.up_eff(i) = windowed ? std::min(up, c + radius) : up;
        if (box.zero_width_index < 0 && !(box.lo_eff(i) < box.up_eff(i))) {
            box.zero_width_index = i;
        }
    }
    return box;
}

bool IpqpBounds::in_domain() const { return zero_width_index < 0; }

IpqpBounds make_ipqp_bounds(const IpqpBox &box) {
    // CLAUDE.md section 4, at a PUBLIC boundary: this function takes an
    // IpqpBox by reference, so a hand-built box (task 6's routing chain will
    // build one, and tests already do) can present blocks of different
    // lengths. The loop below indexes `upper(i)` with `lower`'s length;
    // Eigen's own assert is compiled out under NDEBUG, so without this check
    // a mismatch is an out-of-bounds READ in Release, silently.
    if (box.lo_eff.size() != box.up_eff.size() || box.centre.size() != box.lo_eff.size()) {
        throw std::invalid_argument(
            fmt::format("make_ipqp_bounds: IpqpBox blocks disagree -- centre {}, lo_eff {}, "
                        "up_eff {}; all three must have the same length",
                        box.centre.size(), box.lo_eff.size(), box.up_eff.size()));
    }
    IpqpBounds b;
    b.lower = box.lo_eff;
    b.upper = box.up_eff;
    b.zero_width_index = box.zero_width_index;
    const Index n = b.lower.size();
    for (Index i = 0; i < n; ++i) {
        if (detail::ipqp_has_lower(b.lower(i))) {
            ++b.num_lower;
        }
        if (detail::ipqp_has_upper(b.upper(i))) {
            ++b.num_upper;
        }
    }
    return b;
}

// ---------------------------------------------------------------------------
// The engine
// ---------------------------------------------------------------------------

/// Every vector one solve needs, allocated once at entry.
///
/// A plain member-per-block struct rather than a set of engine members: the
/// tier is re-entrant per solve and holding these across solves would make the
/// engine's state depend on the previous subproblem's SIZE, which is exactly
/// the class of coupling `IpqpKktLayout`'s structure key exists to make
/// explicit. The cache that IS worth holding across solves -- the symbolic
/// analysis and the scatter plan -- is held, on the engine.
struct IpqpEngine::Workspace {
    Index n = 0, me = 0, mi = 0, dim = 0;

    // The iterate.
    Vec x, s, ye, yi, zl, zu;
    // The proximal estimates (spec 3.2).
    Vec zeta, lam_est_e, lam_est_i;
    // Bound distances; +inf at an ABSENT side, so the fraction-to-boundary
    // kernel can never select one and no guard is needed inside it.
    Vec dL, dU;

    // Per-iteration derived quantities.
    Vec grad;  // n:  H x + g + Ae' ye + Ai' yi   (NO bound duals -- see banner)
    Vec sigma; // n:  the condensed bound curvature
    Vec r_pe;  // me: Ae x - be
    Vec r_pi;  // mi: Ai x + s - bi
    Vec bgrad; // n:  the mu-form bound gradient scratch

    // The linear system.
    Vec rhs, sol, dscale, dsq, ruiz_work;

    // The three `max(1, ...)` folds the relative residual divides by, kept so
    // the SCHEDULE'S gate can be stated on the same scale as the STOPPING
    // rule. Two absolute quantities compared against each other is what made
    // the first version of that gate scale-dependent -- see
    // kIpqpRegGateContract.
    double scale_d = 1.0, scale_pe = 1.0, scale_pi = 1.0;

    // Steps.
    Vec dx, ds, dye, dyi, dzl, dzu;
    Vec dx_a, ds_a, dyi_a, dzl_a, dzu_a;

    // Fraction-to-boundary scratch. `detail::max_step_to_boundary` takes
    // NON-CONST `Eigen::Ref`s, so every block handed to it needs a mutable
    // vector of its own -- including the direction, which is why `ft_dx` is
    // here rather than a fresh copy of `dx` twice per iteration.
    Vec neg_dx, ft_dx, ft_dL, ft_dU;

    // Trial point for mu_aff.
    Vec tx, ts, tyi, tzl, tzu;

    void allocate(Index n_, Index me_, Index mi_) {
        n = n_;
        me = me_;
        mi = mi_;
        dim = n + 2 * mi + me;

        x.setZero(n);
        s.setZero(mi);
        ye.setZero(me);
        yi.setZero(mi);
        zl.setZero(n);
        zu.setZero(n);
        zeta.setZero(n);
        lam_est_e.setZero(me);
        lam_est_i.setZero(mi);
        dL.setZero(n);
        dU.setZero(n);
        grad.setZero(n);
        sigma.setZero(n);
        r_pe.setZero(me);
        r_pi.setZero(mi);
        bgrad.setZero(n);
        rhs.setZero(dim);
        sol.setZero(dim);
        dscale.setOnes(dim);
        dsq.setOnes(dim);
        ruiz_work.setZero(dim);
        dx.setZero(n);
        ds.setZero(mi);
        dye.setZero(me);
        dyi.setZero(mi);
        dzl.setZero(n);
        dzu.setZero(n);
        dx_a.setZero(n);
        ds_a.setZero(mi);
        dyi_a.setZero(mi);
        dzl_a.setZero(n);
        dzu_a.setZero(n);
        neg_dx.setZero(n);
        ft_dx.setZero(n);
        ft_dL.setZero(n);
        ft_dU.setZero(n);
        tx.setZero(n);
        ts.setZero(mi);
        tyi.setZero(mi);
        tzl.setZero(n);
        tzu.setZero(n);
    }
};

const QpOptions &IpqpEngine::options() const { return opts_; }

IpqpEngine::IpqpEngine(const QpOptions &opts) : opts_(opts) {
    // THE TIER'S OWN BACKEND OPTIONS, NOT detail::sqp_kkt_options() (spec
    // 4.4). Default-constructed and left alone, which is what makes this
    // commit Accelerate-safe and free of an IPARM-SURFACE label: on Apple
    // Accelerate a non-default `weighted_matching`, `matrix_scaling`,
    // `pivot_strategy`, `factorization_algorithm`, `solve_parallelism` or a
    // positive `cnr_threads` throws std::invalid_argument at construction, and
    // every field that maps to a Pardiso iparm slot (`pivot_perturb_exp` ->
    // iparm[9], `max_refinement_iters` -> iparm[7], the ordering) stays at the
    // value pardisoinit itself chose. Nothing here moves an iparm surface, so
    // CLAUDE.md section 6's labelling rule has nothing to bind on.
    //
    // The DEFAULT-CONSTRUCTED KktFactorization member does exactly this; the
    // constructor body is the place to record WHY it is left alone, since
    // "no code" is otherwise indistinguishable from "not thought about".
}

void IpqpEngine::attach_ledger(Ledger *ledger, std::string label_prefix) {
    ledger_ = ledger;
    label_prefix_ = std::move(label_prefix);
    solve_counter_ = 0;
}

IpqpResult IpqpEngine::solve(const QpProblem &qp, const IpqpSeed *seed, const IpqpOptions &iopts,
                             const SolveOverrides &overrides) {
    // --- boundary validation (CLAUDE.md section 4) -------------------------
    qp.validate();
    validate_overrides(overrides);
    {
        // THE SAME CODE THE DRIVER RUNS, not a second copy of the same rules:
        // `validate_sqp_options` owns every IpqpOptions band (task 1), and a
        // re-derivation here could drift from it silently. This engine is
        // reachable without a driver (tests today, and any future direct
        // consumer), so the check must happen here too -- but it happens by
        // CALLING the owner, with the tier's options dropped into an otherwise
        // default SqpOptions whose remaining fields are valid by construction.
        SqpOptions probe;
        probe.ipqp = iopts;
        validate_sqp_options(probe);
    }
    if (seed != nullptr) {
        throw std::invalid_argument(
            "IpqpEngine::solve: a warm IpqpSeed is M6 W1 task 7's (spec section 5: the "
            "strict-positivity repair, the Skajaa-Andersen-Ye centrality shift, the mu_0 "
            "clamp and the warm-kill rule). This task refuses one rather than consuming an "
            "unrepaired primal-only near-solution, which the specification records as "
            "possibly WORSE THAN NEUTRAL; pass nullptr for a cold solve.");
    }

    const Index n = qp.n();
    const Index me = qp.me();
    const Index mi = qp.mi();

    IpqpResult out;

    // ONE ROW PER NON-THROWING SOLVE, and that includes a DECLINE (I9). The
    // emitter lives here, above the domain gate, because the gate returns
    // early and a decline is an OUTCOME, not a non-event: task 6's routing
    // chain wants to see the subproblems the tier refused just as much as the
    // ones it solved, and a decline that consumed no label would make the
    // ledger's labels stop counting solves.
    auto emit_ledger = [&]() {
        if (ledger_ == nullptr) {
            return;
        }
        SolveRecord rec;
        rec.label = fmt::format("{}{}", label_prefix_, solve_counter_++);
        rec.warm = false; // task 7 owns the warm path; a task-4 solve is cold.
        rec.status = out.status;
        // THE QP-SHAPED PROJECTION (see attach_ledger's contract). Only the
        // three fields that mean the same thing on all three kernels are
        // filled; the rest stay at their defaults rather than being given a
        // plausible-looking value this tier did not measure. On a decline all
        // three are 0, which is the correct reading -- the tier never ran.
        rec.counters.minor_iters = out.counters.ipqp_iters;
        rec.counters.factorizations = out.counters.ipqp_factorizations;
        rec.counters.symbolic_analyses = out.counters.ipqp_symbolic_analyses;
        ledger_->record(std::move(rec));
    };

    out.box = make_ipqp_box(qp, resolve_tr_radius(opts_, overrides));
    const IpqpBounds bounds = make_ipqp_bounds(out.box);

    // --- T4.b: the domain gate --------------------------------------------
    if (!bounds.in_domain()) {
        // A DECLINE, NOT AN ESCAPE. The tier never ran, so nothing here
        // touches `ipqp_escapes` or the K = 3 retirement tally, and every
        // other counter stays at its default. The routing chain (task 6)
        // sends the subproblem to the walk, which solves an exact pin
        // exactly.
        out.declined_pinned = true;
        out.status = QpStatus::kNumericalError;
        out.escape_reason = IpqpEscape::kNone;
        out.counters.ipqp_declined_pinned = 1;
        emit_ledger();
        return out;
    }

    Workspace w;
    w.allocate(n, me, mi);

    const KktFactorization::Counters before = kkt_.counters();

    // --- budgets (spec 3.4) -----------------------------------------------
    //
    // `ipqp_max_iter`'s sentinel discipline is `QpOptions::max_iter`'s own,
    // through the walk's own helper so the two cannot derive different caps
    // for the same subproblem. The HARD CAP is then the binding one at the
    // shipped defaults (60 against a derived 500+), which is exactly what
    // Amendment C intends: the hard cap is the budget of LAST RESORT.
    //
    // BOTH CAPS BIND, so a caller who raises `ipqp_max_iter` alone still gets
    // 60 -- the `min` is the point, not an accident, and `ipqp_hard_iter_cap`
    // has to move too.
    //
    // The FACTORIZATION cap's sentinel is three times the budget ABOVE, i.e.
    // three times `min(derived, hard cap)` rather than three times the
    // size-derived number: one iteration costs one factorization plus ladder
    // rungs, and the budget the iterations actually run under is the clamped
    // one.
    const Index derived = detail::effective_qp_max_iter(qp, iopts.ipqp_max_iter);
    const Index iter_budget = std::min(derived, iopts.ipqp_hard_iter_cap);
    const Index fact_budget = iopts.ipqp_max_factorizations > 0
                                  ? iopts.ipqp_max_factorizations
                                  : detail::kIpqpFactorizationsPerIter * iter_budget;

    // --- cold start (spec 5.6) --------------------------------------------
    //
    // x_0 is the SQP's own start point -- the step-space origin -- which the
    // box already carries as its CENTRE (`clamp(0, lower, upper)`), so the
    // tier's start point and the tier's window are derived from one value
    // rather than two that could disagree. Pushing it into the strict
    // interior is Ipopt's bound_push/bound_frac rule; the box has strictly
    // positive width everywhere (the domain gate above), so the push always
    // lands strictly inside.
    w.x = out.box.centre;
    for (Index i = 0; i < n; ++i) {
        const double lo = bounds.lower(i);
        const double up = bounds.upper(i);
        const bool hl = detail::ipqp_has_lower(lo);
        const bool hu = detail::ipqp_has_upper(up);
        if (hl) {
            double p = detail::kIpqpBoundPushAbs * std::max(1.0, std::abs(lo));
            if (hu) {
                p = std::min(p, detail::kIpqpBoundPushRel * (up - lo));
            }
            w.x(i) = std::max(w.x(i), lo + p);
        }
        if (hu) {
            double p = detail::kIpqpBoundPushAbs * std::max(1.0, std::abs(up));
            if (hl) {
                p = std::min(p, detail::kIpqpBoundPushRel * (up - lo));
            }
            w.x(i) = std::min(w.x(i), up - p);
        }
    }

    const double mu0 = iopts.ipqp_init_mu;
    if (mi > 0) {
        w.s = (qp.bi - qp.Ai * w.x).cwiseMax(detail::kIpqpSlackInit);
        w.yi = mu0 * w.s.cwiseInverse();
    }
    // THE COLD DUALS ARE PLACED SO THAT EVERY COMPLEMENTARY PAIR EQUALS mu_0
    // EXACTLY. Spec 5.6 words this as "lambda_0 = 0 shifted positive by the
    // same [Skajaa-Andersen-Ye] rule"; the SAY shift's job is to push every
    // pair toward mu_0, and from a zero multiplier the shift that achieves it
    // is mu_0 / distance in closed form. Writing it that way rather than
    // shifting-then-measuring makes `mu_measured == ipqp_init_mu` at the cold
    // start a property of the code instead of an approximation of it, which
    // is what the cold-start determinism pin asserts.
    for (Index i = 0; i < n; ++i) {
        if (detail::ipqp_has_lower(bounds.lower(i))) {
            w.zl(i) = mu0 / (w.x(i) - bounds.lower(i));
        }
        if (detail::ipqp_has_upper(bounds.upper(i))) {
            w.zu(i) = mu0 / (bounds.upper(i) - w.x(i));
        }
    }
    w.zeta = w.x;
    w.lam_est_e = w.ye;
    w.lam_est_i = w.yi;

    const Index npairs = mi + bounds.num_lower + bounds.num_upper;

    // --- the (rho, delta) schedule (spec 3.2) -----------------------------
    //
    // TWO LEVELS, KEPT APART, because the counters distinguish them and the
    // certification factorization drops to one of them:
    //   rho_sched / delta_sched -- what the GATED SCHEDULE has decayed to.
    //   rho_floor               -- the INERTIA-DEMANDED monotone floor
    //                              (section 2.2 item 3), which starts at 0 and
    //                              only ever rises. `ipqp_rho_demanded_max`
    //                              is its high-water mark, so a convex
    //                              subproblem -- where the ladder never fires
    //                              -- reports 0 there, which is the
    //                              convex-inertness pin.
    // The value a factorization actually carries is max(rho_sched, rho_floor);
    // an iteration where those differ is one taken AT ELEVATED RHO.
    double rho_sched = iopts.ipqp_rho_init;
    double delta_sched = iopts.ipqp_delta_init;
    double rho_floor = 0.0;
    double rho_demanded_last = 0.0;
    // The regularized relative residual at the last estimate advance -- the
    // contraction gate's reference. NaN until the first iteration measures it,
    // so the gate cannot fire before there is anything to contract against.
    double reg_gate_ref = std::numeric_limits<double>::quiet_NaN();

    // Section 4.1's compute()-vs-refactorize() decision, and with it the
    // section 7 note (a) verify-once discipline.
    if (!iopts.ipqp_hoist_symbolic) {
        analyzed_ = false;
    }
    bool first_factorization = true;

    const Index expect_pos = n + mi;
    const Index expect_neg = me + mi;

    IpqpEscape escape = IpqpEscape::kNone;
    bool converged = false;
    // Raised by `factorize_once` when the factorization cap refused a call, so
    // a caller can tell "no factorization was taken" from any inertia verdict.
    bool fact_budget_hit = false;
    double mu_meas = mu0;

    // ---- the local operations the loop below is written in terms of -------

    // Refresh dL/dU from the current x. +inf at an absent side (see Workspace).
    auto refresh_distances = [&]() {
        for (Index i = 0; i < n; ++i) {
            w.dL(i) = detail::ipqp_has_lower(bounds.lower(i)) ? w.x(i) - bounds.lower(i) : kInf;
            w.dU(i) = detail::ipqp_has_upper(bounds.upper(i)) ? bounds.upper(i) - w.x(i) : kInf;
        }
    };

    // The measured complementarity of an arbitrary point, through the two
    // ipqp_math.h reductions (the slack one is deliberately the tier's own --
    // see that header's banner).
    auto complementarity = [&](const Vec &px, const Vec &ps, const Vec &pyi, const Vec &pzl,
                               const Vec &pzu, double &avg, double &lo, double &hi) {
        detail::ipqp_slack_complementarity(ps, pyi, mi, avg, lo, hi);
        detail::ipqp_augment_bound_complementarity(px, bounds.lower, bounds.upper, pzl, pzu, n, mi,
                                                   avg, lo, hi);
    };

    // T4.d -- the relative KKT residual, on the UNREGULARIZED problem.
    auto residuals = [&]() {
        IpqpResiduals r;
        const Vec hx = qp.H.selfadjointView<Eigen::Upper>() * w.x;
        w.grad = hx + qp.g;
        double scale_d =
            std::max({1.0, hx.lpNorm<Eigen::Infinity>(), qp.g.lpNorm<Eigen::Infinity>()});
        if (me > 0) {
            const Vec t = qp.Ae.transpose() * w.ye;
            w.grad += t;
            scale_d = std::max(scale_d, t.lpNorm<Eigen::Infinity>());
        }
        if (mi > 0) {
            const Vec t = qp.Ai.transpose() * w.yi;
            w.grad += t;
            scale_d = std::max(scale_d, t.lpNorm<Eigen::Infinity>());
        }
        // The z-FORM bound term (never the mu-form): this is the residual a
        // convergence decision consumes, and ipqp_math.h's own note records
        // why the two forms are not interchangeable there.
        Vec rd = w.grad;
        detail::ipqp_accumulate_bound_dual_terms(bounds.lower, bounds.upper, w.zl, w.zu, n, rd);
        scale_d =
            std::max({scale_d, w.zl.lpNorm<Eigen::Infinity>(), w.zu.lpNorm<Eigen::Infinity>()});
        w.scale_d = scale_d;
        r.stationarity = rd.lpNorm<Eigen::Infinity>() / scale_d;

        if (me > 0) {
            const Vec aex = qp.Ae * w.x;
            w.r_pe = aex - qp.be;
            const double sc =
                std::max({1.0, aex.lpNorm<Eigen::Infinity>(), qp.be.lpNorm<Eigen::Infinity>()});
            w.scale_pe = sc;
            r.primal_eq = w.r_pe.lpNorm<Eigen::Infinity>() / sc;
        }
        if (mi > 0) {
            const Vec aix = qp.Ai * w.x;
            w.r_pi = aix + w.s - qp.bi;
            const double sc =
                std::max({1.0, aix.lpNorm<Eigen::Infinity>(), qp.bi.lpNorm<Eigen::Infinity>(),
                          w.s.lpNorm<Eigen::Infinity>()});
            w.scale_pi = sc;
            r.primal_iq = w.r_pi.lpNorm<Eigen::Infinity>() / sc;
        }

        double avg = 0.0, lo = 0.0, hi = 0.0;
        complementarity(w.x, w.s, w.yi, w.zl, w.zu, avg, lo, hi);
        mu_meas = npairs > 0 ? avg : 0.0;
        // RELATIVE, against the MULTIPLIER scale: a complementarity product is
        // distance * multiplier, so dividing by the largest multiplier turns
        // the test into "how close to the boundary is the pair", which is
        // scale-free in the objective the way the stationarity fold above is.
        const double sc =
            std::max({1.0, w.yi.lpNorm<Eigen::Infinity>(), w.zl.lpNorm<Eigen::Infinity>(),
                      w.zu.lpNorm<Eigen::Infinity>()});
        r.complementarity = std::max(std::abs(hi), std::abs(lo)) / sc;
        return r;
    };

    // The four diagonal families, written by assignment from
    // `primal_diag_source()` (never read-modify-write), each pre-multiplied by
    // its own Ruiz factor so a ladder rung stays an O(n) assignment instead of
    // a re-scatter. `dsq` is all ones when equilibration is off, and a
    // multiplication by exactly 1.0 is exact, so the two arms are bit-identical
    // in that case rather than merely close.
    auto write_diagonals = [&](double rho, double delta) {
        double *vals = kkt_.matrix().valuePtr();
        const std::vector<double> &src = layout_.primal_diag_source();
        for (Index i = 0; i < n; ++i) {
            vals[layout_.primal_diag_slot(i)] =
                (src[static_cast<std::size_t>(i)] + rho + w.sigma(i)) * w.dsq(i);
        }
        for (Index j = 0; j < mi; ++j) {
            vals[layout_.slack_diag_slot(j)] = (w.yi(j) / w.s(j)) * w.dsq(n + j);
        }
        for (Index r = 0; r < me; ++r) {
            vals[layout_.eq_pivot_slot(r)] = -delta * w.dsq(n + mi + r);
        }
        for (Index j = 0; j < mi; ++j) {
            vals[layout_.iq_pivot_slot(j)] = -delta * w.dsq(n + mi + me + j);
        }
    };

    // I6 / spec 3.4's SEPARATE FACTORIZATION CAP, enforced BEFORE every
    // factorization rather than once per iteration. A ladder rung is a
    // factorization like any other, so a cap checked only at the top of the
    // loop is not a cap: measured at cap 1 on a strongly indefinite Hessian,
    // the first ladder paid three factorizations before anything stopped it.
    // The check returns false and raises the flag; every call site consults
    // the flag rather than the reading, because "no factorization was taken"
    // is not an inertia verdict.
    auto factorize_once = [&]() {
        if (kkt_.counters().factorize_count - before.factorize_count >= fact_budget) {
            fact_budget_hit = true;
            return false;
        }
        if (!analyzed_) {
            kkt_.compute();
            // A backend SYMBOLIC failure does not propagate out of compute();
            // it is reported as InvalidInput. Leaving `analyzed_` false on
            // that path is what keeps the next entry from calling
            // refactorize() against an analysis that does not exist.
            analyzed_ = (kkt_.info() != Eigen::InvalidInput);
        } else if (first_factorization) {
            // THE ONE-TIME O(nnz) PAYMENT per tier entry (plan section 7 note
            // a): the first factorization of an entry re-checks that the
            // buffer still carries the analyzed pattern, and every later one
            // in the same entry declares it. The declaration is licensed by a
            // named mechanism, not by hope -- IpqpKktLayout is the only writer
            // into this buffer and it writes from a fixed plan.
            kkt_.refactorize(KktFactorization::PatternCheck::kVerify);
        } else {
            kkt_.refactorize(KktFactorization::PatternCheck::kAssumeAnalyzed);
        }
        first_factorization = false;
        return true;
    };

    // Assemble the section 3.1 system at (rho, delta) from scratch: refresh the
    // condensed bound curvature, re-scatter H/Ae/Ai through the layout's plan,
    // write the four diagonal families, and (when equilibration is on) compute
    // and apply the Ruiz diagonal.
    //
    // THE RUIZ DIAGONAL IS FIXED FOR THE WHOLE LADDER, deliberately. A rung
    // rewrites only the four diagonal families, each pre-multiplied by its own
    // `dsq`, so a rung stays an O(n) assignment; recomputing the equilibration
    // per rung would move the matrix under the ladder and the ladder's
    // monotone comparison would no longer be about one system.
    auto assemble = [&](double rho, double delta) {
        w.sigma.setZero();
        detail::ipqp_accumulate_bound_sigma(w.x, bounds.lower, bounds.upper, w.zl, w.zu, n,
                                            w.sigma);

        const bool relaid = layout_.sync(qp.H, qp.Ae, qp.Ai, n, me, mi, kkt_.matrix());
        if (relaid) {
            analyzed_ = false;
        }

        w.dsq.setOnes(w.dim);
        write_diagonals(rho, delta);
        if (iopts.ipqp_ruiz) {
            ruiz_equilibrate(kkt_.matrix(), w.dscale, w.ruiz_work);
            w.dsq = w.dscale.array().square();
        } else {
            w.dscale.setOnes(w.dim);
        }
    };

    // Returns the reading, or -- when the factorization budget refused the
    // call -- `kUnreadable` with `fact_budget_hit` raised. Callers must test
    // the flag FIRST: a budget stop is `kBudget`, never a numerical or
    // indefinite verdict about a factorization that never ran.
    auto factor_and_read = [&]() {
        if (!factorize_once()) {
            return InertiaRead::kUnreadable;
        }
        return classify_inertia(kkt_.inertia_evidence(), expect_pos, expect_neg);
    };

    // EXACTLY ONE FACTORIZATION AND ONE READING (spec 2.2 item 4's own cost
    // statement: "+1 factorization per certified guarded solve"). No ladder,
    // and -- fix round 1, C0b -- no perturbed retry either.
    //
    // The ABSENCE OF A LADDER is the whole point of having this separate from
    // `factorize_with_ladder`: a certification read that CLIMBED would raise
    // `rho` until the inertia came back right and then report the certificate
    // as standing, which is precisely how a saddle point gets certified as a
    // minimum.
    //
    // THE PERTURBED RETRY WENT for two reasons that point the same way. It
    // made the "one extra factorization" two, which is a cost the spec states
    // as a number; and a perturbed reading is not evidence about the
    // assembled matrix AT ALL (section 2.2's evidence-failure policy), so the
    // honest outcome is the same as an unreadable one -- plan section 7 note
    // (h)'s `ipqp_final_inertia_read == 2`, numerical -- rather than a second
    // question whose clean answer could stand a certificate the first answer
    // could not support.
    auto factorize_and_read_once = [&](double rho, double delta) {
        assemble(rho, delta);
        return factor_and_read();
    };

    // Assemble at (rho, delta) and factorize, climbing the Wachter-Biegler
    // ladder until the inertia is the section 4.1 target or the ceiling is
    // reached. Returns the reading the caller must act on. `rho`/`delta` are
    // in/out, so the caller sees where the ladder stopped.
    auto factorize_with_ladder = [&](double &rho, double &delta) {
        assemble(rho, delta);

        for (;;) {
            const InertiaRead read = factor_and_read();
            if (fact_budget_hit || read == InertiaRead::kOk || read == InertiaRead::kUnreadable) {
                return read;
            }
            // The ceiling test carries SsnEngine::escalate_prox's relative
            // slack verbatim, and for its reason: repeated multiplication does
            // not reproduce 1e6 exactly (the sequence ends at
            // 999999.9999999998), so an exact `>=` guard grants a final rung
            // that raises the regularization by 2.3e-10 relative and buys a
            // whole numeric factorization for it.
            const double cap = iopts.ipqp_reg_max * (1.0 - detail::kSsnProxCapSlack);
            if (read == InertiaRead::kPerturbed) {
                // Spec 2.2's evidence-failure policy: a perturbed
                // factorization describes a DIFFERENT matrix, so its inertia
                // is not evidence about this one. The remedy is a larger
                // DUAL shift, never reading it as right.
                if (delta >= cap) {
                    // I8: THE TERMINAL REJECTION IS STILL A REJECTION. This
                    // factorization was refused on evidence the tier could not
                    // use, exactly like every rung before it; returning
                    // without counting it lost one rejection per exhausted
                    // ladder.
                    ++out.counters.ipqp_inertia_retries;
                    return read;
                }
                delta = std::min(delta * detail::kIpqpRhoGrowth, iopts.ipqp_reg_max);
                if (delta >= cap) {
                    delta = iopts.ipqp_reg_max;
                }
            } else {
                if (rho >= cap) {
                    ++out.counters.ipqp_inertia_retries; // I8, as above.
                    return read;
                }
                const double next =
                    std::max(rho * detail::kIpqpRhoGrowth, detail::kIpqpRhoLadderInit);
                rho = std::min(next, iopts.ipqp_reg_max);
                if (rho >= cap) {
                    rho = iopts.ipqp_reg_max;
                }
                rho_floor = std::max(rho_floor, rho);
                rho_demanded_last = rho;
            }
            ++out.counters.ipqp_inertia_retries;
            ++out.counters.ipqp_reg_increases;
            // (The emergency `> 2 * fact_budget` guard that used to sit here
            // went with fix round 1's I6: `factorize_once` now refuses a
            // factorization the budget cannot pay for, so the ladder can no
            // longer outrun the cap and there is nothing left for a second,
            // looser guard to catch.)
            //
            // A RUNG IS AN ASSIGNMENT, NOT A RE-SCATTER: `dsq` and the layout's
            // `primal_diag_source()` snapshot are both still the ones
            // `assemble()` established, so writing the four diagonal families
            // at the new (rho, delta) is all a rung costs beyond its
            // factorization.
            write_diagonals(rho, delta);
        }
    };

    // One backend solve: scale the right-hand side in, unscale the solution
    // out. `D K D u = D b` gives `u = D^-1 v` for `K v = b`, so the recovered
    // step is `D u` -- and every quantity that leaves this lambda is on the
    // caller's own scale, which is spec 4.3's non-negotiable clause.
    auto solve_system = [&]() {
        if (iopts.ipqp_ruiz) {
            w.rhs.array() *= w.dscale.array();
        }
        kkt_.solve(w.rhs, w.sol);
        if (iopts.ipqp_ruiz) {
            w.sol.array() *= w.dscale.array();
        }
    };

    // Build the right-hand side for one step. `mu_t` is the uniform
    // complementarity target (0 on the affine step); `corrector` adds
    // Mehrotra's second-order term from the affine step already in hand.
    auto build_rhs = [&](double rho, double delta, double mu_t, bool corrector) {
        w.bgrad.setZero();
        detail::ipqp_accumulate_bound_barrier_gradient(w.x, bounds.lower, bounds.upper, mu_t, n,
                                                       w.bgrad);
        w.rhs.head(n) = -(w.grad + rho * (w.x - w.zeta) + w.bgrad);
        if (corrector) {
            for (Index i = 0; i < n; ++i) {
                if (detail::ipqp_has_lower(bounds.lower(i))) {
                    w.rhs(i) -= (w.dx_a(i) * w.dzl_a(i)) / w.dL(i);
                }
                if (detail::ipqp_has_upper(bounds.upper(i))) {
                    w.rhs(i) -= (w.dx_a(i) * w.dzu_a(i)) / w.dU(i);
                }
            }
        }
        if (mi > 0) {
            // (y/s) v_s - dyi = y - target/s, target = mu_t - ds_aff * dyi_aff.
            w.rhs.segment(n, mi) = w.yi - mu_t * w.s.cwiseInverse();
            if (corrector) {
                w.rhs.segment(n, mi).array() += (w.ds_a.array() * w.dyi_a.array()) / w.s.array();
            }
        }
        if (me > 0) {
            w.rhs.segment(n + mi, me) = -(w.r_pe - delta * (w.ye - w.lam_est_e));
        }
        if (mi > 0) {
            w.rhs.segment(n + mi + me, mi) = -(w.r_pi - delta * (w.yi - w.lam_est_i));
        }
    };

    // Recover (dx, ds, dye, dyi, dzl, dzu) from the solved vector.
    auto recover = [&](double mu_t, bool corrector, Vec &dx, Vec &ds, Vec &dye, Vec &dyi, Vec &dzl,
                       Vec &dzu) {
        dx = w.sol.head(n);
        // THE SLACK BLOCK'S UNKNOWN IS -ds (see this file's banner).
        if (mi > 0) {
            ds = -w.sol.segment(n, mi);
            dyi = w.sol.segment(n + mi + me, mi);
        }
        if (me > 0) {
            dye = w.sol.segment(n + mi, me);
        }
        dzl.setZero(n);
        dzu.setZero(n);
        for (Index i = 0; i < n; ++i) {
            if (detail::ipqp_has_lower(bounds.lower(i))) {
                double tl = mu_t;
                if (corrector) {
                    tl -= w.dx_a(i) * w.dzl_a(i);
                }
                dzl(i) = (tl - w.dL(i) * w.zl(i) - w.zl(i) * dx(i)) / w.dL(i);
            }
            if (detail::ipqp_has_upper(bounds.upper(i))) {
                double tu = mu_t;
                if (corrector) {
                    tu += w.dx_a(i) * w.dzu_a(i);
                }
                dzu(i) = (tu - w.dU(i) * w.zu(i) + w.zu(i) * dx(i)) / w.dU(i);
            }
        }
    };

    auto steps_to_boundary = [&](const Vec &dx, Vec &ds, Vec &dyi, Vec &dzl, Vec &dzu,
                                 double &alpha_p, double &alpha_d) {
        const double tau = iopts.ipqp_tau;
        w.neg_dx = -dx;
        w.ft_dx = dx;
        w.ft_dL = w.dL;
        w.ft_dU = w.dU;
        alpha_p = std::min({step_to_boundary(w.s, ds, tau, mi),
                            step_to_boundary(w.ft_dL, w.ft_dx, tau, n),
                            step_to_boundary(w.ft_dU, w.neg_dx, tau, n)});
        alpha_d =
            std::min({step_to_boundary(w.yi, dyi, tau, mi), step_to_boundary(w.zl, dzl, tau, n),
                      step_to_boundary(w.zu, dzu, tau, n)});
    };

    // --- the iteration (spec 3.1) -----------------------------------------

    for (;;) {
        refresh_distances();
        const IpqpResiduals res = residuals();
        out.residuals = res;

        if (!std::isfinite(res.worst())) {
            escape = IpqpEscape::kNumerical;
            break;
        }

        // THE STOPPING RULE (spec 2.3 step 1 / 3.4): the QP layer's own
        // relative tolerances, loosened by `ipqp_converge_slack`. The tier
        // does not chase the last two decades -- that is tier 3's job -- and
        // the slack factor is what makes the division of labour a setting
        // rather than a hard-coded convention.
        const double opt_target = opts_.opt_tol * iopts.ipqp_converge_slack;
        const double feas_target = opts_.feas_tol * iopts.ipqp_converge_slack;
        if (res.stationarity <= opt_target && res.primal_eq <= feas_target &&
            res.primal_iq <= feas_target && res.complementarity <= opt_target) {
            converged = true;
            break;
        }

        if (out.counters.ipqp_iters >= iter_budget) {
            escape = IpqpEscape::kBudget;
            break;
        }
        if (kkt_.counters().factorize_count - before.factorize_count >= fact_budget) {
            escape = IpqpEscape::kBudget;
            break;
        }

        // THE GATED DECREASE (spec 3.2), evaluated BEFORE the assembly so an
        // iteration runs at the schedule the previous iteration's progress
        // earned. The gate is on the REGULARIZED residuals -- the ones the
        // proximal subproblem is actually being solved to -- while the
        // stopping rule above is on the unregularized ones. Conflating the
        // two is the mistake that makes a proximal method either never
        // decrease or decrease unconditionally.
        //
        // Relative on BOTH sides (kIpqpRegGateContract carries why): the
        // regularized residual is divided by the same `max(1, ...)` folds the
        // stopping rule uses, and compared against its own value at the last
        // advance.
        {
            Vec rd_reg = w.grad + rho_sched * (w.x - w.zeta);
            detail::ipqp_accumulate_bound_dual_terms(bounds.lower, bounds.upper, w.zl, w.zu, n,
                                                     rd_reg);
            double R = rd_reg.lpNorm<Eigen::Infinity>() / w.scale_d;
            if (me > 0) {
                R = std::max(
                    R, (w.r_pe - delta_sched * (w.ye - w.lam_est_e)).lpNorm<Eigen::Infinity>() /
                           w.scale_pe);
            }
            if (mi > 0) {
                R = std::max(
                    R, (w.r_pi - delta_sched * (w.yi - w.lam_est_i)).lpNorm<Eigen::Infinity>() /
                           w.scale_pi);
            }
            if (!std::isfinite(reg_gate_ref)) {
                reg_gate_ref = R;
            }
            // TWO WAYS TO EARN AN ADVANCE, and the second is not a loophole in
            // the first: the proximal-point outer iteration advances when its
            // INNER (regularized) problem is SOLVED, and contraction is the
            // proxy for that while the inner residual is still large. Once the
            // inner residual is already below the accuracy this solve is
            // aiming at, demanding further contraction is demanding progress
            // that no longer exists -- and the deadlock is not theoretical:
            // with the contraction test alone a well-scaled fixture converged
            // to a relative primal residual of 1.09e-7 against a 1e-7 target
            // and then STOPPED, because the residual it could not contract any
            // further was `delta * (y - lambda_est)` -- the very quantity a
            // stalled schedule refuses to shrink. Measured at 60 iterations
            // and a budget escape; with this clause, 11 iterations and a
            // certificate.
            const double inner_target = std::min(opt_target, feas_target);
            if (R <= std::max(detail::kIpqpRegGateContract * reg_gate_ref, inner_target)) {
                reg_gate_ref = R;
                const double proposed = rho_sched * iopts.ipqp_reg_decrease;
                // THE MONOTONE FLOOR OVERRIDES THE DECREASE (spec 2.2 item
                // 3). A refused move is a FLAP -- counted as the attempt it
                // was, not as a move, exactly as IpqpCounters says.
                //
                // I2: THE FLAP TEST READS THE MONOTONE FLOOR ONLY. `rho_floor`
                // is the inertia-demanded level and starts at 0, so on a
                // convex subproblem -- where section 2.2's ladder is provably
                // inert -- nothing here can fire. The earlier version compared
                // against `max(reg_floor, rho_floor)`, which made the ABSOLUTE
                // floor a "monotone-floor violation": at the defaults the 11th
                // gated advance proposes 8e-11 against a 1e-10 floor and was
                // counted as a down-then-up cycle on a solve that never had a
                // monotone floor, AND co-fired with `ipqp_reg_decreases`,
                // which the field's own doc comment excludes. The CLAMP still
                // honours both floors; only the COUNT is monotone-only.
                if (proposed < rho_floor) {
                    ++out.counters.ipqp_rho_flaps;
                }
                const double target = std::max(proposed, std::max(iopts.ipqp_reg_floor, rho_floor));
                bool applied = false;
                if (target < rho_sched) {
                    rho_sched = target;
                    applied = true;
                }
                // I7: THE COUNTER IS THE `(rho, delta)` SCHEDULE'S, not rho's.
                // `delta` carries no monotone floor (plan section 7 note (g):
                // section 2.2 states the floor for `rho` only), so it falls to
                // the absolute floor on its own -- and an advance that moved
                // delta alone IS an applied decrease of the schedule. Reading
                // it otherwise reported "no decrease applied" on the
                // monotone-floor fixture while delta went 8 -> 0.8.
                const double dtarget =
                    std::max(delta_sched * iopts.ipqp_reg_decrease, iopts.ipqp_reg_floor);
                if (dtarget < delta_sched) {
                    delta_sched = dtarget;
                    applied = true;
                }
                if (applied) {
                    ++out.counters.ipqp_reg_decreases;
                }
                w.zeta = w.x;
                w.lam_est_e = w.ye;
                w.lam_est_i = w.yi;
                ++out.counters.ipqp_prox_center_updates;
            }
        }

        double rho = std::max(rho_sched, rho_floor);
        double delta = delta_sched;

        const InertiaRead read = factorize_with_ladder(rho, delta);

        // I4: COUNTED AFTER THE LADDER SETTLES, not before it runs. `rho` is
        // an in/out parameter, so this reads the level the step is ACTUALLY
        // taken at. Sampling before the ladder missed the iteration in which
        // the ladder first raises the floor -- entry has rho == rho_sched
        // there, the ladder then climbs, and the step is taken elevated. Off
        // by one, low, on every solve that ever arms the ladder.
        if (rho > rho_sched) {
            ++out.counters.ipqp_iters_at_elevated_rho;
        }

        if (fact_budget_hit) {
            // The cap refused a factorization. That is a BUDGET stop, and it
            // is deliberately tested before the reading: no factorization ran,
            // so there is no inertia verdict to classify.
            escape = IpqpEscape::kBudget;
            break;
        }
        if (read != InertiaRead::kOk) {
            // I1: CLASSIFIED FROM THE TERMINAL READING ALONE. There used to be
            // a solve-scoped `saw_readable_wrong` flag here, set by ANY ladder
            // rung anywhere in the solve -- including rungs the ladder then
            // successfully corrected -- so a terminal PERTURBED or UNREADABLE
            // stop after an earlier corrected wrong reading came out
            // `kIndefinite`. Plan section 7 note (h) is explicit that an
            // evidence-invalid terminal state is NUMERICAL ("no evidence state
            // observed to compare against"), and the difference is not
            // cosmetic: task 6 routes `kIndefinite` to SSN as a saddle-suspect
            // and `kNumerical` to the cold walk, and task 5's census stops
            // being a partition of causes. The terminal `read` already carries
            // the whole answer, so the flag is gone rather than re-scoped.
            escape =
                (read == InertiaRead::kWrong) ? IpqpEscape::kIndefinite : IpqpEscape::kNumerical;
            break;
        }

        // 1. THE AFFINE (PREDICTOR) STEP -- mu target 0.
        build_rhs(rho, delta, 0.0, /*corrector=*/false);
        solve_system();
        recover(0.0, false, w.dx_a, w.ds_a, w.dye, w.dyi_a, w.dzl_a, w.dzu_a);
        if (!w.sol.allFinite()) {
            escape = IpqpEscape::kNumerical;
            break;
        }

        double alpha_ap = 1.0;
        double alpha_ad = 1.0;
        steps_to_boundary(w.dx_a, w.ds_a, w.dyi_a, w.dzl_a, w.dzu_a, alpha_ap, alpha_ad);

        // 2. sigma = (mu_aff / mu)^3, Mehrotra's own oracle. The tier computes
        //    it itself: `ClassicAdaptiveGovernor::mpc_mu` is private and takes
        //    a SolverContext this tier does not have (spec 3.3).
        double sigma_m = 0.0;
        double mu_t = 0.0;
        if (npairs > 0 && mu_meas > 0.0) {
            w.tx = w.x + alpha_ap * w.dx_a;
            if (mi > 0) {
                w.ts = w.s + alpha_ap * w.ds_a;
                w.tyi = w.yi + alpha_ad * w.dyi_a;
            }
            w.tzl = w.zl + alpha_ad * w.dzl_a;
            w.tzu = w.zu + alpha_ad * w.dzu_a;
            double mu_aff = 0.0, lo = 0.0, hi = 0.0;
            complementarity(w.tx, w.ts, w.tyi, w.tzl, w.tzu, mu_aff, lo, hi);
            const double ratio = std::max(0.0, mu_aff / mu_meas);
            sigma_m = std::min(1.0, ratio * ratio * ratio);
            // Spec 3.3's clamp. The FLOOR matters as much as the ceiling: a
            // target driven to exactly zero turns the corrector into a second
            // affine step and gives up the centering the corrector exists for.
            mu_t = std::clamp(sigma_m * mu_meas, iopts.ipqp_min_mu, iopts.ipqp_init_mu);
        }

        // 3. THE CORRECTOR, against the SAME numeric factorization (Amendment
        //    E's re-entrancy assertion: KktFactorization::solve is const and
        //    does not mutate the factor, so two solves against one
        //    factorization are well-formed -- W1 asserts this executably
        //    rather than assuming it).
        build_rhs(rho, delta, mu_t, /*corrector=*/true);
        solve_system();
        recover(mu_t, true, w.dx, w.ds, w.dye, w.dyi, w.dzl, w.dzu);
        if (!w.sol.allFinite()) {
            escape = IpqpEscape::kNumerical;
            break;
        }

        double alpha_p = 1.0;
        double alpha_d = 1.0;
        steps_to_boundary(w.dx, w.ds, w.dyi, w.dzl, w.dzu, alpha_p, alpha_d);
        out.counters.ipqp_alpha_p_min = std::min(out.counters.ipqp_alpha_p_min, alpha_p);
        out.counters.ipqp_alpha_d_min = std::min(out.counters.ipqp_alpha_d_min, alpha_d);

        // 4. THE STEP. No line search: globalization is fraction-to-boundary
        //    and nothing else (spec 3.1 item 3).
        w.x += alpha_p * w.dx;
        if (mi > 0) {
            w.s += alpha_p * w.ds;
            w.yi += alpha_d * w.dyi;
        }
        if (me > 0) {
            w.ye += alpha_d * w.dye;
        }
        w.zl += alpha_d * w.dzl;
        w.zu += alpha_d * w.dzu;

        if (!w.x.allFinite() || !w.s.allFinite() || !w.ye.allFinite() || !w.yi.allFinite() ||
            !w.zl.allFinite() || !w.zu.allFinite()) {
            escape = IpqpEscape::kNumerical;
            break;
        }
        // STRICT POSITIVITY IS A THEOREM OF THE FRACTION-TO-BOUNDARY RULE
        // (tau < 1 leaves every positive quantity at >= (1-tau) of its old
        // value), so this guard should be unreachable. It is written anyway,
        // in every build configuration: the alternative to catching a
        // non-positive slack here is a logarithm of a non-positive number
        // deep inside a kernel that documents its caller's invariant and does
        // not re-check it.
        bool positive = (mi == 0) || (w.s.minCoeff() > 0.0 && w.yi.minCoeff() > 0.0);
        for (Index i = 0; positive && i < n; ++i) {
            if (detail::ipqp_has_lower(bounds.lower(i)) &&
                (w.x(i) <= bounds.lower(i) || w.zl(i) <= 0.0)) {
                positive = false;
            }
            if (detail::ipqp_has_upper(bounds.upper(i)) &&
                (w.x(i) >= bounds.upper(i) || w.zu(i) <= 0.0)) {
                positive = false;
            }
        }
        if (!positive) {
            escape = IpqpEscape::kNumerical;
            break;
        }

        // A COMPLETED PREDICTOR+CORRECTOR PAIR, and only that, is one
        // iteration -- see IpqpCounters::ipqp_iters. Every break above leaves
        // this line unreached, which IS the exclusion the field documents.
        ++out.counters.ipqp_iters;
    }

    // --- section 2.2 item 4: the required final inertia read ---------------

    if (converged) {
        if (!iopts.ipqp_require_final_inertia) {
            // I5, SETTLER RULING (spec section 9's own row: "off = certificate
            // always downgraded"). A DOWNGRADE, NOT AN ESCAPE. The earlier
            // reading issued `kNumerical` here, which turned an option-driven
            // choice to skip one factorization into a census entry and a
            // section 6.1 K = 3 retirement charge on a solve that converged
            // cleanly. `certificate_downgraded` alone already delivers exactly
            // what the option's text promises.
            //
            // `ipqp_final_inertia_read == 3` is the distinct "NOT PERFORMED
            // (option off)" value, kept apart from `2` (attempted, evidence
            // unreadable) so the census cannot confuse a declined read with a
            // failed one.
            out.counters.ipqp_final_inertia_read = 3;
            out.certificate_downgraded = true;
            // escape stays kNone -- see the status note at the outcome switch.
        } else {
            // C0, SETTLER RULING on section 2.2 item 4's "the schedule's
            // residual level": it is the level the schedule DECAYS TO, i.e.
            // `ipqp_reg_floor`, not whatever `rho_sched` happens to hold when
            // the solve stops.
            //
            // The distinction is a wrong-answer bug, not a nicety. A solve can
            // converge BEFORE the schedule ever advances -- a one-variable QP
            // with H = [-1], g = 0 and no rows or bounds is stationary at the
            // origin on iteration 0 -- and `rho_sched` is then still
            // `ipqp_rho_init` = 8. Reading the certificate off `H + 8 I` = [7]
            // finds the target inertia (1, 0, 0) and certifies a CONCAVE,
            // unbounded problem as optimal at its maximum. Dropping to the
            // floor asks the question item 4 exists to ask: is the converged
            // point a minimum of the PROBLEM, or only of the modification that
            // got us here.
            //
            // `delta` stays at `delta_sched`: it is a DUAL proximal term, it
            // enters only the `-delta I` blocks, and it cannot change the
            // primal block's contribution to the inertia count.
            //
            // A subproblem that needed the ladder will therefore fail this
            // read, and reporting it as saddle-suspect is the correct answer,
            // not a false negative.
            refresh_distances();
            const InertiaRead read = factorize_and_read_once(iopts.ipqp_reg_floor, delta_sched);
            if (fact_budget_hit) {
                // The cap refused the certification factorization, so no
                // reading exists. Honest census value 2, and the reason the
                // read never happened is the budget.
                out.counters.ipqp_final_inertia_read = 2;
                out.certificate_downgraded = true;
                escape = IpqpEscape::kBudget;
            } else if (read == InertiaRead::kOk) {
                out.counters.ipqp_final_inertia_read = 0;
            } else if (read == InertiaRead::kWrong) {
                // A reading WAS observed and DISAGREED -- plan section 7 note
                // (h)'s saddle-suspect class.
                out.counters.ipqp_final_inertia_read = 1;
                out.certificate_downgraded = true;
                escape = IpqpEscape::kIndefinite;
            } else {
                // C0b: UNREADABLE **or** PERTURBED. A perturbed factorization
                // is not evidence about the assembled matrix at all, so it is
                // not "a reading that disagreed" -- it is no reading. Note
                // (h)'s `== 2`, numerical, exactly as the mid-ladder path
                // already treats an unobservable state.
                out.counters.ipqp_final_inertia_read = 2;
                out.certificate_downgraded = true;
                escape = IpqpEscape::kNumerical;
            }
        }
    }

    // --- outcome ----------------------------------------------------------

    out.escape_reason = escape;
    switch (escape) {
    case IpqpEscape::kNone:
        out.status = QpStatus::kOptimal;
        break;
    case IpqpEscape::kBudget:
        out.status = QpStatus::kMaxIter;
        break;
    default:
        // Never kInfeasible: IP-PMM has no infeasibility certificate (spec
        // 6.3), so the tier has no word for one.
        out.status = QpStatus::kNumericalError;
        break;
    }
    if (out.certificate_downgraded && out.escape_reason == IpqpEscape::kNone) {
        // A DOWNGRADE WITHOUT AN ESCAPE (I5's ruling, reachable only through
        // `ipqp_require_final_inertia == false`). Spec 2.2 item 4 forbids
        // `kOptimal` for a downgraded certificate, and `QpStatus` has no word
        // for "converged but uncertified", so this borrows the walk's own
        // bucket for a trusted-but-not-optimal exit. THE STATUS VOCABULARY FOR
        // A DOWNGRADED-WITHOUT-ESCAPE OUTCOME IS TASK 5'S RULING; what is
        // settled here is only that it is not `kOptimal` and not an escape.
        out.status = QpStatus::kNumericalError;
    }

    out.x = w.x;
    out.s = w.s;
    out.lambda_e = w.ye;
    out.lambda_i = w.yi;
    out.zl = w.zl;
    out.zu = w.zu;
    out.mu = mu_meas;
    out.rho = rho_sched;
    out.delta = delta_sched;

    // --- the face classification (spec 2.3 item 2) -------------------------
    //
    // A RATIO rule, applied to the returned point whatever the outcome was:
    // like SsnResult's implied active set it describes where the solve
    // STOPPED and certifies nothing. An UNCERTAIN verdict is never forced --
    // it is counted and handed on.
    refresh_distances();
    const double kappa = iopts.ipqp_face_kappa;
    const double mu_scale = std::max(mu_meas, 0.0);
    auto classify = [&](double slack, double dual) {
        if (slack < kappa * dual && dual > mu_scale) {
            return IpqpFace::kActive;
        }
        if (dual < kappa * slack) {
            return IpqpFace::kInactive;
        }
        return IpqpFace::kUncertain;
    };

    out.ineq_face.assign(static_cast<std::size_t>(mi), IpqpFace::kInactive);
    out.ineq_active.assign(static_cast<std::size_t>(mi), false);
    for (Index j = 0; j < mi; ++j) {
        const IpqpFace f = classify(w.s(j), w.yi(j));
        out.ineq_face[static_cast<std::size_t>(j)] = f;
        out.ineq_active[static_cast<std::size_t>(j)] = (f == IpqpFace::kActive);
        if (f == IpqpFace::kUncertain) {
            ++out.counters.ipqp_face_uncertain;
        }
    }

    out.lower_face.assign(static_cast<std::size_t>(n), IpqpFace::kInactive);
    out.upper_face.assign(static_cast<std::size_t>(n), IpqpFace::kInactive);
    out.bound_state.assign(static_cast<std::size_t>(n), BoundState::kFree);
    out.tr_active.assign(static_cast<std::size_t>(n), false);
    out.z.setZero(n);
    for (Index i = 0; i < n; ++i) {
        const std::size_t u = static_cast<std::size_t>(i);
        bool at_lower = false;
        bool at_upper = false;
        if (detail::ipqp_has_lower(bounds.lower(i))) {
            const IpqpFace f = classify(w.dL(i), w.zl(i));
            out.lower_face[u] = f;
            at_lower = (f == IpqpFace::kActive);
            if (f == IpqpFace::kUncertain) {
                ++out.counters.ipqp_face_uncertain;
            }
        }
        if (detail::ipqp_has_upper(bounds.upper(i))) {
            const IpqpFace f = classify(w.dU(i), w.zu(i));
            out.upper_face[u] = f;
            at_upper = (f == IpqpFace::kActive);
            if (f == IpqpFace::kUncertain) {
                ++out.counters.ipqp_face_uncertain;
            }
        }
        // TR-PINNED, `QpSolution::tr_active`'s contract verbatim: the
        // variable is held by an effective bound that is TIGHTER than the
        // real one, so its multiplier is a trust-region dual and is internal.
        // A coincidental tie (`lo_eff == lower`) is attributed to the REAL
        // bound, which is the walk's own tie-breaking rule.
        const bool tr_lo = at_lower && bounds.lower(i) > qp.lower(i);
        const bool tr_up = at_upper && bounds.upper(i) < qp.upper(i);
        if (tr_lo || tr_up) {
            out.tr_active[u] = true;
            continue; // bound_state stays kFree, z stays 0.
        }
        if (at_lower && at_upper) {
            out.bound_state[u] = BoundState::kFixed;
        } else if (at_lower) {
            out.bound_state[u] = BoundState::kAtLower;
        } else if (at_upper) {
            out.bound_state[u] = BoundState::kAtUpper;
        }
        out.z(i) = w.zl(i) - w.zu(i);
    }

    // --- counters that mirror the backend's own ---------------------------
    //
    // Taken as DELTAS off SymmetricFactor::Counters rather than tallied here,
    // so `ipqp_pattern_verifies` is literally the backend's own count (the
    // field's doc comment says "mirrors", and this is what makes that a fact
    // rather than a claim) and so the four cannot drift from each other.
    const KktFactorization::Counters after = kkt_.counters();
    out.counters.ipqp_factorizations = after.factorize_count - before.factorize_count;
    out.counters.ipqp_solves = after.solve_count - before.solve_count;
    out.counters.ipqp_symbolic_analyses = after.analyze_count - before.analyze_count;
    out.counters.ipqp_pattern_verifies = after.pattern_verify_count - before.pattern_verify_count;
    out.counters.ipqp_rho_demanded_max = rho_floor;
    out.counters.ipqp_rho_demanded_last = rho_demanded_last;

    emit_ledger();

    return out;
}

} // namespace hven::solvers
