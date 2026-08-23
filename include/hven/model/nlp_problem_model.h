// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// nlp_problem_model.h — the conversion from the triplet-shaped convenience
// problem (model/nlp_problem.h) to the native model contract
// (model/nlp_model.h).
//
// NLPProblem states a problem the way Ipopt's TNLP does: one two-sided bound
// pair per constraint row, a single flat Jacobian value array over a declared
// (row, col) structure, and a lower-triangle Lagrangian Hessian over a second
// such structure. NlpModel states the same problem the way this library's
// engines consume it: separated cE(x) = 0 and cI(x) <= 0 blocks, a sparse
// Jacobian per block, and an upper-triangle Lagrangian Hessian, all returned
// whole by value.
//
// NlpProblemModel is the one object that performs that conversion, and it is
// the only route by which an NLPProblem reaches an engine. Everything the
// conversion decides -- which solver rows a declared row becomes, which sign
// each carries, how the two multiplier shapes correspond -- is decided here,
// once, at construction, and is readable from here rather than inferred from a
// consumer's behaviour.

#include <memory>
#include <vector>

#include <Eigen/Core>
#include <Eigen/SparseCore>

#include "hven/core/types.h"
#include "hven/detail/interior/typedefs/eigen_types.h"
#include "hven/model/nlp_model.h"
#include "hven/model/nlp_problem.h"

namespace hven::solvers {

/// How one declared constraint row is realized in the native model's row spaces.
enum class NLPRowKind { Equality, UpperBounded, LowerBounded, Range, Free };

/// Setup-time classification of the declared constraint rows against their
/// bounds: which native rows each declared row becomes, in declaration order.
/// A Range row (two finite, unequal bounds) becomes two inequality rows, the
/// upper part first, then the negated lower part.
struct NLPRowClassification {
    std::vector<NLPRowKind> kinds_; ///< one entry per declared row
    Eigen::VectorXi eq_row_;        ///< declared row -> equality row, -1 if none
    Eigen::VectorXi iq_upper_row_;  ///< declared row -> inequality row (upper part), -1
    Eigen::VectorXi iq_lower_row_;  ///< declared row -> inequality row (negated lower), -1
    int num_eq_ = 0;
    int num_iq_ = 0;

    /// @brief Classifies every declared row against its bound pair.
    /// @param gl Declared lower bounds, one per row.
    /// @param gu Declared upper bounds, one per row.
    /// @return The classification, with the native row numbering assigned.
    /// @throws std::invalid_argument on mismatched sizes, a NaN bound, an
    ///         inverted pair, or an equality declared at infinity.
    static NLPRowClassification classify(ConstEigenRef<Eigen::VectorXd> gl,
                                         ConstEigenRef<Eigen::VectorXd> gu);
};

/// @brief The triplet-shaped convenience problem, as a native model.
///
/// Bounds are verbatim in both directions. Nothing here rescales, clips, or
/// reinterprets a declared bound: -inf / +inf mean unbounded on that side, and
/// every finite value is a real bound however large. lower() and upper() hand
/// back exactly what NLPProblem::bounds declared, and the constraint-row
/// bounds enter the residuals as the shifts they are. NLPProblem's own
/// migration note states what this differs from.
///
/// Row conversion. A declared row's kind decides its native rows and the sign
/// each carries, so that every inequality reads cI(x) <= 0:
///
///   Equality      (gl == gu, finite)  -> one equality row,   g(x) - gl
///   UpperBounded  (gl == -inf)        -> one inequality row, g(x) - gu
///   LowerBounded  (gu == +inf)        -> one inequality row, gl - g(x)
///   Range         (both finite)       -> two inequality rows, g(x) - gu then gl - g(x)
///   Free          (both infinite)     -> no row
///
/// Multiplier shapes. NLPProblem carries one multiplier per declared row, in
/// Ipopt's sign convention (L = obj_factor*f + lambda^T g). NlpModel carries a
/// lambda_e per equality row and a lambda_i >= 0 per inequality row, in the
/// convention nlp_model.h states. The two correspond row by row, by kind:
///
///   Equality      lambda(r) =  lambda_e(eq_row(r))
///   UpperBounded  lambda(r) =  lambda_i(iq_upper_row(r))
///   LowerBounded  lambda(r) = -lambda_i(iq_lower_row(r))
///   Range         lambda(r) =  lambda_i(iq_upper_row(r)) - lambda_i(iq_lower_row(r))
///   Free          lambda(r) =  0
///
/// compose_user_multipliers applies that map; split_user_multipliers sends a
/// declared multiplier back the other way, a Range row's signed value going to
/// whichever of its two rows that sign names (the other taking zero) and a
/// Free row's value being dropped.
///
/// Round trips, exactly. compose_user_multipliers(split_user_multipliers(l))
/// == l for every declared l whose Free rows are zero: each kind's split is
/// undone by its compose, and a Free row is the only place a declared value
/// has no native image to come back from.
///
/// The other order is not an identity. compose is not injective on the native
/// pairs -- a Range row's two multipliers reach the declared space only as
/// their difference -- so split_user_multipliers(compose_user_multipliers(le,
/// li)) generally differs from (le, li): a Range row carrying
/// (upper, lower) = (3, 4) composes to -1 and splits back to (0, 1). It is an
/// identity only where every Range row has at most one nonzero side.
///
/// Duplicate (row, col) entries in either declared structure are legal and
/// their values are summed, which is where they are summed: the native
/// matrices carry one entry per distinct coordinate.
///
/// Evaluation callbacks are pure, so g(x) and the Jacobian values are cached
/// per iterate: eval_ce and eval_ci share one NLPProblem::eval_g call, and
/// eval_jac_e and eval_jac_i share one NLPProblem::eval_jac call.
///
/// Thread safety. None: the caches above and the storage below are mutable
/// members this model rewrites on every evaluation, so two threads evaluating
/// one instance race each other. One instance per evaluating thread, or one
/// evaluating thread per instance. Const-qualified methods are not an
/// exception -- the mutation is behind `mutable`, which is what makes the
/// caching possible at all.
///
/// Storage. The native patterns are laid once at construction and only values
/// are written afterwards, so an evaluation after the first allocates nothing.
/// The in-place methods write straight into the caller's destination, giving it
/// the laid pattern the first time they see it and filling its value array on
/// every call after; the by-value methods do the same into a member of this
/// model and hand back a copy of it, as their signatures require.
class NlpProblemModel final : public NlpModel {
  public:
    /// @brief Converts a declared problem, validating its declaration.
    /// @param problem The problem to convert; must not be null.
    /// @throws std::invalid_argument on a null problem, a non-positive
    ///         variable count, a negative or inconsistent nonzero count, a
    ///         variable bound pair that is NaN, inverted, or fixes the variable
    ///         at an infinity, a constraint-row bound pair that is NaN,
    ///         inverted, or an equality at infinity, or a structure entry
    ///         outside its matrix (a Hessian entry above the diagonal
    ///         included).
    explicit NlpProblemModel(std::shared_ptr<NLPProblem> problem);

    Index n() const override { return n_; }
    Index me() const override { return rows_.num_eq_; }
    Index mi() const override { return rows_.num_iq_; }

    double eval_f(const Vec &x) const override;
    Vec eval_grad(const Vec &x) const override;
    Vec eval_ce(const Vec &x) const override;
    Vec eval_ci(const Vec &x) const override;

    /// @brief The exact Lagrangian Hessian, upper triangle, at the composed
    ///        declared multipliers.
    /// @param x The iterate.
    /// @param obj_scale The objective scale, passed through as obj_factor.
    /// @param lambda_e Equality multipliers, or empty for all-zero.
    /// @param lambda_i Inequality multipliers, or empty for all-zero.
    /// @return The upper triangle, over the pattern the declared structure
    ///         defines, mirrored across the diagonal.
    /// @throws std::invalid_argument if @p x is not n() long, or if a
    ///         multiplier block is neither empty nor as long as the row count
    ///         it stands for.
    SpMatRM eval_hess(const Vec &x, double obj_scale, const Vec &lambda_e,
                      const Vec &lambda_i) const override;

    SpMatRM eval_jac_e(const Vec &x) const override;
    SpMatRM eval_jac_i(const Vec &x) const override;

    // --- The in-place forms, overridden so a consumer's own storage is filled
    //     directly. Each leaves the destination holding exactly what its
    //     by-value counterpart returns; the by-value methods are implemented
    //     as a copy of these. ---

    /// @brief The objective gradient at @p x, written into @p out.
    /// @param x The iterate.
    /// @param out Filled with n() entries.
    /// @throws std::invalid_argument if @p x is not n() long.
    void eval_grad_in_place(const Vec &x, Vec &out) const override;
    /// @brief The equality residuals at @p x, written into @p out.
    /// @param x The iterate.
    /// @param out Filled with me() entries.
    /// @throws std::invalid_argument if @p x is not n() long.
    void eval_ce_in_place(const Vec &x, Vec &out) const override;
    /// @brief The inequality residuals at @p x, written into @p out.
    /// @param x The iterate.
    /// @param out Filled with mi() entries.
    /// @throws std::invalid_argument if @p x is not n() long.
    void eval_ci_in_place(const Vec &x, Vec &out) const override;
    /// @brief The equality Jacobian at @p x, written into @p out.
    /// @param x The iterate.
    /// @param out Filled over the pattern the declared structure defines,
    ///            compressed.
    /// @throws std::invalid_argument if @p x is not n() long.
    void eval_jac_e_in_place(const Vec &x, SpMatRM &out) const override;
    /// @brief The inequality Jacobian at @p x, written into @p out.
    /// @param x The iterate.
    /// @param out Filled over the pattern the declared structure defines,
    ///            compressed.
    /// @throws std::invalid_argument if @p x is not n() long.
    void eval_jac_i_in_place(const Vec &x, SpMatRM &out) const override;
    /// @brief The exact Lagrangian Hessian, upper triangle, written into
    ///        @p out.
    /// @param x The iterate.
    /// @param obj_scale The objective scale, passed through as obj_factor.
    /// @param lambda_e Equality multipliers, or empty for all-zero.
    /// @param lambda_i Inequality multipliers, or empty for all-zero.
    /// @param out Filled over the pattern the declared structure defines,
    ///            compressed, mirrored across the diagonal.
    /// @throws std::invalid_argument if @p x is not n() long, or if a
    ///         multiplier block is neither empty nor as long as the row count
    ///         it stands for.
    void eval_hess_in_place(const Vec &x, double obj_scale, const Vec &lambda_e,
                            const Vec &lambda_i, SpMatRM &out) const override;

    /// @brief Declared variable lower bounds, verbatim.
    const Vec &lower() const override { return x_lower_; }
    /// @brief Declared variable upper bounds, verbatim.
    const Vec &upper() const override { return x_upper_; }

    /// @brief The origin projected onto the declared variable bounds.
    ///
    /// NLPProblem declares no start point -- the primal guess is an argument
    /// to the solve entry points -- so this is a bounds-respecting default
    /// rather than a value the problem stated.
    Vec start_point() const override;

    /// @brief The problem this model converts.
    const NLPProblem &problem() const { return *problem_; }
    /// @brief Shared ownership of the problem this model converts.
    const std::shared_ptr<NLPProblem> &problem_ptr() const { return problem_; }
    /// @brief The row classification decided at construction.
    const NLPRowClassification &rows() const { return rows_; }
    /// @brief The number of constraint rows the problem declared.
    int num_declared_rows() const { return m_; }

    /// @brief Maps native multipliers onto the declared row space.
    /// @param lambda_e Equality multipliers, or empty for all-zero.
    /// @param lambda_i Inequality multipliers, or empty for all-zero.
    /// @return One multiplier per declared row, Ipopt sign convention.
    Vec compose_user_multipliers(ConstEigenRef<Vec> lambda_e, ConstEigenRef<Vec> lambda_i) const;

    /// @brief Maps declared multipliers onto the native row spaces.
    /// @param lambda_user One multiplier per declared row, Ipopt sign convention.
    /// @param lambda_e Filled with the equality multipliers, sized me().
    /// @param lambda_i Filled with the inequality multipliers, sized mi().
    /// @throws std::invalid_argument if @p lambda_user is not one entry per
    ///         declared row.
    void split_user_multipliers(ConstEigenRef<Vec> lambda_user, Vec &lambda_e, Vec &lambda_i) const;

  private:
    /// One declared Jacobian slot's contribution to one native matrix entry.
    struct JacTarget {
        int slot_;        ///< index into the declared Jacobian value array
        int value_index_; ///< index into the native matrix's value array
        double sign_;     ///< the residual's sign for the row it lands in
    };

    /// One native residual: sign_ * (g[row_] - shift_).
    struct ResTarget {
        int row_;
        double sign_;
        double shift_;
    };

    void validate_sizes() const;
    void validate_variable_bounds() const;
    void build_row_tables(ConstEigenRef<Eigen::VectorXd> gl, ConstEigenRef<Eigen::VectorXd> gu);
    void read_structures();
    void build_jacobian_pattern();
    void build_hessian_pattern();

    void compose_into(Vec &lambda_user, ConstEigenRef<Vec> lambda_e,
                      ConstEigenRef<Vec> lambda_i) const;

    void sync_x(const Vec &x) const;
    void refresh_g(const Vec &x) const;
    void refresh_jac(const Vec &x) const;

    std::shared_ptr<NLPProblem> problem_;
    int n_ = 0, m_ = 0, jac_nnz_ = 0, hess_nnz_ = 0;

    Vec x_lower_, x_upper_;
    NLPRowClassification rows_;
    Eigen::VectorXi jac_rows_, jac_cols_;   ///< declared Jacobian structure
    Eigen::VectorXi hess_rows_, hess_cols_; ///< declared Hessian structure, lower triangle

    std::vector<ResTarget> eq_res_, iq_res_;
    std::vector<JacTarget> eq_jac_, iq_jac_;
    std::vector<int> hess_value_index_; ///< declared Hessian slot -> native value index

    /// The native patterns, laid once at construction. Only their index arrays
    /// are read after that: an in-place evaluation gives its destination this
    /// pattern the first time it sees it, then fills that destination's value
    /// array.
    SpMatRM jac_e_, jac_i_, hess_;

    /// The by-value methods' own destinations, filled by the in-place methods
    /// and copied out.
    mutable SpMatRM jac_e_values_, jac_i_values_, hess_values_;

    // --- Per-iterate cache; the callbacks are pure, so it keys on the iterate.
    mutable Vec x_cache_;
    mutable bool g_valid_ = false;
    mutable bool jac_valid_ = false;
    mutable Vec g_cache_, jac_cache_;
    mutable Vec lambda_user_, hess_vals_;

    /// The by-value methods' own destinations for the dense returns.
    mutable Vec grad_, ce_, ci_;
};

} // namespace hven::solvers
