#pragma once

// How a consumer's function type enters hven's solver interfaces.
//
// ConstraintInterface and ObjectiveInterface (detail/interior/
// solver_interface_specs.h) are the two type-erased handles every engine in
// this library evaluates through. They take a function by converting
// constructor, and the question this header answers is: given some consumer
// type T, WHAT gets stored inside the handle? That is a decision only the
// owner of T can make correctly, so it is not made here and it is not
// guessed -- it is declared, once, by specializing SolverInterfaceAdapter<T>.
//
// ---------------------------------------------------------------------------
// CONTRACT
// ---------------------------------------------------------------------------
//
// Where a registration lives. A type's SolverInterfaceAdapter specialization
// belongs in the file that DEFINES the type -- never at a call site that
// happens to convert one. Two reasons, both load-bearing:
//
//   * It is the only placement that keeps the program well-formed. A
//     specialization that some translation unit fails to see, after that TU
//     has already named the type, is an ill-formed-no-diagnostic-required
//     program: different TUs would disagree about what the adapter is. Put
//     the registration with the definition and no TU can name the type
//     without seeing its registration.
//   * It is the anti-collision rule. More than one engine consumes these
//     interfaces, and a registration written at a call site invites a second
//     engine's call site to write a conflicting one for the same type.
//
// What may be stored. The interfaces hold exactly one erased object and cost
// exactly one virtual dispatch per evaluation call. That budget is the whole
// point of routing through an adapter, and it binds every consumer:
//
//   * A PLAIN VALUE TYPE (a concrete function object, not itself a handle)
//     is stored as itself. Inherit DirectFunctionModel<T> and you are done:
//     one erasure, one dispatch.
//   * An ERASED HANDLE (a type that already wraps a function behind its own
//     vtable) must NOT be stored as itself -- that is two erasures and two
//     dispatches on every solver evaluation, forever, with no diagnostic to
//     say so. Such a type must instead expose a PAYLOAD-EMPLACEMENT HOOK: a
//     member the handle's own inner erasure can call to emplace its
//     CONCRETE payload directly into a caller-supplied interface (the
//     `pack_into_constraint_interface` / `pack_into_objective_interface`
//     pattern). Its adapter specialization then forwards to that hook, and
//     the stored object is the concrete function -- one dispatch again.
//
// The hook cannot be retrofitted from outside: it has to be reachable from
// the handle's own erasure, so it must be part of the handle's design. A
// consumer that freezes an erased function representation WITHOUT such a
// hook has permanently forfeited the single-dispatch path for its functions;
// they will be stored whole, at two dispatches per call. Decide this before
// freezing the representation, not after.
//
// Every specialization provides BOTH operations. A specialization is looked
// up by type, not by interface, so a type registered at all is reachable from
// BOTH ConstraintInterface and ObjectiveInterface. A specialization that
// simply omits one of the two installs does not fall back to the primary
// template -- the primary is not consulted once a specialization matches --
// so its authored message could never fire and the omission would surface as
// a raw "no member named" diagnostic instead. Declare the unsupported
// direction rather than omitting it: inherit ConstraintUnsupported<T> or
// ObjectiveUnsupported<T> below and the refusal gets its own authored
// message. DirectFunctionModel supports both directions and needs neither.
//
// There is deliberately NO default adapter. An unregistered type reaching
// either interface is a compile error with an authored message, because the
// alternative -- quietly accepting it -- is exactly the silent
// double-dispatch this seam exists to make impossible.
//
// Every refusal on every route is an authored message and nothing else. The
// interfaces themselves check that the selected adapter offers the operation
// before calling it, the install bodies check the function surface before
// emplacing, and both guard the work they would otherwise do with the same
// condition they assert on -- so no raw instantiation failure is ever reached
// behind a failed check. The compile-fail fixtures assert the absence of raw
// diagnostics, not just the presence of the authored one.

namespace hven::solvers {

struct ConstraintInterface;
struct ObjectiveInterface;

/// Always-false variable template for the adapter diagnostics below. The
/// dependence on T is what makes each static_assert fire on instantiation of
/// the enclosing function body rather than when the template is parsed.
template <class T> inline constexpr bool adapter_dependent_false_v = false;

/// Customization point: how a function type enters ConstraintInterface /
/// ObjectiveInterface.
///
/// The primary template is safe to instantiate and inspect -- `registered` is
/// a metaprogramming probe, and reading it never errors. Only CALLING an
/// install function on an unregistered type is a diagnostic.
template <class T> struct SolverInterfaceAdapter {
    static constexpr bool registered = false;

    static void install_constraint(const T &, ConstraintInterface &) {
        static_assert(adapter_dependent_false_v<T>,
                      "hven constraint adapter: no SolverInterfaceAdapter<T> specialization "
                      "for this type. A function enters ConstraintInterface only through an "
                      "explicit specialization: inherit hven::solvers::DirectFunctionModel<T> "
                      "to store T itself (one virtual dispatch per solver call), or define "
                      "install_constraint to choose the stored payload. There is deliberately "
                      "no default: an unrecognized type would otherwise be type-erased twice "
                      "and pay a second virtual dispatch on every solver call, silently.");
    }

    static void install_objective(const T &, ObjectiveInterface &) {
        static_assert(adapter_dependent_false_v<T>,
                      "hven objective adapter: no SolverInterfaceAdapter<T> specialization "
                      "for this type (or the specialization does not provide "
                      "install_objective). See install_constraint's message for the contract.");
    }
};

/// Ready-made half-policy: this type cannot be a constraint. Inherit it in a
/// specialization that defines install_objective only, so the constraint
/// route still answers with an authored message instead of a raw lookup
/// failure:
///
///     template <> struct hven::solvers::SolverInterfaceAdapter<MyObjective>
///         : hven::solvers::ConstraintUnsupported<MyObjective> {
///         static void install_objective(const MyObjective &t, ObjectiveInterface &oi) { ... }
///     };
template <class T> struct ConstraintUnsupported {
    static constexpr bool registered = true;

    static void install_constraint(const T &, ConstraintInterface &) {
        static_assert(adapter_dependent_false_v<T>,
                      "hven constraint adapter: this type's SolverInterfaceAdapter inherits "
                      "hven::solvers::ConstraintUnsupported, which declares it registered for "
                      "the OBJECTIVE interface only. It cannot enter ConstraintInterface. If "
                      "that is wrong, give its adapter an install_constraint (or inherit "
                      "hven::solvers::DirectFunctionModel<T> to store it directly).");
    }
};

/// Ready-made half-policy: this type cannot be an objective -- the common
/// case, since most functions are constraints and the scalar objective
/// surface is extra. Inherit it in a specialization that defines
/// install_constraint only:
///
///     template <> struct hven::solvers::SolverInterfaceAdapter<MyConstraint>
///         : hven::solvers::ObjectiveUnsupported<MyConstraint> {
///         static void install_constraint(const MyConstraint &t, ConstraintInterface &ci) { ... }
///     };
template <class T> struct ObjectiveUnsupported {
    static constexpr bool registered = true;

    static void install_objective(const T &, ObjectiveInterface &) {
        static_assert(adapter_dependent_false_v<T>,
                      "hven objective adapter: this type's SolverInterfaceAdapter inherits "
                      "hven::solvers::ObjectiveUnsupported, which declares it registered for "
                      "the CONSTRAINT interface only. It cannot enter ObjectiveInterface. If "
                      "that is wrong, give its adapter an install_objective and give the type "
                      "the scalar objective surface (objective / objective_gradient / "
                      "objective_gradient_hessian).");
    }
};

/// Ready-made policy for a plain value type: store T itself, one erasure and
/// one virtual dispatch per solver call. Supports BOTH directions, so a type
/// registered through it needs neither mixin above. Register a type by
/// inheriting this:
///
///     template <> struct hven::solvers::SolverInterfaceAdapter<MyFunction>
///         : hven::solvers::DirectFunctionModel<MyFunction> {};
///
/// Do NOT use it for an erased handle -- see the CONTRACT above.
///
/// The bodies are defined in detail/solvers/solver_interface_specs.h, after
/// the interface definitions they emplace into.
template <class T> struct DirectFunctionModel {
    static constexpr bool registered = true;
    static void install_constraint(const T &t, ConstraintInterface &ci);
    static void install_objective(const T &t, ObjectiveInterface &oi);
};

} // namespace hven::solvers
