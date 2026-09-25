// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include <hven/detail/qp/ws_independence.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

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

    DepSystem(const QpProblem &qp, const WorkingSet &ws, const DepFactorView &fv,
              const QpOptions &opts)
        : f(fv) {
        const Index n = qp.n();
        const Index me = qp.me();
        const std::vector<Index> &aw = ws.active_ineq();
        var_pos.assign(static_cast<std::size_t>(n), -1);
        if (f.schur != nullptr) {
            bordered = true;
            bsys = build_bordered_system(qp, *f.k0, *f.k0_rows, *f.ledger, ws, opts);
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
        Vec lam_i = Vec::Zero(qp.mi());
        Vec out = Vec::Zero(qp.n());
        for (std::size_t k = 0; k < comp.size(); ++k) {
            const double yk = y(comp_pos[k]);
            switch (comp[k].kind) {
            case WsMember::Kind::kEq:
                lam_e(comp[k].idx) += yk;
                break;
            case WsMember::Kind::kRow:
                lam_i(comp[k].idx) += yk;
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
            out += qp.Ai.transpose() * lam_i;
        }
        return out;
    }

    // rhs - K_{delta,0} y: the factored system with mu (only) backed out.
    Vec residual(const QpProblem &qp, const Vec &rhs, const Vec &y, const QpOptions &opts) const {
        if (bordered) {
            QpOptions keep_delta = opts;
            keep_delta.primal_delta = 0.0;
            return bordered_residual(f.k0->K, qp.n(), bsys.border_v, bsys.border_d, rhs, y,
                                     keep_delta);
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
        const Vec ai_u = qp.mi() > 0 ? Vec(qp.Ai * u) : Vec::Zero(0);
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
                r(comp_pos[k]) -= ai_u(m.idx);
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
        t.verdict = t.screen ? DepVerdict::kDependent : DepVerdict::kUndecidable;
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
        Vec lam_i = Vec::Zero(qp.mi());
        for (std::size_t k = 0; k < aw.size(); ++k) {
            lam_i(aw[k]) = t.v_w(static_cast<Index>(k));
        }
        w -= qp.Ai.transpose() * lam_i;
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

} // namespace hven::solvers::detail
