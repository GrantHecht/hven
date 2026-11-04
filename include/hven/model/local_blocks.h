// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// local_blocks.h -- the local-block piece layer: the default way to write a piece.
//
// A local-block piece states its sizes as compile-time constants and evaluates
// small dense blocks over its own inputs, into buffers the library provides. The
// library does the rest: the KKT claims, the scatter, the lock protocol, the
// elimination of fixed variables and the partitioning. It is one generic adapter
// over the raw piece protocol (docs/assembly-protocol.md): a local-block piece
// enters ConstraintInterface or ObjectiveInterface as LocalConstraintBlock<P> or
// LocalObjectiveBlock<P>, which are ordinary raw-protocol pieces.
//
// A CONSTRAINT piece (an equality row block c(v) = 0, or an inequality row block
// c(v) <= 0):
//
//     struct Defect {
//         static constexpr hven::Index kInputs = 3, kOutputs = 1;
//         void value(hven::ConstVecRef v, hven::VecRef out) const;        // kOutputs
//         void jacobian(hven::ConstVecRef v, hven::MatRef J) const;       // kOutputs x kInputs
//         void hessian(hven::ConstVecRef v, hven::ConstVecRef lambda,     // kInputs x kInputs:
//                      hven::MatRef H) const;                             // sum_j lambda_j d2c_j
//     };
//
// An OBJECTIVE piece (a scalar term added to the objective):
//
//     struct Term {
//         static constexpr hven::Index kInputs = 1;
//         double value(hven::ConstVecRef v) const;
//         void gradient(hven::ConstVecRef v, hven::VecRef g) const;       // kInputs
//         void hessian(hven::ConstVecRef v, hven::MatRef H) const;        // kInputs x kInputs
//     };
//
// Rules, for both kinds:
//
// - The buffers arrive zeroed. Write the entries that are not zero.
// - Hessian blocks are read in the LOWER TRIANGLE (row >= col) only; the upper
//   triangle may be left as it arrives or filled symmetrically.
// - Optional sparsity patterns trim the blocks. A piece may declare
//       static constexpr std::array<hven::solvers::BlockEntry, K> kJacobianPattern{...};
//       static constexpr std::array<hven::solvers::BlockEntry, K> kHessianPattern{...};
//   naming the block entries that can be nonzero. Only those entries are claimed
//   in the KKT matrix and read from the buffers. Without a pattern the Jacobian
//   is dense and the Hessian is its full lower triangle. A Hessian pattern lists
//   lower-triangle entries only. Every entry must be in range and listed once;
//   a pattern that breaks either rule is a compile-time error.
// - A piece with no second derivatives (a linear constraint, a linear
//   objective) declares `static constexpr auto kHessianPattern =
//   hven::solvers::kNoHessian;` and need not provide hessian().
// - name() is optional (`std::string name() const`); it names the piece in
//   diagnostics.
// - Every member is const, and the layout may call them concurrently from
//   several threads when the problem is partitioned: a piece must be safe to
//   evaluate concurrently through its const members. Any movable type is legal.
// - Evaluation failure follows the library's contract (core/errors.h): throw
//   hven::solvers::EvaluationError, or produce a non-finite value, at a point the
//   piece cannot evaluate, and the engine treats the trial as rejected.
// - An application may name one variable more than once; the blocks are summed
//   into the global derivatives with the multiplicity that implies.
//
// The sizes are compile-time constants so the blocks live on the stack. A piece
// whose sizes are only known at run time is a raw-protocol piece.

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <Eigen/Core>
#include <Eigen/SparseCore>

#include "hven/core/types.h"
#include "hven/model/indexing_data.h"
#include "hven/model/solver_interface_specs.h"

namespace hven::solvers {

/// One entry of a local block: its row and column inside the block.
struct BlockEntry {
    Index row = 0;
    Index col = 0;

    friend constexpr bool operator==(const BlockEntry &, const BlockEntry &) = default;
};

/// The Hessian pattern of a piece with no second derivatives.
inline constexpr std::array<BlockEntry, 0> kNoHessian{};

namespace detail {

template <class P>
concept HasJacobianPattern = requires {
    P::kJacobianPattern.size();
    P::kJacobianPattern[0];
};

template <class P>
concept HasHessianPattern = requires { P::kHessianPattern.size(); };

template <class P>
concept HasLocalName = requires(const P &p) {
    { p.name() } -> std::convertible_to<std::string>;
};

template <class P>
concept DeclaresNoHessian = HasHessianPattern<P> && (std::size(P::kHessianPattern) == 0);

/// Every entry of a rows x cols block, row by row.
template <Index Rows, Index Cols> constexpr auto dense_block_pattern() {
    std::array<BlockEntry, static_cast<std::size_t>(Rows * Cols)> pattern{};
    std::size_t k = 0;
    for (Index r = 0; r < Rows; r++) {
        for (Index c = 0; c < Cols; c++) {
            pattern[k++] = BlockEntry{r, c};
        }
    }
    return pattern;
}

/// The lower triangle (row >= col) of an n x n block, row by row.
template <Index N> constexpr auto lower_block_pattern() {
    std::array<BlockEntry, static_cast<std::size_t>(N * (N + 1) / 2)> pattern{};
    std::size_t k = 0;
    for (Index r = 0; r < N; r++) {
        for (Index c = 0; c <= r; c++) {
            pattern[k++] = BlockEntry{r, c};
        }
    }
    return pattern;
}

/// A declared pattern copied into a std::array, whatever constexpr range the
/// piece declared it as.
template <auto &Declared> constexpr auto as_pattern() {
    std::array<BlockEntry, std::size(Declared)> pattern{};
    std::size_t k = 0;
    for (const auto &entry : Declared) {
        pattern[k++] = BlockEntry{static_cast<Index>(entry.row), static_cast<Index>(entry.col)};
    }
    return pattern;
}

template <std::size_t K>
constexpr bool pattern_in_range(const std::array<BlockEntry, K> &p, Index rows, Index cols) {
    for (const BlockEntry &e : p) {
        if (e.row < 0 || e.row >= rows || e.col < 0 || e.col >= cols) {
            return false;
        }
    }
    return true;
}

template <std::size_t K> constexpr bool pattern_lower(const std::array<BlockEntry, K> &p) {
    for (const BlockEntry &e : p) {
        if (e.col > e.row) {
            return false;
        }
    }
    return true;
}

template <std::size_t K> constexpr bool pattern_unique(const std::array<BlockEntry, K> &p) {
    for (std::size_t a = 0; a < K; a++) {
        for (std::size_t b = a + 1; b < K; b++) {
            if (p[a] == p[b]) {
                return false;
            }
        }
    }
    return true;
}

/// The Jacobian pattern a constraint piece is laid with.
template <class P> constexpr auto local_jacobian_pattern() {
    if constexpr (HasJacobianPattern<P>) {
        return as_pattern<P::kJacobianPattern>();
    } else {
        return dense_block_pattern<P::kOutputs, P::kInputs>();
    }
}

/// The Hessian pattern a piece of either kind is laid with.
template <class P> constexpr auto local_hessian_pattern() {
    if constexpr (HasHessianPattern<P>) {
        return as_pattern<P::kHessianPattern>();
    } else {
        return lower_block_pattern<P::kInputs>();
    }
}

/// Throws the refusal for a constraint block claimed with one of its two
/// domains only (src/model/validation_errors.cpp).
[[noreturn]] void throw_local_block_split_claim(const std::string &piece);

/// Adds @p value into the KKT value slot @p slot, under the shared lock protocol:
/// the mutex of @p lock_col is taken when @p check and the column is contested.
inline void local_block_kkt_add(Eigen::SparseMatrix<double, Eigen::RowMajor> &kkt,
                                const Eigen::Ref<Eigen::VectorXi> &locations,
                                const Eigen::Ref<Eigen::VectorXi> &clashes,
                                std::vector<std::mutex> &locks, int slot, int lock_col,
                                double value, bool check) {
    const int mark = check ? clashes[lock_col] : -1;
    if (mark != -1) {
        locks[mark].lock();
    }
    kkt.valuePtr()[locations[slot]] += value;
    if (mark != -1) {
        locks[mark].unlock();
    }
}

} // namespace detail

/// A local-block constraint piece: kInputs and kOutputs, value() and jacobian(),
/// and hessian() unless the piece declares kHessianPattern = kNoHessian.
template <class P>
concept LocalConstraintPiece = std::is_class_v<P> && requires {
    { P::kInputs } -> std::convertible_to<Index>;
    { P::kOutputs } -> std::convertible_to<Index>;
} && requires(const P &p, ConstVecRef v, VecRef out, MatRef J) {
    p.value(v, out);
    p.jacobian(v, J);
} && (detail::DeclaresNoHessian<P> || requires(const P &p, ConstVecRef v, MatRef H) {
                                   p.hessian(v, v, H);
                               });

/// A local-block objective piece: kInputs (and no kOutputs), a scalar value(),
/// gradient(), and hessian() unless the piece declares kHessianPattern =
/// kNoHessian.
template <class P>
concept LocalObjectivePiece = std::is_class_v<P> && !requires { P::kOutputs; } && requires {
    { P::kInputs } -> std::convertible_to<Index>;
} && requires(const P &p, ConstVecRef v, VecRef g) {
    { p.value(v) } -> std::convertible_to<double>;
    p.gradient(v, g);
} && (detail::DeclaresNoHessian<P> || requires(const P &p, ConstVecRef v, MatRef H) {
                                  p.hessian(v, H);
                              });

/// The raw-protocol constraint piece that carries a local-block constraint piece:
/// it gathers each application's inputs, calls the piece's blocks, and writes
/// them through the claimed slots. Claims per application: the Jacobian pattern's
/// entries, then the Hessian pattern's, contiguous.
template <class P> struct LocalConstraintBlock {
    static constexpr int kIn = static_cast<int>(P::kInputs);
    static constexpr int kOut = static_cast<int>(P::kOutputs);
    static_assert(kIn >= 1, "hven local block: a constraint piece needs kInputs >= 1");
    static_assert(kOut >= 1, "hven local block: a constraint piece needs kOutputs >= 1");

    static constexpr auto kJacobian = detail::local_jacobian_pattern<P>();
    static constexpr auto kHessian = detail::local_hessian_pattern<P>();
    static constexpr int kJacobianCount = static_cast<int>(kJacobian.size());
    static constexpr int kHessianCount = static_cast<int>(kHessian.size());

    static_assert(detail::pattern_in_range(kJacobian, P::kOutputs, P::kInputs),
                  "hven local block: a kJacobianPattern entry lies outside the kOutputs x "
                  "kInputs block");
    static_assert(detail::pattern_unique(kJacobian),
                  "hven local block: kJacobianPattern lists an entry more than once");
    static_assert(detail::pattern_in_range(kHessian, P::kInputs, P::kInputs),
                  "hven local block: a kHessianPattern entry lies outside the kInputs x "
                  "kInputs block");
    static_assert(detail::pattern_lower(kHessian),
                  "hven local block: a kHessianPattern entry lies above the diagonal; list the "
                  "lower triangle (row >= col) only");
    static_assert(detail::pattern_unique(kHessian),
                  "hven local block: kHessianPattern lists an entry more than once");

    P piece_;

    std::string name() const {
        if constexpr (detail::HasLocalName<P>) {
            return piece_.name();
        } else {
            return "local constraint block";
        }
    }
    int input_rows() const { return kIn; }
    int output_rows() const { return kOut; }
    bool thread_safe() const { return true; }

    void constraints(const Eigen::Ref<const Eigen::VectorXd> &X, Eigen::Ref<Eigen::VectorXd> FX,
                     const SolverIndexingData &d) const {
        Buffers b;
        for (int V = 0; V < d.num_appl(); V++) {
            b.gather(X, d, V);
            b.value(piece_);
            b.write_values(FX, d, V);
        }
    }

    void constraints_adjointgradient(const Eigen::Ref<const Eigen::VectorXd> &X,
                                     const Eigen::Ref<const Eigen::VectorXd> &L,
                                     Eigen::Ref<Eigen::VectorXd> FX,
                                     Eigen::Ref<Eigen::VectorXd> AGX,
                                     const SolverIndexingData &d) const {
        Buffers b;
        for (int V = 0; V < d.num_appl(); V++) {
            b.gather(X, d, V);
            b.value(piece_);
            b.write_values(FX, d, V);
            b.jacobian(piece_);
            b.write_adjoint_gradient(L, AGX, d, V);
        }
    }

    void constraints_jacobian(const Eigen::Ref<const Eigen::VectorXd> &X,
                              Eigen::Ref<Eigen::VectorXd> FX,
                              Eigen::SparseMatrix<double, Eigen::RowMajor> &kkt,
                              Eigen::Ref<Eigen::VectorXi> locations,
                              Eigen::Ref<Eigen::VectorXi> clashes, std::vector<std::mutex> &locks,
                              const SolverIndexingData &d) const {
        Buffers b;
        for (int V = 0; V < d.num_appl(); V++) {
            b.gather(X, d, V);
            b.value(piece_);
            b.write_values(FX, d, V);
            b.jacobian(piece_);
            b.scatter_jacobian(kkt, locations, clashes, locks, d, V);
        }
    }

    void constraints_jacobian_adjointgradient(
        const Eigen::Ref<const Eigen::VectorXd> &X, const Eigen::Ref<const Eigen::VectorXd> &L,
        Eigen::Ref<Eigen::VectorXd> FX, Eigen::Ref<Eigen::VectorXd> AGX,
        Eigen::SparseMatrix<double, Eigen::RowMajor> &kkt, Eigen::Ref<Eigen::VectorXi> locations,
        Eigen::Ref<Eigen::VectorXi> clashes, std::vector<std::mutex> &locks,
        const SolverIndexingData &d) const {
        Buffers b;
        for (int V = 0; V < d.num_appl(); V++) {
            b.gather(X, d, V);
            b.value(piece_);
            b.write_values(FX, d, V);
            b.jacobian(piece_);
            b.write_adjoint_gradient(L, AGX, d, V);
            b.scatter_jacobian(kkt, locations, clashes, locks, d, V);
        }
    }

    void constraints_jacobian_adjointgradient_adjointhessian(
        const Eigen::Ref<const Eigen::VectorXd> &X, const Eigen::Ref<const Eigen::VectorXd> &L,
        Eigen::Ref<Eigen::VectorXd> FX, Eigen::Ref<Eigen::VectorXd> AGX,
        Eigen::SparseMatrix<double, Eigen::RowMajor> &kkt, Eigen::Ref<Eigen::VectorXi> locations,
        Eigen::Ref<Eigen::VectorXi> clashes, std::vector<std::mutex> &locks,
        const SolverIndexingData &d) const {
        Buffers b;
        for (int V = 0; V < d.num_appl(); V++) {
            b.gather(X, d, V);
            b.value(piece_);
            b.write_values(FX, d, V);
            b.jacobian(piece_);
            b.write_adjoint_gradient(L, AGX, d, V);
            b.scatter_jacobian(kkt, locations, clashes, locks, d, V);
            if constexpr (kHessianCount > 0) {
                b.hessian(piece_, L, d, V);
                b.scatter_hessian(kkt, locations, clashes, locks, d, V);
            }
        }
    }

    void get_kkt_space(Eigen::Ref<Eigen::VectorXi> rows, Eigen::Ref<Eigen::VectorXi> cols,
                       int &free_slot, int con_offset, bool do_jac, bool do_hess,
                       SolverIndexingData &d) {
        // The Hessian scatter addresses its slots after the Jacobian's, so the
        // two domains are claimed together or the piece has no Hessian slots.
        if (kHessianCount > 0 && do_jac != do_hess) {
            detail::throw_local_block_split_claim(name());
        }
        d.inner_kkt_starts_.resize(d.num_appl());
        for (int V = 0; V < d.num_appl(); V++) {
            d.inner_kkt_starts_[V] = free_slot;
            if (do_jac) {
                for (const BlockEntry &e : kJacobian) {
                    const int col = d.v_scatter_loc(static_cast<int>(e.col), V);
                    rows[free_slot] =
                        (col < 0) ? -1 : d.c_loc(static_cast<int>(e.row), V) + con_offset;
                    cols[free_slot] = col;
                    free_slot++;
                }
            }
            if (do_hess) {
                for (const BlockEntry &e : kHessian) {
                    const int r = d.v_scatter_loc(static_cast<int>(e.row), V);
                    const int c = d.v_scatter_loc(static_cast<int>(e.col), V);
                    const bool eliminated = (r < 0 || c < 0);
                    rows[free_slot] = eliminated ? -1 : r;
                    cols[free_slot] = eliminated ? -1 : c;
                    free_slot++;
                }
            }
        }
    }

    int num_kkt_elements(bool do_jac, bool do_hess) const {
        return (do_jac ? kJacobianCount : 0) + (do_hess ? kHessianCount : 0);
    }

  private:
    /// One application's blocks, on the stack.
    struct Buffers {
        std::array<double, kIn> v{};
        std::array<double, kOut> out{};
        std::array<double, kOut * kIn> jac{};
        std::array<double, kOut> lambda{};
        std::array<double, (kHessianCount > 0 ? kIn * kIn : 1)> hess{};

        void gather(const Eigen::Ref<const Eigen::VectorXd> &X, const SolverIndexingData &d,
                    int V) {
            for (int i = 0; i < kIn; i++) {
                v[i] = X[d.v_loc(i, V)];
            }
        }
        void value(const P &piece) {
            out.fill(0.0);
            const Eigen::Map<const Eigen::VectorXd> vm(v.data(), kIn);
            Eigen::Map<Eigen::VectorXd> om(out.data(), kOut);
            piece.value(vm, om);
        }
        void jacobian(const P &piece) {
            jac.fill(0.0);
            const Eigen::Map<const Eigen::VectorXd> vm(v.data(), kIn);
            Eigen::Map<Eigen::MatrixXd> jm(jac.data(), kOut, kIn);
            piece.jacobian(vm, jm);
        }
        void hessian(const P &piece, const Eigen::Ref<const Eigen::VectorXd> &L,
                     const SolverIndexingData &d, int V) {
            for (int j = 0; j < kOut; j++) {
                lambda[j] = L[d.c_loc(j, V)];
            }
            hess.fill(0.0);
            const Eigen::Map<const Eigen::VectorXd> vm(v.data(), kIn);
            const Eigen::Map<const Eigen::VectorXd> lm(lambda.data(), kOut);
            Eigen::Map<Eigen::MatrixXd> hm(hess.data(), kIn, kIn);
            piece.hessian(vm, lm, hm);
        }
        double jac_at(const BlockEntry &e) const {
            return jac[static_cast<std::size_t>(e.col * kOut + e.row)];
        }
        double hess_at(const BlockEntry &e) const {
            return hess[static_cast<std::size_t>(e.col * kIn + e.row)];
        }
        void write_values(Eigen::Ref<Eigen::VectorXd> FX, const SolverIndexingData &d,
                          int V) const {
            const int start = d.inner_constraint_starts_[V];
            for (int j = 0; j < kOut; j++) {
                FX[start + j] = out[j];
            }
        }
        void write_adjoint_gradient(const Eigen::Ref<const Eigen::VectorXd> &L,
                                    Eigen::Ref<Eigen::VectorXd> AGX, const SolverIndexingData &d,
                                    int V) const {
            const int start = d.inner_gradient_starts_[V];
            for (const BlockEntry &e : kJacobian) {
                AGX[start + static_cast<int>(e.col)] +=
                    L[d.c_loc(static_cast<int>(e.row), V)] * jac_at(e);
            }
        }
        void scatter_jacobian(Eigen::SparseMatrix<double, Eigen::RowMajor> &kkt,
                              const Eigen::Ref<Eigen::VectorXi> &locations,
                              const Eigen::Ref<Eigen::VectorXi> &clashes,
                              std::vector<std::mutex> &locks, const SolverIndexingData &d,
                              int V) const {
            int slot = d.inner_kkt_starts_[V];
            for (const BlockEntry &e : kJacobian) {
                const int col = d.v_scatter_loc(static_cast<int>(e.col), V);
                if (col >= 0) {
                    detail::local_block_kkt_add(kkt, locations, clashes, locks, slot, col,
                                                jac_at(e), !d.unique_constraints_);
                }
                slot++;
            }
        }
        void scatter_hessian(Eigen::SparseMatrix<double, Eigen::RowMajor> &kkt,
                             const Eigen::Ref<Eigen::VectorXi> &locations,
                             const Eigen::Ref<Eigen::VectorXi> &clashes,
                             std::vector<std::mutex> &locks, const SolverIndexingData &d,
                             int V) const {
            int slot = d.inner_kkt_starts_[V] + kJacobianCount;
            for (const BlockEntry &e : kHessian) {
                const int r = d.v_scatter_loc(static_cast<int>(e.row), V);
                const int c = d.v_scatter_loc(static_cast<int>(e.col), V);
                if (r >= 0 && c >= 0) {
                    // An off-diagonal block entry whose two inputs name one
                    // variable lands on that variable's diagonal twice, once
                    // from each triangle.
                    const double w = (r == c && e.row != e.col) ? 2.0 * hess_at(e) : hess_at(e);
                    detail::local_block_kkt_add(kkt, locations, clashes, locks, slot,
                                                kkt_canonical_lock_col(r, c), w, true);
                }
                slot++;
            }
        }
    };
};

/// The raw-protocol objective piece that carries a local-block objective piece.
/// Claims per application: the Hessian pattern's entries.
template <class P> struct LocalObjectiveBlock {
    static constexpr int kIn = static_cast<int>(P::kInputs);
    static_assert(kIn >= 1, "hven local block: an objective piece needs kInputs >= 1");

    static constexpr auto kHessian = detail::local_hessian_pattern<P>();
    static constexpr int kHessianCount = static_cast<int>(kHessian.size());

    static_assert(detail::pattern_in_range(kHessian, P::kInputs, P::kInputs),
                  "hven local block: a kHessianPattern entry lies outside the kInputs x "
                  "kInputs block");
    static_assert(detail::pattern_lower(kHessian),
                  "hven local block: a kHessianPattern entry lies above the diagonal; list the "
                  "lower triangle (row >= col) only");
    static_assert(detail::pattern_unique(kHessian),
                  "hven local block: kHessianPattern lists an entry more than once");

    P piece_;

    std::string name() const {
        if constexpr (detail::HasLocalName<P>) {
            return piece_.name();
        } else {
            return "local objective block";
        }
    }
    int input_rows() const { return kIn; }
    bool thread_safe() const { return true; }

    void objective(double scale, const Eigen::Ref<const Eigen::VectorXd> &X, double &val,
                   const SolverIndexingData &d) const {
        std::array<double, kIn> v{};
        for (int V = 0; V < d.num_appl(); V++) {
            gather(v, X, d, V);
            val += scale * piece_.value(Eigen::Map<const Eigen::VectorXd>(v.data(), kIn));
        }
    }

    void objective_gradient(double scale, const Eigen::Ref<const Eigen::VectorXd> &X, double &val,
                            Eigen::Ref<Eigen::VectorXd> GX, const SolverIndexingData &d) const {
        std::array<double, kIn> v{};
        std::array<double, kIn> g{};
        for (int V = 0; V < d.num_appl(); V++) {
            gather(v, X, d, V);
            value_gradient(v, g, scale, val, GX, d, V);
        }
    }

    void objective_gradient_hessian(double scale, const Eigen::Ref<const Eigen::VectorXd> &X,
                                    double &val, Eigen::Ref<Eigen::VectorXd> GX,
                                    Eigen::SparseMatrix<double, Eigen::RowMajor> &kkt,
                                    Eigen::Ref<Eigen::VectorXi> locations,
                                    Eigen::Ref<Eigen::VectorXi> clashes,
                                    std::vector<std::mutex> &locks,
                                    const SolverIndexingData &d) const {
        std::array<double, kIn> v{};
        std::array<double, kIn> g{};
        std::array<double, (kHessianCount > 0 ? kIn * kIn : 1)> h{};
        for (int V = 0; V < d.num_appl(); V++) {
            gather(v, X, d, V);
            value_gradient(v, g, scale, val, GX, d, V);
            if constexpr (kHessianCount > 0) {
                h.fill(0.0);
                Eigen::Map<Eigen::MatrixXd> hm(h.data(), kIn, kIn);
                piece_.hessian(Eigen::Map<const Eigen::VectorXd>(v.data(), kIn), hm);
                int slot = d.inner_kkt_starts_[V];
                for (const BlockEntry &e : kHessian) {
                    const int r = d.v_scatter_loc(static_cast<int>(e.row), V);
                    const int c = d.v_scatter_loc(static_cast<int>(e.col), V);
                    if (r >= 0 && c >= 0) {
                        const double entry = h[static_cast<std::size_t>(e.col * kIn + e.row)];
                        const double w = (r == c && e.row != e.col) ? 2.0 * entry : entry;
                        detail::local_block_kkt_add(kkt, locations, clashes, locks, slot,
                                                    kkt_canonical_lock_col(r, c), scale * w, true);
                    }
                    slot++;
                }
            }
        }
    }

    void get_kkt_space(Eigen::Ref<Eigen::VectorXi> rows, Eigen::Ref<Eigen::VectorXi> cols,
                       int &free_slot, int /*con_offset*/, bool /*do_jac*/, bool do_hess,
                       SolverIndexingData &d) {
        d.inner_kkt_starts_.resize(d.num_appl());
        for (int V = 0; V < d.num_appl(); V++) {
            d.inner_kkt_starts_[V] = free_slot;
            if (do_hess) {
                for (const BlockEntry &e : kHessian) {
                    const int r = d.v_scatter_loc(static_cast<int>(e.row), V);
                    const int c = d.v_scatter_loc(static_cast<int>(e.col), V);
                    const bool eliminated = (r < 0 || c < 0);
                    rows[free_slot] = eliminated ? -1 : r;
                    cols[free_slot] = eliminated ? -1 : c;
                    free_slot++;
                }
            }
        }
    }

    int num_kkt_elements(bool /*do_jac*/, bool do_hess) const {
        return do_hess ? kHessianCount : 0;
    }

  private:
    static void gather(std::array<double, kIn> &v, const Eigen::Ref<const Eigen::VectorXd> &X,
                       const SolverIndexingData &d, int V) {
        for (int i = 0; i < kIn; i++) {
            v[i] = X[d.v_loc(i, V)];
        }
    }

    void value_gradient(const std::array<double, kIn> &v, std::array<double, kIn> &g, double scale,
                        double &val, Eigen::Ref<Eigen::VectorXd> GX, const SolverIndexingData &d,
                        int V) const {
        const Eigen::Map<const Eigen::VectorXd> vm(v.data(), kIn);
        val += scale * piece_.value(vm);
        g.fill(0.0);
        Eigen::Map<Eigen::VectorXd> gm(g.data(), kIn);
        piece_.gradient(vm, gm);
        const int start = d.inner_gradient_starts_[V];
        for (int i = 0; i < kIn; i++) {
            GX[start + i] = scale * g[i];
        }
    }
};

/// A local-block constraint piece enters ConstraintInterface as its
/// LocalConstraintBlock: one erasure, one virtual dispatch per evaluation call.
/// It is not an objective.
template <class P>
    requires LocalConstraintPiece<P>
struct SolverInterfaceAdapter<P> : ObjectiveUnsupported<P> {
    static void install_constraint(const P &piece, ConstraintInterface &ci) {
        ci.storage_.emplace<ConstraintModel<LocalConstraintBlock<P>>>(
            LocalConstraintBlock<P>{piece});
    }
};

/// A local-block objective piece enters ObjectiveInterface as its
/// LocalObjectiveBlock. It is not a constraint.
template <class P>
    requires LocalObjectivePiece<P>
struct SolverInterfaceAdapter<P> : ConstraintUnsupported<P> {
    static void install_objective(const P &piece, ObjectiveInterface &oi) {
        oi.storage_.emplace<ObjectiveModel<LocalObjectiveBlock<P>>>(LocalObjectiveBlock<P>{piece});
    }
};

} // namespace hven::solvers
