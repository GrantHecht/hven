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
#include <hven/detail/qp/ipqp_fault_injection.h>
#include <hven/detail/qp/ipqp_math.h>
#include <hven/detail/qp/qp_engine.h>
#include <hven/detail/qp/ssn_engine.h>

namespace hven::solvers {

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

/// The five-way inertia reading the tier acts on.
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
///
/// `kFactorFailed` IS TASK 5'S FIFTH VALUE, and it is not a refinement of the
/// verdict helper at all -- it is the state in which there is no verdict to
/// refine. Task 4 folded a FAILED numeric factorization into `kUnreadable`,
/// which was harmless while both terminated the solve; task 5 gives them
/// DIFFERENT remedies, so they can no longer share a value. Section 2.2's
/// evidence-failure policy applies to a factorization that SUCCEEDED and
/// could not report its inertia ("a step is permitted only at a conservative
/// rho floor and the certificate is downgraded for the whole solve"); a
/// factorization that did not succeed produced no factor to step against, and
/// plan section 7 note (h) lists "a factorization failure" among the plain
/// `kNumerical` stops. Reading the two apart is `KktFactorization::info()`,
/// which is why this classification is taken at the call site rather than
/// inside `classify_inertia` -- the evidence struct alone cannot tell them
/// apart (a failed factorization leaves `state == kUnavailable` too).
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

/// THE TIER'S ONE INERTIA-EVIDENCE READ, and therefore the tier's one test
/// seam (docs/testing.md; the declarations are in
/// hven/detail/qp/ipqp_fault_injection.h, which compiles to nothing without
/// HVEN_TESTING).
///
/// EVERY reading the tier acts on comes through here -- the ladder's, the
/// evidence-failure branch's, and the section 2.2 item 4 certification read's
/// -- which is what makes ONE hook enough and what keeps the injected
/// scenarios from having to be maintained in three places. `final_read`
/// separates the two KINDS of read because section 2.2 gives them different
/// policies.
///
/// THE HOOK IS AT THE BOUNDARY, not inside anything derived (CLAUDE.md section
/// 6): this file is Apache-2.0 and written here, the fact injected is what
/// this tier believes it read, and no session file is involved. In a build
/// without HVEN_TESTING the whole body is `return kkt.inertia_evidence();`.
const hven::linear::InertiaEvidence &evidence_for_read([[maybe_unused]] const KktFactorization &kkt,
                                                       [[maybe_unused]] bool final_read) {
#ifdef HVEN_TESTING
    using Observer = detail::testing::IpqpInertiaReadObserver;
    using Injector = detail::testing::IpqpInertiaEvidenceInjector;
    const bool eligible = final_read ? Injector::on_final_read : Injector::on_iteration_reads;
    if (Injector::active && eligible) {
        if (Injector::skip_first > 0) {
            --Injector::skip_first;
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

/// THE SECTION 6.3 FARKAS CORROBORATION -- one matvec plus O(m + n), no
/// factorization.
///
/// `SsnEngine::farkas_certificate`'s shape, re-implemented rather than reused
/// for the reason plan section 7 note (d) withdrew the other SSN reuse rows:
/// that function is a PRIVATE MEMBER bound to `SsnEngine::bound_rows_`, the
/// engine's own materialized bound-row list, and this tier has no such list --
/// its bounds are the dense effective `(lower, upper)` with presence decided
/// per index. The DISCIPLINE is identical and deliberately so: project the
/// dual INCREMENT onto the sign cone, normalize it, and test the two Farkas
/// conditions RELATIVELY, each against a `max(1, .)` floor so a near-zero
/// denominator cannot manufacture a certificate.
///
/// **IT ARMS, IT NEVER CERTIFIES** (spec 6.3). The caller reports
/// `IpqpEscape::kInfeasibleSuspect` whether this returns true or false; all
/// this changes is `IpqpInfeasibilityEvidence::farkas_corroborated` and the
/// two numbers beside it. A tier that withdrew its report on a false here
/// would be treating the absence of a certificate as evidence of feasibility,
/// which is the same category error in the other direction.
///
/// The system tested is {Ae x = be, Ai x <= bi, -x <= -lower, x <= upper}:
/// infeasible iff there is `(ye free, yi >= 0, zl >= 0, zu >= 0)` with
/// `Ae' ye + Ai' yi - zl + zu = 0` and `be' ye + bi' yi - lower' zl +
/// upper' zu < 0` (Farkas). Absent bound sides contribute nothing -- their
/// multipliers are structurally 0 and their `+/-kIpqpInfBound` sentinel is
/// not a row.
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

// ---------------------------------------------------------------------------
// The section 6.1 escape ladder
// ---------------------------------------------------------------------------

IpqpEscapeLadder::IpqpEscapeLadder(const IpqpOptions &iopts)
    : retire_after_(iopts.ipqp_retire_after) {
    // Re-checked HERE and not only in `validate_sqp_options`, for the reason
    // `IpqpEngine::solve` re-validates its own options: this type is reachable
    // without a driver, and a retirement threshold of 0 would retire the tier
    // before it had ever run.
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
        // A DECLINE IS NEUTRAL. It cannot advance the tally (the tier never
        // ran, so it produced no evidence of unsuitability -- that is
        // `ipqp_declined_pinned`'s own settled text), and it cannot reset one
        // either (it produced no evidence of suitability, so it says nothing
        // about whether the previous escapes were a pattern).
        return retired_;
    }
    if (outcome == IpqpLadderOutcome::kSuccess) {
        // "ANY SUCCESS RESETS THE COUNT" (spec 6.1). A converged solve whose
        // CERTIFICATE was downgraded without an escape is a success here --
        // plan section 7 note (j) is explicit that it carries no section 6.1
        // K = 3 charge, and section 6.1's own word for the alternative to an
        // escape is "success".
        consecutive_ = 0;
        return retired_;
    }
    ++consecutive_;
    if (!retired_ && consecutive_ >= retire_after_) {
        // FIRES AT MOST ONCE (`ipqp_tier_retired_after`'s marker-not-a-count
        // discipline): section 6.1 retires the tier "for the REMAINDER of that
        // solve", and "any success resets the count" resets the tally toward a
        // FUTURE retirement, not an already-fired one.
        retired_ = true;
        retired_after_ = major;
    }
    return retired_;
}

bool IpqpEscapeLadder::retired() const { return retired_; }

Index IpqpEscapeLadder::retired_after() const { return retired_after_; }

Index IpqpEscapeLadder::consecutive_escapes() const { return consecutive_; }

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

// The walk's own trust-region resolution, unchanged (`qp_types.h`'s
// SolveOverrides sentinel convention): +inf on the override means "use the
// engine's own radius". DECLARED IN THE HEADER since task 6, because the
// routing chain's pre-solve domain gate has to build the same box this
// function's answer defines -- see the declaration's own note.
double ipqp_effective_tr_radius(const QpOptions &opts, const SolveOverrides &overrides) {
    return std::isinf(overrides.tr_radius) && overrides.tr_radius > 0.0 ? opts.tr_radius
                                                                        : overrides.tr_radius;
}

// THE STOPPING RULE (spec 2.3 step 1 / 3.4), and the ONLY statement of it: the
// QP layer's own relative tolerances, loosened by `ipqp_converge_slack`. The
// tier does not chase the last two decades -- that is tier 3's job -- and the
// slack factor is what makes the division of labour a setting rather than a
// hard-coded convention. `solve()`'s loop calls this, so a caller reading it
// back off an IpqpResult reads the same answer the engine acted on.
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

    out.box = make_ipqp_box(qp, ipqp_effective_tr_radius(opts_, overrides));
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
    // TWO QUANTITIES, KEPT APART, because they are different things and T4b
    // exists because the code used to treat them as one number:
    //
    //   rho_sched / delta_sched -- section 3.2's PROXIMAL schedule, gated on
    //                              measured contraction. This IS the
    //                              subproblem: it enters the matrix, the
    //                              right-hand side and the gate's residual,
    //                              all anchored at (zeta, lambda_est).
    //   rho_dem                  -- section 2.2's inertia-demanded
    //                              MODIFICATION, chosen fresh every iteration
    //                              by Algorithm IC (below). Matrix only,
    //                              additive, uniform in the Ruiz-scaled
    //                              system. A per-iteration local, NOT a
    //                              solve-scoped floor: the monotone-per-solve
    //                              floor of the pre-T4b design is DELETED
    //                              (plan section 7 note (p)).
    //
    // THE LADDER'S SOLVE-SCOPED STATE IS ITS MEMORY, NOT A FLOOR:
    //
    //   rho_dem_last  -- Algorithm IC's own memory. Set ONLY by a SUCCESSFUL
    //                    MODIFIED factorization (and by the exhausted-ladder
    //                    and evidence-failure paths, which are escapes or
    //                    downgrades either way); a success on the UNMODIFIED
    //                    system leaves it exactly as IC leaves it. Reported
    //                    as `ipqp_rho_demanded_last`.
    //   rho_dem_max   -- the high-water mark across every rung the solve paid,
    //                    reported as `ipqp_rho_demanded_max`. It rises and
    //                    never falls, so it is also the SAFEGUARD LEVEL the
    //                    section 6.2 window-discard rule watches (see there).
    //                    A convex subproblem never arms the ladder and reports
    //                    0, which is the convex-inertness pin.
    //   consec_modified -- how many CONSECUTIVE preceding iterations needed a
    //                    modification, for IC-1's skip rule.
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
    // Section 2.2's evidence-failure policy, ARMED ONCE PER SOLVE: a
    // factorization succeeded and reported no usable inertia evidence, so the
    // monotone floor was raised to a conservative level, the steps from there
    // on are taken at that floor, and the certificate is downgraded for the
    // WHOLE SOLVE. Once armed it never disarms -- "for the whole solve" is
    // the specification's own scope, not this iteration's.
    bool evidence_failed = false;
    double mu_meas = mu0;

    // --- THE SECTION 6.2 / 6.3 WINDOW -------------------------------------
    //
    // ONE WINDOW SERVES BOTH TESTS, and that is the design rather than a
    // saving: section 6.2's stall and section 6.3's infeasible-suspect are
    // both statements about "a window over which nothing improved", differing
    // only in which OTHER signal they pair that with (dying steps vs
    // diverging multipliers). Two independently advanced windows would be two
    // answers to "how long has nothing been improving".
    //
    // SSN's three window properties, adopted verbatim in kind (spec 6.2):
    //   * IT ADVANCES ON ACCEPTED STEPS ONLY -- `win_steps` is incremented
    //     beside `ipqp_iters`, behind every rejection.
    //   * IMPROVEMENT IS DEMANDED OVER THE WHOLE WINDOW, not per step -- every
    //     conjunct compares the CURRENT state against the reference captured
    //     when the window was armed.
    //   * A SAFEGUARD CHANGE DISCARDS THE WINDOW.
    //
    // THE WINDOW-DISCARD RULE, STATED ONCE AND ONLY HERE. Every other site
    // that touches it -- `arm_window` below, the sampling site in the loop,
    // and the discard itself -- carries a POINTER BACK TO THIS PARAGRAPH and
    // nothing more. Two statements of one rule is how a maintainer ends up
    // trusting the wrong one (co-review I-1, twice).
    //
    //     A window is discarded when, and only when, the INERTIA-DEMANDED
    //     SAFEGUARD REACHES A LEVEL IT HAD NOT REACHED BEFORE -- i.e. when
    //     `rho_dem_max`, the high-water mark of section 2.2's modification,
    //     RISES. A move of the section 3.2 schedule (`rho_sched` /
    //     `delta_sched`) does NOT discard it, and neither does the ordinary
    //     up-and-down cycling of `rho_dem` inside a band the ladder has
    //     already visited.
    //
    // THE HIGH-WATER READING IS T4b'S, AND IT IS THE LITERAL SUCCESSOR OF THE
    // PRE-T4b PREDICATE, not a new policy: before T4b the safeguard was a
    // MONOTONE floor, so "the floor moved" and "the safeguard reached a new
    // level" were the same event, and this rule read the floor. Algorithm IC
    // is deliberately non-monotone -- it retries `rho_dem_last / 3` every
    // iteration -- so a predicate on the WORKING value would discard the
    // window every second iteration of any armed walk and make section 6.2
    // structurally unreachable on exactly the nonconvex rows it was written
    // for. SSN's own rule, which spec 6.2 imports together with its
    // justification, dirties on safeguard INCREASES only
    // (`ssn_engine.cpp:381-412`); the high-water predicate is that rule,
    // stated for a safeguard that can now come back down.
    //
    // THAT IS A DATED AMENDMENT OF SECTION 6.2'S TEXT -- which says "any
    // regularization change discards the window" -- AND NOT A CLARIFICATION
    // OF IT. It is labelled as one (settler ruling, plan section 7 note (l)):
    // the spec's wording is not itself ambiguous enough to exclude
    // `rho_sched` and `delta_sched`, so narrowing it is an amendment, and
    // calling it a reading of the words would be dishonest. It is a CHOSEN
    // reading, ratified on two grounds, NEITHER OF THEM EMPIRICAL:
    //
    //  * THE RULE IS IMPORTED WITH ITS JUSTIFICATION, AND THE JUSTIFICATION
    //    NAMES THE SAFEGUARD -- ssn_engine.h:620, quoted by spec 6.2 itself:
    //    "slow progress under a sigma that JUST CHANGED is THE SAFEGUARD'S
    //    DOING, not the problem's." In this tier the safeguard is section
    //    2.2's ladder, and `rho_dem` is the only quantity it moves. The
    //    section 3.2 schedule is not a safeguard: it is the method's ordinary
    //    outer iteration, it is GATED ON MEASURED PROGRESS, and it moves
    //    regularization DOWNWARD, toward the caller's own QP. Slow progress
    //    under a DECREASING regularization is the problem's doing, which is
    //    exactly the case the rule does not exempt.
    //  * PRECEDENT: SSN's own window dirties on safeguard INCREASES only
    //    (`ssn_engine.cpp`'s proximal-escalation path), so the amendment
    //    aligns this tier with the kernel the rule was imported from rather
    //    than diverging from it.
    //
    // NO UNREACHABILITY CLAIM IS MADE, and one was WITHDRAWN. An earlier draft
    // argued that the literal reading leaves the stall test structurally
    // unreachable. That is false, and it was RE-MEASURED rather than merely
    // conceded: with the literal value-change predicate (`rho_sched != pre ||
    // delta_sched != pre || safeguard != pre`) built behind a scratch toggle,
    // the window reaches the full five accepted steps on thirteen of the
    // suite's own IPQP solves, and the stall fixture still fires at the same
    // ten iterations. The original measurement had counted GATE ADVANCES
    // rather than VALUE CHANGES -- the gate's outcome (c) advances the
    // proximal centre while moving neither quantity. The amendment is
    // therefore a CHOSEN reading resting on the two grounds above, never a
    // forced one.
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
    // The multiplier norm BEFORE the most recently accepted step, and the
    // multiplier state at the SOLVE'S OWN START POINT. Both exist for section
    // 6.3's exhaustion route, which cannot use a windowed growth reference at
    // all -- there the divergence and the last progress are the SAME accepted
    // step, so a windowed ratio would read 1 (ssn_engine.h's own reasoning for
    // `kSsnDualStepGrowth`).
    double dual_prev = 0.0;
    double dual_start = 0.0;
    Vec start_ye, start_yi, start_zl, start_zu;
    bool have_start = false;
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
    // `||(y, z)||inf` at the current iterate -- section 6.3's growth signal.
    // Guarded per block because an infinity norm of an EMPTY Eigen vector is
    // an assert, not a 0, and a QP with no rows at all is legal here.
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

    // Capture the window's reference state at the CURRENT iterate. Called on
    // the first pass, after a window discard (see the rule at the
    // stall-window declarations above), and after any window that closed
    // without firing -- a window is a measurement, and a measurement that has
    // been read is spent.
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

    // Signal (a) of section 6.3, shared by both routes: the primal residual is
    // FLAT ON A POSITIVE FLOOR over the window. Both halves are load-bearing
    // -- "flat" alone is what a CONVERGED solve looks like, and "on a positive
    // floor" alone is what every unconverged iteration looks like.
    auto primal_flat_on_floor = [&](const IpqpResiduals &r, double feas_target,
                                    double *primal_now_out, double *impr_out) {
        const double primal_now = std::max(r.primal_eq, r.primal_iq);
        const double impr = win_primal0 > 0.0 ? 1.0 - primal_now / win_primal0 : 0.0;
        *primal_now_out = primal_now;
        *impr_out = impr;
        return primal_now > feas_target && impr < (1.0 - detail::kSsnStallImproveFactor);
    };

    // Fill the escape's evidence block. `reference_*` is whichever multiplier
    // snapshot the ROUTE selected -- the window's for the standing route, the
    // solve's start point for the exhaustion route -- so the Farkas direction
    // is the same increment the growth conjunct measured.
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
    // The growth conjunct is measured against the SOLVE'S START POINT and is
    // ADDITIONALLY required to have multiplied by `kSsnDualStepGrowth` across
    // the most recently accepted step -- `ssn_engine.h`'s own two-part rule
    // for this route, and for its reason: a windowed reference is meaningless
    // when the divergence and the last progress are the same step, while a
    // start-point reference alone would be vacuous for any feasible QP whose
    // true multipliers exceed the growth factor. "An order of magnitude in one
    // step is not multipliers settling, it is multipliers with no limit to
    // settle onto."
    //
    // Returns false -- leaving the escape as `kBudget` -- unless BOTH
    // conjuncts and the per-step ratio hold, which is what keeps a plain
    // budget exhaustion from being relabelled as a suspicion.
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
    //   `rho_sched` is section 3.2's PROXIMAL term. It defines the subproblem
    //   -- `min Q(x) + (rho_sched/2)||x - zeta||^2` -- so it enters the
    //   diagonal AND the right-hand side (`build_rhs`) AND the gate's
    //   regularized residual, all anchored at the same prox centre. It lives
    //   on the CALLER'S scale, alongside `src[i]` and `sigma[i]`, and is
    //   therefore multiplied by `dsq` with them.
    //
    //   `rho_dem` is section 2.2's inertia-demanded MODIFICATION (Ipopt's
    //   `delta_w`). It is NOT part of the subproblem: its proximal anchor is
    //   the CURRENT ITERATE, so its gradient contribution at `x_k` is zero and
    //   it must never reach the right-hand side. It is ADDED to the proximal
    //   Hessian rather than maxed with it -- with `max`, a trial below
    //   `rho_sched` would be a no-op the ladder's memory would nonetheless
    //   record, and "`rho_dem == 0` means unmodified" would stop being exact.
    //
    // AND IT IS APPLIED IN THE RUIZ-SCALED SYSTEM, which is the system the
    // inertia is read on. `assemble` computes the equilibration on the
    // UNMODIFIED matrix and this function then writes `(...) * dsq(i) +
    // rho_dem`, so the shift is `rho_dem` UNIFORMLY in every scaled
    // coordinate -- equivalently `rho_dem / d_i^2` on the unscaled diagonal.
    // A uniform shift in UNSCALED space would be the non-uniform shift
    // `rho_dem * d_i^2` in the scaled system: a coordinate with a tiny `d_i`
    // would receive almost nothing and the ladder would have to climb far past
    // every other coordinate to cover it. Ipopt adds `delta_w` after its own
    // scaling for the same reason, and Algorithm IC's constants are
    // scale-free only under this choice. `ipqp_reg_max` therefore caps
    // `rho_dem`, the scaled uniform value, not the total.
    //
    // THE `rho_dem == 0` BRANCH IS EXPLICIT AND MUST STAY THAT WAY. Folding a
    // `+ rho_dem` into the sum would re-associate the expression and move the
    // last bits of every convex solve in the tree -- the branch is what makes
    // the convex corpus BIT-IDENTICAL across this change (T4b close gate 4).
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
    //
    // THE EQUILIBRATION IS COMPUTED ON THE UNMODIFIED SYSTEM (`rho_dem = 0`),
    // then the modification is written into the scaled matrix. Two reasons,
    // and they point the same way. The Ruiz diagonal must not move under the
    // ladder -- a rung that changed `D` would leave the ladder comparing two
    // different systems -- and `rho_dem` is a UNIFORM shift of the SCALED
    // matrix (see `write_diagonals`), so letting it participate in computing
    // `D` would make `D` a function of the rung. On the `rho_dem == 0` path
    // the second `write_diagonals` call is skipped entirely, so the assembled
    // values are bit-for-bit what they were before the separation.
    auto assemble = [&](double rho_sched, double rho_dem, double delta) {
        w.sigma.setZero();
        detail::ipqp_accumulate_bound_sigma(w.x, bounds.lower, bounds.upper, w.zl, w.zu, n,
                                            w.sigma);

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
            write_diagonals(rho_sched, rho_dem, delta);
        }
    };

    // Returns the reading, or -- when the factorization budget refused the
    // call -- `kUnreadable` with `fact_budget_hit` raised. Callers must test
    // the flag FIRST: a budget stop is `kBudget`, never a numerical or
    // indefinite verdict about a factorization that never ran.
    auto factor_and_read = [&](bool final_read) {
        if (!factorize_once()) {
            return InertiaRead::kUnreadable;
        }
        // THE FACTORIZATION'S OWN OUTCOME IS READ BEFORE ITS EVIDENCE, and
        // that ordering carries task 5's evidence/failure split: section 2.2's
        // evidence-failure policy permits a step at a conservative floor
        // against a factor that EXISTS but could not be interrogated, and
        // there is no factor at all here. `info()` is the linear layer's own
        // reporting-only status; nothing else in this engine turns on it,
        // which is exactly why it is the honest discriminator.
        if (kkt_.info() != Eigen::Success) {
            return InertiaRead::kFactorFailed;
        }
        return classify_inertia(evidence_for_read(kkt_, final_read), expect_pos, expect_neg);
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
    //
    // NO INERTIA-DEMANDED MODIFICATION EITHER (T4b): the read is taken at
    // `rho_dem = 0` BY CONSTRUCTION, not by whatever the ladder's memory
    // happens to hold. Seeding it from `rho_dem_last` would ask the item 4
    // question about the modified system rather than about the caller's QP,
    // which is the same wrong-answer bug as reading at an elevated schedule
    // value; a mutation that does so must fail A11's final-read pins.
    auto factorize_and_read_once = [&](double rho_sched, double delta) {
        assemble(rho_sched, /*rho_dem=*/0.0, delta);
        return factor_and_read(/*final_read=*/true);
    };

    // ALGORITHM IC'S FIRST TRIAL for this iteration (Wachter-Biegler 2006,
    // step IC-1 / IC-2; the four constants and hven's two declared adaptations
    // are in `detail`'s ladder banner).
    //
    //   * NO MEMORY, or fewer than `kIpqpLadderSkipAfter` consecutive
    //     preceding iterations needed a modification -> TRY ZERO. This is
    //     IC-1, and it is not a nicety: it is the only thing that lets
    //     `rho_dem` fall back to 0 the moment the reduced curvature at the
    //     iterate turns positive, at which point one exact Newton step lands
    //     the residual at machine precision and the section 3.2 gate
    //     advances. A design that never retries zero converges to the
    //     modified problem instead.
    //   * OTHERWISE -> `max(ipqp_reg_floor, rho_dem_last / kIpqpLadderDown)`.
    //     Deliberately SMALLER than the shift that last worked: the ladder is
    //     probing for the smallest sufficient value rather than defending a
    //     floor. When the probe is refused, that is a RECLIMB and is counted.
    //
    // THE EVIDENCE-FAILURE FLOOR RIDES THE TRIAL, NOT THE LADDER, once armed.
    // Section 2.2's policy permits a step at a conservative floor when a
    // factorization succeeds and reports no usable inertia evidence; on the
    // backend that clause exists for, EVERY factorization reports that. Adding
    // the floor here means such a solve pays ONE factorization per iteration
    // at the floor -- what the pre-T4b monotone floor delivered -- instead of
    // paying a rejected trial and a corrective rung every iteration. IC-1's
    // "try zero first" is skipped outright while `evidence_failed` stands,
    // because a reading that can never come back cannot reward the attempt.
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

    // Assemble at `(rho_sched, rho_dem, delta)` and factorize, climbing
    // Algorithm IC's ladder until the inertia is the section 4.1 target or the
    // ceiling is reached. Returns the reading the caller must act on.
    // `rho_dem`/`delta` are in/out, so the caller sees where the ladder
    // stopped; `rho_sched` is the caller's subproblem and the ladder never
    // touches it.
    auto factorize_with_ladder = [&](double rho_sched, double &rho_dem, double &delta) {
        // Whether this iteration STARTED from IC's memory rather than from
        // zero, and whether that start has already been charged as a reclimb.
        // One charge per iteration: the question the counter answers is "did
        // the memory's guess come back too small", not "how many rungs did the
        // recovery take".
        // (While an evidence failure stands the trial is the POLICY FLOOR,
        // not IC's probe, so its refusal is not a reclimb.)
        const bool trial_from_memory = rho_dem > 0.0 && !evidence_failed;
        bool reclimb_charged = false;
        assemble(rho_sched, rho_dem, delta);

        for (;;) {
            const InertiaRead read = factor_and_read(/*final_read=*/false);
            if (fact_budget_hit || read == InertiaRead::kOk || read == InertiaRead::kFactorFailed) {
                if (read == InertiaRead::kOk && rho_dem > 0.0) {
                    // IC UPDATES ITS MEMORY ONLY ON A SUCCESSFUL **MODIFIED**
                    // FACTORIZATION. A success at `rho_dem == 0` leaves it
                    // untouched -- exactly as the paper does -- so the next
                    // iteration that does need a modification still descends
                    // from the last value that was actually sufficient rather
                    // than restarting the whole first climb.
                    rho_dem_last = rho_dem;
                }
                return read;
            }
            if (read == InertiaRead::kUnreadable) {
                // SECTION 2.2'S EVIDENCE-FAILURE POLICY, VERBATIM: "a step is
                // permitted ONLY at a conservative `rho` floor AND the
                // certificate is downgraded for the whole solve. The counts
                // are never zero-filled or inferred."
                //
                // TASK 4 TERMINATED HERE (-> kNumerical) and recorded the
                // question as open; the spec text settles it, so the step is
                // taken. The distinction matters most on the backend this
                // branch actually describes: Accelerate can report
                // `kUnavailable` for a perfectly good factorization, and
                // refusing to step on it would retire the tier on that
                // platform for a reason that is about the QUERY, not the
                // subproblem. (That arm stays UNOBSERVED under CLAUDE.md
                // section 6's never-fabricate rule until real Mac hardware
                // runs it; what is implemented here is the policy, and the
                // seam-injected pins are what exercise it.)
                //
                // THE FLOOR IS `detail::kIpqpEvidenceFailureRhoFloor`, AN
                // ABSOLUTE MINIMUM AND NEVER A RUNG. It is applied HERE the
                // first time evidence goes missing and, from then on, by
                // `ladder_trial` at the top of every later iteration -- which
                // is what keeps an always-unavailable backend at ONE
                // factorization per iteration now that no floor variable
                // carries the level across iterations (T4b). That constant's
                // own banner carries the full argument and is where a change
                // to this policy belongs; the two decisions behind it are
                // restated here because they are what this branch does:
                //
                // 1. IT IS AN ABSOLUTE MAGNITUDE, NOT A MULTIPLE OF THE
                //    CURRENT `rho`. The branch this policy exists for is the
                //    Accelerate one, where `kUnavailable` is reported for
                //    EVERY factorization, not for one unlucky pivot. A floor
                //    proportional to the current level compounds under that:
                //    measured on a two-row convex fixture, `rho * 100` at the
                //    schedule's start put a permanent floor of 800 under the
                //    solve, which then converged to the PROXIMALLY BIASED
                //    point (x = 0.0026 where the answer is 0.75) and ran out
                //    its whole 60-iteration budget doing it. Section 2.2's
                //    clause says a step IS PERMITTED; a floor that makes the
                //    tier unusable on the platform the clause names is not an
                //    implementation of it. The VALUE is the smallest
                //    magnitude this tier regards as a real inertia correction
                //    at all -- everything below being "far too small to change
                //    any inertia" -- which is why the constant is DEFINED
                //    equal to `kIpqpLadderInit` while carrying its own name
                //    and its own contract (co-review I-3: sharing the symbol
                //    let a ladder retune move this policy silently).
                // 2. WHAT CARRIES THE HONESTY IS THE DOWNGRADE, NOT THE SIZE
                //    OF THE SHIFT. No finite `rho` is PROVABLY sufficient
                //    without a reading -- that is precisely what the missing
                //    evidence would have told us -- which is why section 2.2
                //    pairs "a step is permitted" with "the certificate is
                //    downgraded for the whole solve" rather than with a
                //    magnitude. A ladder would be worse than useless here: it
                //    has no stopping criterion when the reading can never come
                //    back right, so it would spend the whole ceiling's worth
                //    of factorizations and take the same step at the end.
                evidence_failed = true;
                const double conservative =
                    std::min(detail::kIpqpEvidenceFailureRhoFloor, iopts.ipqp_reg_max);
                // THE ITERATION RUNS AT `max(trial, floor)` and THAT value is
                // what IC's memory records (T4b plan 2.2's evidence-failure
                // rule, which had to be re-stated once `rho_floor` was gone:
                // later trials descend /3 from it and `ladder_trial` floors
                // them again while evidence stays unavailable, so an
                // always-unavailable backend runs at a constant floor exactly
                // as it did before). The floor never raises the shift above
                // that maximum.
                const double floored = std::max(rho_dem, conservative);
                rho_dem_last = floored;
                rho_dem_max = std::max(rho_dem_max, floored);
                if (floored > rho_dem) {
                    // This factorization WAS rejected -- on evidence the tier
                    // could not use, which `ipqp_inertia_retries` covers
                    // ("wrong OR evidence-invalid", fix round 1's M2).
                    ++out.counters.ipqp_inertia_retries;
                    ++out.counters.ipqp_reg_increases;
                    rho_dem = floored;
                    write_diagonals(rho_sched, rho_dem, delta);
                    continue;
                }
                return read;
            }
            // The ceiling test carries SsnEngine::escalate_prox's relative
            // slack verbatim, and for its reason: repeated multiplication does
            // not reproduce 1e6 exactly (the sequence ends at
            // 999999.9999999998), so an exact `>=` guard grants a final rung
            // that raises the regularization by 2.3e-10 relative and buys a
            // whole numeric factorization for it.
            const double cap = iopts.ipqp_reg_max * (1.0 - detail::kSsnProxCapSlack);
            if (read == InertiaRead::kPerturbed && rho_dem == 0.0) {
                // Spec 2.2's evidence-failure policy: a perturbed
                // factorization describes a DIFFERENT matrix, so its inertia
                // is not evidence about this one. The remedy is a larger
                // DUAL shift, never reading it as right.
                //
                // ... AND THAT REMEDY IS THE RIGHT ONE ONLY WHILE THE PRIMAL
                // LADDER IS UNARMED (T4b, `rho_dem == 0` above). MEASURED, on
                // `H = diag(2, -1000)`, `g = (-2, -4)` on `[-10, 10]^2` at the
                // shipped defaults: the tier reads WRONG at `rho_dem` 0, 1e-4
                // and 1e-2, and PERTURBED at `rho_dem = 1` -- because Ruiz
                // normalizes that coordinate's scaled diagonal to almost
                // exactly `-1` and the UNIFORM scaled shift of `+1` annihilates
                // it. The pivot is zero in the PRIMAL block, so climbing
                // `delta` 8 -> 800 -> 80000 -> 1e6 cannot touch it: the solve
                // spent four more factorizations and escaped `kNumerical` with
                // ZERO iterations taken. Rung `1.0` against a Ruiz-normalized
                // `-1` diagonal is not a coincidence to be tuned away, it is a
                // structural consequence of applying IC's own rungs in a
                // system whose diagonal has been normalized to unit magnitude.
                //
                // THE RULE, therefore: while the ladder is armed, a perturbed
                // pivot is a statement that THIS RUNG did not produce a
                // system with the target inertia -- a zero pivot is neither a
                // positive nor a negative eigenvalue -- so the ladder advances
                // its own quantity, exactly as it does on a wrong reading. It
                // is still never read as right, and the TERMINAL reading still
                // classifies the escape (a ladder that exhausts on perturbed
                // readings reports `kNumerical`, not `kIndefinite`, per plan
                // section 7 note (h)). An amendment of section 2.2's
                // evidence-failure remedy, declared as one and carried in the
                // T4b report; the unarmed path is untouched, which is what
                // keeps the convex corpus bit-identical.
                if (delta >= cap) {
                    // I8: THE TERMINAL REJECTION IS STILL A REJECTION. This
                    // factorization was refused on evidence the tier could not
                    // use, exactly like every rung before it; returning
                    // without counting it lost one rejection per exhausted
                    // ladder.
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
                    // The ceiling rung WAS paid, so it is the last rung this
                    // subproblem paid and IC's memory records it. The solve
                    // escapes from here (`kIndefinite`, plan section 7 note
                    // (h)), so nothing reads the memory again -- recording it
                    // keeps `ipqp_rho_demanded_last` meaning "the last rung
                    // paid" on the one path where that is not the same as
                    // "the last rung that worked".
                    rho_dem_last = rho_dem;
                    return read;
                }
                // ALGORITHM IC'S ESCALATION (plan 2.2's pseudocode, normative).
                if (rho_dem == 0.0) {
                    // IC-2: the unmodified trial was refused. With no memory
                    // this is `delta_w^0`; with a memory it is the memory's
                    // own /3 probe, which the skip rule had not yet licensed
                    // as this iteration's first trial.
                    rho_dem = (rho_dem_last == 0.0)
                                  ? std::min(detail::kIpqpLadderInit, iopts.ipqp_reg_max)
                                  : std::max(iopts.ipqp_reg_floor,
                                             rho_dem_last / detail::kIpqpLadderDown);
                } else {
                    if (trial_from_memory && !reclimb_charged) {
                        ++out.counters.ipqp_ladder_reclimbs;
                        reclimb_charged = true;
                    }
                    // TWO DECADES THROUGH THE WHOLE FIRST CLIMB, EIGHT AFTER.
                    // `rho_dem_last == 0` means this solve has never had a
                    // successful modified factorization, so nothing is known
                    // about the subproblem's scale and W-B's `bar kappa_w^+`
                    // governs; once a memory exists the ladder is refining a
                    // value whose order it already knows and `kappa_w^+` does.
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
            write_diagonals(rho_sched, rho_dem, delta);
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
    //
    // `rho_sched` / `delta` ARE THE SCHEDULE'S, ALWAYS. This function defines
    // which subproblem the step solves, and section 3.2's proximal terms --
    // `rho_sched (x - zeta)` on stationarity, `delta (y - lambda_est)` on the
    // dual blocks -- are the whole of it. Section 2.2's inertia-demanded
    // modification has no place here: its anchor is the current iterate, so
    // its gradient at `x_k` is zero, and passing the TOTAL shift instead is
    // exactly the defect T4b removes (the step then converges to the KKT point
    // of the modified problem while the gate measures the schedule's, so the
    // gate falls silent forever -- see the call sites).
    auto build_rhs = [&](double rho_sched, double delta, double mu_t, bool corrector) {
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

        // The two targets, named once for the tests below that also read
        // them (the section 6.2 window, section 6.3's positive floor, and the
        // inner-loop target); the stopping rule itself is the exported
        // predicate, which reads the same two from the same two fields.
        const double opt_target = opts_.opt_tol * iopts.ipqp_converge_slack;
        const double feas_target = opts_.feas_tol * iopts.ipqp_converge_slack;

        // THE STOPPING RULE (spec 2.3 step 1 / 3.4), through the exported
        // predicate rather than inline: task 6's routing chain has to ask the
        // same question of a returned IpqpResult (a converged iterate whose
        // final certification read the factorization budget refused is routed
        // as a downgraded certificate, not as budget exhaustion), and two
        // statements of one rule could drift.
        if (ipqp_residuals_meet_target(res, opts_, iopts)) {
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
        // EVALUATED BEFORE THE BUDGET CHECKS, which is section 6.1's "budget
        // ... OF LAST RESORT: the stall test below should fire first on
        // anything that is genuinely stuck" made structural rather than
        // hoped for.
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

            // SECTION 6.3 IS TESTED FIRST, and the order is a ruling rather
            // than an accident. A subproblem can satisfy both signatures at
            // once -- an infeasible QP stalls, and its steps die as the
            // multipliers diverge -- and the two escapes go to different
            // places: `kStall` routes onward as a difficult subproblem, while
            // `kInfeasibleSuspect` is the one the W2 feasibility hook exists
            // to answer. The more specific diagnosis is the more useful one,
            // and reporting the generic one first would make the specific
            // test unreachable on exactly the problems it was written for.
            if (flat && growth >= detail::kSsnDualGrowthFactor) {
                fill_infeasibility_evidence(res, /*exhaustion=*/false, primal_now, primal_impr,
                                            dual_now, growth, 0.0, win_ye, win_yi, win_zl, win_zu);
                escape = IpqpEscape::kInfeasibleSuspect;
                break;
            }

            // "RESET ON A MEHROTRA TARGET CHANGE THAT ACTUALLY DROPPED `mu`"
            // (spec 6.2) IS CONJUNCT (i) READ AS A RESET, and implementing it
            // that way rather than as a second mechanism is deliberate: a
            // window across which `mu` genuinely halved IS a window in which
            // the barrier target changed and the change took. Reading it as
            // "reset whenever mu moved at all" would re-arm on every healthy
            // step and make the stall test unreachable; reading it as a
            // separate trigger would give two answers to one question.
            if (mu_ratio >= detail::kIpqpStallMuFactor) {
                arm_window(res);
            } else if (res_impr < (1.0 - detail::kSsnStallImproveFactor) &&
                       win_alpha_max < detail::kIpqpStallAlpha) {
                // ALL THREE CONJUNCTS HOLD. Conjunct (iii) is tested on
                // `win_alpha_max` -- the LARGEST per-step `min(alpha_p,
                // alpha_d)` in the window -- because the specification demands
                // it "on EVERY step in the window": one healthy step anywhere
                // disarms it, which is the "never abort on one tiny-alpha
                // iteration" rule stated as code.
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
                // The window closed and fired nothing. A measurement that has
                // been read is spent: re-arm at the current state so the next
                // `stall_w` steps are measured against where the trajectory
                // actually is, rather than accumulating improvement against a
                // reference that keeps receding.
                arm_window(res);
            }
        }

        if (out.counters.ipqp_iters >= iter_budget) {
            escape = exhaustion_infeasible(res, feas_target) ? IpqpEscape::kInfeasibleSuspect
                                                             : IpqpEscape::kBudget;
            break;
        }
        if (kkt_.counters().factorize_count - before.factorize_count >= fact_budget) {
            escape = exhaustion_infeasible(res, feas_target) ? IpqpEscape::kInfeasibleSuspect
                                                             : IpqpEscape::kBudget;
            break;
        }

        // The safeguard's state this pass started from. Window discard: see
        // the rule at the stall-window declarations above. Sampled here
        // rather than beside each move because the high-water mark rises in
        // TWO places (the inertia ladder and the evidence-failure branch), and
        // two separate "and reset the window" statements would be two places
        // to forget.
        const double rho_dem_max_pre = rho_dem_max;

        // Whether THIS iteration's section 3.2 gate advanced, for
        // `ipqp_iters_ladder_armed_no_advance`. Read off the prox-centre
        // counter rather than a second flag: an advance is exactly a
        // prox-centre update, by the gate's own construction, and one source
        // of truth is what keeps the two from drifting.
        const Index prox_updates_pre = out.counters.ipqp_prox_center_updates;

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
                // NOTHING OVERRIDES THE DECREASE ANY MORE EXCEPT THE ABSOLUTE
                // FLOOR (T4b). Section 2.2 item 3's monotone-per-solve floor
                // is DELETED -- Algorithm IC, which section 2.2 cites by name,
                // restarts each trial at a third of the last shift and has no
                // such rule -- and with it goes the "flap" class this block
                // used to classify. The schedule is now free to decay toward
                // the caller's own QP exactly as section 3.2 says it does; the
                // inertia ladder answers each iteration's curvature on its own
                // per-iteration `rho_dem`, which the schedule never sees.
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
                // THE TWO OUTCOMES OF A GATED ADVANCE, CLASSIFIED EXACTLY
                // ONCE (T4b re-pin; class (a) below is deleted with the
                // monotone floor):
                //
                //  (b) `rho` and/or `delta` MOVED -> DECREASE (I7's rule: the
                //      counter is the `(rho, delta)` SCHEDULE's, so an advance
                //      that moved `delta` alone is an applied decrease of the
                //      schedule).
                //  (c) NOTHING MOVED because both quantities already sit on
                //      the ABSOLUTE floor -> NEITHER. The absolute floor is a
                //      setting every schedule decays onto, not evidence about
                //      this subproblem's curvature, which is the whole of I2.
                //
                // So `ipqp_prox_center_updates == ipqp_reg_decreases +
                // (class (c) advances)` holds on every solve; the difference
                // is exactly the number of advances taken with the schedule
                // already on the floor -- a quantity the section 7 counter
                // table has no field for and T4b does not invent one for. The
                // tests pin the EXACT accounting rather than the inequality.
                if (moved) {
                    ++out.counters.ipqp_reg_decreases;
                }
                w.zeta = w.x;
                w.lam_est_e = w.ye;
                w.lam_est_i = w.yi;
                ++out.counters.ipqp_prox_center_updates;
            }
        }

        // THE TWO REGULARIZATIONS, SELECTED SEPARATELY (T4b). `rho_sched` is
        // the subproblem's own proximal weight and is NOT touched here;
        // `rho_dem` is the inertia-demanded modification, chosen fresh by
        // Algorithm IC every iteration. They are ADDITIVE in the matrix and
        // only `rho_sched` reaches the right-hand side.
        double rho_dem = ladder_trial();
        if (rho_dem > 0.0) {
            rho_dem_max = std::max(rho_dem_max, rho_dem);
        }
        double delta = delta_sched;

        const InertiaRead read = factorize_with_ladder(rho_sched, rho_dem, delta);

        // I4 + N1: READ AFTER THE LADDER SETTLES, COUNTED ONLY IF A STEP IS
        // ACTUALLY TAKEN. `rho` is an in/out parameter, so this reads the
        // level the step would run at -- sampling BEFORE the ladder missed the
        // iteration whose own ladder first raises the floor (I4, off by one
        // low). But the counter's doc says "iterations TAKEN", and every
        // rejection below leaves the loop without a step, so the increment
        // itself waits until the step has been applied (N1). An iteration that
        // armed the ladder and was then refused a factorization by the budget
        // is not an iteration taken at elevated rho; it is not an iteration at
        // all.
        const bool elevated = rho_dem > 0.0;
        // IC-1'S SKIP RULE COUNTS ITERATIONS, NOT RUNGS: an iteration "needed
        // a modification" iff the reading the tier acted on was taken with
        // `rho_dem > 0`. Updated here, once the ladder has settled and before
        // any of the rejection paths below can leave the loop.
        if (elevated) {
            ++consec_modified;
        } else {
            consec_modified = 0;
        }

        if (fact_budget_hit) {
            // The cap refused a factorization. That is a BUDGET stop, and it
            // is deliberately tested before the reading: no factorization ran,
            // so there is no inertia verdict to classify. Section 6.3's
            // exhaustion route gets the same look it gets at the two caps
            // above -- running out of factorizations with the infeasibility
            // signature standing is the same event as running out of
            // iterations with it standing.
            escape = exhaustion_infeasible(res, feas_target) ? IpqpEscape::kInfeasibleSuspect
                                                             : IpqpEscape::kBudget;
            break;
        }
        if (read == InertiaRead::kUnreadable) {
            // SECTION 2.2'S EVIDENCE-FAILURE POLICY (see the ladder). The
            // ladder has already raised the monotone floor to its conservative
            // level and re-factorized there, so the step below runs at that
            // floor. NOT an escape and NOT a break: the solve continues, and
            // what it can no longer produce is a STANDING CERTIFICATE, which
            // is what `evidence_failed` carries to the outcome block.
        } else if (read != InertiaRead::kOk) {
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
        // MECHANISM 4'S FIX (T4b): the right-hand side is built from the
        // SCHEDULE'S subproblem -- `rho_sched`, anchored at `zeta` -- and
        // never from the inertia-demanded modification, whose anchor is the
        // current iterate and whose gradient contribution here is therefore
        // zero. Before the separation this used the total, so the iterate
        // converged to the KKT point of a DIFFERENT problem than the one the
        // section 3.2 gate measures, and the gate never advanced again.
        build_rhs(rho_sched, delta, 0.0, /*corrector=*/false);
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
        build_rhs(rho_sched, delta, mu_t, /*corrector=*/true);
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
        // these two lines unreached, which IS the exclusion both fields
        // document ("iterations taken", N1).
        ++out.counters.ipqp_iters;
        rho_dem_final = rho_dem;
        if (elevated) {
            ++out.counters.ipqp_iters_at_elevated_rho;
            if (out.counters.ipqp_prox_center_updates == prox_updates_pre) {
                // THE ARMED WALK, COUNTED (T4b plan 2.1's declared gap). The
                // ladder is armed and the section 3.2 gate did not advance:
                // `zeta` and `rho_sched` are pinned while the iterate walks
                // down a negative-curvature direction whose residual is
                // GROWING. Expected, bounded by the walk's own geometry, and
                // measured rather than assumed.
                ++out.counters.ipqp_iters_ladder_armed_no_advance;
            }
        }

        // THE WINDOW ADVANCES ON ACCEPTED STEPS ONLY (spec 6.2), so it is
        // advanced here, beside `ipqp_iters` and behind every rejection above.
        ++win_steps;
        const double step_alpha = std::min(alpha_p, alpha_d);
        win_alpha_min = std::min(win_alpha_min, step_alpha);
        win_alpha_max = std::max(win_alpha_max, step_alpha);

        // ... and here it is discarded. Window discard: see the rule at the
        // stall-window declarations above. The next pass finds
        // `win_armed == false` and arms a fresh window at the point the ladder
        // actually left the trajectory at.
        if (rho_dem_max != rho_dem_max_pre) {
            win_armed = false;
        }
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
                // N2 (SETTLER RULING, fix round 2): A READ THAT NEVER HAPPENED
                // IS `3`, NOT `2`. The 0/1/2/3 contract defines `2` as
                // ATTEMPTED-and-unusable, which is why it maps to the
                // numerical escape class; a factorization the budget refused
                // was never attempted, so it belongs with the option-off case.
                // The ESCAPE stays `kBudget` -- the budget is genuinely why
                // this solve stopped -- and the certificate is downgraded,
                // because an unread certificate does not stand.
                out.counters.ipqp_final_inertia_read = 3;
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
                // C0b: UNREADABLE, PERTURBED, **or** a factorization that
                // FAILED. A perturbed factorization is not evidence about the
                // assembled matrix at all, so it is not "a reading that
                // disagreed" -- it is no reading; a failed one is not even a
                // factor. Note (h)'s `== 2`, numerical, for all three.
                //
                // SECTION 2.2'S EVIDENCE-FAILURE POLICY DOES NOT REACH HERE,
                // and that is the ruling rather than an oversight. The policy
                // permits A STEP at a conservative floor; the item 4 read
                // takes no step. There is nothing left to permit -- the
                // question the read exists to ask ("is the converged point a
                // minimum of the PROBLEM") simply has no answer -- so the
                // certificate cannot stand and the escape is the numerical
                // class note (h) assigns to an unreadable inertia.
                out.counters.ipqp_final_inertia_read = 2;
                out.certificate_downgraded = true;
                escape = IpqpEscape::kNumerical;
            }
        }
    }

    // SECTION 2.2'S WHOLE-SOLVE DOWNGRADE. Applied AFTER the item 4 block and
    // never conditioned on it: the policy's own scope is "the certificate is
    // downgraded FOR THE WHOLE SOLVE", so a final read that came back clean
    // (`ipqp_final_inertia_read == 0`) does not undo an evidence failure at
    // iteration 3, and an escaped solve carries the flag too -- it has no
    // certificate to downgrade, and saying so costs nothing while a
    // conditional would make the field mean two things.
    if (evidence_failed) {
        out.inertia_evidence_failed = true;
        out.certificate_downgraded = true;
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
        // A DOWNGRADE WITHOUT AN ESCAPE. Two ways in: the option-off read
        // (I5's ruling, `ipqp_final_inertia_read == 3`), and a mid-solve
        // evidence failure whose final read nonetheless came back clean
        // (`read == 0`). TASK 5'S STATUS RULING, argued in full in
        // ipqp_engine.h's own STATUS VOCABULARY note: `kNumericalError`, the
        // spelling the walk and SSN already use for a converged point whose
        // second-order certificate could not be established, and NOT a new
        // `QpStatus` enumerator -- spec 2.2 item 4 asks for one certification
        // vocabulary across all three kernels, and the certificate itself
        // travels on `certificate_downgraded` / `escape_reason`, never on
        // `status`.
        out.status = QpStatus::kNumericalError;
    }

    // --- THE FIVE-WAY ESCAPE CENSUS (spec section 7) -----------------------
    //
    // Written ONCE, from the single classified `escape_reason`, so the census
    // is a PARTITION by construction rather than by five call sites agreeing:
    // exactly one branch below can run, and each runs `ipqp_escapes` with it,
    // which is the whole content of the sum-to-`ipqp_escapes` invariant
    // (tests/sqp/support/ipqp_test_support.h asserts it on every fixture).
    //
    // THREE OUTCOMES DELIBERATELY CONTRIBUTE NOTHING HERE, each for a reason
    // already settled elsewhere:
    //   * `kNone` on a clean solve -- there is no escape.
    //   * `kNone` on a DOWNGRADED solve (`ipqp_final_inertia_read == 3`, or a
    //     mid-solve evidence failure) -- plan section 7 note (j): "a
    //     downgrade, not an escape ... no census entry, no section 6.1 K=3
    //     charge". The census must NOT fold `3` into
    //     `ipqp_escape_numerical`; there is no escape to count.
    //   * A DECLINED-PINNED subproblem, which returns far above this point
    //     with `ipqp_declined_pinned == 1` and every other counter at its
    //     default -- the tier never ran.
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
        // ---- THE TWO EXPORT INVARIANTS THIS TIER MUST RE-DERIVE ----------
        //
        // qp_engine.h's export contract states both against `QpSolution`, and
        // states in as many words that "a third producer must re-derive the
        // invariant rather than assume it is inherited". This tier is that
        // third producer (M6 W1 task 6).
        //
        // (6b) `bound_state[i] == kFree ==> z(i) == 0.0`, ON EVERY STATUS.
        // A barrier method carries a (zl, zu) pair at EVERY index with a
        // finite effective bound, and at an INACTIVE bound that pair is the
        // barrier residue (~ mu / distance) -- small, nonzero, and a price on
        // a bound this point is not standing on. Exporting it would put this
        // tier in the docket-D0 defect class the walk's own pins guard
        // against, so the price is written only where the classifier put the
        // variable ON a bound; every other index exports an exact 0. A
        // TR-PINNED index took the `continue` above and is already 0.
        //
        // (real-bound-only) THE ABSENT-SIDE TEST READS THE REAL BOUND, NOT THE
        // EFFECTIVE ONE -- `SsnEngine::split_bound_multipliers`' rule
        // verbatim. Under a FINITE radius every variable has finite EFFECTIVE
        // bounds whatever the caller's own box says, so a side gated on the
        // effective bound would price a bound the QP does not have;
        // `SsnEngine::solve` REFUSES such a start outright ("there is no row
        // for that multiplier"). The residue at an absent side is a
        // trust-region dual, and TR duals are internal (qp_problem.h), so
        // dropping it is the contract rather than a loss.
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
    // Taken as DELTAS off SymmetricFactor::Counters rather than tallied here,
    // so `ipqp_pattern_verifies` is literally the backend's own count (the
    // field's doc comment says "mirrors", and this is what makes that a fact
    // rather than a claim) and so the four cannot drift from each other.
    const KktFactorization::Counters after = kkt_.counters();
    out.counters.ipqp_factorizations = after.factorize_count - before.factorize_count;
    out.counters.ipqp_solves = after.solve_count - before.solve_count;
    out.counters.ipqp_symbolic_analyses = after.analyze_count - before.analyze_count;
    out.counters.ipqp_pattern_verifies = after.pattern_verify_count - before.pattern_verify_count;
    out.counters.ipqp_rho_demanded_max = rho_dem_max;
    out.counters.ipqp_rho_demanded_last = rho_dem_last;

    emit_ledger();

    return out;
}

} // namespace hven::solvers
