// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <memory>

#include <hven/core/types.h>

namespace hven::solvers {

class Nlp;
struct AssemblyDeclaration;

namespace detail {
class ProblemState;
struct ProblemAccess;
} // namespace detail

/// @brief The row-kind and bound-kind census of a problem, as Ipopt prints it.
///
/// Variables are counted by their bounds: free (no finite bound), lower-bounded
/// only, upper-bounded only, boxed (two finite, unequal bounds) and fixed (equal
/// finite bounds). Rows are counted by kind: equality (g_lower == g_upper),
/// lower-bounded (g_lower <= g(x) only), upper-bounded (g(x) <= g_upper only),
/// range (two finite, unequal bounds) and free (no finite bound). A pieces
/// problem's rows are its equality rows and its inequality rows (c_I(x) <= 0,
/// upper-bounded).
struct ProblemStatistics {
    Index variables = 0;
    Index free_variables = 0;
    Index lower_bounded_variables = 0;
    Index upper_bounded_variables = 0;
    Index boxed_variables = 0;
    Index fixed_variables = 0;

    Index constraints = 0;
    Index equality_rows = 0;
    Index lower_bounded_rows = 0;
    Index upper_bounded_rows = 0;
    Index range_rows = 0;
    Index free_rows = 0;
};

/// @brief What IpmSolver and SqpSolver solve.
///
/// Built from a callback Nlp (model/nlp.h) or from pieces (an AssemblyDeclaration,
/// model/assembly_declaration.h). It owns each engine's transcription of the
/// problem, built on that engine's first solve and kept for the next.
///
/// Per solve on an Nlp problem. dimensions(), both structures and bounds() are
/// re-read and the per-iterate caches cleared. An unchanged bound set changes
/// nothing. A changed set is adopted by its kind of change:
/// - Values only (every variable keeps which sides are finite and whether it is
///   fixed): adopted in place. Nothing is re-laid and the interior-point engine does
///   not re-analyse (IpmResult::kkt_analyses_this_call == 0); a WarmStartData taken
///   before still matches. The one exception: under
///   FixedVariableTreatments::MakeConstraint a moved fixing value re-installs that
///   treatment's fixing rows, which re-analyses (counted).
/// - A change in which variables are fixed, or in which bounds are finite: the
///   interior-point transcription is re-laid and re-analysed (counted), and a
///   WarmStartData taken before no longer matches the declaration and is refused
///   by the identity rule.
/// - A change in the dimensions, in either structure, or in a row's kind: refused
///   by name (std::invalid_argument), with nothing changed.
///
/// Engines. Both engines solve an Nlp problem, and may alternate on one Problem; an
/// IpmSolver that did not lay the current interior-point analysis re-analyses
/// (counted). The SQP engine refuses a pieces problem by name.
///
/// Results. lambda and g are reported in the problem's own row space: one entry
/// per Nlp row in Ipopt's sign convention, or for a pieces problem the
/// declaration's [equality rows | inequality rows].
///
/// Thread safety. One Problem serves one solve at a time, on one thread. N
/// concurrent solves need N Problems and N solvers. Problems that share one Nlp
/// require its const members to be safe to call concurrently.
///
/// Movable, not copyable. A moved-from Problem may only be destroyed or assigned
/// to; its accessors report an empty problem and a solve refuses it.
class Problem {
  public:
    /// @brief A problem stated by callbacks. Validates the declaration: the
    ///        dimensions, the bounds and both structures are read here.
    /// @param nlp The callbacks; must not be null. Shared, not copied.
    /// @throws std::invalid_argument on a null pointer or an invalid declaration.
    explicit Problem(std::shared_ptr<Nlp> nlp);

    /// @brief A problem stated by pieces, laid out here.
    /// @param pieces The declaration; moved from.
    /// @throws std::invalid_argument on an invalid declaration.
    explicit Problem(AssemblyDeclaration pieces);

    ~Problem();
    Problem(const Problem &) = delete;
    Problem &operator=(const Problem &) = delete;
    Problem(Problem &&) noexcept;
    Problem &operator=(Problem &&) noexcept;

    /// @brief The number of variables.
    Index num_variables() const noexcept;

    /// @brief The number of rows in the problem's own row space.
    Index num_constraints() const noexcept;

    /// @brief The row-kind and bound-kind census, as of the last solve entry (or
    ///        construction).
    const ProblemStatistics &statistics() const noexcept;

    /// @brief The evaluation-partition count the interior-point layout adopts.
    ///        A requested count may be clamped down (one partition per 1000 KKT
    ///        elements at most); this is the adopted one. 1 for an Nlp problem.
    Index partitions() const noexcept;

  private:
    friend struct detail::ProblemAccess;
    explicit Problem(std::unique_ptr<detail::ProblemState> state) noexcept;

    std::unique_ptr<detail::ProblemState> state_;
};

} // namespace hven::solvers
