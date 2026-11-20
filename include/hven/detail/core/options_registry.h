// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief The vocabulary of the options registry: ranges, the per-field metadata a walker hands a
///        visitor, value rendering, and the table-check macros.
///
/// Each options struct has ONE table, an X-macro in `detail/<layer>/..._fields.h`, one row per
/// field in declaration order: `X(member, range, "doc")`. A nested struct is one row,
/// `G(member, table, advanced)`. Everything that used to be a hand-written per-field list is a
/// walk over those tables: the options dump, the metadata behind `ipm_option_info()` and
/// `sqp_option_info()`, the generated options reference, and the bindings' typed access
/// (`detail::for_each_option`).
///
/// The tables are checked against the structs they claim to list at compile time, in the TU that
/// owns each (`HVEN_CHECK_OPTIONS_TABLE`): the row count must equal the struct's aggregate arity,
/// and the rows must name the members in declaration order.

#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <hven/core/types.h>
#include <hven/detail/core/aggregate_arity.h>

namespace hven::detail {

/// @brief Which engine a walk is over; the shared `common.` table's ranges depend on it.
enum class OptionEngine { kIpm, kSqp };

/// @brief The accepted values of one field, as a descriptor the generated reference prints and the
///        range probes read.
///
/// An enum field's range is its enumerator table (read off the type, not stated here); `kText`
/// describes a condition no interval states (a cross-field ordering, a reserved value) and is not
/// probed.
struct OptionRange {
    enum class Kind { kAny, kInterval, kText };

    Kind kind = Kind::kAny;
    bool has_lo = false;
    bool has_hi = false;
    bool lo_open = false;
    bool hi_open = false;
    /// Only meaningful for a half-bounded interval: false means +inf is accepted.
    bool finite = true;
    double lo = 0.0;
    double hi = 0.0;
    std::string_view text;
};

constexpr OptionRange r_any() { return {}; }

/// `> lo`; `finite` false accepts +inf too.
constexpr OptionRange r_gt(double lo, bool finite = true) {
    return {OptionRange::Kind::kInterval, true, false, true, false, finite, lo, 0.0, {}};
}

/// `>= lo`; `finite` false accepts +inf too.
constexpr OptionRange r_ge(double lo, bool finite = true) {
    return {OptionRange::Kind::kInterval, true, false, false, false, finite, lo, 0.0, {}};
}

/// `(lo, hi)`.
constexpr OptionRange r_open(double lo, double hi) {
    return {OptionRange::Kind::kInterval, true, true, true, true, true, lo, hi, {}};
}

/// `[lo, hi]`.
constexpr OptionRange r_closed(double lo, double hi) {
    return {OptionRange::Kind::kInterval, true, true, false, false, true, lo, hi, {}};
}

/// `[lo, hi)`.
constexpr OptionRange r_closed_open(double lo, double hi) {
    return {OptionRange::Kind::kInterval, true, true, false, true, true, lo, hi, {}};
}

/// A condition stated in words.
constexpr OptionRange r_text(std::string_view text) {
    return {OptionRange::Kind::kText, false, false, false, false, true, 0.0, 0.0, text};
}

/// @brief The context a walker threads down: the name prefix of the struct being visited, the
/// engine
///        (for ranges that differ per engine) and whether the subtree is the research tier.
struct OptionCtx {
    std::string_view prefix;
    OptionEngine engine;
    bool advanced;
};

/// @brief What a visitor learns about one field besides its value.
struct OptionMeta {
    /// `""`, `"common."`, `"advanced.ipqp."`, ...
    std::string_view prefix;
    /// The member's own name.
    std::string_view member;
    std::string_view doc;
    OptionRange range;
    /// True under `advanced.` (the research tier).
    bool advanced;
    /// The struct the member is declared in, for tools that read its header.
    std::string_view owner;

    /// The dumped name: prefix + member.
    [[nodiscard]] std::string name() const {
        std::string n(prefix);
        n += member;
        return n;
    }
};

/// @brief An enum option's name table. Specialized, one per option enum, next to the enum's layer
///        (`detail/<layer>/..._enum_names.h`); the definitions are an exhaustive switch each.
///
/// `name` returns the enumerator's own spelling (`kLoqo`), or `"unknown"` for a value in no
/// enumerator. `type` is the enum's spelling.
template <class E> struct EnumTraits;

/// Declares the `EnumTraits` specialization of an option enum (inside `namespace hven::detail`).
#define HVEN_DECLARE_OPTION_ENUM(Qualified, Short)                                                 \
    template <> struct EnumTraits<Qualified> {                                                     \
        static const char *name(Qualified v);                                                      \
        static constexpr const char *type = Short;                                                 \
    };

/// One enumerator and its spelling.
template <class E> struct EnumeratorEntry {
    E value;
    const char *name;
};

/// @brief The enumerators of E, in value order. The option enums are contiguous from 0, which the
///        validators' `enum_in_range(v, last)` already assume; the walk stops at the first value
///        `EnumTraits<E>::name` calls unknown.
template <class E> const std::vector<EnumeratorEntry<E>> &enumerators() {
    static const std::vector<EnumeratorEntry<E>> table = [] {
        std::vector<EnumeratorEntry<E>> out;
        for (long long i = 0;; ++i) {
            const E value = static_cast<E>(i);
            const char *const name = EnumTraits<E>::name(value);
            if (std::string_view(name) == "unknown")
                break;
            out.push_back({value, name});
        }
        return out;
    }();
    return table;
}

// ---- Rendering --------------------------------------------------------------------------------
// The two numeric spellings are fmt's, defined in src/solver/option_enum_names.cpp so this header
// stays free of fmt: an integer in decimal, a double at its shortest round-trip form (`1e-06`).
std::string format_double(double v);
std::string format_int(long long v);

template <class T> struct IsOptional : std::false_type {};
template <class T> struct IsOptional<std::optional<T>> : std::true_type {};
template <class T> struct IsStdVector : std::false_type {};
template <class T, class A> struct IsStdVector<std::vector<T, A>> : std::true_type {};
template <class T> struct IsStdFunction : std::false_type {};
template <class S> struct IsStdFunction<std::function<S>> : std::true_type {};

/// @brief The dump spelling of a value: `true`/`false`, an integer, a shortest-round-trip double,
/// an
///        enumerator name, `[a, b]` for a list, `unset` for an empty optional, an empty string or
///        an empty callable, `set` for a non-empty callable.
template <class T> std::string render_option_value(const T &v) {
    if constexpr (std::is_same_v<T, bool>) {
        return v ? "true" : "false";
    } else if constexpr (std::is_enum_v<T>) {
        return EnumTraits<T>::name(v);
    } else if constexpr (std::is_integral_v<T>) {
        return format_int(static_cast<long long>(v));
    } else if constexpr (std::is_floating_point_v<T>) {
        return format_double(static_cast<double>(v));
    } else if constexpr (IsOptional<T>::value) {
        return v ? render_option_value(*v) : std::string("unset");
    } else if constexpr (IsStdVector<T>::value) {
        // The one sequence-valued field family: bracketed, comma-separated, one line. An EMPTY
        // sequence still renders, as `[]`: a dump records the value it was handed.
        std::string seq = "[";
        for (std::size_t i = 0; i < v.size(); ++i) {
            if (i != 0)
                seq += ", ";
            seq += render_option_value(v[i]);
        }
        seq += ']';
        return seq;
    } else if constexpr (std::is_same_v<T, std::string>) {
        return v.empty() ? std::string("unset") : v;
    } else if constexpr (IsStdFunction<T>::value) {
        // A CALLABLE, not a configuration.
        return v ? "set" : "unset";
    } else {
        static_assert(sizeof(T) == 0, "an option field of a type the registry cannot render");
    }
}

/// @brief The type column: `bool`, `int`, `Index`, `double`, the enum's name, `optional<T>`,
///        `list<T>`, `string`, `callable`.
template <class T> std::string option_type_name() {
    if constexpr (std::is_same_v<T, bool>) {
        return "bool";
    } else if constexpr (std::is_enum_v<T>) {
        return EnumTraits<T>::type;
    } else if constexpr (std::is_same_v<T, int>) {
        return "int";
    } else if constexpr (std::is_same_v<T, Index>) {
        return "Index";
    } else if constexpr (std::is_same_v<T, double>) {
        return "double";
    } else if constexpr (IsOptional<T>::value) {
        return "optional<" + option_type_name<typename T::value_type>() + ">";
    } else if constexpr (IsStdVector<T>::value) {
        return "list<" + option_type_name<typename T::value_type>() + ">";
    } else if constexpr (std::is_same_v<T, std::string>) {
        return "string";
    } else if constexpr (IsStdFunction<T>::value) {
        return "callable";
    } else {
        static_assert(sizeof(T) == 0, "an option field of a type the registry cannot name");
    }
}

} // namespace hven::detail

// ---- Table checks -----------------------------------------------------------------------------
// HVEN_CHECK_OPTIONS_TABLE(Type, TABLE) / HVEN_CHECK_OPTIONS_TABLE_G(Type, TABLE):
//   * the row count equals kAggregateArity<Type>: a field added without a row (or the reverse)
//     does not compile;
//   * the rows name the members in declaration order: the designated-initializer expression below
//     is ill-formed (GCC) or -Wreorder-init-list (clang, which the owning TU promotes to an error)
//     for an out-of-order table.
// A table without nested rows takes only X; one with nested rows takes X and G.
#define HVEN_DETAIL_ROW_COUNT(m, r, d) +1
#define HVEN_DETAIL_NESTED_COUNT(m, t, adv) +1
#define HVEN_DETAIL_ROW_DESIGNATOR(m, r, d) .m = {},
#define HVEN_DETAIL_NESTED_DESIGNATOR(m, t, adv) .m = {},

#define HVEN_CHECK_OPTIONS_TABLE(Type, TABLE)                                                      \
    static_assert(::hven::detail::kAggregateArity<Type> == 0 TABLE(HVEN_DETAIL_ROW_COUNT),         \
                  #Type " and its options table disagree on the field count: give the new "        \
                        "field a row in its table, in declaration order");                         \
    [[maybe_unused]] static Type hven_option_order_check_##Type() {                                \
        return Type{TABLE(HVEN_DETAIL_ROW_DESIGNATOR)};                                            \
    }

#define HVEN_CHECK_OPTIONS_TABLE_G(Type, TABLE)                                                    \
    static_assert(::hven::detail::kAggregateArity<Type> ==                                         \
                      0 TABLE(HVEN_DETAIL_ROW_COUNT, HVEN_DETAIL_NESTED_COUNT),                    \
                  #Type " and its options table disagree on the field count: give the new "        \
                        "field a row in its table, in declaration order");                         \
    [[maybe_unused]] static Type hven_option_order_check_##Type() {                                \
        return Type{TABLE(HVEN_DETAIL_ROW_DESIGNATOR, HVEN_DETAIL_NESTED_DESIGNATOR)};             \
    }
