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
// LocalObjectiveBlock<P>, which are ordinary raw-protocol pieces. The two handles'
// header (model/piece_interfaces.h) includes this one, so wherever a piece
// can be converted to a handle, this layer is what converts it.
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
// - A class with a static member kInputs is a local-block piece. One that does
//   not meet the shape above is refused at compile time with a message naming
//   what is missing (a non-const member, a wrong signature, kOutputs on what is
//   written as an objective).
// - The buffers arrive zeroed. Write the entries that are not zero.
// - Hessian blocks are read in the LOWER TRIANGLE (row >= col) only; the upper
//   triangle may be left as it arrives or filled symmetrically.
// - Optional sparsity patterns trim the blocks. A piece may declare
//       static constexpr std::array<hven::BlockEntry, K> kJacobianPattern{...};
//       static constexpr std::array<hven::BlockEntry, K> kHessianPattern{...};
//   (or a constexpr C array of BlockEntry) naming the block entries that can be
//   nonzero. Only those entries are claimed in the KKT matrix and read from the
//   buffers. Without a pattern the Jacobian is dense and the Hessian is its full
//   lower triangle. A Hessian pattern lists lower-triangle entries only. Every
//   entry must be in range and listed once; a pattern that breaks either rule is
//   a compile-time error.
// - A piece with no second derivatives (a linear constraint, a linear
//   objective) declares `static constexpr auto kHessianPattern =
//   hven::kNoHessian;` and need not provide hessian().
// - name() is optional (`std::string name() const`); it names the piece in
//   diagnostics. Without it the piece is named by its kind and sizes. A name()
//   that is not const, or does not return a string, is a compile-time error.
// - A piece must be COPY-CONSTRUCTIBLE: the layout copies it once per partition.
//   Every member is const, and the copies are evaluated concurrently from several
//   threads when the problem is partitioned. State shared between the copies is
//   held through a std::shared_ptr and must be safe to read concurrently.
// - Evaluation failure follows the library's contract (core/errors.h): throw
//   hven::EvaluationError, or produce a non-finite value, at a point the
//   piece cannot evaluate, and the engine treats the trial as rejected.
// - An application may name one variable more than once; the blocks are summed
//   into the global derivatives with the multiplicity that implies.
// - A piece that also carries a pack_into_*_interface hook is refused: it would
//   have two meanings.
//
// The sizes are compile-time constants. The blocks of one application live on the
// stack while they fit kLocalBlockStackBudget bytes; a larger piece evaluates into
// scratch each partition's copy allocates once, on its first evaluation. A piece
// whose sizes are only known at run time is a raw-protocol piece.

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <Eigen/Core>
#include <Eigen/SparseCore>

#include "hven/core/types.h"
#include "hven/model/piece_interfaces.h"
#include "hven/model/solver_indexing_data.h"

namespace hven {

/// One entry of a local block: its row and column inside the block.
struct BlockEntry {
    Index row = 0;
    Index col = 0;

    friend constexpr bool operator==(const BlockEntry &, const BlockEntry &) = default;
};

/// The Hessian pattern of a piece with no second derivatives.
inline constexpr std::array<BlockEntry, 0> kNoHessian{};

/// The largest block buffer, in bytes, one application of a local-block piece
/// evaluates into on the stack. A piece whose buffers exceed it evaluates into
/// scratch each partition's copy allocates once. Sized well inside the smallest
/// worker-thread stack the library runs on (512 KiB, macOS).
inline constexpr std::size_t kLocalBlockStackBudget = 16 * 1024;

namespace detail {

template <class P>
concept HasJacobianPattern = requires {
    std::size(P::kJacobianPattern);
    std::begin(P::kJacobianPattern);
};
template <class P>
concept NamesJacobianPattern = requires { P::kJacobianPattern; };

template <class P>
concept HasHessianPattern = requires {
    std::size(P::kHessianPattern);
    std::begin(P::kHessianPattern);
};
template <class P>
concept NamesHessianPattern = requires { P::kHessianPattern; };

template <class P>
concept HasLocalName = requires(const P &p) {
    { p.name() } -> std::convertible_to<std::string>;
};
/// A name() callable at all, const or not, whatever it returns.
template <class P>
concept HasAnyName = requires(P &p) { p.name(); };

template <class P>
concept DeclaresNoHessian = HasHessianPattern<P> && (std::size(P::kHessianPattern) == 0);

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

/// Whether a piece's DECLARED Jacobian pattern is in range / unique; true when it
/// declares none (the dense default is both by construction).
template <class P> constexpr bool declared_jacobian_in_range() {
    if constexpr (HasJacobianPattern<P>) {
        return pattern_in_range(as_pattern<P::kJacobianPattern>(), P::kOutputs, P::kInputs);
    } else {
        return true;
    }
}
template <class P> constexpr bool declared_jacobian_unique() {
    if constexpr (HasJacobianPattern<P>) {
        return pattern_unique(as_pattern<P::kJacobianPattern>());
    } else {
        return true;
    }
}
/// Whether a piece's DECLARED Hessian pattern is in range / lower / unique; true
/// when it declares none (the full lower triangle is all three by construction).
template <class P> constexpr bool declared_hessian_in_range() {
    if constexpr (HasHessianPattern<P>) {
        return pattern_in_range(as_pattern<P::kHessianPattern>(), P::kInputs, P::kInputs);
    } else {
        return true;
    }
}
template <class P> constexpr bool declared_hessian_lower() {
    if constexpr (HasHessianPattern<P>) {
        return pattern_lower(as_pattern<P::kHessianPattern>());
    } else {
        return true;
    }
}
template <class P> constexpr bool declared_hessian_unique() {
    if constexpr (HasHessianPattern<P>) {
        return pattern_unique(as_pattern<P::kHessianPattern>());
    } else {
        return true;
    }
}

/// The entries a constraint piece's Jacobian block is laid with, in claim order:
/// the declared pattern's, or every entry of the kOutputs x kInputs block row by
/// row. @p f is called as f(row, col).
template <class P, class F> constexpr void for_each_jacobian_entry(F &&f) {
    if constexpr (HasJacobianPattern<P>) {
        for (const auto &e : P::kJacobianPattern) {
            f(static_cast<int>(e.row), static_cast<int>(e.col));
        }
    } else {
        for (int r = 0; r < static_cast<int>(P::kOutputs); r++) {
            for (int c = 0; c < static_cast<int>(P::kInputs); c++) {
                f(r, c);
            }
        }
    }
}
template <class P> constexpr int jacobian_entry_count() {
    if constexpr (HasJacobianPattern<P>) {
        return static_cast<int>(std::size(P::kJacobianPattern));
    } else {
        return static_cast<int>(P::kOutputs * P::kInputs);
    }
}

/// The entries a piece's Hessian block is laid with, in claim order: the declared
/// pattern's, or the lower triangle (row >= col) of the kInputs x kInputs block
/// row by row. @p f is called as f(row, col).
template <class P, class F> constexpr void for_each_hessian_entry(F &&f) {
    if constexpr (HasHessianPattern<P>) {
        for (const auto &e : P::kHessianPattern) {
            f(static_cast<int>(e.row), static_cast<int>(e.col));
        }
    } else {
        for (int r = 0; r < static_cast<int>(P::kInputs); r++) {
            for (int c = 0; c <= r; c++) {
                f(r, c);
            }
        }
    }
}
template <class P> constexpr int hessian_entry_count() {
    if constexpr (HasHessianPattern<P>) {
        return static_cast<int>(std::size(P::kHessianPattern));
    } else {
        return static_cast<int>(P::kInputs * (P::kInputs + 1) / 2);
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

/// Block scratch owned by one copy of a local block, for a piece too large for
/// the stack. A copy is a new lane: it starts empty and allocates its own on its
/// first evaluation, once, so no two partitions ever share a buffer. One copy is
/// evaluated by one partition, one thread at a time.
class LocalBlockScratch {
  public:
    LocalBlockScratch() = default;
    LocalBlockScratch(const LocalBlockScratch &) noexcept {}
    LocalBlockScratch &operator=(const LocalBlockScratch &) noexcept { return *this; }
    LocalBlockScratch(LocalBlockScratch &&) noexcept = default;
    LocalBlockScratch &operator=(LocalBlockScratch &&) noexcept = default;

    /// @p n doubles, allocated on the first call and reused after it.
    double *get(std::size_t n) const {
        if (buffer_.size() < n) {
            buffer_.assign(n, 0.0);
        }
        return buffer_.data();
    }

  private:
    mutable std::vector<double> buffer_;
};

/// Calls @p f with N doubles of block buffer: a stack array when N doubles fit
/// kLocalBlockStackBudget, else @p scratch's buffer.
template <std::size_t N, class F>
inline void with_block_buffer(const LocalBlockScratch &scratch, F &&f) {
    if constexpr (N * sizeof(double) <= kLocalBlockStackBudget) {
        std::array<double, N> store;
        f(store.data());
    } else {
        f(scratch.get(N));
    }
}

// ---- near-miss diagnostics ----
//
// The shape probes the two piece concepts below are built from, one requirement
// each, so a piece that misses exactly one is told which. Each is probed const
// (what the layout calls) and non-const (what tells "not const" from "absent").

template <class P>
concept LocalShaped = std::is_class_v<P> && requires { P::kInputs; };
template <class P>
concept DeclaresOutputs = requires { P::kOutputs; };

template <class P>
concept ConstraintSizes = requires {
    { P::kInputs } -> std::convertible_to<Index>;
    { P::kOutputs } -> std::convertible_to<Index>;
};
template <class P>
concept ObjectiveSizes = requires {
    { P::kInputs } -> std::convertible_to<Index>;
};

template <class P>
concept ConstConstraintValue = requires(const P &p, ConstVecRef v, VecRef out) { p.value(v, out); };
template <class P>
concept AnyConstraintValue = requires(P &p, ConstVecRef v, VecRef out) { p.value(v, out); };
template <class P>
concept ConstJacobian = requires(const P &p, ConstVecRef v, MatRef J) { p.jacobian(v, J); };
template <class P>
concept AnyJacobian = requires(P &p, ConstVecRef v, MatRef J) { p.jacobian(v, J); };
template <class P>
concept ConstConstraintHessian =
    requires(const P &p, ConstVecRef v, MatRef H) { p.hessian(v, v, H); };
template <class P>
concept AnyConstraintHessian = requires(P &p, ConstVecRef v, MatRef H) { p.hessian(v, v, H); };

template <class P>
concept ConstObjectiveValue = requires(const P &p, ConstVecRef v) {
    { p.value(v) } -> std::convertible_to<double>;
};
template <class P>
concept AnyObjectiveValue = requires(P &p, ConstVecRef v) { p.value(v); };
template <class P>
concept ConstGradient = requires(const P &p, ConstVecRef v, VecRef g) { p.gradient(v, g); };
template <class P>
concept AnyGradient = requires(P &p, ConstVecRef v, VecRef g) { p.gradient(v, g); };
template <class P>
concept ConstObjectiveHessian = requires(const P &p, ConstVecRef v, MatRef H) { p.hessian(v, H); };
template <class P>
concept AnyObjectiveHessian = requires(P &p, ConstVecRef v, MatRef H) { p.hessian(v, H); };

} // namespace detail

/// A local-block constraint piece: kInputs and kOutputs, value() and jacobian(),
/// and hessian() unless the piece declares kHessianPattern = kNoHessian.
template <class P>
concept LocalConstraintPiece =
    std::is_class_v<P> && detail::ConstraintSizes<P> && detail::ConstConstraintValue<P> &&
    detail::ConstJacobian<P> && (detail::DeclaresNoHessian<P> || detail::ConstConstraintHessian<P>);

/// A local-block objective piece: kInputs (and no kOutputs), a scalar value(),
/// gradient(), and hessian() unless the piece declares kHessianPattern =
/// kNoHessian.
template <class P>
concept LocalObjectivePiece =
    std::is_class_v<P> && !detail::DeclaresOutputs<P> && detail::ObjectiveSizes<P> &&
    detail::ConstObjectiveValue<P> && detail::ConstGradient<P> &&
    (detail::DeclaresNoHessian<P> || detail::ConstObjectiveHessian<P>);

namespace detail {

/// The one authored refusal for a local-shaped type that is not a constraint
/// piece, naming the first requirement it misses.
template <class P> constexpr void diagnose_constraint_piece() {
    constexpr bool f = adapter_dependent_false_v<P>;
    if constexpr (!ConstraintSizes<P>) {
        static_assert(f, "hven local block: kInputs and kOutputs must be static constexpr "
                         "integer constants (static constexpr hven::Index kInputs = ..., "
                         "kOutputs = ...)");
    } else if constexpr (!ConstConstraintValue<P>) {
        if constexpr (AnyConstraintValue<P>) {
            static_assert(f, "hven local block: value(v, out) must be a const member: void "
                             "value(hven::ConstVecRef v, hven::VecRef out) const");
        } else if constexpr (ConstObjectiveValue<P> || AnyObjectiveValue<P>) {
            static_assert(f, "hven local block: this piece declares kOutputs, which makes it a "
                             "constraint piece, but its value(v) is an objective piece's. An "
                             "objective piece declares kInputs only (no kOutputs); a constraint "
                             "piece provides void value(hven::ConstVecRef v, hven::VecRef out) "
                             "const");
        } else {
            static_assert(f, "hven local block: a constraint piece provides void "
                             "value(hven::ConstVecRef v, hven::VecRef out) const");
        }
    } else if constexpr (!ConstJacobian<P>) {
        if constexpr (AnyJacobian<P>) {
            static_assert(f, "hven local block: jacobian(v, J) must be a const member: void "
                             "jacobian(hven::ConstVecRef v, hven::MatRef J) const");
        } else {
            static_assert(f, "hven local block: a constraint piece provides void "
                             "jacobian(hven::ConstVecRef v, hven::MatRef J) const");
        }
    } else if constexpr (!DeclaresNoHessian<P> && !ConstConstraintHessian<P>) {
        if constexpr (AnyConstraintHessian<P>) {
            static_assert(f, "hven local block: hessian(v, lambda, H) must be a const member: "
                             "void hessian(hven::ConstVecRef v, hven::ConstVecRef lambda, "
                             "hven::MatRef H) const");
        } else if constexpr (AnyObjectiveHessian<P>) {
            static_assert(f, "hven local block: a constraint piece's hessian takes the "
                             "multipliers: void hessian(hven::ConstVecRef v, hven::ConstVecRef "
                             "lambda, hven::MatRef H) const, which writes sum_j lambda_j d2c_j");
        } else {
            static_assert(f, "hven local block: a constraint piece provides void "
                             "hessian(hven::ConstVecRef v, hven::ConstVecRef lambda, "
                             "hven::MatRef H) const, or declares static constexpr auto "
                             "kHessianPattern = hven::kNoHessian");
        }
    } else {
        static_assert(f, "hven local block: this type is not a local-block constraint piece");
    }
}

/// The one authored refusal for a local-shaped type without kOutputs that is not
/// an objective piece, naming the first requirement it misses.
template <class P> constexpr void diagnose_objective_piece() {
    constexpr bool f = adapter_dependent_false_v<P>;
    if constexpr (!ObjectiveSizes<P>) {
        static_assert(f, "hven local block: kInputs must be a static constexpr integer "
                         "constant (static constexpr hven::Index kInputs = ...)");
    } else if constexpr (!ConstObjectiveValue<P>) {
        if constexpr (ConstConstraintValue<P> || AnyConstraintValue<P>) {
            static_assert(f, "hven local block: this piece has a constraint piece's value(v, "
                             "out) but declares no kOutputs. A constraint piece declares "
                             "static constexpr hven::Index kOutputs; an objective piece "
                             "provides double value(hven::ConstVecRef v) const");
        } else if constexpr (AnyObjectiveValue<P>) {
            static_assert(f, "hven local block: value(v) must be a const member returning the "
                             "term's value: double value(hven::ConstVecRef v) const");
        } else {
            static_assert(f, "hven local block: an objective piece provides double "
                             "value(hven::ConstVecRef v) const");
        }
    } else if constexpr (!ConstGradient<P>) {
        if constexpr (AnyGradient<P>) {
            static_assert(f, "hven local block: gradient(v, g) must be a const member: void "
                             "gradient(hven::ConstVecRef v, hven::VecRef g) const");
        } else {
            static_assert(f, "hven local block: an objective piece provides void "
                             "gradient(hven::ConstVecRef v, hven::VecRef g) const");
        }
    } else if constexpr (!DeclaresNoHessian<P> && !ConstObjectiveHessian<P>) {
        if constexpr (AnyObjectiveHessian<P>) {
            static_assert(f, "hven local block: hessian(v, H) must be a const member: void "
                             "hessian(hven::ConstVecRef v, hven::MatRef H) const");
        } else if constexpr (AnyConstraintHessian<P>) {
            static_assert(f, "hven local block: an objective piece's hessian takes no "
                             "multipliers: void hessian(hven::ConstVecRef v, hven::MatRef H) "
                             "const");
        } else {
            static_assert(f, "hven local block: an objective piece provides void "
                             "hessian(hven::ConstVecRef v, hven::MatRef H) const, or declares "
                             "static constexpr auto kHessianPattern = "
                             "hven::kNoHessian");
        }
    } else {
        static_assert(f, "hven local block: this type is not a local-block objective piece");
    }
}

/// The checks every local-block piece passes before it is stored, in one place so
/// each failure is the only error: copy-constructible, no hook, a const name().
/// @return whether the piece may be stored.
template <class P> constexpr bool local_piece_storable() {
    constexpr bool hooked = ConstraintPackHook<P> || ObjectivePackHook<P>;
    static_assert(!hooked,
                  "hven local block: this type is a local-block piece AND carries a "
                  "pack_into_constraint_interface / pack_into_objective_interface hook, so it "
                  "has two meanings. Drop the hook (the local-block layer stores the piece "
                  "itself), or drop kInputs and write it as a raw-protocol piece");
    static_assert(hooked || std::is_copy_constructible_v<P>,
                  "hven local block: a piece must be copy-constructible: the layout copies it "
                  "once per partition and evaluates the copies concurrently. Hold state the "
                  "copies share through a std::shared_ptr, to data that is safe to read "
                  "concurrently");
    constexpr bool copyable = std::is_copy_constructible_v<P>;
    static_assert(hooked || !copyable || HasLocalName<P> || !HasAnyName<P>,
                  "hven local block: name() must be a const member returning a string: "
                  "std::string name() const");
    return !hooked && copyable && (HasLocalName<P> || !HasAnyName<P>);
}

} // namespace detail

/// The raw-protocol constraint piece that carries a local-block constraint piece:
/// it gathers each application's inputs, calls the piece's blocks, and writes
/// them through the claimed slots. Claims per application: the Jacobian pattern's
/// entries, then the Hessian pattern's, contiguous.
///
/// One copy is evaluated by one partition at a time (a large piece's scratch is
/// the copy's own); the layout makes one copy per partition.
template <class P> struct LocalConstraintBlock {
    static constexpr int kIn = static_cast<int>(P::kInputs);
    static constexpr int kOut = static_cast<int>(P::kOutputs);
    static_assert(kIn >= 1, "hven local block: a constraint piece needs kInputs >= 1");
    static_assert(kOut >= 1, "hven local block: a constraint piece needs kOutputs >= 1");
    static_assert(std::is_copy_constructible_v<P>,
                  "hven local block: a piece must be copy-constructible: the layout copies it "
                  "once per partition");

    static_assert(detail::HasJacobianPattern<P> || !detail::NamesJacobianPattern<P>,
                  "hven local block: kJacobianPattern must be a constexpr array of "
                  "hven::BlockEntry (std::array or a C array)");
    static_assert(detail::HasHessianPattern<P> || !detail::NamesHessianPattern<P>,
                  "hven local block: kHessianPattern must be a constexpr array of "
                  "hven::BlockEntry (std::array or a C array), or "
                  "hven::kNoHessian");
    static_assert(detail::declared_jacobian_in_range<P>(),
                  "hven local block: a kJacobianPattern entry lies outside the kOutputs x "
                  "kInputs block");
    static_assert(detail::declared_jacobian_unique<P>(),
                  "hven local block: kJacobianPattern lists an entry more than once");
    static_assert(detail::declared_hessian_in_range<P>(),
                  "hven local block: a kHessianPattern entry lies outside the kInputs x "
                  "kInputs block");
    static_assert(detail::declared_hessian_lower<P>(),
                  "hven local block: a kHessianPattern entry lies above the diagonal; list the "
                  "lower triangle (row >= col) only");
    static_assert(detail::declared_hessian_unique<P>(),
                  "hven local block: kHessianPattern lists an entry more than once");

    static constexpr int kJacobianCount = detail::jacobian_entry_count<P>();
    static constexpr int kHessianCount = detail::hessian_entry_count<P>();

    /// Doubles one application's blocks take: inputs, outputs, the Jacobian, the
    /// multipliers and (when the piece has one) the Hessian.
    static constexpr std::size_t kBufferDoubles =
        static_cast<std::size_t>(kIn + kOut + kOut * kIn + kOut) +
        static_cast<std::size_t>(kHessianCount > 0 ? kIn * kIn : 0);
    /// Whether the blocks live on the stack (else in the copy's scratch).
    static constexpr bool kBuffersOnStack =
        kBufferDoubles * sizeof(double) <= kLocalBlockStackBudget;

    P piece;
    detail::LocalBlockScratch scratch{};

    std::string name() const {
        if constexpr (detail::HasLocalName<P>) {
            return piece.name();
        } else {
            return "local constraint block (kInputs " + std::to_string(kIn) + ", kOutputs " +
                   std::to_string(kOut) + ")";
        }
    }
    int input_rows() const { return kIn; }
    int output_rows() const { return kOut; }
    bool thread_safe() const { return true; }

    void constraints(const Eigen::Ref<const Eigen::VectorXd> &X, Eigen::Ref<Eigen::VectorXd> FX,
                     const SolverIndexingData &d) const {
        with_buffers([&](Buffers &b) {
            for (int V = 0; V < d.num_appl(); V++) {
                b.gather(X, d, V);
                b.value(piece);
                b.write_values(FX, d, V);
            }
        });
    }

    void constraints_adjointgradient(const Eigen::Ref<const Eigen::VectorXd> &X,
                                     const Eigen::Ref<const Eigen::VectorXd> &L,
                                     Eigen::Ref<Eigen::VectorXd> FX,
                                     Eigen::Ref<Eigen::VectorXd> AGX,
                                     const SolverIndexingData &d) const {
        with_buffers([&](Buffers &b) {
            for (int V = 0; V < d.num_appl(); V++) {
                b.gather(X, d, V);
                b.value(piece);
                b.write_values(FX, d, V);
                b.jacobian(piece);
                b.write_adjoint_gradient(L, AGX, d, V);
            }
        });
    }

    void constraints_jacobian(const Eigen::Ref<const Eigen::VectorXd> &X,
                              Eigen::Ref<Eigen::VectorXd> FX,
                              Eigen::SparseMatrix<double, Eigen::RowMajor> &kkt,
                              Eigen::Ref<Eigen::VectorXi> locations,
                              Eigen::Ref<Eigen::VectorXi> clashes, std::vector<std::mutex> &locks,
                              const SolverIndexingData &d) const {
        with_buffers([&](Buffers &b) {
            for (int V = 0; V < d.num_appl(); V++) {
                b.gather(X, d, V);
                b.value(piece);
                b.write_values(FX, d, V);
                b.jacobian(piece);
                b.scatter_jacobian(kkt, locations, clashes, locks, d, V);
            }
        });
    }

    void constraints_jacobian_adjointgradient(
        const Eigen::Ref<const Eigen::VectorXd> &X, const Eigen::Ref<const Eigen::VectorXd> &L,
        Eigen::Ref<Eigen::VectorXd> FX, Eigen::Ref<Eigen::VectorXd> AGX,
        Eigen::SparseMatrix<double, Eigen::RowMajor> &kkt, Eigen::Ref<Eigen::VectorXi> locations,
        Eigen::Ref<Eigen::VectorXi> clashes, std::vector<std::mutex> &locks,
        const SolverIndexingData &d) const {
        with_buffers([&](Buffers &b) {
            for (int V = 0; V < d.num_appl(); V++) {
                b.gather(X, d, V);
                b.value(piece);
                b.write_values(FX, d, V);
                b.jacobian(piece);
                b.write_adjoint_gradient(L, AGX, d, V);
                b.scatter_jacobian(kkt, locations, clashes, locks, d, V);
            }
        });
    }

    void constraints_jacobian_adjointgradient_adjointhessian(
        const Eigen::Ref<const Eigen::VectorXd> &X, const Eigen::Ref<const Eigen::VectorXd> &L,
        Eigen::Ref<Eigen::VectorXd> FX, Eigen::Ref<Eigen::VectorXd> AGX,
        Eigen::SparseMatrix<double, Eigen::RowMajor> &kkt, Eigen::Ref<Eigen::VectorXi> locations,
        Eigen::Ref<Eigen::VectorXi> clashes, std::vector<std::mutex> &locks,
        const SolverIndexingData &d) const {
        with_buffers([&](Buffers &b) {
            for (int V = 0; V < d.num_appl(); V++) {
                b.gather(X, d, V);
                b.value(piece);
                b.write_values(FX, d, V);
                b.jacobian(piece);
                b.write_adjoint_gradient(L, AGX, d, V);
                b.scatter_jacobian(kkt, locations, clashes, locks, d, V);
                if constexpr (kHessianCount > 0) {
                    b.hessian(piece, L, d, V);
                    b.scatter_hessian(kkt, locations, clashes, locks, d, V);
                }
            }
        });
    }

    void get_kkt_space(Eigen::Ref<Eigen::VectorXi> rows, Eigen::Ref<Eigen::VectorXi> cols,
                       int &free_slot, int con_offset, bool do_jac, bool do_hess,
                       SolverIndexingData &d) {
        // The Hessian scatter addresses its slots after the Jacobian's, so the
        // two domains are claimed together or the piece has no Hessian slots.
        if (kHessianCount > 0 && do_jac != do_hess) {
            detail::throw_local_block_split_claim(name());
        }
        d.inner_kkt_starts.resize(d.num_appl());
        for (int V = 0; V < d.num_appl(); V++) {
            d.inner_kkt_starts[V] = free_slot;
            if (do_jac) {
                detail::for_each_jacobian_entry<P>([&](int er, int ec) {
                    const int col = d.v_scatter_loc(ec, V);
                    rows[free_slot] = (col < 0) ? -1 : d.c_loc(er, V) + con_offset;
                    cols[free_slot] = col;
                    free_slot++;
                });
            }
            if (do_hess) {
                detail::for_each_hessian_entry<P>([&](int er, int ec) {
                    const int r = d.v_scatter_loc(er, V);
                    const int c = d.v_scatter_loc(ec, V);
                    const bool eliminated = (r < 0 || c < 0);
                    rows[free_slot] = eliminated ? -1 : r;
                    cols[free_slot] = eliminated ? -1 : c;
                    free_slot++;
                });
            }
        }
    }

    int num_kkt_elements(bool do_jac, bool do_hess) const {
        return (do_jac ? kJacobianCount : 0) + (do_hess ? kHessianCount : 0);
    }

  private:
    /// One application's blocks, carved out of one buffer of kBufferDoubles.
    struct Buffers {
        double *v;
        double *out;
        double *jac;
        double *lambda;
        double *hess;

        explicit Buffers(double *base)
            : v(base), out(v + kIn), jac(out + kOut), lambda(jac + kOut * kIn),
              hess(lambda + kOut) {}

        void gather(const Eigen::Ref<const Eigen::VectorXd> &X, const SolverIndexingData &d,
                    int V) {
            for (int i = 0; i < kIn; i++) {
                v[i] = X[d.v_loc(i, V)];
            }
        }
        void value(const P &p) {
            std::fill_n(out, kOut, 0.0);
            const Eigen::Map<const Eigen::VectorXd> vm(v, kIn);
            Eigen::Map<Eigen::VectorXd> om(out, kOut);
            p.value(vm, om);
        }
        void jacobian(const P &p) {
            std::fill_n(jac, kOut * kIn, 0.0);
            const Eigen::Map<const Eigen::VectorXd> vm(v, kIn);
            Eigen::Map<Eigen::MatrixXd> jm(jac, kOut, kIn);
            p.jacobian(vm, jm);
        }
        void hessian(const P &p, const Eigen::Ref<const Eigen::VectorXd> &L,
                     const SolverIndexingData &d, int V) {
            for (int j = 0; j < kOut; j++) {
                lambda[j] = L[d.c_loc(j, V)];
            }
            std::fill_n(hess, kIn * kIn, 0.0);
            const Eigen::Map<const Eigen::VectorXd> vm(v, kIn);
            const Eigen::Map<const Eigen::VectorXd> lm(lambda, kOut);
            Eigen::Map<Eigen::MatrixXd> hm(hess, kIn, kIn);
            p.hessian(vm, lm, hm);
        }
        double jac_at(int r, int c) const { return jac[c * kOut + r]; }
        double hess_at(int r, int c) const { return hess[c * kIn + r]; }
        void write_values(Eigen::Ref<Eigen::VectorXd> FX, const SolverIndexingData &d,
                          int V) const {
            const int start = d.inner_constraint_starts[V];
            for (int j = 0; j < kOut; j++) {
                FX[start + j] = out[j];
            }
        }
        void write_adjoint_gradient(const Eigen::Ref<const Eigen::VectorXd> &L,
                                    Eigen::Ref<Eigen::VectorXd> AGX, const SolverIndexingData &d,
                                    int V) const {
            const int start = d.inner_gradient_starts[V];
            detail::for_each_jacobian_entry<P>(
                [&](int er, int ec) { AGX[start + ec] += L[d.c_loc(er, V)] * jac_at(er, ec); });
        }
        void scatter_jacobian(Eigen::SparseMatrix<double, Eigen::RowMajor> &kkt,
                              const Eigen::Ref<Eigen::VectorXi> &locations,
                              const Eigen::Ref<Eigen::VectorXi> &clashes,
                              std::vector<std::mutex> &locks, const SolverIndexingData &d,
                              int V) const {
            int slot = d.inner_kkt_starts[V];
            // The layout sets unique_constraints_ false on a piece copy one of whose
            // rows another partition also writes (mark_shared_constraint_rows); only
            // then can two partitions write one Jacobian entry.
            const bool check = !d.unique_constraints;
            detail::for_each_jacobian_entry<P>([&](int er, int ec) {
                const int col = d.v_scatter_loc(ec, V);
                if (col >= 0) {
                    detail::local_block_kkt_add(kkt, locations, clashes, locks, slot, col,
                                                jac_at(er, ec), check);
                }
                slot++;
            });
        }
        void scatter_hessian(Eigen::SparseMatrix<double, Eigen::RowMajor> &kkt,
                             const Eigen::Ref<Eigen::VectorXi> &locations,
                             const Eigen::Ref<Eigen::VectorXi> &clashes,
                             std::vector<std::mutex> &locks, const SolverIndexingData &d,
                             int V) const {
            int slot = d.inner_kkt_starts[V] + kJacobianCount;
            detail::for_each_hessian_entry<P>([&](int er, int ec) {
                const int r = d.v_scatter_loc(er, V);
                const int c = d.v_scatter_loc(ec, V);
                if (r >= 0 && c >= 0) {
                    // An off-diagonal block entry whose two inputs name one
                    // variable lands on that variable's diagonal twice, once
                    // from each triangle.
                    const double w = (r == c && er != ec) ? 2.0 * hess_at(er, ec) : hess_at(er, ec);
                    detail::local_block_kkt_add(kkt, locations, clashes, locks, slot,
                                                kkt_canonical_lock_col(r, c), w, true);
                }
                slot++;
            });
        }
    };

    template <class F> void with_buffers(F &&f) const {
        detail::with_block_buffer<kBufferDoubles>(scratch, [&](double *base) {
            Buffers b(base);
            f(b);
        });
    }
};

/// The raw-protocol objective piece that carries a local-block objective piece.
/// Claims per application: the Hessian pattern's entries.
///
/// One copy is evaluated by one partition at a time (a large piece's scratch is
/// the copy's own); the layout makes one copy per partition.
template <class P> struct LocalObjectiveBlock {
    static constexpr int kIn = static_cast<int>(P::kInputs);
    static_assert(kIn >= 1, "hven local block: an objective piece needs kInputs >= 1");
    static_assert(std::is_copy_constructible_v<P>,
                  "hven local block: a piece must be copy-constructible: the layout copies it "
                  "once per partition");

    static_assert(detail::HasHessianPattern<P> || !detail::NamesHessianPattern<P>,
                  "hven local block: kHessianPattern must be a constexpr array of "
                  "hven::BlockEntry (std::array or a C array), or "
                  "hven::kNoHessian");
    static_assert(detail::declared_hessian_in_range<P>(),
                  "hven local block: a kHessianPattern entry lies outside the kInputs x "
                  "kInputs block");
    static_assert(detail::declared_hessian_lower<P>(),
                  "hven local block: a kHessianPattern entry lies above the diagonal; list the "
                  "lower triangle (row >= col) only");
    static_assert(detail::declared_hessian_unique<P>(),
                  "hven local block: kHessianPattern lists an entry more than once");

    static constexpr int kHessianCount = detail::hessian_entry_count<P>();

    /// Doubles one application's blocks take: inputs, the gradient and (when the
    /// piece has one) the Hessian.
    static constexpr std::size_t kBufferDoubles =
        static_cast<std::size_t>(2 * kIn) +
        static_cast<std::size_t>(kHessianCount > 0 ? kIn * kIn : 0);
    /// Whether the blocks live on the stack (else in the copy's scratch).
    static constexpr bool kBuffersOnStack =
        kBufferDoubles * sizeof(double) <= kLocalBlockStackBudget;

    P piece;
    detail::LocalBlockScratch scratch{};

    std::string name() const {
        if constexpr (detail::HasLocalName<P>) {
            return piece.name();
        } else {
            return "local objective block (kInputs " + std::to_string(kIn) + ")";
        }
    }
    int input_rows() const { return kIn; }
    bool thread_safe() const { return true; }

    void objective(double scale, const Eigen::Ref<const Eigen::VectorXd> &X, double &val,
                   const SolverIndexingData &d) const {
        detail::with_block_buffer<kBufferDoubles>(scratch, [&](double *v) {
            for (int V = 0; V < d.num_appl(); V++) {
                gather(v, X, d, V);
                val += scale * piece.value(Eigen::Map<const Eigen::VectorXd>(v, kIn));
            }
        });
    }

    void objective_gradient(double scale, const Eigen::Ref<const Eigen::VectorXd> &X, double &val,
                            Eigen::Ref<Eigen::VectorXd> GX, const SolverIndexingData &d) const {
        detail::with_block_buffer<kBufferDoubles>(scratch, [&](double *v) {
            double *g = v + kIn;
            for (int V = 0; V < d.num_appl(); V++) {
                gather(v, X, d, V);
                value_gradient(v, g, scale, val, GX, d, V);
            }
        });
    }

    void objective_gradient_hessian(double scale, const Eigen::Ref<const Eigen::VectorXd> &X,
                                    double &val, Eigen::Ref<Eigen::VectorXd> GX,
                                    Eigen::SparseMatrix<double, Eigen::RowMajor> &kkt,
                                    Eigen::Ref<Eigen::VectorXi> locations,
                                    Eigen::Ref<Eigen::VectorXi> clashes,
                                    std::vector<std::mutex> &locks,
                                    const SolverIndexingData &d) const {
        detail::with_block_buffer<kBufferDoubles>(scratch, [&](double *v) {
            double *g = v + kIn;
            [[maybe_unused]] double *h = g + kIn;
            for (int V = 0; V < d.num_appl(); V++) {
                gather(v, X, d, V);
                value_gradient(v, g, scale, val, GX, d, V);
                if constexpr (kHessianCount > 0) {
                    std::fill_n(h, kIn * kIn, 0.0);
                    Eigen::Map<Eigen::MatrixXd> hm(h, kIn, kIn);
                    piece.hessian(Eigen::Map<const Eigen::VectorXd>(v, kIn), hm);
                    int slot = d.inner_kkt_starts[V];
                    detail::for_each_hessian_entry<P>([&](int er, int ec) {
                        const int r = d.v_scatter_loc(er, V);
                        const int c = d.v_scatter_loc(ec, V);
                        if (r >= 0 && c >= 0) {
                            const double entry = h[ec * kIn + er];
                            const double w = (r == c && er != ec) ? 2.0 * entry : entry;
                            detail::local_block_kkt_add(kkt, locations, clashes, locks, slot,
                                                        kkt_canonical_lock_col(r, c), scale * w,
                                                        true);
                        }
                        slot++;
                    });
                }
            }
        });
    }

    void get_kkt_space(Eigen::Ref<Eigen::VectorXi> rows, Eigen::Ref<Eigen::VectorXi> cols,
                       int &free_slot, int /*con_offset*/, bool /*do_jac*/, bool do_hess,
                       SolverIndexingData &d) {
        d.inner_kkt_starts.resize(d.num_appl());
        for (int V = 0; V < d.num_appl(); V++) {
            d.inner_kkt_starts[V] = free_slot;
            if (do_hess) {
                detail::for_each_hessian_entry<P>([&](int er, int ec) {
                    const int r = d.v_scatter_loc(er, V);
                    const int c = d.v_scatter_loc(ec, V);
                    const bool eliminated = (r < 0 || c < 0);
                    rows[free_slot] = eliminated ? -1 : r;
                    cols[free_slot] = eliminated ? -1 : c;
                    free_slot++;
                });
            }
        }
    }

    int num_kkt_elements(bool /*do_jac*/, bool do_hess) const {
        return do_hess ? kHessianCount : 0;
    }

  private:
    static void gather(double *v, const Eigen::Ref<const Eigen::VectorXd> &X,
                       const SolverIndexingData &d, int V) {
        for (int i = 0; i < kIn; i++) {
            v[i] = X[d.v_loc(i, V)];
        }
    }

    void value_gradient(const double *v, double *g, double scale, double &val,
                        Eigen::Ref<Eigen::VectorXd> GX, const SolverIndexingData &d, int V) const {
        const Eigen::Map<const Eigen::VectorXd> vm(v, kIn);
        val += scale * piece.value(vm);
        std::fill_n(g, kIn, 0.0);
        Eigen::Map<Eigen::VectorXd> gm(g, kIn);
        piece.gradient(vm, gm);
        const int start = d.inner_gradient_starts[V];
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
struct SolverInterfaceAdapter<P> {
    static constexpr bool registered = true;

    static void install_constraint(const P &p, ConstraintInterface &ci) {
        if constexpr (detail::local_piece_storable<P>()) {
            ci.storage.emplace<ConstraintModel<LocalConstraintBlock<P>>>(
                LocalConstraintBlock<P>{p});
        }
    }
    static void install_objective(const P &, ObjectiveInterface &) {
        static_assert(adapter_dependent_false_v<P>,
                      "hven local block: this piece declares kOutputs, so it is a CONSTRAINT "
                      "piece; add it with add_equality or add_inequality. An objective piece "
                      "declares kInputs only, with double value(hven::ConstVecRef v) const and "
                      "void gradient(hven::ConstVecRef v, hven::VecRef g) const");
    }
};

/// A local-block objective piece enters ObjectiveInterface as its
/// LocalObjectiveBlock. It is not a constraint.
template <class P>
    requires LocalObjectivePiece<P>
struct SolverInterfaceAdapter<P> {
    static constexpr bool registered = true;

    static void install_constraint(const P &, ConstraintInterface &) {
        static_assert(adapter_dependent_false_v<P>,
                      "hven local block: this piece declares no kOutputs, so it is an OBJECTIVE "
                      "piece; add it with add_objective. A constraint piece declares kInputs and "
                      "kOutputs, with void value(hven::ConstVecRef v, hven::VecRef out) const "
                      "and void jacobian(hven::ConstVecRef v, hven::MatRef J) const");
    }
    static void install_objective(const P &p, ObjectiveInterface &oi) {
        if constexpr (detail::local_piece_storable<P>()) {
            oi.storage.emplace<ObjectiveModel<LocalObjectiveBlock<P>>>(LocalObjectiveBlock<P>{p});
        }
    }
};

/// A class declaring kInputs that is neither kind of local-block piece: refused on
/// both routes with one authored message naming the first requirement it misses.
/// Never stored, and never handed to the default adapter.
template <class P>
    requires(detail::LocalShaped<P> && !LocalConstraintPiece<P> && !LocalObjectivePiece<P>)
struct SolverInterfaceAdapter<P> {
    static constexpr bool registered = true;

    static void install_constraint(const P &, ConstraintInterface &) { diagnose(); }
    static void install_objective(const P &, ObjectiveInterface &) { diagnose(); }

  private:
    static void diagnose() {
        if constexpr (detail::DeclaresOutputs<P>) {
            detail::diagnose_constraint_piece<P>();
        } else {
            detail::diagnose_objective_piece<P>();
        }
    }
};

} // namespace hven
