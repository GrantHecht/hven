// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// ipqp_engine.cpp -- the IP-PMM interior-point QP tier's iteration.
//
// The header carries the contracts; this file carries the algorithm. Slack form, in
// KKTVector's block order [primals(n) | slacks(mi) | eq_lmults(me) | iq_lmults(mi)], with
// s = bi - Ai x >= 0, lambda_i >= 0, and variable bounds CONDENSED into the (1,1) diagonal:
//
//     [ H + rho I + Sigma_b     0        Ae'       Ai'    ]
//     [      0            Lam S^-1        0        -I     ]
//     [     Ae                  0     -delta I      0     ]
//     [     Ai                 -I          0     -delta I ]
//
// THE SLACK BLOCK'S UNKNOWN IS -ds, NOT ds, forced by the matrix: the (iq, s) coupling is -I,
// so the inequality row reads `Ai dx - v_s - delta dyi` while the linearized constraint reads
// `Ai dx + ds - delta dyi`. A sign error here converges plausibly to the wrong point.
//
// THE CONDENSED BOUND ALGEBRA: eliminating dzl/dzu contributes Sigma_b_i = zl_i/dL_i +
// zu_i/dU_i (exactly `ipqp_accumulate_bound_sigma`) and moves `tl/dL - tu/dU` to the RHS.
// Derivation: docs/notes/data/2026-08-m6-w1-acceptance/comment-trim-sidecar.md.
//
// For a UNIFORM target that RHS term is precisely the NEGATIVE of ipqp_math.h's mu-form bound
// gradient -- hence the kernel call, and hence no special case for the AFFINE (mu = 0) RHS.
// ONE DEVIATION: the Mehrotra corrector's second-order target term is written separately.
//
// BOUND DAMPING IS STRUCTURALLY INERT ON THE DRIVER'S PATH: under a FINITE trust-region
// radius the clamp-centred box makes every variable two-sided, so Ipopt's one-sided damping
// indicator is identically zero. The term survives only for a caller who disables the radius.

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/Core>
#include <fmt/format.h>

#include <hven/detail/globalization/inertia_regularization.h>
#include <hven/detail/interior/barrier_math.h>
#include <hven/detail/qp/ipqp_engine.h>
#include <hven/detail/qp/ipqp_fault_injection.h>
#include <hven/detail/qp/ipqp_math.h>
#include <hven/detail/qp/ipqp_trace.h>
#include <hven/detail/qp/qp_engine.h>
#include <hven/detail/qp/ssn_engine.h>

namespace hven::solvers {

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

/// The five-way inertia reading the tier acts on.
///
/// REFINES `detail::inertia_verdict` (qp_engine.h), reused verbatim for the kOk/kWrong
/// decision: that helper collapses "perturbed pivots" and "no evidence state" into one
/// `kSuspect`, and spec 2.2 gives them DIFFERENT remedies (raise `delta` vs downgrade).
///
/// `kFactorFailed` IS TASK 5'S FIFTH VALUE and not a refinement at all: the evidence-failure
/// policy applies to a factorization that SUCCEEDED, while a failed one is a plain
/// `kNumerical` stop (plan section 7 note (h)); only `KktFactorization::info()` separates them.
enum class InertiaRead { kOk, kWrong, kPerturbed, kUnreadable, kFactorFailed };

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
        // What reaches here is the SHORT-SUM case (n_pos + n_neg != dim): a zero eigenvalue,
        // read from an OBSERVED state, so it is a disagreement -- plan section 7 note (h)'s
        // "read and disagreed" -- and raising the regularization is the move that removes it.
        return InertiaRead::kWrong;
    }
    return InertiaRead::kUnreadable;
}

/// THE TIER'S ONE INERTIA-EVIDENCE READ, and therefore the tier's one test seam
/// (docs/testing.md; declarations in ipqp_fault_injection.h, compiles to nothing
/// without HVEN_TESTING).
///
/// EVERY reading the tier acts on comes through here -- the ladder's, the evidence-failure
/// branch's, and the section 2.2 item 4 certification read's -- which makes ONE hook enough.
/// `final_read` separates the two KINDS of read, which section 2.2 gives different policies.
///
/// THE HOOK IS AT THE BOUNDARY (CLAUDE.md section 6): this file is Apache-2.0 and no session
/// file is involved. Without HVEN_TESTING the whole body is `return kkt.inertia_evidence();`.
const hven::linear::InertiaEvidence &evidence_for_read([[maybe_unused]] const KktFactorization &kkt,
                                                       [[maybe_unused]] bool final_read) {
#ifdef HVEN_TESTING
    using Observer = detail::testing::IpqpInertiaReadObserver;
    using Injector = detail::testing::IpqpInertiaEvidenceInjector;
    const bool eligible = final_read ? Injector::on_final_read : Injector::on_iteration_reads;
    if (Injector::active && eligible) {
        if (Injector::skip_first > 0) {
            --Injector::skip_first;
        } else if (Injector::max_injections >= 0 &&
                   Injector::injections >= Injector::max_injections) {
            // The injection WINDOW has closed -- fall through to the real
            // reading so a fixture can observe the tier recovering.
        } else {
            ++Injector::injections;
            if (Observer::active) {
                ++Observer::reads;
                Observer::last = Injector::evidence;
                Observer::last_injected = Injector::evidence;
                if (final_read) {
                    ++Observer::final_reads;
                    Observer::last_final = Injector::evidence;
                }
            }
            return Injector::evidence;
        }
    }
    if (Observer::active) {
        ++Observer::reads;
        Observer::last = kkt.inertia_evidence();
        if (final_read) {
            ++Observer::final_reads;
            Observer::last_final = kkt.inertia_evidence();
        }
    }
#endif
    return kkt.inertia_evidence();
}

#ifdef HVEN_TESTING
/// The two M6 W1 T9 OBSERVERS, on the same seam as `evidence_for_read`. Both
/// they and their call sites are `#ifdef`-guarded, so the production build
/// carries neither the code nor the call.
void capture_first_iterate(const Vec &x, const Vec &s, const Vec &ye, const Vec &yi, const Vec &zl,
                           const Vec &zu, double mu) {
    using Obs = detail::testing::IpqpFirstIterateObserver;
    if (!Obs::active || Obs::captures > 0) {
        return;
    }
    ++Obs::captures;
    Obs::x = x;
    Obs::s = s;
    Obs::ye = ye;
    Obs::yi = yi;
    Obs::zl = zl;
    Obs::zu = zu;
    Obs::mu = mu;
}

/// Gate 9's per-step form: the EXECUTED iterate update's inf-norm -- the
/// alpha-scaled step the solve actually applies, not the raw Newton direction
/// (fix round 1, Codex 1) -- beside the regularized residual it was taken at.
void observe_accepted_step(const Vec &dx, const Vec &ds, const Vec &dye, const Vec &dyi,
                           const Vec &dzl, const Vec &dzu, double alpha_p, double alpha_d,
                           double res_worst, bool met_target) {
    using Obs = detail::testing::IpqpStepObserver;
    if (!Obs::active) {
        return;
    }
    double step_inf = 0.0;
    // The two scalings are the step's own: alpha_p on the primal blocks
    // (`x`, `s`), alpha_d on every dual block -- exactly as applied below.
    const std::pair<const Vec *, double> blocks[] = {{&dx, alpha_p},  {&ds, alpha_p},
                                                     {&dye, alpha_d}, {&dyi, alpha_d},
                                                     {&dzl, alpha_d}, {&dzu, alpha_d}};
    for (const auto &[v, alpha] : blocks) {
        if (v->size() > 0) {
            step_inf = std::max(step_inf, alpha * v->lpNorm<Eigen::Infinity>());
        }
    }
    ++Obs::steps;
    Obs::last_step_inf = step_inf;
    Obs::last_res = res_worst;
    if (step_inf < Obs::min_step_inf) {
        Obs::min_step_inf = step_inf;
        Obs::res_at_min_step = res_worst;
        Obs::met_target_at_min_step = met_target;
    }
}
#endif // HVEN_TESTING

/// THE SECTION 6.3 FARKAS CORROBORATION -- one matvec plus O(m + n), no
/// factorization.
///
/// `SsnEngine::farkas_certificate`'s shape, re-implemented because that one is a PRIVATE member
/// bound to `bound_rows_` (plan section 7 note d). The DISCIPLINE is identical: project the dual
/// INCREMENT onto the sign cone, normalize, and test RELATIVELY against a `max(1, .)` floor.
///
/// **IT ARMS, IT NEVER CERTIFIES** (spec 6.3): the caller reports `kInfeasibleSuspect` whether
/// this returns true or false, and all that changes is `farkas_corroborated` and the two
/// numbers beside it.
///
/// The system tested is {Ae x = be, Ai x <= bi, -x <= -lower, x <= upper}: infeasible iff some
/// `(ye free, yi >= 0, zl >= 0, zu >= 0)` has `Ae' ye + Ai' yi - zl + zu = 0` and
/// `be' ye + bi' yi - lower' zl + upper' zu < 0`. Absent bound sides contribute nothing.
bool ipqp_farkas_corroborates(const QpProblem &qp, const IpqpBounds &bounds, const Vec &dye,
                              const Vec &dyi, const Vec &dzl, const Vec &dzu, double *resid_out,
                              double *gap_out) {
    const Index n = qp.n();
    const Index me = qp.me();
    const Index mi = qp.mi();

    Vec ye = me > 0 ? dye : Vec(0);
    Vec yi = mi > 0 ? Vec(dyi.cwiseMax(0.0)) : Vec(0);
    Vec zl = dzl.cwiseMax(0.0);
    Vec zu = dzu.cwiseMax(0.0);
    for (Index i = 0; i < n; ++i) {
        if (!detail::ipqp_has_lower(bounds.lower(i))) {
            zl(i) = 0.0;
        }
        if (!detail::ipqp_has_upper(bounds.upper(i))) {
            zu(i) = 0.0;
        }
    }

    double scale = std::max(zl.lpNorm<Eigen::Infinity>(), zu.lpNorm<Eigen::Infinity>());
    if (me > 0) {
        scale = std::max(scale, ye.lpNorm<Eigen::Infinity>());
    }
    if (mi > 0) {
        scale = std::max(scale, yi.lpNorm<Eigen::Infinity>());
    }
    if (!(scale > 0.0) || !std::isfinite(scale)) {
        return false;
    }
    if (me > 0) {
        ye /= scale;
    }
    if (mi > 0) {
        yi /= scale;
    }
    zl /= scale;
    zu /= scale;

    Vec r = Vec::Zero(n);
    Vec r_abs = Vec::Zero(n);
    double gap = 0.0;
    double gap_abs = 0.0;
    for (Index k = 0; k < me; ++k) {
        const double y = ye(k);
        for (SpMatRM::InnerIterator it(qp.Ae, k); it; ++it) {
            r(it.col()) += it.value() * y;
            r_abs(it.col()) += std::abs(it.value() * y);
        }
        gap += qp.be(k) * y;
        gap_abs += std::abs(qp.be(k) * y);
    }
    for (Index k = 0; k < mi; ++k) {
        const double y = yi(k);
        for (SpMatRM::InnerIterator it(qp.Ai, k); it; ++it) {
            r(it.col()) += it.value() * y;
            r_abs(it.col()) += std::abs(it.value() * y);
        }
        gap += qp.bi(k) * y;
        gap_abs += std::abs(qp.bi(k) * y);
    }
    for (Index i = 0; i < n; ++i) {
        if (zl(i) != 0.0) {
            r(i) -= zl(i);
            r_abs(i) += zl(i);
            gap += -bounds.lower(i) * zl(i);
            gap_abs += std::abs(bounds.lower(i) * zl(i));
        }
        if (zu(i) != 0.0) {
            r(i) += zu(i);
            r_abs(i) += zu(i);
            gap += bounds.upper(i) * zu(i);
            gap_abs += std::abs(bounds.upper(i) * zu(i));
        }
    }

    const double rel_resid =
        n > 0 ? r.cwiseAbs().maxCoeff() / std::max(1.0, r_abs.maxCoeff()) : 0.0;
    const double rel_gap = gap / std::max(1.0, gap_abs);
    *resid_out = rel_resid;
    *gap_out = rel_gap;
    if (!std::isfinite(rel_resid) || !std::isfinite(rel_gap)) {
        return false;
    }
    return rel_resid <= detail::kSsnFarkasResidualTol && rel_gap <= -detail::kSsnFarkasGapTol;
}

/// Ruiz equilibration of a symmetric matrix stored as its UPPER TRIANGLE in
/// row-major CSR (spec 4.3).
///
/// Returns the accumulated diagonal `d` with `D K D` written back into `k`. One pass over the
/// stored half updates the norm of BOTH the row and the column an entry sits in.
///
/// A SYMMETRIC diagonal scaling PRESERVES INERTIA (Sylvester's law), which is what makes the
/// section 2.2 gate readable off the scaled factor at all. Residuals, multipliers and counters
/// are all stated UNSCALED: the scaling never leaves this function.
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

/// `SsnEngine::validate_overrides`' rule, applied unchanged: a negative or NaN radius would
/// silently cross `lo_eff`/`up_eff` behind an assert a Release build compiles out, and NaN in
/// either regularizer is absorbable by no downstream arithmetic.
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

/// Boundary validation for a warm seed (spec 5.1 flow (b)): every block is
/// stated over THIS subproblem, at exactly its dimensions, and finite.
void validate_seed(const IpqpSeed &seed, Index n, Index me, Index mi) {
    const auto check = [](const char *block, const Vec &v, Index want) {
        if (v.size() != want) {
            throw std::invalid_argument(
                fmt::format("IpqpEngine::solve: warm seed block {0} holds {1} entries but this "
                            "subproblem declares {2}",
                            block, v.size(), want));
        }
        if (!v.allFinite()) {
            throw std::invalid_argument(fmt::format(
                "IpqpEngine::solve: warm seed block {0} holds a non-finite value", block));
        }
    };
    check("x", seed.x, n);
    check("s", seed.s, mi);
    check("lambda_e", seed.lambda_e, me);
    check("lambda_i", seed.lambda_i, mi);
    check("zl", seed.zl, n);
    check("zu", seed.zu, n);
    check("zeta", seed.zeta, n);
    check("lambda_est_e", seed.lambda_est_e, me);
    check("lambda_est_i", seed.lambda_est_i, mi);
    if (!std::isfinite(seed.mu)) {
        throw std::invalid_argument(
            fmt::format("IpqpEngine::solve: warm seed mu ({}) must be finite", seed.mu));
    }
    // BOUNDS-CHECKED like every other field: an out-of-range enumerator is a
    // caller error, and `kCold` names "no seed" rather than a seed.
    if (seed.grade != IpqpRestartGrade::kBaseWarm && seed.grade != IpqpRestartGrade::kFullWarm) {
        throw std::invalid_argument(fmt::format(
            "IpqpEngine::solve: warm seed grade ({}) must be kBaseWarm or kFullWarm -- a cold "
            "solve is a null seed, not a seed labelled cold",
            static_cast<int>(seed.grade)));
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

    // A radius of +inf disables the window EXACTLY: `lo_eff == lower` and `up_eff == upper`,
    // bit for bit, with no arithmetic performed on them. `c - Delta` at Delta == +inf would be
    // -inf rather than `lower` -- a different value, and differently classified.
    const bool windowed = std::isfinite(radius);

    for (Index i = 0; i < n; ++i) {
        const double lo = qp.lower(i);
        const double up = qp.upper(i);
        if (std::isnan(lo) || std::isnan(up)) {
            throw std::invalid_argument(
                fmt::format("make_ipqp_box: bound {} is NaN (lower={}, upper={})", i, lo, up));
        }
        // THE CENTRE RULE IS refine_on_face's OWN, character for character (qp_engine.cpp's
        // gate: `c = min(max(0, lo), up)`). Why sharing it is load-bearing: IpqpBox's doc.
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

// ---------------------------------------------------------------------------
// The section 6.1 escape ladder
// ---------------------------------------------------------------------------

IpqpEscapeLadder::IpqpEscapeLadder(const IpqpOptions &iopts)
    : retire_after_(iopts.ipqp_retire_after) {
    // Re-checked HERE and not only in `validate_sqp_options`: this type is reachable without a
    // driver, and a retirement threshold of 0 would retire the tier before it had ever run.
    if (retire_after_ <= 0) {
        throw std::invalid_argument(
            fmt::format("IpqpEscapeLadder: ipqp_retire_after ({}) must be > 0; retiring "
                        "\"after zero consecutive escapes\" is not a count, it is disabling "
                        "the tier outright, which that field does not exist to express.",
                        retire_after_));
    }
}

bool IpqpEscapeLadder::record(const IpqpResult &result, Index major) {
    return record(result.declined_pinned                      ? IpqpLadderOutcome::kDeclined
                  : result.escape_reason == IpqpEscape::kNone ? IpqpLadderOutcome::kSuccess
                                                              : IpqpLadderOutcome::kEscape,
                  major);
}

bool IpqpEscapeLadder::record(IpqpLadderOutcome outcome, Index major) {
    if (major <= 0) {
        throw std::invalid_argument(fmt::format(
            "IpqpEscapeLadder::record: major ({}) must be > 0; "
            "IpqpCounters::ipqp_tier_retired_after uses 0 to mean \"never retired\", so "
            "major 0 is not a representable place for retirement to have happened.",
            major));
    }
    if (outcome == IpqpLadderOutcome::kDeclined) {
        // A DECLINE IS NEUTRAL: it cannot advance the tally (the tier never ran) and cannot
        // reset one either (it produced no evidence of suitability). `ipqp_declined_pinned`.
        return retired_;
    }
    if (outcome == IpqpLadderOutcome::kSuccess) {
        // "ANY SUCCESS RESETS THE COUNT" (spec 6.1). A converged solve whose CERTIFICATE was
        // downgraded without an escape is a success here -- plan section 7 note (j) gives it
        // no section 6.1 charge, and 6.1's own word for the alternative to an escape is that.
        consecutive_ = 0;
        return retired_;
    }
    ++consecutive_;
    if (!retired_ && consecutive_ >= retire_after_) {
        // FIRES AT MOST ONCE (`ipqp_tier_retired_after` is a marker, not a count): 6.1 retires
        // for the REMAINDER of the solve, and a reset aims at a FUTURE retirement, not this.
        retired_ = true;
        retired_after_ = major;
    }
    return retired_;
}

bool IpqpEscapeLadder::retired() const { return retired_; }

Index IpqpEscapeLadder::retired_after() const { return retired_after_; }

Index IpqpEscapeLadder::consecutive_escapes() const { return consecutive_; }

IpqpBounds make_ipqp_bounds(const IpqpBox &box) {
    // CLAUDE.md section 4, at a PUBLIC boundary: this function takes an IpqpBox by reference, so
    // a hand-built box can present blocks of different lengths. Without this check the loop
    // below is an out-of-bounds READ in Release, where Eigen's own assert is compiled out.
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

// The walk's own trust-region resolution, unchanged (`qp_types.h`'s SolveOverrides sentinel:
// +inf on the override means "use the engine's own radius"). DECLARED IN THE HEADER since
// task 6, because the routing chain's pre-solve domain gate builds the same box.
double ipqp_effective_tr_radius(const QpOptions &opts, const SolveOverrides &overrides) {
    return std::isinf(overrides.tr_radius) && overrides.tr_radius > 0.0 ? opts.tr_radius
                                                                        : overrides.tr_radius;
}

// THE STOPPING RULE (spec 2.3 step 1 / 3.4), and the ONLY statement of it: the QP layer's own
// relative tolerances, loosened by `ipqp_converge_slack`. `solve()`'s loop calls this, so a
// caller reading it back off an IpqpResult reads the same answer the engine acted on.
bool ipqp_residuals_meet_target(const IpqpResiduals &residuals, const QpOptions &opts,
                                const IpqpOptions &iopts) {
    const double opt_target = opts.opt_tol * iopts.ipqp_converge_slack;
    const double feas_target = opts.feas_tol * iopts.ipqp_converge_slack;
    return residuals.stationarity <= opt_target && residuals.primal_eq <= feas_target &&
           residuals.primal_iq <= feas_target && residuals.complementarity <= opt_target;
}

// ---------------------------------------------------------------------------
// The engine
// ---------------------------------------------------------------------------

/// Every vector one solve needs, allocated once at entry.
///
/// A plain member-per-block struct rather than engine members: holding these across solves
/// would make the engine's state depend on the previous subproblem's SIZE. What IS worth
/// holding across solves -- the symbolic analysis and the scatter plan -- is held.
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

    // The three `max(1, ...)` folds the relative residual divides by, kept so the SCHEDULE'S
    // gate can be stated on the same scale as the STOPPING rule -- see kIpqpRegGateContract for
    // what comparing two absolute quantities cost the first version of that gate.
    double scale_d = 1.0, scale_pe = 1.0, scale_pi = 1.0;

    // Steps.
    Vec dx, ds, dye, dyi, dzl, dzu;
    Vec dx_a, ds_a, dyi_a, dzl_a, dzu_a;

    // Fraction-to-boundary scratch. `detail::max_step_to_boundary` takes NON-CONST
    // `Eigen::Ref`s, so every block handed to it needs a mutable vector of its own --
    // including the direction, which is why `ft_dx` is here.
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
    // THE TIER'S OWN BACKEND OPTIONS, NOT detail::sqp_kkt_options() (spec 4.4).
    // Default-constructed and left alone: every field that maps to a Pardiso iparm slot stays
    // at the value pardisoinit chose, so CLAUDE.md section 6's labelling rule has nothing to
    //
    // bind on, and the Accelerate backend (which throws on several non-default fields at
    // construction) is safe. Recorded here because "no code" is otherwise indistinguishable
    // from "not thought about".
}

const IpqpSeed *IpqpEngine::warm_carry() const { return carry_armed_ ? &carry_ : nullptr; }

void IpqpEngine::reset_warm_carry() {
    carry_ = IpqpSeed{};
    carry_armed_ = false;
}

void IpqpEngine::attach_ledger(Ledger *ledger, std::string label_prefix) {
    ledger_ = ledger;
    label_prefix_ = std::move(label_prefix);
    solve_counter_ = 0;
}

void IpqpEngine::attach_trace(IpqpTraceSink *sink) {
    trace_ = sink;
    trace_solve_counter_ = 0;
    last_trace_solve_id_ = 0; // R5: a reattach must not leak the prior attach's id.
}

void IpqpEngine::set_trace_major(Index major) {
    if (major < 0) {
        throw std::invalid_argument(
            fmt::format("IpqpEngine::set_trace_major: major must be >= 0, got {}", major));
    }
    trace_major_ = major;
}

Index IpqpEngine::last_trace_solve_id() const { return last_trace_solve_id_; }

// task 8: five no-op-when-unattached emit sites, one per event this engine
// owns. Each call site guards the STRUCT BUILD too, not just this call, so
// an unattached sink costs one pointer compare per event point.
void IpqpEngine::emit_trace_iter(const IpqpTraceIterEvent &event) const {
    if (trace_ != nullptr) {
        trace_->on_ipqp_iter(event);
    }
}
void IpqpEngine::emit_trace_reg(const IpqpTraceRegEvent &event) const {
    if (trace_ != nullptr) {
        trace_->on_ipqp_reg(event);
    }
}
void IpqpEngine::emit_trace_restart(const IpqpTraceRestartEvent &event) const {
    if (trace_ != nullptr) {
        trace_->on_ipqp_restart(event);
    }
}
void IpqpEngine::emit_trace_certify(const IpqpTraceCertifyEvent &event) const {
    if (trace_ != nullptr) {
        trace_->on_ipqp_certify(event);
    }
}
void IpqpEngine::emit_trace_escape(const IpqpTraceEscapeEvent &event) const {
    if (trace_ != nullptr) {
        trace_->on_ipqp_escape(event);
    }
}

IpqpResult IpqpEngine::solve(const QpProblem &qp, const IpqpSeed *seed, const IpqpOptions &iopts,
                             const SolveOverrides &overrides) {
    // --- boundary validation (CLAUDE.md section 4) -------------------------
    qp.validate();
    validate_overrides(overrides);
    {
        // THE SAME CODE THE DRIVER RUNS, not a second copy of the rules: `validate_sqp_options`
        // owns every IpqpOptions band (task 1), and this engine is reachable without a driver,
        // so the check happens here by CALLING the owner with an otherwise default SqpOptions.
        SqpOptions probe;
        probe.ipqp = iopts;
        validate_sqp_options(probe);
    }

    const Index n = qp.n();
    const Index me = qp.me();
    const Index mi = qp.mi();

    // THE SEED IS A CALLER ASSERTION ABOUT THIS PROBLEM, so a wrongly sized or
    // non-finite block is a caller error and throws; spec 5.4's cold DEGRADE
    // is the driver's, taken before a seed is built (report section 3).
    bool started_warm = seed != nullptr;
    if (started_warm) {
        validate_seed(*seed, n, me, mi);
    }

    IpqpResult out;

    // task 8: this solve's trace `solve` id, READ only -- see
    // complete_trace_solve below for why the counter itself advances later.
    const Index trace_solve_id = trace_solve_counter_;

    // ONE ROW PER NON-THROWING SOLVE, and that includes a DECLINE (I9). The emitter sits above
    // the domain gate because the gate returns early and a decline is an OUTCOME, not a
    // non-event: a decline consuming no label would make the ledger's labels stop counting.
    auto emit_ledger = [&]() {
        if (ledger_ == nullptr) {
            return;
        }
        SolveRecord rec;
        rec.label = fmt::format("{}{}", label_prefix_, solve_counter_++);
        rec.warm = started_warm; // the section 5.4 grade this solve started at.
        rec.status = out.status;
        // THE QP-SHAPED PROJECTION (see attach_ledger's contract). Only the three fields that
        // mean the same thing on all three kernels are filled; the rest stay at their defaults
        // rather than carrying a plausible value this tier did not measure. All 0 on a decline.
        rec.counters.minor_iters = out.counters.ipqp_iters;
        rec.counters.factorizations = out.counters.ipqp_factorizations;
        rec.counters.symbolic_analyses = out.counters.ipqp_symbolic_analyses;
        ledger_->record(std::move(rec));
    };

    // R5: a solve is marked "completed" (the id becomes observable through
    // last_trace_solve_id()) only at the two points past which this call
    // cannot throw -- alongside emit_ledger(), never at entry.
    auto complete_trace_solve = [&]() {
        if (trace_ != nullptr) {
            ++trace_solve_counter_;
            last_trace_solve_id_ = trace_solve_id;
        }
    };

    out.box = make_ipqp_box(qp, ipqp_effective_tr_radius(opts_, overrides));
    const IpqpBounds bounds = make_ipqp_bounds(out.box);

    // --- T4.b: the domain gate --------------------------------------------
    if (!bounds.in_domain()) {
        // A DECLINE, NOT AN ESCAPE: nothing here touches `ipqp_escapes` or the K = 3 tally and
        // every other counter stays at its default. The routing chain sends it to the walk.
        out.declined_pinned = true;
        out.status = QpStatus::kNumericalError;
        out.escape_reason = IpqpEscape::kNone;
        out.counters.ipqp_declined_pinned = 1;
        emit_ledger();
        complete_trace_solve();
        return out;
    }

    Workspace w;
    w.allocate(n, me, mi);

    const KktFactorization::Counters before = kkt_.counters();

    // --- budgets (spec 3.4) -----------------------------------------------
    //
    // `ipqp_max_iter`'s sentinel discipline is `QpOptions::max_iter`'s own, taken through the
    // walk's own helper. BOTH CAPS BIND -- the `min` is the point, so a caller who raises
    // `ipqp_max_iter` alone still gets the hard cap, Amendment C's budget of LAST RESORT.
    //
    // The FACTORIZATION cap's sentinel is three times the budget ABOVE, i.e. three times the
    // CLAMPED one: an iteration costs one factorization plus ladder rungs.
    const Index derived = detail::effective_qp_max_iter(qp, iopts.ipqp_max_iter);
    const Index iter_budget = std::min(derived, iopts.ipqp_hard_iter_cap);
    const Index fact_budget = iopts.ipqp_max_factorizations > 0
                                  ? iopts.ipqp_max_factorizations
                                  : detail::kIpqpFactorizationsPerIter * iter_budget;

    const Index npairs = mi + bounds.num_lower + bounds.num_upper;

    // The barrier parameter this solve STARTS at. Not const: spec 5.3's clamp
    // rewrites it on a warm restart, and spec 5.5's kill restores the cold
    // value with the cold point.
    double mu0 = iopts.ipqp_init_mu;
    double mu_meas = mu0;

    // Push x strictly inside the effective box (Ipopt's bound_push/bound_frac rule). The box has
    // strictly positive width everywhere (the domain gate above), so the push always lands
    // strictly inside; both sides move by at most kIpqpBoundPushRel of the width.
    auto push_into_box = [&]() {
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
    };

    // Move a WARM x just inside the effective box -- a DIFFERENT rule from the cold push above,
    // and the difference is the point: Ipopt's bound_push would shove an active bound off its
    // own solution. This moves by the repair floor instead.
    auto clamp_seed_into_box = [&]() {
        for (Index i = 0; i < n; ++i) {
            const double lo = bounds.lower(i);
            const double up = bounds.upper(i);
            const bool hl = detail::ipqp_has_lower(lo);
            const bool hu = detail::ipqp_has_upper(up);
            if (hl) {
                double p = detail::kIpqpRepairEps * std::max(1.0, std::abs(lo));
                if (hu) {
                    p = std::min(p, detail::kIpqpRepairEps * (up - lo));
                }
                w.x(i) = std::max(w.x(i), lo + p);
            }
            if (hu) {
                double p = detail::kIpqpRepairEps * std::max(1.0, std::abs(up));
                if (hl) {
                    p = std::min(p, detail::kIpqpRepairEps * (up - lo));
                }
                w.x(i) = std::min(w.x(i), up - p);
            }
        }
    };

    // The measured complementarity of the CURRENT workspace point, through the
    // two ipqp_math.h reductions. `lo` is the smallest pair product, which is
    // spec 5.2's centrality test.
    auto measure_complementarity = [&](double &avg, double &lo) {
        double hi = 0.0;
        avg = 0.0;
        lo = 0.0;
        detail::ipqp_slack_complementarity(w.s, w.yi, mi, avg, lo, hi);
        detail::ipqp_augment_bound_complementarity(w.x, bounds.lower, bounds.upper, w.zl, w.zu, n,
                                                   mi, avg, lo, hi);
        if (npairs <= 0) {
            avg = 0.0;
            lo = 0.0;
        }
    };

    // --- cold start (spec 5.6) --------------------------------------------
    //
    // x_0 is the SQP's own start point -- the step-space origin -- which the box already carries
    // as its CENTRE, so the start point and the window derive from one value, not two.
    //
    // A LAMBDA because spec 5.5's warm-kill RE-RUNS it in place: a second spelling of the cold
    // start could drift from this one.
    auto cold_start = [&]() {
        mu0 = iopts.ipqp_init_mu;
        w.x = out.box.centre;
        push_into_box();
        w.ye.setZero();
        w.zl.setZero();
        w.zu.setZero();
        if (mi > 0) {
            w.s = (qp.bi - qp.Ai * w.x).cwiseMax(detail::kIpqpSlackInit);
            w.yi = mu0 * w.s.cwiseInverse();
        }
        // THE COLD DUALS ARE PLACED SO EVERY COMPLEMENTARY PAIR EQUALS mu_0 EXACTLY. Spec 5.6
        // words this as the SAY shift from `lambda_0 = 0`; from a zero multiplier that shift is
        // `mu_0 / distance` in closed form, which makes `mu_measured == ipqp_init_mu` exact.
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
        mu_meas = mu0;
    };

    // --- the warm restart (spec 5.2/5.3) ----------------------------------
    //
    // The section 5.3 clamp, ON EVERY BRANCH so the counter's iff contract holds on the
    // repair-off arm too (fix round 1, F2). `mu` is adopted as STATE, bounded by the measured
    // floor and the cold default.
    auto clamp_mu0 = [&](double payload_mu) {
        double lo_pair = 0.0;
        measure_complementarity(mu_meas, lo_pair);
        const double floor_only = std::clamp(mu_meas, iopts.ipqp_min_mu, iopts.ipqp_init_mu);
        mu0 = std::clamp(std::max(mu_meas, iopts.ipqp_mu_adopt_factor * std::max(payload_mu, 0.0)),
                         iopts.ipqp_min_mu, iopts.ipqp_init_mu);
        if (mu0 > floor_only) {
            out.counters.ipqp_mu_adopted = 1;
        }
        return lo_pair;
    };

    // THE DOMAIN GATE ON AN UNREPAIRED SEED (fix round 1, R1): `ipqp_warm_repair` disables the
    // REPAIR, never the validation, so an invariant-breaking seed degrades COLD rather than
    // being consumed raw. `.superpowers/w1-t7-report.md` FIX ROUND 2.
    auto seed_needs_the_repair = [&](const IpqpSeed &sd) {
        for (Index j = 0; j < mi; ++j) {
            if (!(sd.s(j) > 0.0) || !(sd.lambda_i(j) > 0.0)) {
                return true;
            }
        }
        for (Index i = 0; i < n; ++i) {
            if (detail::ipqp_has_lower(bounds.lower(i)) &&
                (!(sd.x(i) > bounds.lower(i)) || !(sd.zl(i) > 0.0))) {
                return true;
            }
            if (detail::ipqp_has_upper(bounds.upper(i)) &&
                (!(sd.x(i) < bounds.upper(i)) || !(sd.zu(i) > 0.0))) {
                return true;
            }
        }
        return false;
    };

    // R4 (settler ruling, fix round 1): the SAY shift's own delta_p/delta_d,
    // captured for `ipqp.restart` only -- report values, no trajectory
    // change. 0.0 when the SAY shift never runs.
    double trace_restart_shift_p = 0.0;
    double trace_restart_shift_d = 0.0;

    // Section 5.2 in three steps -- strict positivity, the SAY shift, proximal re-centering --
    // with the 5.3 clamp between the first two, since the shift's target IS `mu_0`. Every move
    // folds into `shift_max`. `.superpowers/w1-t7-report.md` FIX ROUND 2, section 4.
    auto warm_start_from = [&](const IpqpSeed &sd) {
        w.x = sd.x;
        clamp_seed_into_box();
        w.ye = sd.lambda_e;
        w.yi = sd.lambda_i;
        w.zl = sd.zl;
        w.zu = sd.zu;
        w.s = sd.s;

        // The clamp above is a repair move like any other: the seed named a
        // point the tier may not start from, and the tier moved it.
        double shift_max = (w.x - sd.x).lpNorm<Eigen::Infinity>();
        const auto raise = [&shift_max](double &v, double floor) {
            if (v < floor) {
                shift_max = std::max(shift_max, floor - v);
                v = floor;
            }
        };

        // 1. STRICT POSITIVITY (5.2 item 1). The slack is RECOMPUTED from `bi - Ai x`, since the
        //    seed's `s` belongs to the previous subproblem's `(Ai, bi)`; a recompute off an
        //    ABSENT block is not counted. `.superpowers/w1-t7-report.md` FIX ROUND 2.
        if (mi > 0) {
            const Vec s_new = (qp.bi - qp.Ai * w.x).cwiseMax(detail::kIpqpRepairSlackEps);
            if (sd.s.squaredNorm() > 0.0) {
                shift_max = std::max(shift_max, (s_new - sd.s).lpNorm<Eigen::Infinity>());
            }
            w.s = s_new;
            for (Index j = 0; j < mi; ++j) {
                raise(w.yi(j), detail::kIpqpRepairEps);
            }
        }
        for (Index i = 0; i < n; ++i) {
            // A price on an ABSENT side is not a pair: forced to 0, and that
            // is NOT a repair shift -- the residue there is a trust-region
            // dual, and TR duals are internal.
            if (detail::ipqp_has_lower(bounds.lower(i))) {
                raise(w.zl(i), 0.0);
            } else {
                w.zl(i) = 0.0;
            }
            if (detail::ipqp_has_upper(bounds.upper(i))) {
                raise(w.zu(i), 0.0);
            } else {
                w.zu(i) = 0.0;
            }
        }

        // 2. THE 5.3 CLAMP, on the repaired point's own complementarity.
        double lo_pair = clamp_mu0(sd.mu);

        // The mu_0-relative floor of 5.2 item 1 -- a clamp, not a refusal, since a price at exact
        // 0 is legitimate. THE BASE GRADE TAKES IT ADDITIVELY instead: `max(z, 0) + eps`.
        // `.superpowers/w1-t7-report.md` FIX ROUND 2.
        const double eps = detail::kIpqpRepairEps * mu0;
        const bool additive_eps = sd.grade == IpqpRestartGrade::kBaseWarm;
        for (Index j = 0; j < mi; ++j) {
            raise(w.yi(j), eps);
        }
        for (Index i = 0; i < n; ++i) {
            const bool hl = detail::ipqp_has_lower(bounds.lower(i));
            const bool hu = detail::ipqp_has_upper(bounds.upper(i));
            if (additive_eps) {
                if (hl) {
                    w.zl(i) += eps;
                }
                if (hu) {
                    w.zu(i) += eps;
                }
                if (hl || hu) {
                    shift_max = std::max(shift_max, eps);
                }
                continue;
            }
            if (hl) {
                raise(w.zl(i), eps);
            }
            if (hu) {
                raise(w.zu(i), eps);
            }
        }

        // 3. THE SAY TWO-SCALAR SHIFT (5.2 item 2), applied ONLY to a seed
        //    that is not already centred: a good warm seed pays nothing, which
        //    is what keeps `ipqp_restart_repairs` a signal.
        measure_complementarity(mu_meas, lo_pair);
        if (npairs > 0 && lo_pair < detail::kIpqpSayCentralityFactor * mu0) {
            double sum_d = 0.0;
            double sum_z = 0.0;
            for (Index j = 0; j < mi; ++j) {
                sum_d += w.s(j);
                sum_z += w.yi(j);
            }
            for (Index i = 0; i < n; ++i) {
                if (detail::ipqp_has_lower(bounds.lower(i))) {
                    sum_d += w.x(i) - bounds.lower(i);
                    sum_z += w.zl(i);
                }
                if (detail::ipqp_has_upper(bounds.upper(i))) {
                    sum_d += bounds.upper(i) - w.x(i);
                    sum_z += w.zu(i);
                }
            }
            const double npd = static_cast<double>(npairs);
            const double delta_d =
                sum_d > 0.0 ? detail::kIpqpSayTargetFraction * mu0 * npd / sum_d : 0.0;
            const double delta_p =
                sum_z > 0.0 ? detail::kIpqpSayTargetFraction * mu0 * npd / sum_z : 0.0;
            trace_restart_shift_p = delta_p; // R4: report-only, ahead of the moves below.
            trace_restart_shift_d = delta_d;
            // THE PRIMAL SCALAR MOVES THE SLACKS ONLY: a variable's two bound
            // distances are both functions of one `x`, so no scalar can raise
            // them together. Bound pairs are centred on the dual side alone.
            if (mi > 0 && delta_p > 0.0) {
                w.s.array() += delta_p;
                shift_max = std::max(shift_max, delta_p);
            }
            if (delta_d > 0.0) {
                if (mi > 0) {
                    w.yi.array() += delta_d;
                }
                for (Index i = 0; i < n; ++i) {
                    if (detail::ipqp_has_lower(bounds.lower(i))) {
                        w.zl(i) += delta_d;
                    }
                    if (detail::ipqp_has_upper(bounds.upper(i))) {
                        w.zu(i) += delta_d;
                    }
                }
                shift_max = std::max(shift_max, delta_d);
            }
            measure_complementarity(mu_meas, lo_pair);
        }

        if (shift_max > 0.0) {
            out.counters.ipqp_restart_repairs = 1;
            out.counters.ipqp_restart_shift_max = shift_max;
        }

        // 4. PROXIMAL RE-CENTERING (5.2 item 3): the estimates are the REPAIRED point, never the
        //    seed's own -- a centre inherited from another subproblem would anchor this one
        //    where its data never named.
        w.zeta = w.x;
        w.lam_est_e = w.ye;
        w.lam_est_i = w.yi;
    };

    // The repair-off arm. The seed is taken as given and only `mu_0` is
    // clamped; a seed that would have NEEDED the repair degraded cold above
    // (ruling R1), so nothing here consumes a zero slack or a zero price.
    auto warm_start_unrepaired = [&](const IpqpSeed &sd) {
        w.x = sd.x;
        w.ye = sd.lambda_e;
        w.yi = sd.lambda_i;
        w.zl = sd.zl;
        w.zu = sd.zu;
        w.s = sd.s;
        w.zeta = sd.zeta;
        w.lam_est_e = sd.lambda_est_e;
        w.lam_est_i = sd.lambda_est_i;
        (void)clamp_mu0(sd.mu);
    };

    // task 8: the seed's OWN `mu`, captured before any repair touches it --
    // the trace's `mu_payload` field (`ipqp.restart`, emitted near this
    // solve's end once `mu0`/the repair counters are all final too).
    double trace_payload_mu = 0.0;

    // COPIED FIRST: the driver hands this call `warm_carry()`, a pointer into
    // this instance, and the commit at the end of the solve writes that same
    // object.
    if (started_warm) {
        const IpqpSeed seed_copy = *seed;
        trace_payload_mu = seed_copy.mu;
        if (iopts.ipqp_warm_repair) {
            out.restart_grade = seed_copy.grade;
            warm_start_from(seed_copy);
        } else if (seed_needs_the_repair(seed_copy)) {
            // RULING R1: repair off is not a licence to consume a defective
            // seed. Degrade cold, mode-local, reported as `kCold`.
            started_warm = false;
            cold_start();
        } else {
            out.restart_grade = seed_copy.grade;
            warm_start_unrepaired(seed_copy);
        }
    } else {
        cold_start();
    }

    // Spec 5.5 as amended by plan section 7 note (c): the warm budget is
    // `min(ipqp_warm_iter_budget, effective ipqp_max_iter)`, so a caller value larger than the
    // solve's own budget can never itself bind. `warm_live` falls once the kill has fired.
    const Index warm_budget = std::min(iopts.ipqp_warm_iter_budget, iter_budget);
    bool warm_live = started_warm;

    // THE BUDGETS ARE PER ATTEMPT (spec 5.5): a second overrun after the kill
    // is an ORDINARY budget escape against the ordinary budget, not the
    // abandoned attempt's remainder. `.superpowers/w1-t7-report.md` FIX ROUND 3.
    Index iters_base = 0;
    Index facts_base = before.factorize_count;

    // --- the (rho, delta) schedule (spec 3.2) -----------------------------
    //
    // TWO QUANTITIES, KEPT APART (T4b exists because the code once treated them as one number):
    // `rho_sched`/`delta_sched` are section 3.2's PROXIMAL schedule and ARE the subproblem --
    // matrix, RHS and gate; `rho_dem` is 2.2's per-iteration MODIFICATION, matrix-only.
    //
    // THE LADDER'S SOLVE-SCOPED STATE IS ITS MEMORY, NOT A FLOOR (plan section 7 note (p)):
    // `rho_dem_last` is IC's memory, `rho_dem_max` the high-water mark the 6.2 window-discard
    // rule watches, `consec_modified` the count IC-1's skip rule reads.
    double rho_sched = iopts.ipqp_rho_init;
    double delta_sched = iopts.ipqp_delta_init;
    double rho_dem_last = 0.0;
    double rho_dem_max = 0.0;
    double rho_dem_final = 0.0;
    int consec_modified = 0;
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
    // T4c's disclosure band: the item 4 read's own `assemble` call (`weak_scale > 0`, exactly
    // once per solve) fills these with the band-counted sides' indices; every ladder rung's
    // `weak_scale == 0` call never touches them. See `.superpowers/w1-t4c-report.md`.
    std::vector<Index> band_lower_idx, band_upper_idx;
    // T4c fix round 3 (T4): the `mu_measured` the band read itself used,
    // captured at that one call site -- asserted equal to `mu_meas` at the
    // exponent test below, so a future second `mu` cannot silently diverge.
    double mu_at_band_read = std::numeric_limits<double>::quiet_NaN();
    // Section 2.2's evidence-failure policy, ARMED ONCE PER SOLVE: a factorization succeeded and
    // reported no usable inertia, so steps from there on are taken at a conservative floor and
    // the certificate is downgraded. Once armed it never disarms -- "for the WHOLE SOLVE".
    bool evidence_failed = false;
    // Raw item-4 verdict; `out.read_kept_tight` is derived from it only after
    // every downgrade path has run (T4c R2, report).
    bool kept_tight_raw = false;

    // --- THE SECTION 6.2 / 6.3 WINDOW -------------------------------------
    //
    // ONE WINDOW SERVES BOTH TESTS by design: 6.2's stall and 6.3's infeasible-suspect are both
    // "a window over which nothing improved", differing only in the other signal they pair it
    // with. SSN's three properties hold: accepted steps only, whole-window, safeguard discards.
    //
    // THE WINDOW-DISCARD RULE, STATED ONCE AND ONLY HERE; every other site that touches it
    // points back to this paragraph and says nothing more (co-review I-1, twice).
    //
    //     A window is discarded when, and only when, the INERTIA-DEMANDED SAFEGUARD REACHES A
    //     LEVEL IT HAD NOT REACHED BEFORE -- i.e. when `rho_dem_max` RISES. A move of the
    //     3.2 schedule does NOT discard it, nor does `rho_dem` cycling inside a visited band.
    //
    // The high-water reading is T4b's and the literal successor of the pre-T4b monotone floor.
    // It is a DATED AMENDMENT of 6.2's text, not a clarification, ratified on two non-empirical
    // grounds; both, and the WITHDRAWN unreachability claim, are plan section 7 note (l).
    const Index stall_w = iopts.ipqp_stall_window;
    bool win_armed = false;
    Index win_steps = 0;
    double win_mu0 = 0.0;
    double win_res0 = 0.0;
    double win_primal0 = 0.0;
    double win_dual0 = 0.0;
    double win_alpha_min = kInf;
    double win_alpha_max = 0.0;
    Vec win_ye, win_yi, win_zl, win_zu;
    // The multiplier norm BEFORE the most recently accepted step, and the state at the SOLVE'S
    // OWN START POINT. Both exist for section 6.3's exhaustion route, where the divergence and
    // the last progress are the SAME accepted step, so a windowed ratio would read 1.
    double dual_prev = 0.0;
    double dual_start = 0.0;
    Vec start_ye, start_yi, start_zl, start_zu;
    bool have_start = false;
    // T4c exponent test: the bound duals and mu at the SECOND-TO-LAST accepted iterate. R1 (fix
    // round 2): `attempt_accepted` counts ACCEPTED steps in THIS ATTEMPT only, so the snapshot
    // never treats the seed as z_{k-1}, and both are reset on a warm-kill (see there).
    Vec prev_zl, prev_zu;
    double prev_mu = 0.0;
    bool have_prev_accepted = false;
    Index attempt_accepted = 0;
    // Section 6.3's third required item: the LEAST-INFEASIBLE point seen.
    double best_primal = kInf;
    Vec best_x;

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
        // distance * multiplier, so dividing by the largest multiplier asks "how close to the
        // boundary is the pair" -- scale-free in the objective, like the stationarity fold above.
        const double sc =
            std::max({1.0, w.yi.lpNorm<Eigen::Infinity>(), w.zl.lpNorm<Eigen::Infinity>(),
                      w.zu.lpNorm<Eigen::Infinity>()});
        r.complementarity = std::max(std::abs(hi), std::abs(lo)) / sc;
        return r;
    };

    // `||(y, z)||inf` at the current iterate -- section 6.3's growth signal. Guarded per block
    // because an infinity norm of an EMPTY Eigen vector is an assert, not a 0, and a QP with no
    // rows at all is legal here.
    auto dual_norm_now = [&]() {
        double d = std::max(w.zl.lpNorm<Eigen::Infinity>(), w.zu.lpNorm<Eigen::Infinity>());
        if (me > 0) {
            d = std::max(d, w.ye.lpNorm<Eigen::Infinity>());
        }
        if (mi > 0) {
            d = std::max(d, w.yi.lpNorm<Eigen::Infinity>());
        }
        return d;
    };

    // Capture the window's reference state at the CURRENT iterate. Called on the first pass,
    // after a window discard (the rule is at the stall-window declarations above), and after any
    // window that closed without firing -- a measurement that has been read is spent.
    auto arm_window = [&](const IpqpResiduals &r) {
        win_armed = true;
        win_steps = 0;
        win_mu0 = mu_meas;
        win_res0 = std::max({r.primal_eq, r.primal_iq, r.stationarity});
        win_primal0 = std::max(r.primal_eq, r.primal_iq);
        win_dual0 = dual_norm_now();
        win_alpha_min = kInf;
        win_alpha_max = 0.0;
        win_ye = w.ye;
        win_yi = w.yi;
        win_zl = w.zl;
        win_zu = w.zu;
        if (!have_start) {
            have_start = true;
            dual_start = win_dual0;
            dual_prev = win_dual0;
            start_ye = w.ye;
            start_yi = w.yi;
            start_zl = w.zl;
            start_zu = w.zu;
        }
    };

    // Signal (a) of section 6.3, shared by both routes: the primal residual is FLAT ON A POSITIVE
    // FLOOR over the window. Both halves are load-bearing -- "flat" alone is what a CONVERGED
    // solve looks like, "on a positive floor" what every unconverged iteration looks like.
    auto primal_flat_on_floor = [&](const IpqpResiduals &r, double feas_target,
                                    double *primal_now_out, double *impr_out) {
        const double primal_now = std::max(r.primal_eq, r.primal_iq);
        const double impr = win_primal0 > 0.0 ? 1.0 - primal_now / win_primal0 : 0.0;
        *primal_now_out = primal_now;
        *impr_out = impr;
        return primal_now > feas_target && impr < (1.0 - detail::kSsnStallImproveFactor);
    };

    // Fill the escape's evidence block. `reference_*` is whichever multiplier snapshot the ROUTE
    // selected -- the window's for the standing route, the solve's start point for the exhaustion
    // route -- so the Farkas direction is the same increment the growth conjunct measured.
    auto fill_infeasibility_evidence =
        [&](const IpqpResiduals &r, bool exhaustion, double primal_now, double impr,
            double dual_now, double growth, double step_growth, const Vec &ref_ye,
            const Vec &ref_yi, const Vec &ref_zl, const Vec &ref_zu) {
            IpqpInfeasibilityEvidence &ev = out.infeasibility_evidence;
            ev.fired = true;
            ev.exhaustion_route = exhaustion;
            ev.window = win_steps;
            ev.primal_start = win_primal0;
            ev.primal_end = primal_now;
            ev.primal_improvement = impr;
            ev.dual_norm_start = exhaustion ? dual_start : win_dual0;
            ev.dual_norm_end = dual_now;
            ev.dual_growth = growth;
            ev.dual_step_growth = step_growth;
            ev.least_infeasible_x = best_x.size() > 0 ? best_x : w.x;
            ev.least_infeasible_primal =
                best_x.size() > 0 ? best_primal : std::max(r.primal_eq, r.primal_iq);
            if (iopts.ipqp_farkas_gate) {
                const Vec dye = me > 0 ? Vec(w.ye - ref_ye) : Vec(0);
                const Vec dyi = mi > 0 ? Vec(w.yi - ref_yi) : Vec(0);
                ev.farkas_corroborated =
                    ipqp_farkas_corroborates(qp, bounds, dye, dyi, w.zl - ref_zl, w.zu - ref_zu,
                                             &ev.farkas_residual, &ev.farkas_gap);
            }
        };

    // SECTION 6.3'S EXHAUSTION-ROUTE VARIANT, consulted at every budget stop.
    //
    // The growth conjunct is measured against the SOLVE'S START POINT and must ADDITIONALLY have
    // multiplied by `kSsnDualStepGrowth` across the last accepted step -- `ssn_engine.h`'s own
    // two-part rule: a windowed reference is meaningless here, a start-point one alone vacuous.
    //
    // Returns false -- leaving the escape as `kBudget` -- unless BOTH conjuncts and the per-step
    // ratio hold, which keeps a plain budget exhaustion from being relabelled as a suspicion.
    auto exhaustion_infeasible = [&](const IpqpResiduals &r, double feas_target) {
        if (!win_armed || win_steps < 1 || !have_start) {
            return false;
        }
        double primal_now = 0.0;
        double primal_impr = 0.0;
        if (!primal_flat_on_floor(r, feas_target, &primal_now, &primal_impr)) {
            return false;
        }
        const double dual_now = dual_norm_now();
        const double growth = dual_now / std::max(1.0, dual_start);
        const double step_growth = dual_now / std::max(1.0, dual_prev);
        if (growth < detail::kSsnDualGrowthFactor || step_growth < detail::kSsnDualStepGrowth) {
            return false;
        }
        fill_infeasibility_evidence(r, /*exhaustion=*/true, primal_now, primal_impr, dual_now,
                                    growth, step_growth, start_ye, start_yi, start_zl, start_zu);
        return true;
    };

    // THE TWO PRIMAL REGULARIZATIONS ARE DIFFERENT OBJECTS AND ENTER THE
    // MATRIX DIFFERENTLY (spec 2.2 items 2-3 / 3.1 / 3.2 as amended by the T4b
    // plan of record, plan section 7 note (p)):
    //
    //   `rho_sched` is 3.2's PROXIMAL term and DEFINES the subproblem: diagonal, right-hand side
    //   and gate residual alike, on the CALLER'S scale (hence `dsq`). `rho_dem` is 2.2's
    //   MODIFICATION: anchored at the CURRENT iterate, so it NEVER reaches the RHS, and ADDED.
    //
    // AND IT IS APPLIED IN THE RUIZ-SCALED SYSTEM, the system the inertia is read on: `assemble`
    // equilibrates the UNMODIFIED matrix and this writes `(...) * dsq(i) + rho_dem`, a UNIFORM
    // shift in every scaled coordinate. `ipqp_reg_max` caps `rho_dem`, the scaled value.
    //
    // THE `rho_dem == 0` BRANCH IS EXPLICIT AND MUST STAY THAT WAY: folding `+ rho_dem` into the
    // sum would re-associate the expression and move the last bits of every convex solve -- the
    // branch is what keeps the convex corpus BIT-IDENTICAL (T4b close gate 4). Primal only.
    auto write_primal_diagonal = [&](double rho_sched, double rho_dem) {
        double *vals = kkt_.matrix().valuePtr();
        const std::vector<double> &src = layout_.primal_diag_source();
        if (rho_dem == 0.0) {
            for (Index i = 0; i < n; ++i) {
                vals[layout_.primal_diag_slot(i)] =
                    (src[static_cast<std::size_t>(i)] + rho_sched + w.sigma(i)) * w.dsq(i);
            }
        } else {
            for (Index i = 0; i < n; ++i) {
                vals[layout_.primal_diag_slot(i)] =
                    (src[static_cast<std::size_t>(i)] + rho_sched + w.sigma(i)) * w.dsq(i) +
                    rho_dem;
            }
        }
    };

    auto write_diagonals = [&](double rho_sched, double rho_dem, double delta) {
        double *vals = kkt_.matrix().valuePtr();
        const std::vector<double> &src = layout_.primal_diag_source();
        if (rho_dem == 0.0) {
            for (Index i = 0; i < n; ++i) {
                vals[layout_.primal_diag_slot(i)] =
                    (src[static_cast<std::size_t>(i)] + rho_sched + w.sigma(i)) * w.dsq(i);
            }
        } else {
            for (Index i = 0; i < n; ++i) {
                vals[layout_.primal_diag_slot(i)] =
                    (src[static_cast<std::size_t>(i)] + rho_sched + w.sigma(i)) * w.dsq(i) +
                    rho_dem;
            }
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

    // I6 / spec 3.4's SEPARATE FACTORIZATION CAP, enforced BEFORE every factorization, not once
    // per iteration: a ladder rung is a factorization too, and at cap 1 the first ladder paid
    // three before anything stopped it. Call sites consult the flag, never the reading.
    auto factorize_once = [&]() {
        if (kkt_.counters().factorize_count - facts_base >= fact_budget) {
            fact_budget_hit = true;
            return false;
        }
        if (!analyzed_) {
            kkt_.compute();
            // A backend SYMBOLIC failure does not propagate out of compute(); it is reported as
            // InvalidInput, and leaving `analyzed_` false is what stops the next refactorize().
            analyzed_ = (kkt_.info() != Eigen::InvalidInput);
        } else if (first_factorization) {
            // THE ONE-TIME O(nnz) PAYMENT per tier entry (plan section 7 note a): the first
            // factorization re-checks that the buffer still carries the analyzed pattern, every
            // later one declares it -- licensed by IpqpKktLayout being the buffer's only writer.
            kkt_.refactorize(KktFactorization::PatternCheck::kVerify);
        } else {
            kkt_.refactorize(KktFactorization::PatternCheck::kAssumeAnalyzed);
        }
        first_factorization = false;
        return true;
    };

    // Assemble the section 3.1 system at (rho, delta) from scratch: refresh the condensed bound
    // curvature, re-scatter H/Ae/Ai through the layout's plan, write the four diagonal families,
    // and (when equilibration is on) compute and apply the Ruiz diagonal.
    //
    // THE RUIZ DIAGONAL IS FIXED FOR THE WHOLE LADDER, deliberately: a rung rewrites only the
    // four diagonal families, each pre-multiplied by its own `dsq`, so recomputing the
    // equilibration per rung would move the matrix under the ladder.
    //
    // THE EQUILIBRATION IS COMPUTED ON THE UNMODIFIED SYSTEM (`rho_dem = 0`) and the modification
    // is written into the scaled matrix: `rho_dem` is a UNIFORM shift of the SCALED matrix, so
    // letting it participate in computing `D` would make `D` a function of the rung.
    auto assemble = [&](double rho_sched, double rho_dem, double delta, double weak_scale,
                        double band_upper = 0.0) {
        w.sigma.setZero();
        // `weak_scale <= 0` is the ordinary path and reproduces `ipqp_accumulate_bound_sigma`
        // exactly, which is what keeps every iteration's assembly -- and the convex corpus with
        // it -- bit-identical across the critical-cone rule.
        detail::ipqp_accumulate_bound_sigma_critical_cone(w.x, bounds.lower, bounds.upper, w.zl,
                                                          w.zu, n, weak_scale, w.sigma, band_upper,
                                                          &band_lower_idx, &band_upper_idx);

        const bool relaid = layout_.sync(qp.H, qp.Ae, qp.Ai, n, me, mi, kkt_.matrix());
        if (relaid) {
            analyzed_ = false;
        }

        w.dsq.setOnes(w.dim);
        write_diagonals(rho_sched, 0.0, delta);
        if (iopts.ipqp_ruiz) {
            ruiz_equilibrate(kkt_.matrix(), w.dscale, w.ruiz_work);
            w.dsq = w.dscale.array().square();
        } else {
            w.dscale.setOnes(w.dim);
        }
        if (rho_dem != 0.0) {
            // PRIMAL ONLY: the slack and pivot families were written with `dsq == 1` above and
            // then scaled in place by Ruiz, so rewriting them would recompute identical values.
            write_primal_diagonal(rho_sched, rho_dem);
        }
    };

    // R1 (fix round 2): what `evidence_for_read` actually returned (injected
    // or real), so `ipqp.iter` reports the algorithm's own read, not a
    // second unseamed one. Copied only when a sink is attached.
    hven::linear::InertiaEvidence trace_last_evidence{};

    // Returns the reading, or -- when the factorization budget refused the call -- `kUnreadable`
    // with `fact_budget_hit` raised. Callers must test the flag FIRST: a budget stop is
    // `kBudget`, never a verdict about a factorization that never ran.
    auto factor_and_read = [&](bool final_read) {
        if (!factorize_once()) {
            return InertiaRead::kUnreadable;
        }
        // THE FACTORIZATION'S OWN OUTCOME IS READ BEFORE ITS EVIDENCE, and that ordering carries
        // task 5's split: 2.2's policy permits a step against a factor that EXISTS but could not
        // be interrogated, and there is no factor at all here. `info()` is the discriminator.
        if (kkt_.info() != Eigen::Success) {
            return InertiaRead::kFactorFailed;
        }
        const hven::linear::InertiaEvidence &ev = evidence_for_read(kkt_, final_read);
        if (trace_ != nullptr && !final_read) {
            trace_last_evidence = ev; // R1: the same object classify_inertia sees.
        }
        return classify_inertia(ev, expect_pos, expect_neg);
    };

    // EXACTLY ONE FACTORIZATION AND ONE READING (spec 2.2 item 4's own cost
    // statement: "+1 factorization per certified guarded solve"). No ladder,
    // and -- fix round 1, C0b -- no perturbed retry either.
    //
    // The ABSENCE OF A LADDER is the point of keeping this separate from `factorize_with_ladder`:
    // a certification read that CLIMBED is precisely how a saddle gets certified as a minimum.
    //
    // THE PERTURBED RETRY WENT for two reasons pointing the same way: it made the spec's "one
    // extra factorization" two, and a perturbed reading is not evidence about the assembled
    // matrix at all -- so the honest outcome is note (h)'s `ipqp_final_inertia_read == 2`.
    //
    // NO INERTIA-DEMANDED MODIFICATION EITHER (T4b): the read is taken at `rho_dem = 0` BY
    // CONSTRUCTION, never from the ladder's memory. Seeding it would ask the item 4 question
    // about the MODIFIED system; a mutation that does so must fail A11's final-read pins.
    auto factorize_and_read_once = [&](double rho_sched, double delta) {
        // THE READ IS TAKEN ON THE CRITICAL CONE (fix round 1, settler ruling R1): at a bound
        // whose multiplier is essentially zero the off-bound direction IS in the cone, so the
        // read must verify it. Rule and derivation: `ipqp_accumulate_bound_sigma_critical_cone`.
        //
        // WHAT IT COSTS: nothing -- the same single factorization, one extra comparison per bound
        // side. What it BUYS is the wrong-answer class gate 8 measured (w1-t4b-report.md).
        //
        // A DROPPED SIDE CAN ONLY COST A DOWNGRADE, NEVER A FALSE CERTIFICATE: removing curvature
        // can turn a right reading wrong, but can never turn a wrong reading right.
        const double weak_scale = detail::kIpqpWeakActiveFactor * std::sqrt(std::max(mu_meas, 0.0));
        mu_at_band_read = mu_meas; // T4c T4: what this read's own scale used.
        // T4c fix round 1: cleared here, the read's own one call site, so a
        // stale index from an earlier read can never survive into this one.
        band_lower_idx.clear();
        band_upper_idx.clear();
        const double band_upper = detail::kIpqpTightBandFactor * weak_scale;
        assemble(rho_sched, /*rho_dem=*/0.0, delta, weak_scale, band_upper);
        return factor_and_read(/*final_read=*/true);
    };

    // ALGORITHM IC'S FIRST TRIAL for this iteration (Wachter-Biegler 2006,
    // step IC-1 / IC-2; the four constants and hven's two declared adaptations
    // are in `detail`'s ladder banner).
    //
    //   * NO MEMORY, or fewer than `kIpqpLadderSkipAfter` consecutive modified iterations:
    //     TRY ZERO (IC-1). OTHERWISE: `max(ipqp_reg_floor, rho_dem_last / kIpqpLadderDown)`,
    //     probing down for the smallest sufficient value; a refused probe is a RECLIMB.
    //
    // THE EVIDENCE-FAILURE FLOOR RIDES THE TRIAL, NOT THE LADDER, once armed: on the backend that
    // clause exists for EVERY factorization reports no usable evidence, so this keeps such a
    // solve at ONE factorization per iteration. IC-1's zero trial is skipped while it stands.
    auto ladder_trial = [&]() {
        double trial = 0.0;
        if (rho_dem_last > 0.0 && consec_modified >= detail::kIpqpLadderSkipAfter) {
            trial = std::max(iopts.ipqp_reg_floor, rho_dem_last / detail::kIpqpLadderDown);
        }
        if (evidence_failed) {
            trial =
                std::max(trial, std::min(detail::kIpqpEvidenceFailureRhoFloor, iopts.ipqp_reg_max));
        }
        return trial;
    };

    // Assemble at `(rho_sched, rho_dem, delta)` and factorize, climbing Algorithm IC's ladder
    // until the inertia is the section 4.1 target or the ceiling is reached. `rho_dem`/`delta`
    // are in/out so the caller sees where the ladder stopped; `rho_sched` is never touched.
    auto factorize_with_ladder = [&](double rho_sched, double &rho_dem, double &delta) {
        // WHETHER A `rho_dem_last / kIpqpLadderDown` VALUE IS CURRENTLY BEING TRIED, and whether
        // its refusal has already been charged as a reclimb. ONE CHARGE PER ITERATION: the
        // counter answers "did the memory's guess come back too small", not "how many rungs".
        //
        // TWO ROUTES REACH THAT VALUE and both must be charged (fix round 1, I2 / CX4): this
        // iteration's FIRST trial once the skip rule licenses it, and the rung that ANSWERS a
        // refused zero-trial before then -- which round 1 charged not at all.
        //
        // (While an evidence failure stands the trial is the POLICY FLOOR, not IC's probe, so its
        // refusal is not a reclimb.)
        bool trying_memory_value = rho_dem > 0.0 && !evidence_failed;
        bool reclimb_charged = false;
        // Consecutive primal escalations spent on an unresolved perturbed-pivot
        // report, and whether this iteration has given up on the primal route;
        // see `kIpqpPivotReroutePrimalMax`.
        int perturbed_primal_run = 0;
        bool perturbed_dual_latched = false;
        assemble(rho_sched, rho_dem, delta, /*weak_scale=*/0.0);

        for (;;) {
            const InertiaRead read = factor_and_read(/*final_read=*/false);
            if (fact_budget_hit || read == InertiaRead::kOk || read == InertiaRead::kFactorFailed) {
                if (read == InertiaRead::kOk && rho_dem > 0.0) {
                    // IC UPDATES ITS MEMORY ONLY ON A SUCCESSFUL **MODIFIED** FACTORIZATION. A
                    // success at `rho_dem == 0` leaves it untouched, exactly as the paper does,
                    // so the next iteration that needs one descends from a sufficient value.
                    rho_dem_last = rho_dem;
                }
                return read;
            }
            if (read == InertiaRead::kUnreadable) {
                // SECTION 2.2'S EVIDENCE-FAILURE POLICY (spec 2.2, `InertiaEvidence::State !=
                // kObserved`): a step is permitted only at a conservative `rho` floor, with the
                // certificate downgraded for the whole solve.
                //
                // TASK 4 TERMINATED HERE (-> kNumerical) and recorded the question as open; the
                // spec text settles it, so the step is taken. Accelerate can report `kUnavailable`
                // for a good factorization; that arm stays UNOBSERVED (CLAUDE.md section 6).
                //
                // THE FLOOR IS `detail::kIpqpEvidenceFailureRhoFloor`, AN ABSOLUTE MINIMUM AND
                // NEVER A RUNG, applied here the first time and by `ladder_trial` thereafter.
                // Its own banner carries the argument: the honesty is the downgrade, not the size.
                evidence_failed = true;
                const double conservative =
                    std::min(detail::kIpqpEvidenceFailureRhoFloor, iopts.ipqp_reg_max);
                // THE ITERATION RUNS AT `max(trial, floor)` and THAT value is what IC's memory
                // records: later trials descend /3 from it and `ladder_trial` floors them again
                // while evidence stays unavailable, so such a backend runs at a constant floor.
                const double floored = std::max(rho_dem, conservative);
                rho_dem_last = floored;
                rho_dem_max = std::max(rho_dem_max, floored);
                if (floored > rho_dem) {
                    // This factorization WAS rejected -- on evidence the tier
                    // could not use, which `ipqp_inertia_retries` covers
                    // ("wrong OR evidence-invalid", fix round 1's M2).
                    ++out.counters.ipqp_inertia_retries;
                    ++out.counters.ipqp_reg_increases;
                    // task 8: `ipqp.reg`, the evidence-failure conservative
                    // floor -- the one increase reason with no ladder rung
                    // behind it.
                    if (trace_ != nullptr) {
                        IpqpTraceRegEvent ev;
                        ev.dir = IpqpTraceRegDir::kUp;
                        ev.rho = rho_sched + floored;
                        ev.delta = delta;
                        ev.reason = IpqpTraceRegReason::kFloor;
                        emit_trace_reg(ev);
                    }
                    rho_dem = floored;
                    write_diagonals(rho_sched, rho_dem, delta);
                    continue;
                }
                return read;
            }
            // The ceiling test carries SsnEngine::escalate_prox's relative slack verbatim, and for
            // its reason: repeated multiplication does not reproduce 1e6 exactly, so an exact `>=`
            // guard grants a final rung worth 2.3e-10 relative and a whole numeric factorization.
            const double cap = iopts.ipqp_reg_max * (1.0 - detail::kSsnProxCapSlack);
            // THE PERTURBED-PIVOT ROUTE, AND ITS BOUND (fix round 1, settler ruling R2). Answered
            // by the PRIMAL ladder while it is ARMED and the primal route has not been abandoned
            // this iteration; otherwise by the DUAL shift. The LATCH lasts one iteration.
            const bool perturbed = read == InertiaRead::kPerturbed;
            if (perturbed && rho_dem > 0.0 && !perturbed_dual_latched &&
                perturbed_primal_run >= detail::kIpqpPivotReroutePrimalMax) {
                perturbed_dual_latched = true;
                ++out.counters.ipqp_pivot_reroute_dual_fallback;
            }
            const bool primal_route = perturbed && rho_dem > 0.0 && !perturbed_dual_latched;
            if (primal_route) {
                ++perturbed_primal_run;
                ++out.counters.ipqp_pivot_reroute_primal;
            }
            if (!perturbed) {
                // "Consecutive" means consecutive within ONE unresolved
                // perturbation: a reading the ladder could use resets the run.
                perturbed_primal_run = 0;
            }
            if (perturbed && !primal_route) {
                // SPEC 2.2'S PERTURBED-PIVOT RULE: a perturbed factorization describes a
                // DIFFERENT matrix, so its inertia isn't evidence here; remedy is a larger
                // DUAL shift, never read as right.
                //
                // ... AND THAT REMEDY IS THE RIGHT ONE WHILE THE PRIMAL LADDER IS UNARMED, AND
                // AGAIN ONCE THE PRIMAL ROUTE HAS BEEN TRIED AND FAILED (T4b). Ruiz normalizes a
                // diagonal to -1 and rung 1.0 annihilates it: `.superpowers/w1-t4b-report.md`.
                //
                // THE RULE: while the ladder is armed a perturbed pivot says THIS RUNG did not
                // produce the target inertia, so the ladder advances its own quantity. It is never
                // read as right, and the TERMINAL reading classifies the escape (note (h)).
                //
                // AND IT IS BOUNDED (fix round 1, settler ruling R2): a perturbed pivot whose
                // cause is DUAL is cleared by NO primal rung, and an unbounded re-route would ride
                // the ladder to `ipqp_reg_max` for an answer the dual escalation gives in one.
                //
                // THE HONEST INSTRUMENT WOULD BE PIVOT PROVENANCE, AND THE BACKEND DOES NOT CARRY
                // IT: `InertiaEvidence` reports pivot COUNTS, never LOCATIONS. Two failed primal
                // escalations is the approximation; the two reroute counters measure its misses.
                if (delta >= cap) {
                    // I8: THE TERMINAL REJECTION IS STILL A REJECTION -- refused on evidence the
                    // tier could not use, exactly like every rung before it.
                    ++out.counters.ipqp_inertia_retries;
                    return read;
                }
                delta = std::min(delta * detail::kIpqpDeltaGrowth, iopts.ipqp_reg_max);
                if (delta >= cap) {
                    delta = iopts.ipqp_reg_max;
                }
            } else {
                if (rho_dem >= cap) {
                    ++out.counters.ipqp_inertia_retries; // I8, as above.
                    // THE CEILING RUNG IS REFUSED, SO IT DOES NOT ENTER THE MEMORY (fix round 1,
                    // CX3): the memory's whole job is to seed the NEXT trial, and `ipqp_reg_max`
                    // produced no successful modified factorization. The high-water mark shows it.
                    return read;
                }
                // ALGORITHM IC'S ESCALATION (plan 2.2's pseudocode, normative).
                if (rho_dem == 0.0) {
                    // IC-2: the unmodified trial was refused. With no memory this is `delta_w^0`;
                    // with a memory it is the memory's own /3 probe, which the skip rule had not
                    // yet licensed -- and which is a memory value for the reclimb charge below.
                    if (rho_dem_last == 0.0) {
                        rho_dem = std::min(detail::kIpqpLadderInit, iopts.ipqp_reg_max);
                    } else {
                        rho_dem =
                            std::max(iopts.ipqp_reg_floor, rho_dem_last / detail::kIpqpLadderDown);
                        trying_memory_value = !evidence_failed;
                    }
                } else {
                    // Only a WRONG reading charges a reclimb: a perturbed report on the primal
                    // route reaches this block too, and it is a backend fact, not curvature.
                    // (T4b F2; `.superpowers/w1-t4b-report.md`.)
                    if (trying_memory_value && !reclimb_charged && !perturbed) {
                        ++out.counters.ipqp_ladder_reclimbs;
                        reclimb_charged = true;
                    }
                    // TWO DECADES THROUGH THE WHOLE FIRST CLIMB, EIGHT AFTER. `rho_dem_last == 0`
                    // means this solve has never had a successful modified factorization, so
                    // nothing is known about its scale and W-B's `bar kappa_w^+` governs.
                    const double up =
                        (rho_dem_last == 0.0) ? detail::kIpqpLadderUpFirst : detail::kIpqpLadderUp;
                    rho_dem = std::min(rho_dem * up, iopts.ipqp_reg_max);
                }
                if (rho_dem >= cap) {
                    rho_dem = iopts.ipqp_reg_max;
                }
                rho_dem_max = std::max(rho_dem_max, rho_dem);
            }
            ++out.counters.ipqp_inertia_retries;
            ++out.counters.ipqp_reg_increases;
            // task 8: `ipqp.reg`, the ladder's own climb -- covers both the
            // primal and the perturbed-pivot dual route; the unmoved
            // quantity reports its unchanged value.
            if (trace_ != nullptr) {
                IpqpTraceRegEvent ev;
                ev.dir = IpqpTraceRegDir::kUp;
                ev.rho = rho_sched + rho_dem;
                ev.delta = delta;
                ev.reason = IpqpTraceRegReason::kInertia;
                emit_trace_reg(ev);
            }
            // (The emergency `> 2 * fact_budget` guard went with fix round 1's I6:
            // `factorize_once` now refuses a factorization the budget cannot pay for.)
            //
            // A RUNG IS AN ASSIGNMENT, NOT A RE-SCATTER: `dsq` and the layout's
            // `primal_diag_source()` snapshot are both still the ones `assemble()` established.
            write_diagonals(rho_sched, rho_dem, delta);
        }
    };

    // One backend solve: scale the right-hand side in, unscale the solution out. `D K D u = D b`
    // gives `u = D^-1 v` for `K v = b`, so the recovered step is `D u` -- and everything leaving
    // this lambda is on the caller's own scale, spec 4.3's non-negotiable clause.
    auto solve_system = [&]() {
        if (iopts.ipqp_ruiz) {
            w.rhs.array() *= w.dscale.array();
        }
        kkt_.solve(w.rhs, w.sol);
        if (iopts.ipqp_ruiz) {
            w.sol.array() *= w.dscale.array();
        }
    };

    // Build the right-hand side for one step. `mu_t` is the uniform complementarity target (0 on
    // the affine step); `corrector` adds Mehrotra's second-order term from the affine step.
    //
    // BOTH ARGUMENTS ARE THE SCHEDULE'S, ALWAYS. This function defines which subproblem the step
    // solves, and section 3.2's proximal terms are the whole of it. Neither of 2.2's
    // modifications belongs here: each is anchored at the CURRENT iterate, contributing zero.
    //
    // THE DUAL SIDE IS THE SYMMETRIC COMPLETION OF THAT (fix round 1, settler ruling R3): round 1
    // passed the LADDER-SETTLED `delta` here while the gate measured at `delta_sched`. The
    // escalated `delta` now enters the DIAGONAL only, exactly as `rho_dem` does.
    auto build_rhs = [&](double rho_sched, double delta_sched, double mu_t, bool corrector) {
        w.bgrad.setZero();
        detail::ipqp_accumulate_bound_barrier_gradient(w.x, bounds.lower, bounds.upper, mu_t, n,
                                                       w.bgrad);
        w.rhs.head(n) = -(w.grad + rho_sched * (w.x - w.zeta) + w.bgrad);
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
            w.rhs.segment(n + mi, me) = -(w.r_pe - delta_sched * (w.ye - w.lam_est_e));
        }
        if (mi > 0) {
            w.rhs.segment(n + mi + me, mi) = -(w.r_pi - delta_sched * (w.yi - w.lam_est_i));
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

#ifdef HVEN_TESTING
    capture_first_iterate(w.x, w.s, w.ye, w.yi, w.zl, w.zu, mu_meas);
#endif

    // --- the iteration (spec 3.1) -----------------------------------------

    for (;;) {
        refresh_distances();
        const IpqpResiduals res = residuals();
        out.residuals = res;

        if (!std::isfinite(res.worst())) {
            escape = IpqpEscape::kNumerical;
            break;
        }

        // The two targets, named once for the tests below that also read them (the 6.2 window,
        // 6.3's positive floor, the inner-loop target). The stopping rule itself is the exported
        // predicate, which reads the same two fields.
        const double opt_target = opts_.opt_tol * iopts.ipqp_converge_slack;
        const double feas_target = opts_.feas_tol * iopts.ipqp_converge_slack;

        // THE STOPPING RULE (spec 2.3 step 1 / 3.4), through the EXPORTED predicate rather than
        // inline: task 6's routing chain asks the same question of a returned IpqpResult, and two
        // statements of one rule could drift.
        //
        // SECTION 5.5's TRUST THRESHOLD (fix round 1, F3): warm DATA is trusted only if its raw
        // barrier level is inside the target too, at the warm ENTRY alone -- after a step the
        // point is this solve's. `.superpowers/w1-t7-report.md` FIX ROUND 2.
        const bool untrusted_warm_seed = warm_live && out.counters.ipqp_iters == iters_base &&
                                         mu_meas > iopts.ipqp_converge_slack * opts_.opt_tol;
        if (!untrusted_warm_seed && ipqp_residuals_meet_target(res, opts_, iopts)) {
            converged = true;
            break;
        }

        // --- the least-infeasible point (spec 6.3's third required item) ---
        {
            const double primal_now = std::max(res.primal_eq, res.primal_iq);
            if (primal_now < best_primal) {
                best_primal = primal_now;
                best_x = w.x;
            }
        }

        // --- SECTIONS 6.2 AND 6.3, THE STANDING ROUTES ---------------------
        //
        // EVALUATED BEFORE THE BUDGET CHECKS, which makes section 6.1's "budget ... OF LAST
        // RESORT: the stall test should fire first on anything genuinely stuck" structural.
        if (!win_armed) {
            arm_window(res);
        } else if (win_steps >= stall_w) {
            const double mu_ratio =
                mu_meas > 0.0 ? win_mu0 / mu_meas : std::numeric_limits<double>::infinity();
            const double res_now = std::max({res.primal_eq, res.primal_iq, res.stationarity});
            const double res_impr = win_res0 > 0.0 ? 1.0 - res_now / win_res0 : 0.0;
            double primal_now = 0.0;
            double primal_impr = 0.0;
            const bool flat = primal_flat_on_floor(res, feas_target, &primal_now, &primal_impr);
            const double dual_now = dual_norm_now();
            const double growth = dual_now / std::max(1.0, win_dual0);

            // SECTION 6.3 IS TESTED FIRST, and the order is a ruling. A subproblem can satisfy
            // both signatures at once, and the two escapes go to different places: `kStall`
            // routes onward, `kInfeasibleSuspect` is what the W2 feasibility hook answers.
            if (flat && growth >= detail::kSsnDualGrowthFactor) {
                fill_infeasibility_evidence(res, /*exhaustion=*/false, primal_now, primal_impr,
                                            dual_now, growth, 0.0, win_ye, win_yi, win_zl, win_zu);
                escape = IpqpEscape::kInfeasibleSuspect;
                break;
            }

            // "RESET ON A MEHROTRA TARGET CHANGE THAT ACTUALLY DROPPED `mu`" (spec 6.2) IS
            // CONJUNCT (i) READ AS A RESET: a window across which `mu` genuinely halved IS one in
            // which the target changed and took. "Reset whenever mu moved" would re-arm always.
            if (mu_ratio >= detail::kIpqpStallMuFactor) {
                arm_window(res);
            } else if (res_impr < (1.0 - detail::kSsnStallImproveFactor) &&
                       win_alpha_max < detail::kIpqpStallAlpha) {
                // ALL THREE CONJUNCTS HOLD. Conjunct (iii) is tested on `win_alpha_max` -- the
                // LARGEST per-step `min(alpha_p, alpha_d)` -- because the spec demands it "on
                // EVERY step": one healthy step anywhere in the window disarms it.
                IpqpStallEvidence &ev = out.stall_evidence;
                ev.fired = true;
                ev.window = win_steps;
                ev.mu_ratio = mu_ratio;
                ev.residual_improvement = res_impr;
                ev.min_alpha = win_alpha_min;
                ev.max_step_alpha = win_alpha_max;
                escape = IpqpEscape::kStall;
                break;
            } else {
                // The window closed and fired nothing. A measurement that has been read is spent:
                // re-arm at the current state so the next `stall_w` steps are measured against
                // where the trajectory actually is.
                arm_window(res);
            }
        }

        // --- SECTION 5.5: THE WARM-KILL ------------------------------------
        //
        // AHEAD of the two ordinary budget tests (fix round 1, R2): while WARM, budget exhaustion
        // is 5.5's overrun and takes the exactly-once cold restart; the cold attempt owns
        // ordinary escapes. `.superpowers/w1-t7-report.md` FIX ROUND 2, section 5.
        if (warm_live && out.counters.ipqp_iters - iters_base >= warm_budget) {
            warm_live = false;
            out.counters.ipqp_warm_restart_abandoned = 1;
            iters_base = out.counters.ipqp_iters;
            facts_base = kkt_.counters().factorize_count;
            cold_start();
            rho_sched = iopts.ipqp_rho_init;
            delta_sched = iopts.ipqp_delta_init;
            rho_dem_last = 0.0;
            consec_modified = 0;
            reg_gate_ref = std::numeric_limits<double>::quiet_NaN();
            // `rho_dem_max` is NOT reset: it is the solve's high-water mark and
            // the section 6.2 window-discard safeguard level, which rises and
            // never falls.
            win_armed = false;
            best_primal = kInf;
            best_x = Vec();
            have_start = false;
            // R1 (settler ruling, fix round 2): the abandoned warm attempt's accepted-iterate
            // history is CLEARED, not carried into the cold attempt -- same precedent as
            // `have_start` above.
            have_prev_accepted = false;
            attempt_accepted = 0;
            continue;
        }

        if (out.counters.ipqp_iters - iters_base >= iter_budget) {
            escape = exhaustion_infeasible(res, feas_target) ? IpqpEscape::kInfeasibleSuspect
                                                             : IpqpEscape::kBudget;
            break;
        }
        if (kkt_.counters().factorize_count - facts_base >= fact_budget) {
            escape = exhaustion_infeasible(res, feas_target) ? IpqpEscape::kInfeasibleSuspect
                                                             : IpqpEscape::kBudget;
            break;
        }

        // The safeguard's state this pass started from. Window discard: see the rule at the
        // stall-window declarations above. Sampled here rather than beside each move because the
        // high-water mark rises in TWO places, and two "reset the window" statements is two.
        const double rho_dem_max_pre = rho_dem_max;

        // Whether THIS iteration's section 3.2 gate advanced, for
        // `ipqp_iters_ladder_armed_no_advance`. Read off the prox-centre counter rather than a
        // second flag: an advance IS a prox-centre update, by the gate's own construction.
        const Index prox_updates_pre = out.counters.ipqp_prox_center_updates;

        // THE GATED DECREASE (spec 3.2), evaluated BEFORE the assembly so an iteration runs at the
        // schedule the previous iteration's progress earned. The gate is on the REGULARIZED
        // residuals; the stopping rule above is on the unregularized ones, never both at once.
        //
        // Relative on BOTH sides (kIpqpRegGateContract carries why): the regularized residual is
        // divided by the same `max(1, ...)` folds the stopping rule uses, and compared against
        // its own value at the last advance.
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
            // TWO WAYS TO EARN AN ADVANCE, and the second is not a loophole: the outer iteration
            // advances when its INNER problem is SOLVED, and contraction is only the proxy for
            // that while the inner residual is large. Deadlock measured: w1-t4-report.md.
            const double inner_target = std::min(opt_target, feas_target);
            if (R <= std::max(detail::kIpqpRegGateContract * reg_gate_ref, inner_target)) {
                reg_gate_ref = R;
                const double proposed = rho_sched * iopts.ipqp_reg_decrease;
                // NOTHING OVERRIDES THE DECREASE ANY MORE EXCEPT THE ABSOLUTE FLOOR (T4b): section
                // 2.2 item 3's monotone-per-solve floor is DELETED, so the schedule decays toward
                // the caller's QP while the ladder answers curvature on its own `rho_dem`.
                const double target = std::max(proposed, iopts.ipqp_reg_floor);
                bool moved = false;
                if (target < rho_sched) {
                    rho_sched = target;
                    moved = true;
                }
                // I7: `delta` carries no monotone floor at all (plan section 7
                // note (g): section 2.2 states the floor for `rho` only), so it
                // falls to the absolute floor on its own.
                const double dtarget =
                    std::max(delta_sched * iopts.ipqp_reg_decrease, iopts.ipqp_reg_floor);
                if (dtarget < delta_sched) {
                    delta_sched = dtarget;
                    moved = true;
                }
                // THE TWO OUTCOMES OF A GATED ADVANCE, CLASSIFIED EXACTLY ONCE (T4b re-pin; class
                // (a) is deleted with the monotone floor):
                //
                //  (b) `rho` and/or `delta` MOVED -> DECREASE (I7: the counter is the SCHEDULE's,
                //      so an advance that moved `delta` alone is an applied decrease of it).
                //  (c) NOTHING MOVED, both already on the ABSOLUTE floor -> NEITHER (I2).
                //
                // So `ipqp_prox_center_updates == ipqp_reg_decreases + (class (c) advances)` on
                // every solve; the difference is the advances taken with the schedule already on
                // the floor. The tests pin the EXACT accounting rather than the inequality.
                if (moved) {
                    ++out.counters.ipqp_reg_decreases;
                    // task 8: `ipqp.reg`, the gate's own decrease -- rho_dem
                    // is 0 outside the ladder, which this gate never touches.
                    if (trace_ != nullptr) {
                        IpqpTraceRegEvent ev;
                        ev.dir = IpqpTraceRegDir::kDown;
                        ev.rho = rho_sched;
                        ev.delta = delta_sched;
                        ev.reason = IpqpTraceRegReason::kAccept;
                        emit_trace_reg(ev);
                    }
                }
                w.zeta = w.x;
                w.lam_est_e = w.ye;
                w.lam_est_i = w.yi;
                ++out.counters.ipqp_prox_center_updates;
            }
        }

        // THE TWO REGULARIZATIONS, SELECTED SEPARATELY (T4b): `rho_sched` is the subproblem's own
        // proximal weight and is NOT touched here; `rho_dem` is the inertia-demanded modification
        // chosen fresh every iteration. Additive in the matrix; only `rho_sched` reaches the RHS.
        double rho_dem = ladder_trial();
        if (rho_dem > 0.0) {
            rho_dem_max = std::max(rho_dem_max, rho_dem);
        }
        double delta = delta_sched;

        const InertiaRead read = factorize_with_ladder(rho_sched, rho_dem, delta);

        // I4 + N1: READ AFTER THE LADDER SETTLES, COUNTED ONLY IF A STEP IS ACTUALLY TAKEN. `rho`
        // is in/out, so this reads the level the step would run at; the increment waits until the
        // step has been applied, since the counter's doc says "iterations TAKEN".
        const bool elevated = rho_dem > 0.0;
        // IC-1'S SKIP RULE COUNTS ITERATIONS, NOT RUNGS: an iteration "needed a modification" iff
        // the reading the tier acted on was taken with `rho_dem > 0`. Updated here, once the
        // ladder has settled and before any rejection path below can leave the loop.
        if (elevated) {
            ++consec_modified;
        } else {
            consec_modified = 0;
        }

        if (fact_budget_hit) {
            // The cap refused a factorization: a BUDGET stop, tested before the reading because no
            // factorization ran and there is no verdict to classify. Section 6.3's exhaustion
            // route gets the same look it gets at the two caps above.
            escape = exhaustion_infeasible(res, feas_target) ? IpqpEscape::kInfeasibleSuspect
                                                             : IpqpEscape::kBudget;
            break;
        }
        if (read == InertiaRead::kUnreadable) {
            // SECTION 2.2'S EVIDENCE-FAILURE POLICY (see the ladder): this iteration's
            // modification is already at the conservative floor and re-factorized there. NOT an
            // escape and NOT a break -- what the solve can no longer produce is a CERTIFICATE.
        } else if (read != InertiaRead::kOk) {
            // I1: CLASSIFIED FROM THE TERMINAL READING ALONE. A solve-scoped `saw_readable_wrong`
            // flag used to make a terminal PERTURBED/UNREADABLE stop come out `kIndefinite`; plan
            // section 7 note (h) makes an evidence-invalid terminal state NUMERICAL.
            escape =
                (read == InertiaRead::kWrong) ? IpqpEscape::kIndefinite : IpqpEscape::kNumerical;
            break;
        }

        // 1. THE AFFINE (PREDICTOR) STEP -- mu target 0.
        // MECHANISM 4'S FIX (T4b): the RHS is built from the SCHEDULE'S subproblem, anchored at
        // `zeta`, never from the inertia-demanded modification, whose gradient here is zero.
        build_rhs(rho_sched, delta_sched, 0.0, /*corrector=*/false);
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

        // 3. THE CORRECTOR, against the SAME numeric factorization (Amendment E's re-entrancy
        //    assertion: `KktFactorization::solve` is const and does not mutate the factor, so two
        //    solves against one factorization are well-formed; W1 asserts this executably).
        build_rhs(rho_sched, delta_sched, mu_t, /*corrector=*/true);
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

        // The multiplier norm BEFORE this step, so section 6.3's exhaustion
        // route can measure growth ACROSS one accepted step. Taken here, not
        // after: once the step is applied the previous norm is gone.
        dual_prev = dual_norm_now();

        // Exponent-test snapshot, taken before the step like `dual_prev`; only
        // a prior accepted step of this attempt qualifies -- never the seed (T4c R1).
        if (attempt_accepted > 0) {
            prev_zl = w.zl;
            prev_zu = w.zu;
            prev_mu = mu_meas;
            have_prev_accepted = true;
        }

#ifdef HVEN_TESTING
        // The gate is the WHOLE stopping rule, section 5.5's warm-trust guard
        // included (fix round 1, review M7): on an untrusted warm entry the
        // solve does not stop even where the residual predicate holds.
        observe_accepted_step(
            w.dx, w.ds, w.dye, w.dyi, w.dzl, w.dzu, alpha_p, alpha_d, out.residuals.worst(),
            !untrusted_warm_seed && ipqp_residuals_meet_target(out.residuals, opts_, iopts));
#endif

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
        // STRICT POSITIVITY IS A THEOREM OF THE FRACTION-TO-BOUNDARY RULE (tau < 1 leaves every
        // positive quantity at >= (1-tau) of its old value), so this guard should be unreachable.
        // Written anyway: the alternative is a logarithm of a non-positive number in a kernel.
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

        // A COMPLETED PREDICTOR+CORRECTOR PAIR, and only that, is one iteration -- see
        // `IpqpCounters::ipqp_iters`. Every break above leaves these two lines unreached, which
        // IS the exclusion both fields document ("iterations taken", N1).
        ++out.counters.ipqp_iters;
        ++attempt_accepted; // R1: this attempt's own accepted-step count.
        rho_dem_final = rho_dem;

        // task 8: `ipqp.iter`. `trace_last_evidence` is what
        // `evidence_for_read` returned for this iteration's accepted read --
        // the injection seam's own value when active, R1 fix round 2.
        if (trace_ != nullptr) {
            const hven::linear::InertiaEvidence &iev = trace_last_evidence;
            IpqpTraceIterEvent ev;
            ev.solve = trace_solve_id;
            ev.major = trace_major_;
            ev.it = out.counters.ipqp_iters;
            ev.mu = mu_meas;
            ev.rho = rho_sched + rho_dem;
            ev.delta = delta;
            ev.res_p = std::max(res.primal_eq, res.primal_iq);
            ev.res_d = res.stationarity;
            ev.res_c = res.complementarity;
            ev.sigma = sigma_m;
            ev.alpha_p = alpha_p;
            ev.alpha_d = alpha_d;
            // R1: absence is DERIVED from the read's own state, explicitly
            // both ways -- never left to a default that happens to agree.
            if (iev.state == hven::linear::InertiaEvidence::State::kObserved) {
                ev.inertia = std::array<Index, 3>{iev.n_pos, iev.n_neg, iev.n_zero};
                ev.zero_derived = iev.zero_is_derived;
            } else {
                ev.inertia = std::nullopt;
                ev.zero_derived = false;
            }
            ev.perturbed = iev.perturbed_pivots; // absent on Accelerate, R1.
            emit_trace_iter(ev);
        }
        if (elevated) {
            ++out.counters.ipqp_iters_at_elevated_rho;
            if (out.counters.ipqp_prox_center_updates == prox_updates_pre) {
                // THE ARMED WALK, COUNTED (T4b plan 2.1's declared gap): the ladder is armed and
                // the 3.2 gate did not advance, so `zeta` and `rho_sched` are pinned while the
                // iterate walks a negative-curvature direction. Measured rather than assumed.
                ++out.counters.ipqp_iters_ladder_armed_no_advance;
            }
        }

        // THE WINDOW ADVANCES ON ACCEPTED STEPS ONLY (spec 6.2), so it is
        // advanced here, beside `ipqp_iters` and behind every rejection above.
        ++win_steps;
        const double step_alpha = std::min(alpha_p, alpha_d);
        win_alpha_min = std::min(win_alpha_min, step_alpha);
        win_alpha_max = std::max(win_alpha_max, step_alpha);

        // ... and here it is discarded. Window discard: see the rule at the stall-window
        // declarations above. The next pass finds `win_armed == false` and arms a fresh window
        // where the ladder actually left the trajectory.
        if (rho_dem_max != rho_dem_max_pre) {
            win_armed = false;
        }
    }

    // --- section 2.2 item 4: the required final inertia read ---------------

    if (converged) {
        if (!iopts.ipqp_require_final_inertia) {
            // I5, SETTLER RULING (spec section 9's own row: "off = certificate always
            // downgraded"). A DOWNGRADE, NOT AN ESCAPE -- the earlier `kNumerical` turned an
            // option-driven skip into a census entry and a K = 3 charge on a clean solve.
            //
            // `ipqp_final_inertia_read == 3` is the distinct "NOT PERFORMED (option off)" value,
            // kept apart from `2` (attempted, evidence unreadable).
            out.counters.ipqp_final_inertia_read = 3;
            out.certificate_downgraded = true;
            // escape stays kNone -- see the status note at the outcome switch.
        } else {
            // C0, SETTLER RULING: section 2.2 item 4's "schedule's residual level" is the level
            // the schedule DECAYS TO (`ipqp_reg_floor`), not wherever `rho_sched` sits when the
            // solve stops. `.superpowers/w1-t4-report.md:495` (F1).
            //
            // The distinction is a wrong-answer bug, not a nicety: a solve can converge BEFORE the
            // schedule ever advances (H = [-1], g = 0 is stationary at iteration 0 with
            // `rho_sched` still 8), and reading `H + 8 I` = [7] certifies a CONCAVE problem.
            //
            // `delta` stays at `delta_sched`: a DUAL proximal term entering only the `-delta I`
            // blocks cannot change the primal block's contribution to the inertia count.
            //
            // A subproblem that needed the ladder will therefore fail this read, and reporting it
            // as saddle-suspect is the correct answer, not a false negative.
            refresh_distances();
            const InertiaRead read = factorize_and_read_once(iopts.ipqp_reg_floor, delta_sched);
            if (fact_budget_hit) {
                // N2 (SETTLER RULING, fix round 2): A READ THAT NEVER HAPPENED IS `3`, NOT `2` --
                // `2` is ATTEMPTED-and-unusable. The ESCAPE stays `kBudget`, and the certificate
                // is downgraded because an unread certificate does not stand.
                out.counters.ipqp_final_inertia_read = 3;
                out.certificate_downgraded = true;
                escape = IpqpEscape::kBudget;
            } else if (read == InertiaRead::kOk) {
                out.counters.ipqp_final_inertia_read = 0;
                // T4c disclosure (R4): exposure exists only in a certificate issued STANDING ALONE
                // at mu_stop -- composition downstream closes it, and a stopping-mu/geometry rule
                // is M7. See `.superpowers/w1-t4c-report.md`.
                const Index band_count =
                    static_cast<Index>(band_lower_idx.size() + band_upper_idx.size());
                out.counters.ipqp_read_kept_tight_sides = band_count;

                // T4: the exponent test reads the SAME mu the band read
                // itself used -- asserted, not just argued (see
                // `mu_at_band_read`'s own declaration).
                assert((std::isnan(mu_at_band_read) || mu_at_band_read == mu_meas) &&
                       "T4c: band read and exponent test must share mu_measured");
                const bool informative = have_prev_accepted && prev_mu > 0.0 && mu_meas > 0.0 &&
                                         !(mu_meas / prev_mu > 0.5);
                Index noise_count = 0;
                bool any_side_uninformative = false;
                double e_min = std::numeric_limits<double>::quiet_NaN();
                double e_max = std::numeric_limits<double>::quiet_NaN();
                if (informative) {
                    const auto fold = [&](double zp, double zc) {
                        const detail::IpqpBarrierNoiseVerdict v =
                            detail::ipqp_classify_barrier_noise(zp, zc, prev_mu, mu_meas);
                        if (v.cls == detail::IpqpBarrierNoiseClass::kSuspect) {
                            ++noise_count;
                        } else if (v.cls == detail::IpqpBarrierNoiseClass::kUninformative) {
                            any_side_uninformative = true;
                        }
                        if (std::isfinite(v.exponent)) {
                            e_min = std::isnan(e_min) ? v.exponent : std::min(e_min, v.exponent);
                            e_max = std::isnan(e_max) ? v.exponent : std::max(e_max, v.exponent);
                        }
                    };
                    for (Index i : band_lower_idx) {
                        fold(prev_zl(i), w.zl(i));
                    }
                    for (Index i : band_upper_idx) {
                        fold(prev_zu(i), w.zu(i));
                    }
                }
                out.counters.ipqp_read_barrier_noise_sides = informative ? noise_count : 0;
                out.read_barrier_noise_exponent_min = e_min;
                out.read_barrier_noise_exponent_max = e_max;
                kept_tight_raw = detail::ipqp_barrier_noise_flag(
                    informative, noise_count, any_side_uninformative, band_count);
            } else if (read == InertiaRead::kWrong) {
                // A reading WAS observed and DISAGREED -- plan section 7 note
                // (h)'s saddle-suspect class.
                out.counters.ipqp_final_inertia_read = 1;
                out.certificate_downgraded = true;
                escape = IpqpEscape::kIndefinite;
            } else {
                // C0b: UNREADABLE, PERTURBED, **or** a factorization that FAILED. A perturbed
                // factorization is no reading at all and a failed one is not even a factor;
                // note (h)'s `== 2`, numerical, covers all three.
                //
                // SECTION 2.2'S EVIDENCE-FAILURE POLICY DOES NOT REACH HERE, and that is the
                // ruling rather than an oversight: the policy permits A STEP at a conservative
                // floor, and the item 4 read takes no step. So the certificate cannot stand.
                out.counters.ipqp_final_inertia_read = 2;
                out.certificate_downgraded = true;
                escape = IpqpEscape::kNumerical;
            }
        }
    }

    // SECTION 2.2'S WHOLE-SOLVE DOWNGRADE. Applied AFTER the item 4 block and never conditioned on
    // it: the policy's own scope is "downgraded FOR THE WHOLE SOLVE", so a clean final read does
    // not undo an evidence failure at iteration 3, and an escaped solve carries the flag too.
    if (evidence_failed) {
        out.inertia_evidence_failed = true;
        out.certificate_downgraded = true;
    }

    // R2 (settler ruling, fix round 2): the flag is set HERE, once the downgrade above (and every
    // earlier one) has had its say -- a clean item 4 read that this whole-solve rule later
    // overrides reports FALSE, counters left populated. See `.superpowers/w1-t4c-report.md`.
    out.read_kept_tight = kept_tight_raw && !out.certificate_downgraded;

    // task 8: `ipqp.restart`/`ipqp.certify`, emitted here because every fact
    // either needs is already final by this point -- see the report for why
    // that lets one place read them rather than threading a partial event.
    if (trace_ != nullptr) {
        IpqpTraceRestartEvent rev;
        rev.grade = out.restart_grade == IpqpRestartGrade::kFullWarm ? IpqpTraceRestartGrade::kFull
                    : out.restart_grade == IpqpRestartGrade::kBaseWarm
                        ? IpqpTraceRestartGrade::kBase
                        : IpqpTraceRestartGrade::kCold;
        rev.repaired = out.counters.ipqp_restart_repairs != 0;
        rev.shift_p = trace_restart_shift_p; // R4: the SAY shift's own split, not the max-fold.
        rev.shift_d = trace_restart_shift_d;
        rev.mu0 = mu0;
        rev.mu_payload = trace_payload_mu;
        rev.adopted = out.counters.ipqp_mu_adopted != 0;
        rev.abandoned = out.counters.ipqp_warm_restart_abandoned != 0;
        emit_trace_restart(rev);
    }
    // `ipqp.certify` fires only when a read was actually attempted -- schema
    // v0's `final_inertia` has no "not performed" value, matching
    // `ipqp_final_inertia_read`'s 0/1/2 states and excluding its `3`.
    if (trace_ != nullptr && converged && out.counters.ipqp_final_inertia_read != 3) {
        IpqpTraceCertifyEvent cev;
        cev.final_inertia = out.counters.ipqp_final_inertia_read == 0 ? IpqpTraceFinalInertia::kOk
                            : out.counters.ipqp_final_inertia_read == 1
                                ? IpqpTraceFinalInertia::kWrong
                                : IpqpTraceFinalInertia::kUnreadable;
        cev.downgraded = out.certificate_downgraded;
        emit_trace_certify(cev);
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
        // A DOWNGRADE WITHOUT AN ESCAPE. Two ways in: the option-off read (I5's ruling,
        // `ipqp_final_inertia_read == 3`) and a mid-solve evidence failure whose final read came
        // back clean. TASK 5'S STATUS RULING, argued in ipqp_engine.h's STATUS VOCABULARY note.
        out.status = QpStatus::kNumericalError;
    }

    // --- THE FIVE-WAY ESCAPE CENSUS (spec section 7) -----------------------
    //
    // Written ONCE, from the single classified `escape_reason`, so the census is a PARTITION by
    // construction: exactly one branch below can run, and each runs `ipqp_escapes` with it --
    // the whole of the sum invariant `tests/sqp/support/ipqp_test_support.h` asserts.
    //
    // THREE OUTCOMES DELIBERATELY CONTRIBUTE NOTHING: `kNone` on a clean solve; `kNone` on a
    // DOWNGRADED solve (plan section 7 note (j): "a downgrade, not an escape ... no census entry,
    // no section 6.1 K=3 charge"); and a DECLINED-PINNED subproblem, which returns far above.
    switch (out.escape_reason) {
    case IpqpEscape::kNone:
        break;
    case IpqpEscape::kBudget:
        ++out.counters.ipqp_escapes;
        ++out.counters.ipqp_escape_budget;
        break;
    case IpqpEscape::kStall:
        ++out.counters.ipqp_escapes;
        ++out.counters.ipqp_escape_stall;
        break;
    case IpqpEscape::kIndefinite:
        ++out.counters.ipqp_escapes;
        ++out.counters.ipqp_escape_indefinite;
        break;
    case IpqpEscape::kNumerical:
        ++out.counters.ipqp_escapes;
        ++out.counters.ipqp_escape_numerical;
        break;
    case IpqpEscape::kInfeasibleSuspect:
        ++out.counters.ipqp_escapes;
        ++out.counters.ipqp_escape_infeasible_suspect;
        break;
    }

    // task 8: `ipqp.escape`, one per genuine escape -- `kNone` (clean or
    // downgraded-without-escape) never fires it, matching the census above.
    if (trace_ != nullptr && out.escape_reason != IpqpEscape::kNone) {
        IpqpTraceEscapeEvent eev;
        switch (out.escape_reason) {
        case IpqpEscape::kNone:
            break; // unreachable (guarded above); silences -Wswitch.
        case IpqpEscape::kBudget:
            eev.reason = IpqpTraceEscapeReason::kBudget;
            break;
        case IpqpEscape::kStall:
            eev.reason = IpqpTraceEscapeReason::kStall;
            break;
        case IpqpEscape::kIndefinite:
            eev.reason = IpqpTraceEscapeReason::kIndefinite;
            break;
        case IpqpEscape::kNumerical:
            eev.reason = IpqpTraceEscapeReason::kNumerical;
            break;
        case IpqpEscape::kInfeasibleSuspect:
            eev.reason = IpqpTraceEscapeReason::kInfeasibleSuspect;
            break;
        }
        eev.evidence.stall = out.stall_evidence;
        eev.evidence.infeasibility = out.infeasibility_evidence;
        emit_trace_escape(eev);
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
    out.rho_mod = rho_dem_final;

    // --- THE CROSS-MAJOR CARRY (spec 5.1 flow (b)) -------------------------
    //
    // COMMITTED ONLY ON A FINITE STATE, and it is the LAST write: a solve that produced nothing
    // usable leaves the previous carry standing, which is what makes "a trust-region shrink-retry
    // does not reset the seed" true without the retry path having to say so.
    if (w.x.allFinite() && w.s.allFinite() && w.ye.allFinite() && w.yi.allFinite() &&
        w.zl.allFinite() && w.zu.allFinite() && std::isfinite(mu_meas)) {
        carry_.x = w.x;
        carry_.s = w.s;
        carry_.lambda_e = w.ye;
        carry_.lambda_i = w.yi;
        carry_.zl = w.zl;
        carry_.zu = w.zu;
        carry_.mu = mu_meas;
        carry_.zeta = w.zeta;
        carry_.lambda_est_e = w.lam_est_e;
        carry_.lambda_est_i = w.lam_est_i;
        carry_.grade = IpqpRestartGrade::kFullWarm;
        carry_armed_ = true;
    }

    // --- the face classification (spec 2.3 item 2) -------------------------
    //
    // A RATIO rule, applied to the returned point whatever the outcome was: like SsnResult's
    // implied active set it describes where the solve STOPPED and certifies nothing. An UNCERTAIN
    // verdict is never forced -- it is counted and handed on.
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
        // TR-PINNED, `QpSolution::tr_active`'s contract verbatim: the variable is held by an
        // effective bound TIGHTER than the real one, so its multiplier is a trust-region dual and
        // internal. A coincidental tie is attributed to the REAL bound -- the walk's own rule.
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
        // ---- THE TWO EXPORT INVARIANTS THIS TIER MUST RE-DERIVE ----------
        //
        // qp_engine.h's export contract states both against `QpSolution`, and says in as many
        // words that a third producer must RE-DERIVE them. This tier is that third producer.
        //
        // (6b) `bound_state[i] == kFree ==> z(i) == 0.0`, ON EVERY STATUS. A barrier method carries
        // a (zl, zu) pair at every finite effective bound, and at an INACTIVE bound that pair is
        // barrier residue, so the price is written only where the classifier put the variable ON.
        //
        // (real-bound-only) THE ABSENT-SIDE TEST READS THE REAL BOUND, NOT THE EFFECTIVE ONE --
        // `SsnEngine::split_bound_multipliers`' rule verbatim: under a FINITE radius every variable
        // has finite effective bounds, and the residue at an absent side is an internal TR dual.
        if (at_lower || at_upper) {
            const double z_lower =
                (at_lower && detail::ipqp_has_lower(qp.lower(i))) ? w.zl(i) : 0.0;
            const double z_upper =
                (at_upper && detail::ipqp_has_upper(qp.upper(i))) ? w.zu(i) : 0.0;
            out.z(i) = z_lower - z_upper;
        }
        // else: `bound_state[u]` is kFree and `out.z(i)` stays the 0 that
        // `out.z.setZero(n)` above put there -- invariant 6b, by construction
        // rather than by a second write.
    }

    // --- counters that mirror the backend's own ---------------------------
    //
    // Taken as DELTAS off `SymmetricFactor::Counters` rather than tallied here, so
    // `ipqp_pattern_verifies` is literally the backend's own count -- which is what makes the
    // field's "mirrors" a fact rather than a claim -- and so the four cannot drift.
    const KktFactorization::Counters after = kkt_.counters();
    out.counters.ipqp_factorizations = after.factorize_count - before.factorize_count;
    out.counters.ipqp_solves = after.solve_count - before.solve_count;
    out.counters.ipqp_symbolic_analyses = after.analyze_count - before.analyze_count;
    out.counters.ipqp_pattern_verifies = after.pattern_verify_count - before.pattern_verify_count;
    out.counters.ipqp_rho_demanded_max = rho_dem_max;
    out.counters.ipqp_rho_demanded_last = rho_dem_last;

    emit_ledger();
    complete_trace_solve();

    return out;
}

} // namespace hven::solvers
