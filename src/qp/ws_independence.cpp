// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include <hven/detail/qp/ws_independence.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>

#include <Eigen/Eigenvalues>
#include <Eigen/QR>

#include <hven/detail/kkt/bordered_eqp.h>

namespace hven::solvers::detail {

namespace {

// The pass's solve system, labelled: where each variable and each live
// constraint component sits in the factor's coordinates.
struct DepSystem {
    const DepFactorView &f;
    bool bordered = false;
    Index dim = 0;
    std::vector<Index> var_pos; // full variable -> position, or -1 (eliminated)
    std::vector<Index> comp_pos;
    std::vector<WsMember> comp;
    BorderedSystem bsys; // border path only
    // The kRow members' rows, ascending, gathered once (RowSubset), and each
    // kRow member's position in it (-1 for other kinds).
    std::optional<RowSubset> rows_w;
    std::vector<Index> comp_row_pos;

    void gather_rows(const QpProblem &qp) {
        std::vector<Index> rows;
        for (const WsMember &m : comp) {
            if (m.kind == WsMember::Kind::kRow) {
                rows.push_back(m.idx);
            }
        }
        std::sort(rows.begin(), rows.end());
        comp_row_pos.assign(comp.size(), -1);
        for (std::size_t k = 0; k < comp.size(); ++k) {
            if (comp[k].kind == WsMember::Kind::kRow) {
                comp_row_pos[k] = static_cast<Index>(
                    std::lower_bound(rows.begin(), rows.end(), comp[k].idx) - rows.begin());
            }
        }
        rows_w.emplace(qp.Ai, std::move(rows));
    }

    DepSystem(const QpProblem &qp, const WorkingSet &ws, const DepFactorView &fv,
              const QpOptions &opts)
        : f(fv) {
        const Index n = qp.n();
        const Index me = qp.me();
        const std::vector<Index> &aw = ws.active_ineq();
        var_pos.assign(static_cast<std::size_t>(n), -1);
        if (f.schur != nullptr) {
            bordered = true;
            // THE PROXIMAL CENTRE IS IRRELEVANT HERE, and a zero vector says so
            // rather than borrowing an iterate this code does not have: the only
            // members of the built system this class ever reads are `border_v`,
            // `border_d` and `k0_row_deleted` (the two uses below are the whole
            // list), and the centre reaches none of them -- it enters `rhs`, which
            // the dependency test supplies itself.
            bsys = build_bordered_system(qp, *f.k0, *f.k0_rows, *f.ledger, ws, Vec::Zero(qp.n()),
                                         opts);
            const Index n0 = f.k0->K.rows();
            dim = n0 + f.schur->dim();
            for (Index i = 0; i < n; ++i) {
                var_pos[static_cast<std::size_t>(i)] = i;
            }
            for (Index r = 0; r < me; ++r) {
                push(n + r, WsMember::Kind::kEq, r);
            }
            for (std::size_t p = 0; p < f.k0_rows->size(); ++p) {
                if (!bsys.k0_row_deleted[p]) {
                    push(n + me + static_cast<Index>(p), WsMember::Kind::kRow, (*f.k0_rows)[p]);
                }
            }
            for (std::size_t b = 0; b < f.ledger->size(); ++b) {
                const BorderLedgerEntry &e = (*f.ledger)[b];
                if (e.kind == BorderLedgerEntry::Kind::kVarPin) {
                    push(n0 + static_cast<Index>(b), WsMember::Kind::kPin, e.target);
                } else if (e.kind == BorderLedgerEntry::Kind::kIneqRow) {
                    push(n0 + static_cast<Index>(b), WsMember::Kind::kRow, e.target);
                }
            }
            gather_rows(qp);
            return;
        }
        Index nf = 0;
        for (Index i = 0; i < n; ++i) {
            if (ws.bound_state()[static_cast<std::size_t>(i)] == BoundState::kFree) {
                var_pos[static_cast<std::size_t>(i)] = nf++;
            }
        }
        dim = nf + me + static_cast<Index>(aw.size());
        for (Index r = 0; r < me; ++r) {
            push(nf + r, WsMember::Kind::kEq, r);
        }
        for (std::size_t k = 0; k < aw.size(); ++k) {
            push(nf + me + static_cast<Index>(k), WsMember::Kind::kRow, aw[k]);
        }
        gather_rows(qp);
    }

    void push(Index pos, WsMember::Kind kind, Index idx) {
        comp_pos.push_back(pos);
        comp.push_back(WsMember{kind, idx});
    }

    Vec solve(const Vec &rhs) const {
        return bordered ? f.schur->solve(rhs) : solve_vec(*f.eliminated, rhs);
    }

    // A_W^T y in full-variable space, from y's constraint components.
    Vec at_times(const QpProblem &qp, const Vec &y) const {
        Vec lam_e = Vec::Zero(qp.me());
        Vec lam_i = Vec::Zero(static_cast<Index>(rows_w->rows().size()));
        Vec out = Vec::Zero(qp.n());
        for (std::size_t k = 0; k < comp.size(); ++k) {
            const double yk = y(comp_pos[k]);
            switch (comp[k].kind) {
            case WsMember::Kind::kEq:
                lam_e(comp[k].idx) += yk;
                break;
            case WsMember::Kind::kRow:
                lam_i(comp_row_pos[k]) += yk;
                break;
            case WsMember::Kind::kPin:
                out(comp[k].idx) += yk;
                break;
            }
        }
        if (qp.me() > 0) {
            out += qp.Ae.transpose() * lam_e;
        }
        if (qp.mi() > 0) {
            rows_w->add_t(out, lam_i, /*subtract=*/false);
        }
        return out;
    }

    // rhs - K_{delta,0} y: the factored system with mu (only) backed out.
    Vec residual(const QpProblem &qp, const Vec &rhs, const Vec &y, const QpOptions &opts) const {
        if (bordered) {
            // bordered_residual backs out the DUAL regularization and the borders'
            // own d_i, and leaves the primal delta in the model -- which is this
            // function's contract exactly (M7 W2 E4; before it, this call had to
            // zero primal_delta in a local copy of `opts` to get the same object).
            return bordered_residual(f.k0->K, qp.n(), bsys.border_v, bsys.border_d, rhs, y, opts);
        }
        const Index n = qp.n();
        Vec u = Vec::Zero(n);
        for (Index i = 0; i < n; ++i) {
            const Index p = var_pos[static_cast<std::size_t>(i)];
            if (p >= 0) {
                u(i) = y(p);
            }
        }
        const Vec top =
            Vec(qp.H.selfadjointView<Eigen::Upper>() * u) + opts.primal_delta * u + at_times(qp, y);
        const Vec ae_u = qp.me() > 0 ? Vec(qp.Ae * u) : Vec::Zero(0);
        const Vec ai_u = qp.mi() > 0 ? rows_w->times(u) : Vec::Zero(0);
        Vec r = rhs;
        for (Index i = 0; i < n; ++i) {
            const Index p = var_pos[static_cast<std::size_t>(i)];
            if (p >= 0) {
                r(p) -= top(i);
            }
        }
        for (std::size_t k = 0; k < comp.size(); ++k) {
            const WsMember &m = comp[k];
            if (m.kind == WsMember::Kind::kEq) {
                r(comp_pos[k]) -= ae_u(m.idx);
            } else if (m.kind == WsMember::Kind::kRow) {
                r(comp_pos[k]) -= ai_u(comp_row_pos[k]);
            } else {
                r(comp_pos[k]) -= u(m.idx);
            }
        }
        return r;
    }
};

double member_norm(const QpProblem &qp, const WsMember &m) {
    switch (m.kind) {
    case WsMember::Kind::kEq:
        return qp.Ae.row(m.idx).norm();
    case WsMember::Kind::kRow:
        return qp.Ai.row(m.idx).norm();
    case WsMember::Kind::kPin:
        break;
    }
    return 1.0;
}

Index ws_rows(const QpProblem &qp, const WorkingSet &ws) {
    return qp.me() + static_cast<Index>(ws.active_ineq().size()) + (qp.n() - ws.num_free());
}

} // namespace

namespace {

// The dependency test's body; `target` is the refinement's stopping residual
// (<= 0: refine until a step stops lowering the residual or the steps run out).
DepTest dependency_test_to(const QpProblem &qp, const WorkingSet &ws, const DepFactorView &f,
                           const Vec &a, const QpOptions &opts, double target) {
    const Index n = qp.n();
    const std::vector<Index> &aw = ws.active_ineq();
    DepTest t;
    t.screen = ws_rows(qp, ws) + 1 > n;
    t.v_e = Vec::Zero(qp.me());
    t.v_w = Vec::Zero(static_cast<Index>(aw.size()));
    t.v_pin = Vec::Zero(n);
    const double a_norm = a.norm();

    // No free column: the pins alone span every candidate.
    if (ws.num_free() == 0 || !(a_norm > 0.0)) {
        for (Index i = 0; i < n; ++i) {
            if (ws.bound_state()[static_cast<std::size_t>(i)] != BoundState::kFree) {
                t.v_pin(i) = a(i);
            }
        }
        t.rho = 0.0;
        t.verdict = DepVerdict::kDependent;
        return t;
    }
    if (!f.usable()) {
        // AN UN-RUN TEST IS NOT A VERDICT OF DEPENDENCE (E2 fix7, settler
        // E4-37(b)). There is no factor for this pass's set, so nothing was
        // computed: rho stays at its NaN default and `solves` at 0. The
        // integer screen is the only statement available, and it is exact --
        // positive it PROVES dependence; negative it proves nothing, and the
        // candidate is admitted as the pre-E2 walk admits it (S17). `unrun`
        // says the verdict rests on the screen alone so the caller can count
        // it (QpCounters::dep_unrun_tests) and so no reader mistakes the NaN
        // rho for a measurement. R-A still runs at every growth site: an
        // exact structural dependency needs no factor and overrules this.
        t.verdict = t.screen ? DepVerdict::kDependent : DepVerdict::kIndependent;
        t.unrun = true;
        return t;
    }

    const DepSystem sys(qp, ws, f, opts);
    Vec rhs = Vec::Zero(sys.dim);
    for (Index i = 0; i < n; ++i) {
        const Index p = sys.var_pos[static_cast<std::size_t>(i)];
        if (p >= 0) {
            rhs(p) = a(i);
        }
    }
    Vec y = sys.solve(rhs);
    ++t.solves;
    Vec res = sys.residual(qp, rhs, y, opts);
    double res_norm = res.lpNorm<Eigen::Infinity>();
    for (Index s = 0; s < kDepRefineSteps && res_norm > target; ++s) {
        const Vec cand = y + sys.solve(res);
        ++t.solves;
        Vec cres = sys.residual(qp, rhs, cand, opts);
        const double cn = cres.lpNorm<Eigen::Infinity>();
        if (!(cn < res_norm)) {
            break;
        }
        y = cand;
        res = std::move(cres);
        res_norm = cn;
    }

    // Coefficients on the rows; pins absorb what is left at pinned columns.
    for (std::size_t k = 0; k < sys.comp.size(); ++k) {
        const WsMember &m = sys.comp[k];
        const double yk = y(sys.comp_pos[k]);
        if (m.kind == WsMember::Kind::kEq) {
            t.v_e(m.idx) = yk;
        } else if (m.kind == WsMember::Kind::kRow) {
            const auto it = std::lower_bound(aw.begin(), aw.end(), m.idx);
            if (it != aw.end() && *it == m.idx) {
                t.v_w(static_cast<Index>(it - aw.begin())) = yk;
            }
        }
    }
    Vec w = a;
    if (qp.me() > 0) {
        w -= qp.Ae.transpose() * t.v_e;
    }
    if (qp.mi() > 0) {
        // t.v_w is indexed by aw's (ascending) positions already.
        if (sys.rows_w->rows() == aw) {
            sys.rows_w->add_t(w, t.v_w, /*subtract=*/true);
        } else {
            RowSubset(qp.Ai, aw).add_t(w, t.v_w, /*subtract=*/true);
        }
    }
    double free_sq = 0.0;
    for (Index i = 0; i < n; ++i) {
        if (ws.bound_state()[static_cast<std::size_t>(i)] == BoundState::kFree) {
            free_sq += w(i) * w(i);
        } else {
            t.v_pin(i) = w(i);
        }
    }
    t.rho = std::sqrt(free_sq) / a_norm;
    const bool converged = res_norm <= std::max(target, 0.0);
    // The landed contract (header, S23): a converged rho above kDepTol is
    // independent, including kDepTol < rho < kDepUndecidable; a stall below
    // kDepUndecidable is undecidable; a stall at or above it is admitted as
    // independent and flagged for the caller's dep_stall_admits.
    if (t.screen || t.rho <= kDepTol) {
        t.verdict = DepVerdict::kDependent;
    } else if (!converged && t.rho < kDepUndecidable) {
        t.verdict = DepVerdict::kUndecidable;
    } else if (std::isnan(t.rho)) {
        t.verdict = DepVerdict::kUndecidable;
    } else {
        t.verdict = DepVerdict::kIndependent;
        t.stall_admit = !converged;
    }
    return t;
}

} // namespace

DepTest dependency_test(const QpProblem &qp, const WorkingSet &ws, const DepFactorView &f,
                        const Vec &a, const QpOptions &opts) {
    return dependency_test_to(qp, ws, f, a, opts, 0.1 * kDepTol * a.lpNorm<Eigen::Infinity>());
}

DepTest certificate_coefficients(const QpProblem &qp, const WorkingSet &ws, const DepFactorView &f,
                                 const Vec &a, const QpOptions &opts) {
    return dependency_test_to(qp, ws, f, a, opts, 0.0);
}

double dependency_rounding_floor(const QpProblem &qp, const WorkingSet &ws, const DepTest &t,
                                 const Vec &a) {
    const double a_norm = a.norm();
    const std::vector<Index> &aw = ws.active_ineq();
    double v1 = 1.0; // a's own unit coefficient
    double amax = a_norm;
    for (Index r = 0; r < t.v_e.size(); ++r) {
        if (t.v_e(r) != 0.0) {
            v1 += std::abs(t.v_e(r));
            amax = std::max(amax, qp.Ae.row(r).norm());
        }
    }
    for (Index k = 0; k < t.v_w.size(); ++k) {
        if (t.v_w(k) != 0.0) {
            v1 += std::abs(t.v_w(k));
            amax = std::max(amax, qp.Ai.row(aw[static_cast<std::size_t>(k)]).norm());
        }
    }
    const double m = static_cast<double>(qp.n() + qp.me() + static_cast<Index>(aw.size()));
    return kDepRereadKappa * m * std::numeric_limits<double>::epsilon() * v1 * amax / a_norm;
}

EntryDetection detect_dependency(const QpProblem &qp, const WorkingSet &ws, const DepFactorView &f,
                                 const QpOptions &opts) {
    EntryDetection d;
    // STRUCTURAL: a member row whose nonzeros all sit at pinned columns is
    // spanned by those pins. Exact, no solve.
    const std::vector<BoundState> &bs = ws.bound_state();
    const auto pinned_row = [&](const SpMatRM &A, Index r, WsMember::Kind kind) {
        for (SpMatRM::InnerIterator it(A, r); it; ++it) {
            if (it.value() != 0.0 && bs[static_cast<std::size_t>(it.col())] == BoundState::kFree) {
                return false;
            }
        }
        d.found = true;
        d.support.push_back(WsMember{kind, r});
        for (SpMatRM::InnerIterator it(A, r); it; ++it) {
            if (it.value() != 0.0) {
                d.support.push_back(WsMember{WsMember::Kind::kPin, it.col()});
            }
        }
        return true;
    };
    if (ws.num_free() < qp.n()) {
        for (Index r = 0; r < qp.me(); ++r) {
            if (pinned_row(qp.Ae, r, WsMember::Kind::kEq)) {
                return d;
            }
        }
        for (const Index r : ws.active_ineq()) {
            if (pinned_row(qp.Ai, r, WsMember::Kind::kRow)) {
                return d;
            }
        }
    }
    if (!f.usable()) {
        return d;
    }
    const DepSystem sys(qp, ws, f, opts);
    const std::size_t nc = sys.comp.size();
    if (nc == 0) {
        return d;
    }
    double row_max = 1.0;
    for (std::size_t k = 0; k < nc; ++k) {
        row_max = std::max(row_max, member_norm(qp, sys.comp[k]));
    }
    // ||A_W^T y||, free columns only, for y on the constraint components.
    const auto null_residual = [&](const Vec &y) {
        const Vec aty = sys.at_times(qp, y);
        double s = 0.0;
        for (Index i = 0; i < qp.n(); ++i) {
            if (sys.var_pos[static_cast<std::size_t>(i)] >= 0 &&
                (sys.bordered ||
                 ws.bound_state()[static_cast<std::size_t>(i)] == BoundState::kFree)) {
                s += aty(i) * aty(i);
            }
        }
        return std::sqrt(s);
    };
    // Irrational start: a rational progression is orthogonal to second-
    // difference null vectors (observed: two equalities plus three pins).
    constexpr double kGolden = 0.6180339887498949;
    Vec rhs = Vec::Zero(sys.dim);
    for (std::size_t k = 0; k < nc; ++k) {
        const double t = static_cast<double>(k + 1) * kGolden;
        rhs(sys.comp_pos[k]) = 1.0 + (t - std::floor(t));
    }
    bool converged = false;
    for (Index pass = 0; pass < kEntryDetectSolves && !converged; ++pass) {
        const Vec z = sys.solve(rhs);
        ++d.solves;
        double zn = 0.0;
        for (std::size_t k = 0; k < nc; ++k) {
            zn += z(sys.comp_pos[k]) * z(sys.comp_pos[k]);
        }
        zn = std::sqrt(zn);
        if (!(zn > 0.0) || !std::isfinite(zn)) {
            return d;
        }
        rhs.setZero();
        for (std::size_t k = 0; k < nc; ++k) {
            rhs(sys.comp_pos[k]) = z(sys.comp_pos[k]) / zn;
        }
        d.null_residual = null_residual(rhs) / row_max;
        converged = d.null_residual <= kDepTol;
    }
    if (!converged) {
        return d;
    }
    // rhs holds y_hat (unit norm) on the constraint components.
    std::vector<double> scaled(nc, 0.0);
    double scaled_max = 0.0;
    for (std::size_t k = 0; k < nc; ++k) {
        scaled[k] = std::abs(rhs(sys.comp_pos[k])) * member_norm(qp, sys.comp[k]);
        scaled_max = std::max(scaled_max, scaled[k]);
    }
    // THE PINS' COEFFICIENTS (E2 fix1, D2). On the eliminated path the pins
    // are eliminated columns, not constraint components of the factor: the
    // null vector gives A_W^T y_hat = 0 on the free columns only, and its
    // value at a pinned column IS that pin's coefficient in the dependency
    // (unit norm, like the pin). Without them a dependency an optional pin
    // creates is attributed to the rows alone -- mandatory equalities, so
    // nothing is removed and the walk runs on a dependent set with 1/mu
    // multipliers. Same threshold as the rows. (On the border path the pins
    // are ledger components and already in `sys.comp`.)
    Vec pin_coef;
    if (!sys.bordered) {
        pin_coef = sys.at_times(qp, rhs);
        for (Index i = 0; i < qp.n(); ++i) {
            if (bs[static_cast<std::size_t>(i)] != BoundState::kFree) {
                scaled_max = std::max(scaled_max, std::abs(pin_coef(i)));
            }
        }
    }
    if (!(scaled_max > 0.0)) {
        return d;
    }
    d.found = true;
    // THE REMOVAL CERTIFICATE (E2 fix1b, E2-D2). From A_W^T y_hat = r,
    // dist(a_k, span{a_i, i != k}) <= ||r|| / |y_k|: rho_k = ||r|| / (|y_k|
    // ||a_k||) is an UPPER BOUND on member k's relative distance from the
    // others' span, so rho_k <= kDepRemovalTol is a SUFFICIENT numerical
    // certificate that k is dependent, and 1e-3 is a numerical-rank policy
    // (not the candidate test's boundary: there a converged 1e-6 < rho < 1e-3
    // is independent). A member that fails it is in the support by a small
    // coefficient, not certified, and removing it need not remove the
    // dependency (observed: f7_n800_path_warm, a pin at 2e-4 of the largest
    // coefficient removed, re-blocking at zero motion). Such members are not
    // offered for removal; mandatory members (equalities, fixed pins) are
    // never removed and stay listed. With no removable member certified, the
    // uncertified one with the largest scaled coefficient is offered alone:
    // a HEURISTIC fallback, kept (settler S24), counted by the caller as
    // dep_uncertified_fallbacks.
    const double resid = d.null_residual * row_max;
    const auto removable = [&](const WsMember &m) {
        return m.kind == WsMember::Kind::kRow ||
               (m.kind == WsMember::Kind::kPin &&
                bs[static_cast<std::size_t>(m.idx)] != BoundState::kFixed);
    };
    WsMember best{};
    double best_coef = 0.0;
    bool any_certified = false;
    const auto offer = [&](const WsMember &m, double coef) {
        if (coef < kDepCoeffTol * scaled_max) {
            return;
        }
        if (!removable(m)) {
            d.support.push_back(m);
            return;
        }
        if (resid <= kDepRemovalTol * coef) {
            d.support.push_back(m);
            any_certified = true;
            return;
        }
        d.uncertified_dropped = true;
        if (coef > best_coef) {
            best = m;
            best_coef = coef;
        }
    };
    for (std::size_t k = 0; k < nc; ++k) {
        offer(sys.comp[k], scaled[k]);
    }
    if (!sys.bordered) {
        for (Index i = 0; i < qp.n(); ++i) {
            if (bs[static_cast<std::size_t>(i)] != BoundState::kFree) {
                offer(WsMember{WsMember::Kind::kPin, i}, std::abs(pin_coef(i)));
            }
        }
    }
    if (!any_certified && best_coef > 0.0) {
        // Heuristic, uncertified (see above).
        d.support.push_back(best);
        d.uncertified_fallback = true;
    }
    return d;
}

StructuralDependency structural_dependency(const QpProblem &qp, const WorkingSet &ws, bool is_ineq,
                                           Index idx) {
    const Index n = qp.n();
    const std::vector<Index> &aw = ws.active_ineq();
    const std::vector<BoundState> &bs = ws.bound_state();
    const auto pinned = [&](Index c) {
        return bs[static_cast<std::size_t>(c)] != BoundState::kFree;
    };
    StructuralDependency sd;
    DepTest &t = sd.test;
    t.v_e = Vec::Zero(qp.me());
    t.v_w = Vec::Zero(static_cast<Index>(aw.size()));
    t.v_pin = Vec::Zero(n);
    t.screen = ws_rows(qp, ws) + 1 > n;
    const auto exact = [&]() {
        t.rho = 0.0;
        t.verdict = DepVerdict::kDependent;
        sd.found = true;
        return sd;
    };
    if (is_ineq) {
        // A row on pinned columns only: c = sum_i a_i e_i over its pins.
        for (SpMatRM::InnerIterator it(qp.Ai, idx); it; ++it) {
            if (it.value() != 0.0 && !pinned(it.col())) {
                return sd;
            }
        }
        bool any = false;
        for (SpMatRM::InnerIterator it(qp.Ai, idx); it; ++it) {
            t.v_pin(it.col()) += it.value();
            any = any || it.value() != 0.0;
        }
        return any ? exact() : sd;
    }
    if (pinned(idx)) {
        return sd;
    }
    // A pin completing a member row r (every nonzero at a pinned column or at
    // idx, a_r,idx != 0): e_idx = (a_r - sum_{j pinned} a_rj e_j) / a_r,idx.
    // Equalities first, then working rows, each ascending.
    const auto completes = [&](const SpMatRM &A, Index r, double &a_ri) {
        a_ri = 0.0;
        for (SpMatRM::InnerIterator it(A, r); it; ++it) {
            if (it.value() == 0.0) {
                continue;
            }
            if (it.col() == idx) {
                a_ri += it.value();
            } else if (!pinned(it.col())) {
                return false;
            }
        }
        return a_ri != 0.0;
    };
    const auto fill_pins = [&](const SpMatRM &A, Index r, double a_ri) {
        for (SpMatRM::InnerIterator it(A, r); it; ++it) {
            if (it.col() != idx) {
                t.v_pin(it.col()) -= it.value() / a_ri;
            }
        }
    };
    double a_ri = 0.0;
    for (Index r = 0; r < qp.me(); ++r) {
        if (completes(qp.Ae, r, a_ri)) {
            t.v_e(r) = 1.0 / a_ri;
            fill_pins(qp.Ae, r, a_ri);
            return exact();
        }
    }
    for (std::size_t k = 0; k < aw.size(); ++k) {
        if (completes(qp.Ai, aw[k], a_ri)) {
            t.v_w(static_cast<Index>(k)) = 1.0 / a_ri;
            fill_pins(qp.Ai, aw[k], a_ri);
            return exact();
        }
    }
    return sd;
}

bool terminal_null_direction(const QpProblem &qp, const WorkingSet &ws, const DepFactorView &f,
                             const Vec &x, const QpOptions &opts,
                             const std::function<bool(const Vec &, const Vec &)> &accept,
                             Index &solves) {
    if (!f.usable()) {
        return false;
    }
    const DepSystem sys(qp, ws, f, opts);
    const std::size_t nc = sys.comp.size();
    if (nc == 0) {
        return false;
    }
    const Vec re = qp.me() > 0 ? Vec(qp.Ae * x - qp.be) : Vec::Zero(0);
    Vec r = Vec::Zero(static_cast<Index>(nc));
    for (std::size_t k = 0; k < nc; ++k) {
        const WsMember &m = sys.comp[k];
        if (m.kind == WsMember::Kind::kEq) {
            r(static_cast<Index>(k)) = re(m.idx);
        } else if (m.kind == WsMember::Kind::kRow) {
            r(static_cast<Index>(k)) = qp.Ai.row(m.idx).dot(x) - qp.bi(m.idx);
        }
    }
    const double rn = r.norm();
    if (!(rn > 0.0) || !std::isfinite(rn)) {
        return false;
    }
    // THE CORRECTION FORM: rho = A_W^T y on the free columns, formed from A itself; K [u; v] =
    // [rho; 0]; y <- y - v. The same operator as solving K [0; y] (y' ~ mu (M + mu)^-1 y), but its
    // error scales with the shrinking correction, so A_W^T y reaches rounding (an unbounded column
    // needs it there) rather than eps * cond(K).
    Vec y = r / rn;
    double prev = std::numeric_limits<double>::infinity();
    for (Index pass = 0;; ++pass) {
        Vec y_pos = Vec::Zero(sys.dim);
        Vec y_e = Vec::Zero(qp.me());
        Vec y_i = Vec::Zero(qp.mi());
        const double orient = y.dot(r) < 0.0 ? -1.0 : 1.0;
        for (std::size_t k = 0; k < nc; ++k) {
            const WsMember &m = sys.comp[k];
            const double yk = y(static_cast<Index>(k));
            y_pos(sys.comp_pos[k]) = yk;
            if (m.kind == WsMember::Kind::kEq) {
                y_e(m.idx) = orient * yk;
            } else if (m.kind == WsMember::Kind::kRow) {
                y_i(m.idx) = orient * yk;
            }
        }
        if (accept(y_e, y_i)) {
            return true;
        }
        const Vec aty = sys.at_times(qp, y_pos);
        Vec rhs = Vec::Zero(sys.dim);
        double res_sq = 0.0;
        for (Index i = 0; i < qp.n(); ++i) {
            const Index p = sys.var_pos[static_cast<std::size_t>(i)];
            if (p >= 0 && (sys.bordered ||
                           ws.bound_state()[static_cast<std::size_t>(i)] == BoundState::kFree)) {
                rhs(p) = aty(i);
                res_sq += aty(i) * aty(i);
            }
        }
        const double res = std::sqrt(res_sq);
        if (pass == kTerminalNullSolves || !(res > 0.0) ||
            !(res <= kTerminalNullContraction * prev)) {
            return false; // the budget, or a stall: no further separation of the null space
        }
        prev = res;
        const Vec z = sys.solve(rhs);
        ++solves;
        for (std::size_t k = 0; k < nc; ++k) {
            y(static_cast<Index>(k)) -= z(sys.comp_pos[k]);
        }
        const double yn = y.norm();
        if (!(yn > 0.0) || !std::isfinite(yn)) {
            return false;
        }
        y /= yn;
    }
}

Vec face_solve(const QpProblem &qp, const WorkingSet &ws, const DepFactorView &f, const Vec &u,
               const QpOptions &opts, Index &solves) {
    if (!f.usable() || ws.num_free() == 0) {
        return Vec();
    }
    const Index n = qp.n();
    const DepSystem sys(qp, ws, f, opts);
    const auto is_free = [&](Index i) {
        return ws.bound_state()[static_cast<std::size_t>(i)] == BoundState::kFree;
    };
    Vec rhs = Vec::Zero(sys.dim);
    for (Index i = 0; i < n; ++i) {
        const Index p = sys.var_pos[static_cast<std::size_t>(i)];
        if (p >= 0 && is_free(i)) {
            rhs(p) = u(i);
        }
    }
    Vec y = sys.solve(rhs);
    ++solves;
    Vec res = sys.residual(qp, rhs, y, opts);
    double res_norm = res.lpNorm<Eigen::Infinity>();
    const double target = 1e-14 * std::max(1.0, rhs.lpNorm<Eigen::Infinity>());
    for (Index s = 0; s < kDepRefineSteps && res_norm > target; ++s) {
        const Vec cand = y + sys.solve(res);
        ++solves;
        Vec cres = sys.residual(qp, rhs, cand, opts);
        const double cn = cres.lpNorm<Eigen::Infinity>();
        if (!(cn < res_norm)) {
            break;
        }
        y = cand;
        res = std::move(cres);
        res_norm = cn;
    }
    Vec out = Vec::Zero(n);
    for (Index i = 0; i < n; ++i) {
        const Index p = sys.var_pos[static_cast<std::size_t>(i)];
        if (p >= 0 && is_free(i)) {
            out(i) = y(p);
        }
    }
    return out;
}

Vec face_negative_curvature(const QpProblem &qp, const WorkingSet &ws, const DepFactorView &f,
                            const QpOptions &opts, double curvature_tol, Index &solves) {
    const Index n = qp.n();
    if (!f.usable() || ws.num_free() == 0) {
        return Vec();
    }
    // A fixed, sign-varied start on the free columns (deterministic), carried
    // into the face by one application of the operator.
    Vec u0 = Vec::Zero(n);
    for (Index i = 0; i < n; ++i) {
        if (ws.bound_state()[static_cast<std::size_t>(i)] == BoundState::kFree) {
            u0(i) = (1.0 + static_cast<double>(i % 7) / 7.0) * ((i % 2 == 1) ? -1.0 : 1.0);
        }
    }
    Vec v = face_solve(qp, ws, f, u0, opts, solves);
    const double v_norm = v.size() > 0 ? v.norm() : 0.0;
    if (!(v_norm > 0.0) || !std::isfinite(v_norm)) {
        return Vec();
    }
    v /= v_norm;
    std::vector<Vec> basis;
    std::vector<double> alpha;
    std::vector<double> beta;
    for (Index k = 0; k < kSaddleLanczosSteps; ++k) {
        basis.push_back(v);
        Vec w = face_solve(qp, ws, f, v, opts, solves);
        if (w.size() == 0 || !std::isfinite(w.norm())) {
            basis.pop_back();
            break;
        }
        const double a = v.dot(w);
        alpha.push_back(a);
        // Full reorthogonalisation against every basis vector, twice.
        for (int pass = 0; pass < 2; ++pass) {
            for (const Vec &b : basis) {
                w -= b.dot(w) * b;
            }
        }
        const double b = w.norm();
        if (!(b > 1e-12 * std::max(1.0, std::abs(a)))) {
            break; // an invariant subspace: the Krylov space is exhausted
        }
        if (k + 1 < kSaddleLanczosSteps) {
            beta.push_back(b);
            v = w / b;
        }
    }
    const auto m = static_cast<Index>(alpha.size());
    if (m == 0) {
        return Vec();
    }
    Eigen::MatrixXd tri = Eigen::MatrixXd::Zero(m, m);
    for (Index k = 0; k < m; ++k) {
        tri(k, k) = alpha[static_cast<std::size_t>(k)];
        if (k + 1 < m) {
            tri(k, k + 1) = beta[static_cast<std::size_t>(k)];
            tri(k + 1, k) = beta[static_cast<std::size_t>(k)];
        }
    }
    const Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(tri);
    if (es.info() != Eigen::Success) {
        return Vec();
    }
    // Every NEGATIVE Ritz value is a candidate (the operator is the INVERSE, so
    // its most negative end is the negative eigenvalue nearest zero, which can be
    // a rounding-level one); the verified quotient on H itself decides.
    Vec best;
    double best_q = 0.0;
    for (Index j = 0; j < m && es.eigenvalues()(j) < 0.0; ++j) {
        Vec d = Vec::Zero(n);
        for (Index k = 0; k < m; ++k) {
            d += es.eigenvectors()(k, j) * basis[static_cast<std::size_t>(k)];
        }
        const double dd = d.squaredNorm();
        if (!(dd > 0.0)) {
            continue;
        }
        const double q = d.dot(qp.H.selfadjointView<Eigen::Upper>() * d) / dd;
        if (q <= -curvature_tol && (best.size() == 0 || q < best_q)) {
            best = d / std::sqrt(dd);
            best_q = q;
        }
    }
    return best;
}

namespace {

// A deterministic value in [-1, 1] of (i, j): splitmix64, identical on every platform.
double rank_start_value(std::uint64_t i, std::uint64_t j) {
    std::uint64_t z = i * 0x9E3779B97F4A7C15ULL + j + 0x632BE59BD9B4E019ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    z ^= z >> 31;
    return static_cast<double>(z >> 11) * (2.0 / 9007199254740992.0) - 1.0;
}

// The thin orthonormal basis of X's range (Householder; X has at least as many rows as columns).
Eigen::MatrixXd thin_q(const Eigen::MatrixXd &X) {
    Eigen::HouseholderQR<Eigen::MatrixXd> qr(X);
    return qr.householderQ() * Eigen::MatrixXd::Identity(X.rows(), X.cols());
}

} // namespace

RankRepair rank_repair(const QpProblem &qp, const WorkingSet &ws, const Vec &shift,
                       const QpOptions &opts, int threads, double block_cap) {
    RankRepair out;
    const Index n = qp.n();
    const Index me = qp.me();
    const std::vector<Index> &aw = ws.active_ineq();
    const std::vector<BoundState> &bs = ws.bound_state();
    const Index mr = me + static_cast<Index>(aw.size());
    if (mr == 0) {
        return out; // no rows: the pins alone are independent
    }
    std::vector<Index> fpos(static_cast<std::size_t>(n), -1);
    std::vector<Index> pins;
    Index nf = 0;
    for (Index i = 0; i < n; ++i) {
        if (bs[static_cast<std::size_t>(i)] == BoundState::kFree) {
            fpos[static_cast<std::size_t>(i)] = nf++;
        } else {
            pins.push_back(i);
        }
    }
    // The rows, each divided by its 2-norm (an empty row by 1): M on every column, M_F on the free
    // columns, and their right-hand sides.
    std::vector<Eigen::Triplet<double>> tf;
    std::vector<Eigen::Triplet<double>> tm;
    Vec rhs(mr);
    const auto row_of = [&](Index k, const SpMatRM *&A, Index &r) {
        A = k < me ? &qp.Ae : &qp.Ai;
        r = k < me ? k : aw[static_cast<std::size_t>(k - me)];
    };
    for (Index k = 0; k < mr; ++k) {
        const SpMatRM *A = nullptr;
        Index r = 0;
        row_of(k, A, r);
        double s = 0.0;
        for (SpMatRM::InnerIterator it(*A, r); it; ++it) {
            s += it.value() * it.value();
        }
        s = std::sqrt(s);
        if (!(s > 0.0) || !std::isfinite(s)) {
            if (!std::isfinite(s)) {
                out.outcome = RankRepair::Outcome::kRefused;
                return out;
            }
            s = 1.0;
        }
        for (SpMatRM::InnerIterator it(*A, r); it; ++it) {
            tm.emplace_back(k, it.col(), it.value() / s);
            const Index f = fpos[static_cast<std::size_t>(it.col())];
            if (f >= 0) {
                tf.emplace_back(k, f, it.value() / s);
            }
        }
        rhs(k) = (k < me ? qp.be(r) : qp.bi(r)) / s;
    }
    SpMatRM M(mr, n);
    M.setFromTriplets(tm.begin(), tm.end());
    SpMatRM MF(mr, std::max<Index>(nf, 1));
    MF.setFromTriplets(tf.begin(), tf.end());

    // The numerical left null space Y of M_F (mr x k, orthonormal columns).
    Eigen::MatrixXd Y;
    SpMatRM G = SpMatRM(MF * SpMatRM(MF.transpose()));
    double omega = 0.0;
    for (Index r = 0; r < G.outerSize(); ++r) {
        double s = 0.0;
        for (SpMatRM::InnerIterator it(G, r); it; ++it) {
            s += std::abs(it.value());
        }
        omega = std::max(omega, s);
    }
    if (nf == 0 || !(omega > 0.0)) {
        Y = Eigen::MatrixXd::Identity(mr, mr); // every row is empty on the free columns
    } else {
        const double t = kRankGramShift * omega;
        hven::linear::SymmetricFactor factor(sqp_kkt_options(threads));
        bool factored = false;
        const double mnorm = std::sqrt(omega); // >= ||M_F||_2
        Index b = std::min<Index>(mr, kRankBlockStart);
        for (;;) {
            if (mr <= kRankDenseRows) {
                // Few rows: the Gram's own dense eigenpairs, the same squared resolution as B'B
                // below.
                const Eigen::MatrixXd Gd = Eigen::MatrixXd(G);
                const Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(Gd);
                if (es.info() != Eigen::Success) {
                    out.outcome = RankRepair::Outcome::kRefused;
                    return out;
                }
                std::vector<Index> nulls;
                for (Index j = 0; j < mr; ++j) {
                    if (std::sqrt(std::max(0.0, es.eigenvalues()(j))) <= kRankNullTol * mnorm) {
                        nulls.push_back(j);
                    }
                }
                Y.resize(mr, static_cast<Index>(nulls.size()));
                for (std::size_t q = 0; q < nulls.size(); ++q) {
                    Y.col(static_cast<Index>(q)) = es.eigenvectors().col(nulls[q]);
                }
                break;
            }
            if (static_cast<double>(mr) * static_cast<double>(b) > block_cap) {
                out.outcome = RankRepair::Outcome::kRefused;
                return out;
            }
            if (!factored) {
                SpMatRM shiftI(mr, mr);
                shiftI.setIdentity();
                SpMatRM A = SpMatRM(G + t * shiftI).triangularView<Eigen::Upper>();
                A.makeCompressed();
                try {
                    factor.analyze(A);
                    const hven::linear::FactorizeOutcome fo = factor.factorize(A);
                    if (fo.status != hven::linear::FactorizeOutcome::Status::kOk ||
                        (fo.inertia.perturbed_pivots.has_value() &&
                         *fo.inertia.perturbed_pivots != 0)) {
                        out.outcome = RankRepair::Outcome::kRefused;
                        return out;
                    }
                } catch (const std::exception &) {
                    out.outcome = RankRepair::Outcome::kRefused;
                    return out;
                }
                factored = true;
            }
            Eigen::MatrixXd X(mr, b);
            for (Index i = 0; i < mr; ++i) {
                for (Index j = 0; j < b; ++j) {
                    X(i, j) = rank_start_value(static_cast<std::uint64_t>(i),
                                               static_cast<std::uint64_t>(j));
                }
            }
            X = thin_q(X);
            Eigen::MatrixXd Xn(mr, b);
            try {
                for (int it = 0; it < kRankIterations; ++it) {
                    factor.solve(X, Xn);
                    if (!Xn.allFinite()) {
                        out.outcome = RankRepair::Outcome::kRefused;
                        return out;
                    }
                    X = thin_q(Xn);
                }
            } catch (const std::exception &) {
                out.outcome = RankRepair::Outcome::kRefused;
                return out;
            }
            // Ritz pairs of G on span(X) from the thin B = M_F^T X: B'B = X'(M_F M_F^T)X, so its
            // eigenpairs are the Ritz pairs and the square roots of its eigenvalues the Ritz
            // singular values of M_F^T. B'B is b x b; its rounding is eps ||B||^2, i.e. a
            // singular-value resolution of about sqrt(eps) ||M_F||, beneath kRankNullTol.
            const Eigen::MatrixXd B = SpMatRM(MF.transpose()) * X; // nf x b
            const Eigen::MatrixXd C = B.transpose() * B;
            const Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(C);
            if (es.info() != Eigen::Success) {
                out.outcome = RankRepair::Outcome::kRefused;
                return out;
            }
            const Eigen::MatrixXd V = es.eigenvectors();
            Vec sv(b);
            for (Index j = 0; j < b; ++j) {
                sv(j) = std::sqrt(std::max(0.0, es.eigenvalues()(j)));
            }
            // The block is adequate once its largest Ritz value clears the shift's band.
            const double smax = sv.size() > 0 ? sv.maxCoeff() : 0.0;
            if (b < mr && !(smax * smax >= kRankBlockEdge * t)) {
                b = std::min<Index>(mr, 2 * b);
                continue;
            }
            std::vector<Index> nulls;
            for (Index j = 0; j < b; ++j) {
                if (sv(j) <= kRankNullTol * mnorm) {
                    nulls.push_back(j);
                }
            }
            Y.resize(mr, static_cast<Index>(nulls.size()));
            for (std::size_t q = 0; q < nulls.size(); ++q) {
                Y.col(static_cast<Index>(q)) = X * V.col(nulls[q]);
            }
            break;
        }
    }
    const Index k0 = Y.cols();
    out.deficiency = k0;
    if (k0 == 0) {
        return out;
    }

    // Z: the null space extended to every member, rows then pins (pin i carries -(M^T y)_i).
    const auto np = static_cast<Index>(pins.size());
    const Index nm = mr + np;
    Eigen::MatrixXd Z(nm, k0);
    Z.topRows(mr) = Y;
    const Eigen::MatrixXd MtY = SpMatRM(M.transpose()) * Y; // n x k0
    Vec rf(nm);
    Vec orient = Vec::Zero(nm); // 0: mandatory
    rf.head(mr) = rhs;
    for (Index k = me; k < mr; ++k) {
        const Index r = aw[static_cast<std::size_t>(k - me)];
        orient(k) = shift.size() > r && shift(r) > 0.0 ? 0.0 : 1.0;
    }
    for (Index q = 0; q < np; ++q) {
        const Index i = pins[static_cast<std::size_t>(q)];
        Z.row(mr + q) = -MtY.row(i);
        const BoundState st = bs[static_cast<std::size_t>(i)];
        rf(mr + q) = st == BoundState::kAtUpper ? qp.upper(i) : qp.lower(i);
        orient(mr + q) = st == BoundState::kAtUpper ? 1.0 : st == BoundState::kAtLower ? -1.0 : 0.0;
    }
    std::vector<char> used(static_cast<std::size_t>(nm), 0);
    std::vector<WsMember> chosen;
    std::vector<char> chosen_consistent;
    while (Z.cols() > 0) {
        const Vec c = Z.transpose() * rf;
        const double gamma = c.norm();
        // The consistency tolerance is scaled by the members that PARTICIPATE in the direction
        // (the unit rows' rhs and the pins' raw bounds alike): sum_i |y*_i| |r_i| >= gamma, so a
        // large pinned bound elsewhere cannot inflate it.
        Vec ystar;
        double rscale = 0.0;
        if (gamma > 0.0) {
            ystar = Z * c / gamma;
            rscale = ystar.cwiseAbs().dot(rf.cwiseAbs());
        }
        const double ctol = kRankConsistencyFactor * opts.feas_tol * std::max(1.0, rscale);
        const bool incons = gamma > ctol;
        Index best = -1;
        double bv = 0.0;
        for (Index j = 0; j < nm; ++j) {
            if (orient(j) == 0.0 || used[static_cast<std::size_t>(j)] != 0) {
                continue;
            }
            const double v = incons ? orient(j) * ystar(j) : Z.row(j).norm();
            if (v > bv) {
                bv = v;
                best = j;
            }
        }
        if (best < 0 || !(bv > kRankReleaseTol)) {
            if (incons) {
                out.outcome = RankRepair::Outcome::kStuck;
                out.release.clear();
                return out;
            }
            break; // the mandatory rows' own consistent dependence
        }
        if (incons) {
            ++out.inconsistent_steps;
        }
        used[static_cast<std::size_t>(best)] = 1;
        chosen_consistent.push_back(incons ? 0 : 1);
        chosen.push_back(
            best < mr ? WsMember{WsMember::Kind::kRow, aw[static_cast<std::size_t>(best - me)]}
                      : WsMember{WsMember::Kind::kPin, pins[static_cast<std::size_t>(best - mr)]});
        // Restrict Z to y_best = 0: the orthogonal complement of its row in coefficient space.
        if (Z.cols() == 1) {
            break;
        }
        const Vec zr = Z.row(best).transpose();
        Eigen::HouseholderQR<Eigen::MatrixXd> hq(zr);
        const Eigen::MatrixXd Q = hq.householderQ();
        Z = (Z * Q.rightCols(Z.cols() - 1)).eval();
    }
    if (!chosen.empty()) {
        out.outcome = RankRepair::Outcome::kRelease;
        out.release = std::move(chosen);
        out.release_consistent = std::move(chosen_consistent);
    }
    return out;
}

} // namespace hven::solvers::detail
