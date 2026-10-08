// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// QnLiftBridge: the quasi-Newton engine-signature decorator. The contract is in
// qn_lift_bridge.h; the lift itself is mixed_lift.h's.

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <utility>

#include <fmt/format.h>

#include <hven/detail/globalization/sqp/qn_lift_bridge.h>

namespace hven::solvers::detail {

namespace {

bool same_bytes(const Vec &a, const Vec &b) {
    return a.size() == b.size() &&
           (a.size() == 0 || std::memcmp(a.data(), b.data(),
                                         static_cast<std::size_t>(a.size()) * sizeof(double)) == 0);
}

bool same_sparse(const SpMatRM &a, const SpMatRM &b) {
    if (!a.isCompressed() || !b.isCompressed() || a.rows() != b.rows() || a.cols() != b.cols() ||
        a.nonZeros() != b.nonZeros()) {
        return false;
    }
    const auto outer = static_cast<std::size_t>(a.outerSize()) + 1;
    const auto nnz = static_cast<std::size_t>(a.nonZeros());
    return std::equal(a.outerIndexPtr(), a.outerIndexPtr() + outer, b.outerIndexPtr()) &&
           std::equal(a.innerIndexPtr(), a.innerIndexPtr() + nnz, b.innerIndexPtr()) &&
           (nnz == 0 || std::memcmp(a.valuePtr(), b.valuePtr(), nnz * sizeof(double)) == 0);
}

// Everything a lift depends on besides g/be/bi.
bool same_structure(const QpProblem &a, const QpProblem &b) {
    return a.n() == b.n() && a.me() == b.me() && a.mi() == b.mi() && same_bytes(a.lower, b.lower) &&
           same_bytes(a.upper, b.upper) && same_sparse(a.Ae, b.Ae) && same_sparse(a.Ai, b.Ai);
}

bool same_rhs(const QpProblem &a, const QpProblem &b) {
    return same_bytes(a.g, b.g) && same_bytes(a.be, b.be) && same_bytes(a.bi, b.bi);
}

// [head of v (want entries); zeros] -- v at the unlifted size or longer (a lifted carry).
Vec extend_zero(const Vec &v, Index want, Index lifted) {
    Vec out = Vec::Zero(lifted);
    out.head(want) = v.head(want);
    return out;
}

// The lifted point of p WITHOUT the clamp lift_primal applies: aux = col^T (p - p_ref) / kappa.
Vec lift_point(const MixedLift &L, const MixedLiftData &d, Index n_struct, const Vec &p_head) {
    const Index n = L.n_orig();
    Vec x(L.n_lifted());
    x.head(n) = p_head;
    const Vec dp = (p_head - L.reference()).head(n_struct);
    if (L.num_plus() > 0) {
        x.segment(n, L.num_plus()) = (d.u_plus.transpose() * dp).cwiseQuotient(d.kappa_plus);
    }
    if (L.num_minus() > 0) {
        x.tail(L.num_minus()) = (d.v.transpose() * dp).cwiseQuotient(d.kappa_v);
    }
    return x;
}

template <typename T> void head_if(std::vector<T> &v, Index lifted, Index n) {
    if (static_cast<Index>(v.size()) == lifted) {
        v.resize(static_cast<std::size_t>(n));
    }
}

void head_if(Vec &v, Index lifted, Index n) {
    if (v.size() == lifted) {
        v = Vec(v.head(n));
    }
}

bool walk_read_wrong_inertia(const QpCounters &c) {
    return c.dep_inertia_rereads + c.convex_certificates + c.saddle_exits + c.saddle_exit_misses >
           0;
}

} // namespace

UpdateOutcome offer_pair_with_skip_reset(QnModel &qn, Index &skip_streak, const Vec &s,
                                         const Vec &y, double x_inf) {
    const Index ns = qn.dimension();
    if (s.size() < ns || y.size() < ns) {
        throw std::invalid_argument(
            fmt::format("offer_pair_with_skip_reset: s/y have {}/{} entries, the window acts on {}",
                        s.size(), y.size(), ns));
    }
    const UpdateOutcome o = qn.update(Vec(s.head(ns)), Vec(y.head(ns)), x_inf);
    if (o == UpdateOutcome::kUpdated) {
        skip_streak = 0;
    } else if (o != UpdateOutcome::kSkippedZeroStep && ++skip_streak >= kQnSkipResetStreak) {
        qn.reset(ResetReason::kConsecutiveSkips);
        skip_streak = 0;
    }
    return o;
}

QnLiftBridge::QnLiftBridge(const QnModel &qn, Index n_struct) : qn_(&qn), n_struct_(n_struct) {
    if (n_struct < 1 || n_struct > qn.dimension()) {
        throw std::invalid_argument(
            fmt::format("QnLiftBridge: n_struct = {} outside [1, {}]", n_struct, qn.dimension()));
    }
}

void QnLiftBridge::invalidate() {
    for (Slot &s : slots_) {
        s = Slot{};
    }
}

const QpProblem &QnLiftBridge::lift_of(const QpProblem &qp) {
    const MixedLift *L = nullptr;
    return lifted(qp, &L);
}

QnLiftBridge::Tally QnLiftBridge::take_tally() {
    Tally t = tally_;
    t.dimension = qn_->carried_dimension();
    tally_ = Tally{};
    return t;
}

double QnLiftBridge::predicted_decrease(const QpProblem &qp, const Vec &p) const {
    if (p.size() != qp.n() || qp.n() < n_struct_) {
        throw std::invalid_argument(
            fmt::format("QnLiftBridge::predicted_decrease: p has size {}, qp.n() = {}, "
                        "n_struct = {}",
                        p.size(), qp.n(), n_struct_));
    }
    const Vec ps = p.head(n_struct_);
    const double linear = qp.g.dot(p);
    const double quadratic = ps.dot(qn_->apply(ps));
    return -(linear + 0.5 * quadratic);
}

const QpProblem &QnLiftBridge::lifted(const QpProblem &qp, const MixedLift **lift) {
    ++clock_;
    for (Slot &s : slots_) {
        if (!s.lift || !same_structure(s.source, qp)) {
            continue;
        }
        s.used = clock_;
        *lift = &*s.lift;
        if (same_rhs(s.source, qp)) {
            return s.lift->problem();
        }
        if (!s.has_variant || !same_rhs(s.variant_source, qp)) {
            s.variant = s.lift->lift_variant(qp);
            s.variant_source.g = qp.g;
            s.variant_source.be = qp.be;
            s.variant_source.bi = qp.bi;
            s.has_variant = true;
        }
        return s.variant;
    }
    Slot &s = slots_[0].used <= slots_[1].used ? slots_[0] : slots_[1];
    s = Slot{};
    s.lift.emplace(qp, n_struct_, qn_->mixed_lift());
    tally_.nonpd_seen += qn_->positive_definite() ? 0 : 1;
    s.source = qp;
    s.source.H = SpMatRM();
    s.used = clock_;
    tally_.lift_rank_peak =
        std::max(tally_.lift_rank_peak, s.lift->num_plus() + s.lift->num_minus());
    *lift = &*s.lift;
    return s.lift->problem();
}

namespace {

// A walk seed, lifted: wrongly sized fields are dropped (the walk ignores them), then
// MixedLift::lift_seed (which clamps x, as the walk's own start does).
QpSolution lift_walk_seed(const MixedLift &L, const QpSolution &seed) {
    const Index n = L.n_orig();
    QpSolution s = seed;
    if (s.x.size() != n) {
        s.x.resize(0);
    }
    if (s.z.size() != n) {
        s.z.resize(0);
    }
    if (s.lambda_e.size() != L.orig_eq_rows()) {
        s.lambda_e.resize(0);
    }
    if (static_cast<Index>(s.bound_state.size()) != n) {
        s.bound_state.clear();
    }
    if (static_cast<Index>(s.tr_active.size()) != n) {
        s.tr_active.clear();
    }
    return L.lift_seed(s);
}

// A lifted walk answer, un-lifted field by field (a field the engine left empty stays empty).
QpSolution unlift_walk(const MixedLift &L, QpSolution l) {
    const Index n = L.n_orig();
    const Index nl = L.n_lifted();
    const Index me = L.orig_eq_rows();
    const Index mel = me + L.num_plus();
    head_if(l.x, nl, n);
    head_if(l.z, nl, n);
    head_if(l.lambda_e, mel, me);
    head_if(l.bound_state, nl, n);
    head_if(l.tr_active, nl, n);
    if (l.farkas.present) {
        if (!l.farkas.blocker_is_ineq && l.farkas.blocker >= n) {
            l.farkas = QpFarkasCertificate{}; // an auxiliary blocker: not expressible
        } else {
            head_if(l.farkas.y_e, mel, me);
            head_if(l.farkas.box_part, nl, n);
        }
    }
    return l;
}

} // namespace

QpSolution QnLiftBridge::solve(const QpEngine &engine, const QpProblem &qp,
                               const SolveOverrides &o) {
    const MixedLift *L = nullptr;
    const QpProblem &lp = lifted(qp, &L);
    QpSolution out = unlift_walk(*L, engine.solve(lp, o));
    tally_.kwrong_seen += walk_read_wrong_inertia(out.counters) ? 1 : 0;
    return out;
}

QpSolution QnLiftBridge::solve(const QpEngine &engine, const QpProblem &qp, const QpSolution &seed,
                               const SolveOverrides &o) {
    const MixedLift *L = nullptr;
    const QpProblem &lp = lifted(qp, &L);
    QpSolution out = unlift_walk(*L, engine.solve(lp, lift_walk_seed(*L, seed), o));
    tally_.kwrong_seen += walk_read_wrong_inertia(out.counters) ? 1 : 0;
    return out;
}

QpSolution QnLiftBridge::solve(const QpEngine &engine, const QpProblem &qp, const QpSolution &seed,
                               const SolveOverrides &o,
                               const std::shared_ptr<const HotState> &hot) {
    const MixedLift *L = nullptr;
    const QpProblem &lp = lifted(qp, &L);
    QpSolution out = unlift_walk(*L, engine.solve(lp, lift_walk_seed(*L, seed), o, hot));
    tally_.kwrong_seen += walk_read_wrong_inertia(out.counters) ? 1 : 0;
    return out;
}

bool QnLiftBridge::refine_on_face(const QpEngine &engine, const QpProblem &qp,
                                  const QpSolution &face, const SolveOverrides &o,
                                  QpSolution &out) {
    const MixedLift *L = nullptr;
    const QpProblem &lp = lifted(qp, &L);
    QpSolution lface = lift_walk_seed(*L, face);
    if (face.x.size() == qp.n()) { // the face's own point, unclamped
        lface.x = lift_point(*L, qn_->mixed_lift(), n_struct_, face.x);
    }
    QpSolution lout;
    const bool took = engine.refine_on_face(lp, lface, o, lout);
    out = unlift_walk(*L, std::move(lout));
    tally_.kwrong_seen += walk_read_wrong_inertia(out.counters) ? 1 : 0;
    return took;
}

void QnLiftBridge::solve(SsnEngine &engine, const QpProblem &qp, const SsnStart &start,
                         const SsnOptions &sopts, const SolveOverrides &o, SsnResult *out) {
    const MixedLift *L = nullptr;
    const QpProblem &lp = lifted(qp, &L);
    const MixedLiftData &d = qn_->mixed_lift();
    const Index n = qp.n();
    const Index me = qp.me();
    const Index mi = qp.mi();
    const Index nl = L->n_lifted();
    const Index mel = me + L->num_plus();
    // A field of any other size is passed on unchanged: the engine refuses it, as it would
    // have refused the unlifted one.
    SsnStart ls = start;
    if (ls.x.size() == n) {
        ls.x = lift_point(*L, d, n_struct_, start.x);
    }
    if (ls.lambda_e.size() == me) {
        ls.lambda_e = extend_zero(start.lambda_e, me, mel);
    }
    if (ls.z.size() == n) {
        ls.z = extend_zero(start.z, n, nl);
    }
    if (ls.prox_center_x.size() == n) {
        ls.prox_center_x = lift_point(*L, d, n_struct_, start.prox_center_x);
    }
    if (ls.prox_center_lambda.size() == me + mi) {
        ls.prox_center_lambda = Vec::Zero(mel + mi);
        ls.prox_center_lambda.head(me) = start.prox_center_lambda.head(me);
        ls.prox_center_lambda.tail(mi) = start.prox_center_lambda.tail(mi);
    }
    if (ls.box_center.has_value() && ls.box_center->size() == n) {
        ls.box_center = lift_point(*L, d, n_struct_, *start.box_center);
    }
    if (static_cast<Index>(ls.activity_hint.bounds.size()) == n) {
        ls.activity_hint.bounds.resize(static_cast<std::size_t>(nl), BoundState::kFree);
    }
    engine.solve(lp, ls, sopts, o, out);
    head_if(out->x, nl, n);
    head_if(out->lambda_e, mel, me);
    head_if(out->z, nl, n);
    head_if(out->bound_state, nl, n);
    head_if(out->tr_active, nl, n);
    head_if(out->bound_uncertain, nl, n);
    tally_.kwrong_seen += out->counters.ssn_escape_indefinite > 0 ? 1 : 0;
}

bool QnLiftBridge::ipqp_seed_usable(const IpqpSeed &seed, const QpProblem &qp) const {
    const bool unlifted = seed.x.size() == qp.n() && seed.lambda_e.size() == qp.me();
    const bool carry = qp.n() == ipqp_n_ && qp.me() == ipqp_me_ &&
                       seed.x.size() == ipqp_n_lifted_ && seed.lambda_e.size() == ipqp_me_lifted_;
    return (unlifted || carry) && seed.lambda_i.size() == qp.mi();
}

IpqpResult QnLiftBridge::solve(IpqpEngine &engine, const QpProblem &qp, const IpqpSeed *seed,
                               const IpqpOptions &iopts, const SolveOverrides &o) {
    const MixedLift *L = nullptr;
    const QpProblem &lp = lifted(qp, &L);
    const MixedLiftData &d = qn_->mixed_lift();
    const Index n = qp.n();
    const Index me = qp.me();
    const Index nl = L->n_lifted();
    const Index mel = me + L->num_plus();
    IpqpSeed ls;
    const IpqpSeed *sp = nullptr;
    if (seed != nullptr && ipqp_seed_usable(*seed, qp)) {
        ls = *seed;
        // Every n-/me-block at the unlifted size or longer is truncated and re-lifted through
        // THIS B; a shorter one is left for the engine to refuse, as it would have.
        const auto lift_x = [&](Vec &v) {
            if (v.size() >= n) {
                v = lift_point(*L, d, n_struct_, Vec(v.head(n)));
            }
        };
        const auto lift_e = [&](Vec &v) {
            if (v.size() >= me) {
                v = extend_zero(v, me, mel);
            }
        };
        const auto lift_n = [&](Vec &v) {
            if (v.size() >= n) {
                v = extend_zero(v, n, nl);
            }
        };
        lift_x(ls.x);
        lift_x(ls.zeta);
        lift_e(ls.lambda_e);
        lift_e(ls.lambda_est_e);
        lift_n(ls.zl);
        lift_n(ls.zu);
        sp = &ls;
    }
    IpqpResult res = engine.solve(lp, sp, iopts, o);
    // The tier's own carry now has this lift's sizes (ipqp_seed_usable).
    ipqp_n_ = n;
    ipqp_me_ = me;
    ipqp_n_lifted_ = nl;
    ipqp_me_lifted_ = mel;
    head_if(res.x, nl, n);
    head_if(res.lambda_e, mel, me);
    head_if(res.z, nl, n);
    head_if(res.zl, nl, n);
    head_if(res.zu, nl, n);
    head_if(res.lower_face, nl, n);
    head_if(res.upper_face, nl, n);
    head_if(res.bound_state, nl, n);
    head_if(res.tr_active, nl, n);
    head_if(res.box.centre, nl, n);
    head_if(res.box.lo_eff, nl, n);
    head_if(res.box.up_eff, nl, n);
    if (res.box.zero_width_index >= n) {
        res.box.zero_width_index = -1; // an auxiliary's window: never zero-width (BLOCKING-1)
    }
    IpqpInfeasibilityEvidence &ev = res.infeasibility_evidence;
    head_if(ev.least_infeasible_x, nl, n);
    head_if(ev.least_infeasible_zl, nl, n);
    head_if(ev.least_infeasible_zu, nl, n);
    tally_.kwrong_seen +=
        (res.counters.ipqp_inertia_retries > 0 || res.escape_reason == IpqpEscape::kIndefinite) ? 1
                                                                                                : 0;
    return res;
}

} // namespace hven::solvers::detail
