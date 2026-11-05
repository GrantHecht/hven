// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// How a piece type enters hven's two piece handles.
//
// ConstraintInterface and ObjectiveInterface (model/piece_interfaces.h)
// are the two type-erased handles the interior-point layout evaluates pieces
// through. They take a piece by converting constructor, and this header decides
// WHAT gets stored inside the handle for a given type T.
//
// ---------------------------------------------------------------------------
// CONTRACT
// ---------------------------------------------------------------------------
//
// The default needs no registration. SolverInterfaceAdapter<T>'s primary
// template is the default policy, and it decides by detection:
//
//   * A type exposing a PAYLOAD-EMPLACEMENT HOOK -- a const member
//     `pack_into_constraint_interface(ConstraintInterface &)` (and, for the
//     objective route, `pack_into_objective_interface(ObjectiveInterface &)`)
//     -- is an erased handle that knows its concrete payload. The default calls
//     the hook, and the hook emplaces the concrete payload directly: one
//     erasure, one virtual dispatch per evaluation call.
//   * Any other type is stored as itself (DirectFunctionModel<T>): one erasure,
//     one virtual dispatch per evaluation call.
//
// The handles hold exactly one erased object and cost exactly one virtual
// dispatch per evaluation call. An erased handle WITHOUT a hook is still stored
// whole, because nothing outside the handle can reach its payload, and then
// every evaluation pays two dispatches: the handle's and the stored model's.
// A consumer that freezes an erased function representation should give it the
// hook before freezing it; the hook has to be reachable from the handle's own
// erasure, so it cannot be retrofitted from outside.
//
// A piece must be COPY-CONSTRUCTIBLE: the layout copies it once per partition
// (the handles copy deeply) and evaluates the copies concurrently from several
// threads. State the copies share is held through a std::shared_ptr and must be
// safe to read concurrently. The handle stores a small piece inline and relocates
// it by move construction, so a piece may own a std::string, a std::vector or any
// other copyable member. A piece that is not copy-constructible is refused with
// an authored message.
//
// A class with a static member kInputs is a LOCAL-BLOCK piece
// (model/local_blocks.h). That header's partial specializations take it, and the
// handles' header includes it, so such a type never reaches the default below.
//
// Explicit specializations remain the override. A specialization of
// SolverInterfaceAdapter<T> replaces the default for T, and it belongs in the
// file that DEFINES T -- never at a call site that happens to convert one: a
// specialization some translation unit fails to see, after that TU has named
// the type, is an ill-formed-no-diagnostic-required program.
//
// A specialization provides BOTH installs. It is selected by type, so it is
// reachable from both handles, and a specialization that omits one does not
// fall back to the primary template. Declare the unsupported direction rather
// than omitting it: inherit ConstraintUnsupported<T> or ObjectiveUnsupported<T>
// below and the refusal gets its own authored message. DirectFunctionModel
// supports both directions and needs neither.
//
// Every refusal on every route is an authored message and nothing else. The
// handles check that the selected adapter offers the operation before calling
// it, the install bodies check the piece surface before emplacing, and both
// guard the work they would otherwise do with the same condition they assert
// on -- so no raw instantiation failure is reached behind a failed check.

namespace hven {

struct ConstraintInterface;
struct ObjectiveInterface;

/// Always-false variable template for the adapter diagnostics below. The
/// dependence on T is what makes each static_assert fire on instantiation of
/// the enclosing function body rather than when the template is parsed.
template <class T> inline constexpr bool adapter_dependent_false_v = false;

/// The constraint-route payload-emplacement hook, detected on T.
template <class T>
concept ConstraintPackHook =
    requires(const T &t, ConstraintInterface &ci) { t.pack_into_constraint_interface(ci); };

/// The objective-route payload-emplacement hook, detected on T.
template <class T>
concept ObjectivePackHook =
    requires(const T &t, ObjectiveInterface &oi) { t.pack_into_objective_interface(oi); };

/// Ready-made policy for a plain value type: store T itself, one erasure and
/// one virtual dispatch per evaluation call. Supports BOTH directions. It is
/// what the default policy uses for a type without a hook, and it may be
/// inherited by an explicit specialization:
///
///     template <> struct hven::SolverInterfaceAdapter<MyFunction>
///         : hven::DirectFunctionModel<MyFunction> {};
///
/// The bodies are defined in model/piece_interfaces.h, after the handle
/// definitions they emplace into.
template <class T> struct DirectFunctionModel {
    static constexpr bool registered = true;
    static void install_constraint(const T &t, ConstraintInterface &ci);
    static void install_objective(const T &t, ObjectiveInterface &oi);
};

/// Customization point: how a piece type enters ConstraintInterface /
/// ObjectiveInterface.
///
/// The primary template is the default policy stated in the CONTRACT above:
/// the hook when T exposes one, direct storage otherwise. `registered` is a
/// metaprogramming probe that reads false here and true on a specialization
/// built from the policies below, so it says whether T's route was chosen
/// explicitly; reading it never errors.
template <class T> struct SolverInterfaceAdapter {
    static constexpr bool registered = false;

    static void install_constraint(const T &t, ConstraintInterface &ci) {
        static_assert(
            !requires { T::kInputs; },
            "hven local block: a local-block piece reached the default adapter; "
            "include hven/model/piece_interfaces.h (or hven/model/pieces.h) "
            "before naming SolverInterfaceAdapter for it");
        if constexpr (ConstraintPackHook<T>) {
            t.pack_into_constraint_interface(ci);
        } else {
            DirectFunctionModel<T>::install_constraint(t, ci);
        }
    }

    static void install_objective(const T &t, ObjectiveInterface &oi) {
        static_assert(
            !requires { T::kInputs; },
            "hven local block: a local-block piece reached the default adapter; "
            "include hven/model/piece_interfaces.h (or hven/model/pieces.h) "
            "before naming SolverInterfaceAdapter for it");
        if constexpr (ObjectivePackHook<T>) {
            t.pack_into_objective_interface(oi);
        } else {
            // A handle that carries the constraint hook only is a handle whose
            // payload cannot be an objective; storing the handle whole would
            // hand ObjectiveModel a type with neither surface.
            static_assert(!ConstraintPackHook<T>,
                          "hven objective adapter: this type exposes "
                          "pack_into_constraint_interface but not "
                          "pack_into_objective_interface, so it cannot enter ObjectiveInterface. "
                          "Give it the objective hook, or use it as a constraint only.");
            if constexpr (!ConstraintPackHook<T>) {
                DirectFunctionModel<T>::install_objective(t, oi);
            }
        }
    }
};

/// Ready-made half-policy: this type cannot be a constraint. Inherit it in a
/// specialization that defines install_objective only, so the constraint
/// route still answers with an authored message instead of a raw lookup
/// failure:
///
///     template <> struct hven::SolverInterfaceAdapter<MyObjective>
///         : hven::ConstraintUnsupported<MyObjective> {
///         static void install_objective(const MyObjective &t, ObjectiveInterface &oi) { ... }
///     };
template <class T> struct ConstraintUnsupported {
    static constexpr bool registered = true;

    static void install_constraint(const T &, ConstraintInterface &) {
        static_assert(adapter_dependent_false_v<T>,
                      "hven constraint adapter: this type's SolverInterfaceAdapter inherits "
                      "hven::ConstraintUnsupported, which declares it an OBJECTIVE "
                      "only. It cannot enter ConstraintInterface. If that is wrong, give its "
                      "adapter an install_constraint (or inherit "
                      "hven::DirectFunctionModel<T> to store it directly).");
    }
};

/// Ready-made half-policy: this type cannot be an objective. Inherit it in a
/// specialization that defines install_constraint only:
///
///     template <> struct hven::SolverInterfaceAdapter<MyConstraint>
///         : hven::ObjectiveUnsupported<MyConstraint> {
///         static void install_constraint(const MyConstraint &t, ConstraintInterface &ci) { ... }
///     };
template <class T> struct ObjectiveUnsupported {
    static constexpr bool registered = true;

    static void install_objective(const T &, ObjectiveInterface &) {
        static_assert(adapter_dependent_false_v<T>,
                      "hven objective adapter: this type's SolverInterfaceAdapter inherits "
                      "hven::ObjectiveUnsupported, which declares it a CONSTRAINT "
                      "only. It cannot enter ObjectiveInterface. If that is wrong, give its "
                      "adapter an install_objective and give the type the scalar objective "
                      "surface (objective / objective_gradient / objective_gradient_hessian).");
    }
};

} // namespace hven
