// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include <hven/detail/drivers/equality_basis.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include <Eigen/OrderingMethods>
#include <Eigen/SparseCholesky>
#include <Eigen/SparseQR>
#include <fmt/format.h>

namespace hven::solvers::detail {

namespace {

constexpr double kEps = std::numeric_limits<double>::epsilon();
constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

void fnv_mix(std::uint64_t &h, const void *data, std::size_t len) {
    const auto *bytes = static_cast<const unsigned char *>(data);
    for (std::size_t i = 0; i < len; ++i) {
        h ^= bytes[i];
        h *= kFnvPrime;
    }
}

using SpMatCM = Eigen::SparseMatrix<double>;

} // namespace

struct EqualityBasis::Impl {
    EqualityBasisVerdict verdict = EqualityBasisVerdict::kNotComputed;
    Index me = 0;
    Index n = 0;
    std::vector<Index> retained;
    std::vector<Index> dropped;
    std::vector<Index> fixed;           // S, increasing
    Vec fixed_lower;                    // l on S
    std::vector<char> is_fixed;         // n
    SpMatCM C;                          // |B| x |D|
    Eigen::SimplicialLLT<SpMatCM> gram; // I + C^T C
    double tau_lo = 0.0;
    double tau_hi = 0.0;
    double min_pivot = kInf;
    double max_residual = 0.0;
    double max_coef = 0.0;
    std::uint64_t hash = 0;

    void set_identity(Index rows) {
        me = rows;
        retained.resize(static_cast<std::size_t>(rows));
        for (Index r = 0; r < rows; ++r) {
            retained[static_cast<std::size_t>(r)] = r;
        }
        dropped.clear();
        C = SpMatCM(rows, 0);
    }

    double free_norm(const SpMatRM &Je, Index r) const {
        double s = 0.0;
        for (SpMatRM::InnerIterator it(Je, r); it; ++it) {
            if (is_fixed[static_cast<std::size_t>(it.col())] == 0) {
                s += it.value() * it.value();
            }
        }
        return std::sqrt(s);
    }
};

EqualityBasis::EqualityBasis() : impl_(std::make_unique<Impl>()) {}
EqualityBasis::~EqualityBasis() = default;
EqualityBasis::EqualityBasis(EqualityBasis &&) noexcept = default;
EqualityBasis &EqualityBasis::operator=(EqualityBasis &&) noexcept = default;

void EqualityBasis::reset(Index me) {
    impl_ = std::make_unique<Impl>();
    impl_->set_identity(me);
}

EqualityBasisVerdict EqualityBasis::verdict() const { return impl_->verdict; }
bool EqualityBasis::reduces() const { return !impl_->dropped.empty(); }
Index EqualityBasis::me() const { return impl_->me; }
const std::vector<Index> &EqualityBasis::retained() const { return impl_->retained; }
const std::vector<Index> &EqualityBasis::dropped() const { return impl_->dropped; }
const Eigen::SparseMatrix<double> &EqualityBasis::coefficients() const { return impl_->C; }
double EqualityBasis::tau_lo() const { return impl_->tau_lo; }
double EqualityBasis::tau_hi() const { return impl_->tau_hi; }
double EqualityBasis::min_retained_pivot() const { return impl_->min_pivot; }
double EqualityBasis::max_backward_residual() const { return impl_->max_residual; }
double EqualityBasis::max_coefficient() const { return impl_->max_coef; }
std::uint64_t EqualityBasis::jacobian_hash() const { return impl_->hash; }

std::uint64_t EqualityBasis::hash_jacobian(const SpMatRM &Je_in) {
    SpMatRM tmp;
    const SpMatRM *p = &Je_in;
    if (!Je_in.isCompressed()) {
        tmp = Je_in;
        tmp.makeCompressed();
        p = &tmp;
    }
    const SpMatRM &m = *p;
    std::uint64_t h = kFnvOffset;
    const Index rows = m.rows();
    const Index cols = m.cols();
    const Index nnz = m.nonZeros();
    fnv_mix(h, &rows, sizeof(rows));
    fnv_mix(h, &cols, sizeof(cols));
    fnv_mix(h, &nnz, sizeof(nnz));
    fnv_mix(h, m.outerIndexPtr(), sizeof(int) * static_cast<std::size_t>(rows + 1));
    fnv_mix(h, m.innerIndexPtr(), sizeof(int) * static_cast<std::size_t>(nnz));
    fnv_mix(h, m.valuePtr(), sizeof(double) * static_cast<std::size_t>(nnz));
    return h;
}

void EqualityBasis::compute(const SpMatRM &Je, const Vec &lower, const Vec &upper,
                            const EqualityBasisTolerances &tol) {
    const Index me = Je.rows();
    const Index n = Je.cols();
    if (lower.size() != n || upper.size() != n) {
        throw std::invalid_argument(
            fmt::format("EqualityBasis::compute: Je is {}x{} but lower/upper have sizes {}/{}", me,
                        n, lower.size(), upper.size()));
    }
    if (!(tol.pivot_factor > 0.0) || !(tol.gap_ratio >= 1.0) || !(tol.coef_cap > 0.0)) {
        throw std::invalid_argument(
            fmt::format("EqualityBasis::compute: tolerances must satisfy pivot_factor > 0, "
                        "gap_ratio >= 1, coef_cap > 0 (got {}, {}, {})",
                        tol.pivot_factor, tol.gap_ratio, tol.coef_cap));
    }

    auto next = std::make_unique<Impl>();
    Impl &b = *next;
    b.n = n;
    b.hash = hash_jacobian(Je);
    b.is_fixed.assign(static_cast<std::size_t>(n), 0);
    std::vector<Index> free_pos(static_cast<std::size_t>(n), -1);
    Index n_free = 0;
    for (Index i = 0; i < n; ++i) {
        if (lower(i) == upper(i) && std::isfinite(lower(i))) {
            b.is_fixed[static_cast<std::size_t>(i)] = 1;
            b.fixed.push_back(i);
        } else {
            free_pos[static_cast<std::size_t>(i)] = n_free++;
        }
    }
    b.fixed_lower.resize(static_cast<Index>(b.fixed.size()));
    for (std::size_t s = 0; s < b.fixed.size(); ++s) {
        b.fixed_lower(static_cast<Index>(s)) = lower(b.fixed[s]);
    }
    b.tau_lo = tol.pivot_factor * static_cast<double>(n_free + me) * kEps;
    b.tau_hi = tol.gap_ratio * b.tau_lo;
    b.set_identity(me);

    // Row norms on F; empty rows are dropped outright.
    Vec nu(me);
    std::vector<Index> nonempty;
    std::vector<Index> empty_rows;
    for (Index r = 0; r < me; ++r) {
        nu(r) = b.free_norm(Je, r);
        if (nu(r) > 0.0) {
            nonempty.push_back(r);
        } else {
            empty_rows.push_back(r);
        }
    }
    const Index m_ne = static_cast<Index>(nonempty.size());

    // M = Je(nonempty, F)^T diag(1/nu), n_free x m_ne.
    SpMatCM M(n_free, m_ne);
    if (m_ne > 0) {
        std::vector<Eigen::Triplet<double>> trip;
        trip.reserve(static_cast<std::size_t>(Je.nonZeros()));
        for (Index j = 0; j < m_ne; ++j) {
            const Index r = nonempty[static_cast<std::size_t>(j)];
            for (SpMatRM::InnerIterator it(Je, r); it; ++it) {
                const Index f = free_pos[static_cast<std::size_t>(it.col())];
                if (f >= 0 && it.value() != 0.0) {
                    trip.emplace_back(static_cast<int>(f), static_cast<int>(j), it.value() / nu(r));
                }
            }
        }
        M.setFromTriplets(trip.begin(), trip.end());
        M.makeCompressed();
    }

    std::vector<Index> dropped_ne; // indices into `nonempty`
    std::vector<Index> retained_ne;
    Eigen::MatrixXd Ct; // rank x |dropped_ne|, equilibrated, retained in pivot order
    bool undecidable = false;
    if (m_ne > 0 && n_free > 0) {
        Eigen::SparseQR<SpMatCM, Eigen::COLAMDOrdering<int>> qr;
        qr.setPivotThreshold(b.tau_lo);
        qr.compute(M);
        if (qr.info() != Eigen::Success) {
            undecidable = true;
        } else {
            const Index rank = qr.rank();
            const auto &perm = qr.colsPermutation().indices();
            const SpMatCM &R = qr.matrixR();
            for (Index k = 0; k < rank; ++k) {
                double diag = 0.0;
                for (SpMatCM::InnerIterator it(R, k); it; ++it) {
                    if (it.row() == k) {
                        diag = std::abs(it.value());
                    }
                }
                b.min_pivot = std::min(b.min_pivot, diag);
                retained_ne.push_back(perm(k));
            }
            for (Index k = rank; k < m_ne; ++k) {
                dropped_ne.push_back(perm(k));
            }
            if (b.min_pivot < b.tau_hi) {
                undecidable = true;
            }
            if (!undecidable && !dropped_ne.empty()) {
                const Index nd = static_cast<Index>(dropped_ne.size());
                Eigen::MatrixXd MD(n_free, nd);
                for (Index j = 0; j < nd; ++j) {
                    MD.col(j) = M.col(dropped_ne[static_cast<std::size_t>(j)]).toDense();
                }
                const Eigen::MatrixXd QtMD = qr.matrixQ().adjoint() * MD;
                Ct = qr.matrixR()
                         .topLeftCorner(rank, rank)
                         .template triangularView<Eigen::Upper>()
                         .solve(QtMD.topRows(rank));
                SpMatCM MB(n_free, rank);
                {
                    std::vector<Eigen::Triplet<double>> trip;
                    for (Index k = 0; k < rank; ++k) {
                        for (SpMatCM::InnerIterator it(M, retained_ne[static_cast<std::size_t>(k)]);
                             it; ++it) {
                            trip.emplace_back(static_cast<int>(it.row()), static_cast<int>(k),
                                              it.value());
                        }
                    }
                    MB.setFromTriplets(trip.begin(), trip.end());
                }
                for (Index j = 0; j < nd && !undecidable; ++j) {
                    const Vec res = MD.col(j) - MB * Ct.col(j);
                    const double rn = res.norm();
                    const double c1 = Ct.col(j).lpNorm<1>();
                    const double cinf = Ct.col(j).lpNorm<Eigen::Infinity>();
                    if (!std::isfinite(rn) || !std::isfinite(c1) || rn > b.tau_lo * (1.0 + c1) ||
                        cinf > tol.coef_cap) {
                        undecidable = true;
                    }
                    b.max_residual = std::max(b.max_residual, rn);
                    b.max_coef = std::max(b.max_coef, cinf);
                }
            }
        }
    } else if (m_ne > 0) {
        // No free column: every nonempty row is empty on F. Unreachable by
        // construction (nu > 0 needs a free entry); kept for completeness.
        undecidable = true;
    }

    if (undecidable) {
        b.verdict = EqualityBasisVerdict::kUndecidable;
        b.max_coef = 0.0;
        b.max_residual = 0.0;
        impl_ = std::move(next);
        return;
    }

    // Assemble B (increasing), D (increasing) and C in Je's units.
    std::vector<Index> dropped_rows;
    for (Index j : dropped_ne) {
        dropped_rows.push_back(nonempty[static_cast<std::size_t>(j)]);
    }
    for (Index r : empty_rows) {
        dropped_rows.push_back(r);
    }
    if (dropped_rows.empty()) {
        b.verdict = EqualityBasisVerdict::kFullRank;
        impl_ = std::move(next);
        return;
    }
    std::vector<Index> order(dropped_rows.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = static_cast<Index>(i);
    }
    std::sort(order.begin(), order.end(), [&](Index a, Index c) {
        return dropped_rows[static_cast<std::size_t>(a)] <
               dropped_rows[static_cast<std::size_t>(c)];
    });
    std::vector<char> is_dropped(static_cast<std::size_t>(me), 0);
    for (Index r : dropped_rows) {
        is_dropped[static_cast<std::size_t>(r)] = 1;
    }
    b.retained.clear();
    std::vector<Index> b_pos(static_cast<std::size_t>(me), -1);
    for (Index r = 0; r < me; ++r) {
        if (is_dropped[static_cast<std::size_t>(r)] == 0) {
            b_pos[static_cast<std::size_t>(r)] = static_cast<Index>(b.retained.size());
            b.retained.push_back(r);
        }
    }
    const Index nb = static_cast<Index>(b.retained.size());
    const Index nd = static_cast<Index>(dropped_rows.size());
    b.dropped.resize(static_cast<std::size_t>(nd));
    std::vector<Eigen::Triplet<double>> trip;
    for (Index jj = 0; jj < nd; ++jj) {
        const Index src = order[static_cast<std::size_t>(jj)];
        const Index r = dropped_rows[static_cast<std::size_t>(src)];
        b.dropped[static_cast<std::size_t>(jj)] = r;
        if (src >= static_cast<Index>(dropped_ne.size())) {
            continue; // an empty row: zero column
        }
        for (Index k = 0; k < Ct.rows(); ++k) {
            const double ct = Ct(k, src);
            if (ct == 0.0) {
                continue;
            }
            const Index rk =
                nonempty[static_cast<std::size_t>(retained_ne[static_cast<std::size_t>(k)])];
            trip.emplace_back(static_cast<int>(b_pos[static_cast<std::size_t>(rk)]),
                              static_cast<int>(jj), ct * nu(r) / nu(rk));
        }
    }
    b.C = SpMatCM(nb, nd);
    b.C.setFromTriplets(trip.begin(), trip.end());
    b.C.makeCompressed();
    SpMatCM G = SpMatCM(b.C.transpose()) * b.C;
    SpMatCM I(nd, nd);
    I.setIdentity();
    G += I;
    b.gram.compute(G);
    if (b.gram.info() != Eigen::Success) {
        throw std::logic_error("EqualityBasis::compute: I + C^T C failed to factor");
    }
    b.verdict = EqualityBasisVerdict::kReduced;
    impl_ = std::move(next);
}

bool EqualityBasis::verify(const SpMatRM &Je) const {
    const Impl &b = *impl_;
    if (b.dropped.empty()) {
        return true;
    }
    if (Je.rows() != b.me || Je.cols() != b.n) {
        return false;
    }
    Vec acc = Vec::Zero(b.n);
    const Index nd = static_cast<Index>(b.dropped.size());
    for (Index j = 0; j < nd; ++j) {
        const Index r = b.dropped[static_cast<std::size_t>(j)];
        acc.setZero();
        double bound = b.free_norm(Je, r);
        for (SpMatRM::InnerIterator it(Je, r); it; ++it) {
            acc(it.col()) += it.value();
        }
        for (SpMatCM::InnerIterator ct(b.C, j); ct; ++ct) {
            const Index rk = b.retained[static_cast<std::size_t>(ct.row())];
            bound += std::abs(ct.value()) * b.free_norm(Je, rk);
            for (SpMatRM::InnerIterator it(Je, rk); it; ++it) {
                acc(it.col()) -= ct.value() * it.value();
            }
        }
        for (Index s : b.fixed) {
            acc(s) = 0.0;
        }
        const double res = acc.norm();
        if (!std::isfinite(res) || res > b.tau_lo * bound) {
            return false;
        }
    }
    return true;
}

void EqualityBasis::read_gaps(const Vec &ce, const SpMatRM &Je, const Vec &x, double feas_tol,
                              Vec &gap, std::vector<EqualityGapBand> &band) const {
    const Impl &b = *impl_;
    const Index nd = static_cast<Index>(b.dropped.size());
    gap.resize(nd);
    band.assign(static_cast<std::size_t>(nd), EqualityGapBand::kNoise);
    if (nd == 0) {
        return;
    }
    if (ce.size() != b.me || Je.rows() != b.me || Je.cols() != b.n || x.size() != b.n) {
        throw std::invalid_argument(fmt::format(
            "EqualityBasis::read_gaps: ce {} / Je {}x{} / x {} disagree with the basis ({} x {})",
            ce.size(), Je.rows(), Je.cols(), x.size(), b.me, b.n));
    }
    Vec w = ce;
    if (!b.fixed.empty()) {
        Vec d = Vec::Zero(b.n);
        for (std::size_t s = 0; s < b.fixed.size(); ++s) {
            d(b.fixed[s]) = b.fixed_lower(static_cast<Index>(s)) - x(b.fixed[s]);
        }
        w += Je * d;
    }
    for (Index j = 0; j < nd; ++j) {
        const Index r = b.dropped[static_cast<std::size_t>(j)];
        double g = w(r);
        double scale = std::abs(w(r));
        double c1 = 0.0;
        for (SpMatCM::InnerIterator ct(b.C, j); ct; ++ct) {
            const double wk = w(b.retained[static_cast<std::size_t>(ct.row())]);
            g -= ct.value() * wk;
            scale += std::abs(ct.value()) * std::abs(wk);
            c1 += std::abs(ct.value());
        }
        gap(j) = g;
        const double ag = std::abs(g);
        if (ag <= 10.0 * kEps * scale) {
            band[static_cast<std::size_t>(j)] = EqualityGapBand::kNoise;
        } else if (ag <= feas_tol * (1.0 + c1)) {
            band[static_cast<std::size_t>(j)] = EqualityGapBand::kFeasible;
        } else {
            band[static_cast<std::size_t>(j)] = EqualityGapBand::kInconsistent;
        }
    }
}

std::uint64_t EqualityBasis::group_hash(const SpMatRM &Je, Index j) const {
    const Impl &b = *impl_;
    if (j < 0 || j >= static_cast<Index>(b.dropped.size()) || Je.rows() != b.me) {
        throw std::invalid_argument(
            fmt::format("EqualityBasis::group_hash: dropped index {} out of range [0, {}) or Je "
                        "has {} rows (expected {})",
                        j, b.dropped.size(), Je.rows(), b.me));
    }
    std::uint64_t h = kFnvOffset;
    auto mix_row = [&](Index r) {
        fnv_mix(h, &r, sizeof(r));
        for (SpMatRM::InnerIterator it(Je, r); it; ++it) {
            const Index c = it.col();
            const double v = it.value();
            fnv_mix(h, &c, sizeof(c));
            fnv_mix(h, &v, sizeof(v));
        }
    };
    mix_row(b.dropped[static_cast<std::size_t>(j)]);
    for (SpMatCM::InnerIterator ct(b.C, j); ct; ++ct) {
        mix_row(b.retained[static_cast<std::size_t>(ct.row())]);
    }
    return h;
}

Vec EqualityBasis::project_rhs(const Vec &gamma) const {
    const Impl &b = *impl_;
    if (gamma.size() != static_cast<Index>(b.dropped.size())) {
        throw std::invalid_argument(
            fmt::format("EqualityBasis::project_rhs: gamma has size {}, expected {} (= |D|)",
                        gamma.size(), b.dropped.size()));
    }
    if (b.dropped.empty()) {
        return Vec::Zero(static_cast<Index>(b.retained.size()));
    }
    return b.C * Vec(b.gram.solve(gamma));
}

Vec EqualityBasis::reduce_rhs(const Vec &be, const SpMatRM &Ae, const Vec &lower) const {
    const Impl &b = *impl_;
    if (be.size() != b.me || Ae.rows() != b.me || Ae.cols() != b.n || lower.size() != b.n) {
        throw std::invalid_argument(fmt::format(
            "EqualityBasis::reduce_rhs: be {} / Ae {}x{} / lower {} disagree with the basis "
            "({} x {})",
            be.size(), Ae.rows(), Ae.cols(), lower.size(), b.me, b.n));
    }
    Vec w = be;
    if (!b.fixed.empty()) {
        Vec d = Vec::Zero(b.n);
        for (Index s : b.fixed) {
            d(s) = lower(s);
        }
        w -= Ae * d;
    }
    const Index nd = static_cast<Index>(b.dropped.size());
    Vec gamma(nd);
    for (Index j = 0; j < nd; ++j) {
        double g = w(b.dropped[static_cast<std::size_t>(j)]);
        for (SpMatCM::InnerIterator ct(b.C, j); ct; ++ct) {
            g -= ct.value() * w(b.retained[static_cast<std::size_t>(ct.row())]);
        }
        gamma(j) = g;
    }
    const Vec t = project_rhs(gamma);
    const Index nb = static_cast<Index>(b.retained.size());
    Vec out(nb);
    for (Index k = 0; k < nb; ++k) {
        out(k) = be(b.retained[static_cast<std::size_t>(k)]) + t(k);
    }
    return out;
}

void EqualityBasis::reduce(QpProblem &qp) const {
    const Impl &b = *impl_;
    if (b.dropped.empty()) {
        return;
    }
    if (qp.me() != b.me || qp.n() != b.n) {
        throw std::invalid_argument(
            fmt::format("EqualityBasis::reduce: QP has me={} n={}, basis expects me={} n={}",
                        qp.me(), qp.n(), b.me, b.n));
    }
    if (!qp.Ae.isCompressed()) {
        qp.Ae.makeCompressed();
    }
    Vec be = reduce_rhs(qp.be, qp.Ae, qp.lower);
    const Index nb = static_cast<Index>(b.retained.size());
    SpMatRM Ae(nb, b.n);
    Index nnz = 0;
    for (Index r : b.retained) {
        nnz += qp.Ae.outerIndexPtr()[r + 1] - qp.Ae.outerIndexPtr()[r];
    }
    Ae.reserve(nnz);
    for (Index k = 0; k < nb; ++k) {
        Ae.startVec(k);
        for (SpMatRM::InnerIterator it(qp.Ae, b.retained[static_cast<std::size_t>(k)]); it; ++it) {
            Ae.insertBack(k, it.col()) = it.value();
        }
    }
    Ae.finalize();
    Ae.makeCompressed();
    qp.Ae = std::move(Ae);
    qp.be = std::move(be);
}

Vec EqualityBasis::lift(const Vec &lambda_B) const {
    const Impl &b = *impl_;
    const Index nb = static_cast<Index>(b.retained.size());
    if (lambda_B.size() != nb) {
        throw std::invalid_argument(fmt::format(
            "EqualityBasis::lift: lambda_B has size {}, expected {} (= |B|)", lambda_B.size(), nb));
    }
    if (b.dropped.empty()) {
        return lambda_B;
    }
    const Vec w = b.gram.solve(Vec(b.C.transpose() * lambda_B));
    const Vec lb = lambda_B - b.C * w;
    Vec out(b.me);
    for (Index k = 0; k < nb; ++k) {
        out(b.retained[static_cast<std::size_t>(k)]) = lb(k);
    }
    for (std::size_t j = 0; j < b.dropped.size(); ++j) {
        out(b.dropped[j]) = w(static_cast<Index>(j));
    }
    return out;
}

Vec EqualityBasis::fold(const Vec &lambda) const {
    const Impl &b = *impl_;
    if (lambda.size() != b.me) {
        throw std::invalid_argument(fmt::format(
            "EqualityBasis::fold: lambda has size {}, expected {} (= me)", lambda.size(), b.me));
    }
    if (b.dropped.empty()) {
        return lambda;
    }
    const Index nb = static_cast<Index>(b.retained.size());
    const Index nd = static_cast<Index>(b.dropped.size());
    Vec ld(nd);
    for (Index j = 0; j < nd; ++j) {
        ld(j) = lambda(b.dropped[static_cast<std::size_t>(j)]);
    }
    Vec out = b.C * ld;
    for (Index k = 0; k < nb; ++k) {
        out(k) += lambda(b.retained[static_cast<std::size_t>(k)]);
    }
    return out;
}

EqualityRowSubsetModel::EqualityRowSubsetModel(const NlpModel &model, std::vector<Index> rows)
    : model_(model), rows_(std::move(rows)) {
    for (std::size_t k = 0; k < rows_.size(); ++k) {
        if (rows_[k] < 0 || rows_[k] >= model.me() || (k > 0 && rows_[k] <= rows_[k - 1])) {
            throw std::invalid_argument(fmt::format(
                "EqualityRowSubsetModel: row {} at position {} is out of [0, {}) or not "
                "strictly increasing",
                rows_[k], k, model.me()));
        }
    }
}

Index EqualityRowSubsetModel::n() const { return model_.n(); }
Index EqualityRowSubsetModel::me() const { return static_cast<Index>(rows_.size()); }
Index EqualityRowSubsetModel::mi() const { return model_.mi(); }
double EqualityRowSubsetModel::eval_f(const Vec &x) const { return model_.eval_f(x); }

Vec EqualityRowSubsetModel::restrict(const Vec &full) const {
    Vec out(me());
    for (std::size_t k = 0; k < rows_.size(); ++k) {
        out(static_cast<Index>(k)) = full(rows_[k]);
    }
    return out;
}

SpMatRM EqualityRowSubsetModel::restrict(const SpMatRM &full) const {
    SpMatRM out(me(), full.cols());
    Index nnz = 0;
    for (Index r : rows_) {
        for (SpMatRM::InnerIterator it(full, r); it; ++it) {
            ++nnz;
        }
    }
    out.reserve(nnz);
    for (std::size_t k = 0; k < rows_.size(); ++k) {
        out.startVec(static_cast<Index>(k));
        for (SpMatRM::InnerIterator it(full, rows_[k]); it; ++it) {
            out.insertBack(static_cast<Index>(k), it.col()) = it.value();
        }
    }
    out.finalize();
    out.makeCompressed();
    return out;
}

Vec EqualityRowSubsetModel::eval_grad(const Vec &x) const {
    Vec g;
    model_.eval_grad_in_place(x, g);
    return g;
}
Vec EqualityRowSubsetModel::eval_ce(const Vec &x) const {
    Vec c;
    eval_ce_in_place(x, c);
    return c;
}
Vec EqualityRowSubsetModel::eval_ci(const Vec &x) const {
    Vec c;
    model_.eval_ci_in_place(x, c);
    return c;
}
SpMatRM EqualityRowSubsetModel::eval_hess(const Vec &x, double obj_scale, const Vec &lambda_e,
                                          const Vec &lambda_i) const {
    SpMatRM H;
    eval_hess_in_place(x, obj_scale, lambda_e, lambda_i, H);
    return H;
}
SpMatRM EqualityRowSubsetModel::eval_jac_e(const Vec &x) const {
    SpMatRM J;
    eval_jac_e_in_place(x, J);
    return J;
}
SpMatRM EqualityRowSubsetModel::eval_jac_i(const Vec &x) const {
    SpMatRM J;
    model_.eval_jac_i_in_place(x, J);
    return J;
}
void EqualityRowSubsetModel::eval_grad_in_place(const Vec &x, Vec &out) const {
    model_.eval_grad_in_place(x, out);
}
void EqualityRowSubsetModel::eval_ce_in_place(const Vec &x, Vec &out) const {
    Vec full;
    model_.eval_ce_in_place(x, full);
    out = restrict(full);
}
void EqualityRowSubsetModel::eval_ci_in_place(const Vec &x, Vec &out) const {
    model_.eval_ci_in_place(x, out);
}
void EqualityRowSubsetModel::eval_jac_e_in_place(const Vec &x, SpMatRM &out) const {
    SpMatRM full;
    model_.eval_jac_e_in_place(x, full);
    out = restrict(full);
}
void EqualityRowSubsetModel::eval_jac_i_in_place(const Vec &x, SpMatRM &out) const {
    model_.eval_jac_i_in_place(x, out);
}
void EqualityRowSubsetModel::eval_hess_in_place(const Vec &x, double obj_scale, const Vec &lambda_e,
                                                const Vec &lambda_i, SpMatRM &out) const {
    if (lambda_e.size() != me()) {
        throw std::invalid_argument(
            fmt::format("EqualityRowSubsetModel::eval_hess: lambda_e has size {}, expected {}",
                        lambda_e.size(), me()));
    }
    Vec full = Vec::Zero(model_.me());
    for (std::size_t k = 0; k < rows_.size(); ++k) {
        full(rows_[k]) = lambda_e(static_cast<Index>(k));
    }
    model_.eval_hess_in_place(x, obj_scale, full, lambda_i, out);
}
const Vec &EqualityRowSubsetModel::lower() const { return model_.lower(); }
const Vec &EqualityRowSubsetModel::upper() const { return model_.upper(); }
Vec EqualityRowSubsetModel::start_point() const { return model_.start_point(); }

} // namespace hven::solvers::detail
