// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// aggregate_arity.h -- the DECLARED FIELD COUNT of an aggregate, at compile
// time, so a hand-written field TABLE can be pinned against the struct it
// claims to enumerate (M6 W4 T2(d), plan amendment E).
//
// NOT sizeof: sizeof is padding-dependent and moves for reasons that have
// nothing to do with the field list. This counts INITIALIZERS -- the largest N
// for which `T{a1, ..., aN}` is well-formed with each `a` convertible to
// anything -- which is exactly the number of fields aggregate initialization
// addresses.
//
// A NESTED AGGREGATE COUNTS AS ONE FIELD here, because `AggregateAnyInit`
// converts to the nested type directly and that conversion is preferred over
// brace elision. The counts the tables assert are stated at their own
// static_asserts, which is where a reader can check the arithmetic.
//
// UNDER core/, NOT detail/, and that is the layering rule rather than a
// preference: `core/solver_counters.h` holds the field tables this pins, and
// `CoreLayering.NoCoreHeaderDependsUpwardOnAnotherTier` forbids a core/ header
// from including `hven/detail/`. The namespace stays `hven::detail`.

#include <cstddef>
#include <type_traits>
#include <utility>

namespace hven::detail {

/// @brief Converts to ANY type but its own host `T`.
///
/// The `T` exclusion is what keeps the count honest: without it `T{x}` would be
/// well-formed for every class at N == 1 through the copy constructor, and a
/// one-field reading would be indistinguishable from a copy.
///
/// Declared, never defined -- it is only ever named in the unevaluated operand
/// of the requires-expression below.
template <class T> struct AggregateAnyInit {
    template <class U>
        requires(!std::is_same_v<T, std::remove_cvref_t<U>>)
    constexpr operator U() const;
};

/// Names one initializer slot; the index is discarded and exists only to let a
/// pack expansion produce N of them.
template <class T, std::size_t> using AggregateAnyAt = AggregateAnyInit<T>;

/// @brief Is `T{a1, ..., aN}` well-formed, with N == sizeof...(I)?
template <class T, std::size_t... I>
constexpr bool aggregate_initializable_with(std::index_sequence<I...>) {
    return requires { T{AggregateAnyAt<T, I>{}...}; };
}

/// @brief The largest N for which `T` is brace-initializable with N arguments.
///
/// Linear from 0 upward rather than a binary search: initializability is NOT
/// monotone in general (a struct with a reference member refuses N < arity),
/// and the climb stops at the first refusal, which for an aggregate with all
/// members initializable is its field count.
template <class T, std::size_t N = 0> constexpr std::size_t aggregate_arity() {
    if constexpr (aggregate_initializable_with<T>(std::make_index_sequence<N + 1>{})) {
        return aggregate_arity<T, N + 1>();
    } else {
        return N;
    }
}

/// @brief `aggregate_arity<T>()` as a variable template, for use in a
///        `static_assert` beside a field table.
template <class T> inline constexpr std::size_t kAggregateArity = aggregate_arity<T>();

} // namespace hven::detail
