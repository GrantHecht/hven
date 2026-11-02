// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include "hven/detail/model/nlp_problem_model.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

#include <fmt/format.h>

namespace hven::solvers {

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

/// The index into a compressed row-major matrix's value array that holds
/// (row, col). The pattern was laid to carry the coordinate, so a miss is a
/// defect in this file rather than a caller error.
int compressed_value_index(const SpMatRM &m, int row, int col) {
    const auto *outer = m.outerIndexPtr();
    const auto *inner = m.innerIndexPtr();
    const auto *begin = inner + outer[row];
    const auto *end = inner + outer[row + 1];
    const auto *hit = std::lower_bound(begin, end, col);
    if (hit == end || *hit != col) {
        throw std::logic_error(
            fmt::format("NlpProblemModel: coordinate ({}, {}) is absent from the pattern laid for "
                        "it",
                        row, col));
    }
    return static_cast<int>(hit - inner);
}

/// Whether @p out already carries exactly @p pattern's stored coordinates.
bool same_pattern(const SpMatRM &out, const SpMatRM &pattern) {
    if (out.rows() != pattern.rows() || out.cols() != pattern.cols() || !out.isCompressed() ||
        out.nonZeros() != pattern.nonZeros()) {
        return false;
    }
    const std::size_t outer_bytes =
        static_cast<std::size_t>(out.outerSize() + 1) * sizeof(SpMatRM::StorageIndex);
    const std::size_t inner_bytes =
        static_cast<std::size_t>(out.nonZeros()) * sizeof(SpMatRM::StorageIndex);
    // A zero-nonzero pattern can leave both inner index pointers null; memcmp
    // on a null pointer is undefined even with a zero length, so that case is
    // taken as equal without calling it.
    return std::memcmp(out.outerIndexPtr(), pattern.outerIndexPtr(), outer_bytes) == 0 &&
           (inner_bytes == 0 ||
            std::memcmp(out.innerIndexPtr(), pattern.innerIndexPtr(), inner_bytes) == 0);
}

/// Gives @p out the laid pattern if it does not already carry it. A caller
/// reusing one destination pays the copy once and a comparison every call
/// after, which is what lets the value fill below run without allocating.
void adopt_pattern(SpMatRM &out, const SpMatRM &pattern) {
    if (!same_pattern(out, pattern)) {
        out = pattern;
    }
}

/// Zeroes a compressed matrix's value array.
void zero_values(SpMatRM &m) {
    if (m.nonZeros() > 0) {
        Eigen::Map<Vec>(m.valuePtr(), m.nonZeros()).setZero();
    }
}

/// Lays a compressed pattern over the given coordinates, summing duplicates
/// into one entry.
SpMatRM lay_pattern(int rows, int cols, const std::vector<Eigen::Triplet<double>> &entries) {
    SpMatRM m(rows, cols);
    m.setFromTriplets(entries.begin(), entries.end());
    m.makeCompressed();
    return m;
}

/// One declared row's kind, from its bound pair; refuses a pair that describes
/// no row.
NLPRowKind classify_row(double lo, double up, int r) {
    if (std::isnan(lo) || std::isnan(up)) {
        throw std::invalid_argument(
            fmt::format("constraint row {}: bound is NaN (lower={}, upper={})", r, lo, up));
    }
    if (lo > up) {
        throw std::invalid_argument(
            fmt::format("constraint row {}: lower bound {} exceeds upper bound {}", r, lo, up));
    }
    if (lo == up) {
        if (!std::isfinite(lo)) {
            throw std::invalid_argument(fmt::format(
                "constraint row {}: both bounds are {} — an equality at infinity", r, lo));
        }
        return NLPRowKind::Equality;
    }
    if (lo == -kInf && up == kInf) {
        return NLPRowKind::Free;
    }
    if (lo == -kInf) {
        return NLPRowKind::UpperBounded;
    }
    if (up == kInf) {
        return NLPRowKind::LowerBounded;
    }
    return NLPRowKind::Range;
}

const char *row_kind_name(NLPRowKind kind) {
    switch (kind) {
    case NLPRowKind::Equality:
        return "equality";
    case NLPRowKind::UpperBounded:
        return "upper-bounded";
    case NLPRowKind::LowerBounded:
        return "lower-bounded";
    case NLPRowKind::Range:
        return "range";
    case NLPRowKind::Free:
        return "free";
    }
    return "unknown";
}

/// The bits of a variable's bound pair the layouts depend on: which sides are
/// finite, and whether the two are equal (fixed).
int box_structure(double lo, double up) {
    return (std::isfinite(lo) ? 1 : 0) | (std::isfinite(up) ? 2 : 0) | (lo == up ? 4 : 0);
}

} // namespace

NLPRowClassification NLPRowClassification::classify(ConstEigenRef<Eigen::VectorXd> gl,
                                                    ConstEigenRef<Eigen::VectorXd> gu) {
    if (gl.size() != gu.size()) {
        throw std::invalid_argument(
            fmt::format("NLP row classification: g_lower has {} rows but g_upper has {}", gl.size(),
                        gu.size()));
    }
    const int m = static_cast<int>(gl.size());
    NLPRowClassification rc;
    rc.kinds_.resize(m);
    rc.eq_row_ = Eigen::VectorXi::Constant(m, -1);
    rc.iq_upper_row_ = Eigen::VectorXi::Constant(m, -1);
    rc.iq_lower_row_ = Eigen::VectorXi::Constant(m, -1);
    for (int r = 0; r < m; r++) {
        rc.kinds_[r] = classify_row(gl[r], gu[r], r);
        switch (rc.kinds_[r]) {
        case NLPRowKind::Equality:
            rc.eq_row_[r] = rc.num_eq_++;
            break;
        case NLPRowKind::UpperBounded:
            rc.iq_upper_row_[r] = rc.num_iq_++;
            break;
        case NLPRowKind::LowerBounded:
            rc.iq_lower_row_[r] = rc.num_iq_++;
            break;
        case NLPRowKind::Range:
            rc.iq_upper_row_[r] = rc.num_iq_++;
            rc.iq_lower_row_[r] = rc.num_iq_++;
            break;
        case NLPRowKind::Free:
            break;
        }
    }
    return rc;
}

NlpProblemModel::NlpProblemModel(std::shared_ptr<Nlp> problem) : problem_(std::move(problem)) {
    if (!problem_) {
        throw std::invalid_argument("NlpProblemModel: the problem pointer is null");
    }
    const NlpDimensions dims = problem_->dimensions();
    this->validate_sizes(dims);
    n_ = static_cast<int>(dims.n);
    m_ = static_cast<int>(dims.m);
    jac_nnz_ = static_cast<int>(dims.jacobian_nonzeros);
    hess_nnz_ = static_cast<int>(dims.hessian_nonzeros);

    x_lower_.resize(n_);
    x_upper_.resize(n_);
    g_lower_.resize(m_);
    g_upper_.resize(m_);
    problem_->bounds(x_lower_, x_upper_, g_lower_, g_upper_);
    this->validate_variable_bounds(x_lower_, x_upper_);

    rows_ = NLPRowClassification::classify(g_lower_, g_upper_);
    this->build_row_tables();
    this->read_structures();
    this->build_jacobian_pattern();
    this->build_hessian_pattern();

    g_cache_.resize(m_);
    jac_cache_.resize(jac_nnz_);
    lambda_user_.resize(m_);
    hess_vals_.resize(hess_nnz_);
    grad_.resize(n_);
    ce_.resize(rows_.num_eq_);
    ci_.resize(rows_.num_iq_);
}

void NlpProblemModel::validate_sizes(const NlpDimensions &dims) const {
    if (dims.n <= 0) {
        throw std::invalid_argument(
            fmt::format("{}: dimensions().n must be positive, got {}", problem_->name(), dims.n));
    }
    if (dims.m < 0 || dims.jacobian_nonzeros < 0 || dims.hessian_nonzeros < 0) {
        throw std::invalid_argument(fmt::format(
            "{}: dimensions() m={}, jacobian_nonzeros={}, hessian_nonzeros={} — all must be "
            "non-negative",
            problem_->name(), dims.m, dims.jacobian_nonzeros, dims.hessian_nonzeros));
    }
    // The sparse backends index with 32-bit integers (core/types.h, SpMatRM), so
    // every size the transcription lays must fit one.
    constexpr Index kMax = std::numeric_limits<int>::max();
    if (dims.n > kMax || dims.m > kMax || dims.jacobian_nonzeros > kMax ||
        dims.hessian_nonzeros > kMax) {
        throw std::invalid_argument(fmt::format(
            "{}: dimensions() n={}, m={}, jacobian_nonzeros={}, hessian_nonzeros={} — each must "
            "fit a 32-bit index ({} at most)",
            problem_->name(), dims.n, dims.m, dims.jacobian_nonzeros, dims.hessian_nonzeros, kMax));
    }
    if (dims.m == 0 && dims.jacobian_nonzeros != 0) {
        throw std::invalid_argument(
            fmt::format("{}: {} Jacobian nonzeros declared for a problem with no constraints",
                        problem_->name(), dims.jacobian_nonzeros));
    }
}

void NlpProblemModel::validate_variable_bounds(const Vec &x_lower, const Vec &x_upper) const {
    // The declared variable bounds, checked here because this is where they
    // enter the library: every consumer of this model reads them through
    // lower()/upper(), and start_point() projects the origin onto them, so a
    // pair that describes no interval has to be refused before either.
    for (int i = 0; i < n_; i++) {
        const double lo = x_lower[i], up = x_upper[i];
        if (std::isnan(lo) || std::isnan(up)) {
            throw std::invalid_argument(fmt::format(
                "{}: variable {} bound is NaN (lower={}, upper={})", problem_->name(), i, lo, up));
        }
        if (lo > up) {
            throw std::invalid_argument(
                fmt::format("{}: variable {} lower bound {} exceeds upper bound {}",
                            problem_->name(), i, lo, up));
        }
        // A variable fixed at an infinity describes no value at all, and every
        // check that could catch it lets it through: an infinity is not NaN,
        // and inf > inf is false. Consumers then install a bound only where
        // isfinite holds, so such a variable arrives free -- declared fixed,
        // installed unbounded, which is a wrong answer rather than a missing
        // diagnostic. Fixing a variable at a finite value is untouched and
        // remains the ordinary way to fix one.
        if (lo == up && !std::isfinite(lo)) {
            throw std::invalid_argument(
                fmt::format("{}: variable {}: both bounds are {} — a variable fixed at infinity",
                            problem_->name(), i, lo));
        }
    }
}

void NlpProblemModel::build_row_tables() {
    // In native row order. classify() numbered the native rows in declaration
    // order (a Range row's upper part before its lower part), and this loop
    // follows the same order, so entry k of each table IS native row k of its
    // block. Re-run on a bound-value update: clear() keeps the capacity, and the
    // kinds are unchanged, so the tables refill to the same length.
    const Vec &gl = g_lower_;
    const Vec &gu = g_upper_;
    eq_res_.clear();
    iq_res_.clear();
    for (int r = 0; r < m_; r++) {
        switch (rows_.kinds_[r]) {
        case NLPRowKind::Equality:
            eq_res_.push_back({r, 1.0, gl[r]});
            break;
        case NLPRowKind::UpperBounded:
            iq_res_.push_back({r, 1.0, gu[r]});
            break;
        case NLPRowKind::LowerBounded:
            iq_res_.push_back({r, -1.0, gl[r]});
            break;
        case NLPRowKind::Range:
            iq_res_.push_back({r, 1.0, gu[r]});
            iq_res_.push_back({r, -1.0, gl[r]});
            break;
        case NLPRowKind::Free:
            break;
        }
    }
}

bool NlpProblemModel::update_bounds(const Vec &x_lower, const Vec &x_upper, const Vec &g_lower,
                                    const Vec &g_upper) {
    if (x_lower.size() != n_ || x_upper.size() != n_ || g_lower.size() != m_ ||
        g_upper.size() != m_) {
        throw std::invalid_argument(fmt::format(
            "{}: a bound update is sized (x_lower {}, x_upper {}, g_lower {}, g_upper {}) for a "
            "problem with {} variables and {} rows",
            problem_->name(), x_lower.size(), x_upper.size(), g_lower.size(), g_upper.size(), n_,
            m_));
    }
    this->validate_variable_bounds(x_lower, x_upper);
    for (int r = 0; r < m_; r++) {
        const NLPRowKind was = rows_.kinds_[r];
        const NLPRowKind now = classify_row(g_lower[r], g_upper[r], r);
        if (was != now) {
            throw std::invalid_argument(fmt::format(
                "Problem '{}': row {} changed kind between solves, from {} to {} (bounds "
                "[{}, {}] -> [{}, {}]). A row's kind fixes the engines' row layout and the "
                "multiplier map; build a new Problem",
                problem_->name(), r, row_kind_name(was), row_kind_name(now), g_lower_[r],
                g_upper_[r], g_lower[r], g_upper[r]));
        }
    }
    bool box_changed = false;
    for (int i = 0; i < n_ && !box_changed; i++) {
        box_changed =
            box_structure(x_lower_[i], x_upper_[i]) != box_structure(x_lower[i], x_upper[i]);
    }
    // Commit. Same-length assignments: nothing here allocates.
    x_lower_ = x_lower;
    x_upper_ = x_upper;
    g_lower_ = g_lower;
    g_upper_ = g_upper;
    this->build_row_tables();
    this->invalidate_cache();
    return box_changed;
}

void NlpProblemModel::read_structures() {
    // Queried at the callback's own width, range-checked there, and only then
    // narrowed to the backends' 32-bit index.
    IndexVec rows(jac_nnz_), cols(jac_nnz_);
    if (jac_nnz_ > 0) {
        problem_->jacobian_structure(rows, cols);
    }
    for (int s = 0; s < jac_nnz_; s++) {
        if (rows[s] < 0 || rows[s] >= m_ || cols[s] < 0 || cols[s] >= n_) {
            throw std::invalid_argument(
                fmt::format("{}: Jacobian slot {} is ({}, {}), outside the {}x{} constraint "
                            "Jacobian",
                            problem_->name(), s, rows[s], cols[s], m_, n_));
        }
    }
    jac_rows_ = rows.cast<int>();
    jac_cols_ = cols.cast<int>();
    rows.resize(hess_nnz_);
    cols.resize(hess_nnz_);
    if (hess_nnz_ > 0) {
        problem_->hessian_structure(rows, cols);
    }
    for (int s = 0; s < hess_nnz_; s++) {
        if (rows[s] < 0 || rows[s] >= n_ || cols[s] < 0 || cols[s] >= n_) {
            throw std::invalid_argument(
                fmt::format("{}: Hessian slot {} is ({}, {}), outside the {}x{} Hessian",
                            problem_->name(), s, rows[s], cols[s], n_, n_));
        }
        if (rows[s] < cols[s]) {
            throw std::invalid_argument(fmt::format(
                "{}: Hessian slot {} is ({}, {}) — above the diagonal. Declare the LOWER "
                "triangle of the Lagrangian Hessian (row >= col)",
                problem_->name(), s, rows[s], cols[s]));
        }
    }
    hess_rows_ = rows.cast<int>();
    hess_cols_ = cols.cast<int>();
}

void NlpProblemModel::build_jacobian_pattern() {
    // A declared slot contributes to the native block its row landed in, with
    // that row's residual sign; a Range row's single declared slot contributes
    // to both of its native rows, with opposite signs.
    std::vector<Eigen::Triplet<double>> eq_entries, iq_entries;
    std::vector<std::pair<int, int>> eq_coords, iq_coords; // (native row, column), per target

    auto record = [&](std::vector<Eigen::Triplet<double>> &entries,
                      std::vector<std::pair<int, int>> &coords, std::vector<JacTarget> &targets,
                      int slot, int native_row, double sign) {
        const int col = jac_cols_[slot];
        entries.emplace_back(native_row, col, 1.0);
        coords.emplace_back(native_row, col);
        targets.push_back({slot, -1, sign});
    };

    for (int s = 0; s < jac_nnz_; s++) {
        const int r = jac_rows_[s];
        switch (rows_.kinds_[r]) {
        case NLPRowKind::Equality:
            record(eq_entries, eq_coords, eq_jac_, s, rows_.eq_row_[r], 1.0);
            break;
        case NLPRowKind::UpperBounded:
            record(iq_entries, iq_coords, iq_jac_, s, rows_.iq_upper_row_[r], 1.0);
            break;
        case NLPRowKind::LowerBounded:
            record(iq_entries, iq_coords, iq_jac_, s, rows_.iq_lower_row_[r], -1.0);
            break;
        case NLPRowKind::Range:
            record(iq_entries, iq_coords, iq_jac_, s, rows_.iq_upper_row_[r], 1.0);
            record(iq_entries, iq_coords, iq_jac_, s, rows_.iq_lower_row_[r], -1.0);
            break;
        case NLPRowKind::Free:
            break;
        }
    }

    jac_e_ = lay_pattern(rows_.num_eq_, n_, eq_entries);
    jac_i_ = lay_pattern(rows_.num_iq_, n_, iq_entries);
    for (std::size_t k = 0; k < eq_jac_.size(); k++) {
        eq_jac_[k].value_index_ =
            compressed_value_index(jac_e_, eq_coords[k].first, eq_coords[k].second);
    }
    for (std::size_t k = 0; k < iq_jac_.size(); k++) {
        iq_jac_[k].value_index_ =
            compressed_value_index(jac_i_, iq_coords[k].first, iq_coords[k].second);
    }
}

void NlpProblemModel::build_hessian_pattern() {
    // The declaration is the lower triangle (row >= col); the native contract
    // is the upper triangle, so each coordinate is mirrored on the way in.
    std::vector<Eigen::Triplet<double>> entries;
    entries.reserve(static_cast<std::size_t>(hess_nnz_));
    for (int k = 0; k < hess_nnz_; k++) {
        entries.emplace_back(hess_cols_[k], hess_rows_[k], 1.0);
    }
    hess_ = lay_pattern(n_, n_, entries);
    hess_value_index_.resize(static_cast<std::size_t>(hess_nnz_));
    for (int k = 0; k < hess_nnz_; k++) {
        hess_value_index_[static_cast<std::size_t>(k)] =
            compressed_value_index(hess_, hess_cols_[k], hess_rows_[k]);
    }
}

void NlpProblemModel::sync_x(const Vec &x) const {
    if (x.size() != n_) {
        throw std::invalid_argument(
            fmt::format("{}: a {}-element iterate was handed to a {}-variable problem",
                        problem_->name(), x.size(), n_));
    }
    if (x_cache_.size() != n_ || x_cache_ != x) {
        x_cache_ = x;
        g_valid_ = false;
        jac_valid_ = false;
    }
}

void NlpProblemModel::invalidate_cache() const {
    x_cache_.resize(0);
    g_valid_ = false;
    jac_valid_ = false;
}

void NlpProblemModel::refresh_g(const Vec &x) const {
    this->sync_x(x);
    if (!g_valid_) {
        if (m_ > 0) {
            problem_->eval_g(x, g_cache_);
        }
        g_valid_ = true;
    }
}

void NlpProblemModel::refresh_jac(const Vec &x) const {
    this->sync_x(x);
    if (!jac_valid_) {
        if (jac_nnz_ > 0) {
            problem_->eval_jacobian(x, jac_cache_);
        }
        jac_valid_ = true;
    }
}

double NlpProblemModel::eval_f(const Vec &x) const {
    this->sync_x(x);
    return problem_->eval_f(x);
}

void NlpProblemModel::eval_grad_in_place(const Vec &x, Vec &out) const {
    this->sync_x(x);
    out.resize(n_);
    problem_->eval_grad_f(x_cache_, out);
}

void NlpProblemModel::eval_ce_in_place(const Vec &x, Vec &out) const {
    this->refresh_g(x);
    out.resize(rows_.num_eq_);
    for (std::size_t k = 0; k < eq_res_.size(); k++) {
        const ResTarget &t = eq_res_[k];
        out[static_cast<Index>(k)] = t.sign_ * (g_cache_[t.row_] - t.shift_);
    }
}

void NlpProblemModel::eval_ci_in_place(const Vec &x, Vec &out) const {
    this->refresh_g(x);
    out.resize(rows_.num_iq_);
    for (std::size_t k = 0; k < iq_res_.size(); k++) {
        const ResTarget &t = iq_res_[k];
        out[static_cast<Index>(k)] = t.sign_ * (g_cache_[t.row_] - t.shift_);
    }
}

void NlpProblemModel::eval_jac_e_in_place(const Vec &x, SpMatRM &out) const {
    this->refresh_jac(x);
    adopt_pattern(out, jac_e_);
    zero_values(out);
    for (const JacTarget &t : eq_jac_) {
        out.valuePtr()[t.value_index_] += t.sign_ * jac_cache_[t.slot_];
    }
}

void NlpProblemModel::eval_jac_i_in_place(const Vec &x, SpMatRM &out) const {
    this->refresh_jac(x);
    adopt_pattern(out, jac_i_);
    zero_values(out);
    for (const JacTarget &t : iq_jac_) {
        out.valuePtr()[t.value_index_] += t.sign_ * jac_cache_[t.slot_];
    }
}

void NlpProblemModel::eval_hess_in_place(const Vec &x, double obj_scale, const Vec &lambda_e,
                                         const Vec &lambda_i, SpMatRM &out) const {
    if (!problem_->has_hessian()) {
        throw std::logic_error(fmt::format(
            "{}: eval_hess was reached on a problem that declares has_hessian() == false; "
            "only a quasi-Newton SQP solves it, and that path never reads the Hessian",
            problem_->name()));
    }
    this->sync_x(x);
    this->compose_into(lambda_user_, lambda_e, lambda_i, "eval_hess_in_place");
    if (hess_nnz_ > 0) {
        problem_->eval_hessian(x_cache_, obj_scale, lambda_user_, hess_vals_);
    }
    adopt_pattern(out, hess_);
    zero_values(out);
    for (int k = 0; k < hess_nnz_; k++) {
        out.valuePtr()[hess_value_index_[static_cast<std::size_t>(k)]] += hess_vals_[k];
    }
}

// The by-value contract: the same fill into a member of this model, then one
// copy of it.
Vec NlpProblemModel::eval_grad(const Vec &x) const {
    this->eval_grad_in_place(x, grad_);
    return grad_;
}
Vec NlpProblemModel::eval_ce(const Vec &x) const {
    this->eval_ce_in_place(x, ce_);
    return ce_;
}
Vec NlpProblemModel::eval_ci(const Vec &x) const {
    this->eval_ci_in_place(x, ci_);
    return ci_;
}
SpMatRM NlpProblemModel::eval_jac_e(const Vec &x) const {
    this->eval_jac_e_in_place(x, jac_e_values_);
    return jac_e_values_;
}
SpMatRM NlpProblemModel::eval_jac_i(const Vec &x) const {
    this->eval_jac_i_in_place(x, jac_i_values_);
    return jac_i_values_;
}
SpMatRM NlpProblemModel::eval_hess(const Vec &x, double obj_scale, const Vec &lambda_e,
                                   const Vec &lambda_i) const {
    this->eval_hess_in_place(x, obj_scale, lambda_e, lambda_i, hess_values_);
    return hess_values_;
}

Vec NlpProblemModel::start_point() const {
    Vec x0(n_);
    for (int i = 0; i < n_; i++) {
        x0[i] = std::min(std::max(0.0, x_lower_[i]), x_upper_[i]);
    }
    return x0;
}

void NlpProblemModel::compose_into(Vec &lambda_user, ConstEigenRef<Vec> lambda_e,
                                   ConstEigenRef<Vec> lambda_i, const char *site) const {
    // Exact length only, empty included: the symmetric rule with the chain
    // surface (nlp_adapter.h's refuse_short_multiplier_block, which reads the
    // head of a block at least as long as its rows) is that the model surface
    // takes exactly its own row count. Empty is legal exactly where that count
    // is itself 0 -- not a special case that means "all-zero" regardless of
    // how many rows are actually hosted. A caller that wants all-zero
    // multipliers on a model with rows passes that many zeros explicitly.
    if (lambda_e.size() != rows_.num_eq_) {
        throw std::invalid_argument(fmt::format(
            "{}: {} refused {} equality multipliers for {} equality rows -- exactly that many "
            "are accepted; empty is the exact length only when zero rows are hosted",
            problem_->name(), site, lambda_e.size(), rows_.num_eq_));
    }
    if (lambda_i.size() != rows_.num_iq_) {
        throw std::invalid_argument(fmt::format(
            "{}: {} refused {} inequality multipliers for {} inequality rows -- exactly that "
            "many are accepted; empty is the exact length only when zero rows are hosted",
            problem_->name(), site, lambda_i.size(), rows_.num_iq_));
    }
    lambda_user.resize(m_);
    for (int r = 0; r < m_; r++) {
        switch (rows_.kinds_[r]) {
        case NLPRowKind::Equality:
            lambda_user[r] = lambda_e[rows_.eq_row_[r]];
            break;
        case NLPRowKind::UpperBounded:
            lambda_user[r] = lambda_i[rows_.iq_upper_row_[r]];
            break;
        case NLPRowKind::LowerBounded:
            lambda_user[r] = -lambda_i[rows_.iq_lower_row_[r]];
            break;
        case NLPRowKind::Range:
            lambda_user[r] = lambda_i[rows_.iq_upper_row_[r]] - lambda_i[rows_.iq_lower_row_[r]];
            break;
        case NLPRowKind::Free:
            lambda_user[r] = 0.0;
            break;
        }
    }
}

Vec NlpProblemModel::compose_user_multipliers(ConstEigenRef<Vec> lambda_e,
                                              ConstEigenRef<Vec> lambda_i) const {
    Vec lambda_user;
    this->compose_into(lambda_user, lambda_e, lambda_i, "compose_user_multipliers");
    return lambda_user;
}

void NlpProblemModel::split_user_multipliers(ConstEigenRef<Vec> lambda_user, Vec &lambda_e,
                                             Vec &lambda_i) const {
    if (lambda_user.size() != m_) {
        throw std::invalid_argument(
            fmt::format("{}: {} multipliers were supplied for {} declared constraint rows",
                        problem_->name(), lambda_user.size(), m_));
    }
    lambda_e = Vec::Zero(rows_.num_eq_);
    lambda_i = Vec::Zero(rows_.num_iq_);
    for (int r = 0; r < m_; r++) {
        switch (rows_.kinds_[r]) {
        case NLPRowKind::Equality:
            lambda_e[rows_.eq_row_[r]] = lambda_user[r];
            break;
        case NLPRowKind::UpperBounded:
            lambda_i[rows_.iq_upper_row_[r]] = lambda_user[r];
            break;
        case NLPRowKind::LowerBounded:
            lambda_i[rows_.iq_lower_row_[r]] = -lambda_user[r];
            break;
        case NLPRowKind::Range:
            // The two native rows are one-sided, so the signed declared value
            // goes to whichever side it names and the other side takes zero.
            // Composing this undoes it exactly; the reverse order does not,
            // since a pair with both sides nonzero composes to their
            // difference and cannot be recovered from it.
            lambda_i[rows_.iq_upper_row_[r]] = std::max(lambda_user[r], 0.0);
            lambda_i[rows_.iq_lower_row_[r]] = std::max(-lambda_user[r], 0.0);
            break;
        case NLPRowKind::Free:
            break;
        }
    }
}

} // namespace hven::solvers
