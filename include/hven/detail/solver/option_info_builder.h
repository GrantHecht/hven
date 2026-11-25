// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief Builds an `OptionInfo` row from a walker's `(OptionMeta, value)` pair.

#include <string>
#include <type_traits>

#include <hven/detail/core/options_registry.h>
#include <hven/solver/options_reference.h>

namespace hven::detail {

/// The range text of a non-enum descriptor; @p integral omits the vacuous `finite,`.
std::string describe_option_range(const OptionRange &r, bool integral);

/// @brief One row: name, type, default (the value's dump spelling), range, doc, tier.
template <class T> OptionInfo describe_option(const OptionMeta &m, const T &value) {
    using V = std::remove_cvref_t<T>;
    OptionInfo info;
    info.name = m.name();
    info.type = option_type_name<V>();
    info.default_value = render_option_value(value);
    info.doc = std::string(m.doc);
    info.tier = m.advanced ? OptionTier::kAdvanced : OptionTier::kUser;

    if constexpr (std::is_enum_v<V>) {
        std::string range = "one of ";
        bool first = true;
        for (const auto &e : enumerators<V>()) {
            if (!first)
                range += ", ";
            first = false;
            range += e.name;
        }
        info.range = std::move(range);
    } else if constexpr (std::is_same_v<V, bool>) {
        info.range = "true or false";
    } else if constexpr (IsOptional<V>::value) {
        using U = typename V::value_type;
        const std::string inner = describe_option_range(m.range, std::is_integral_v<U>);
        info.range = m.range.kind == OptionRange::Kind::kInterval ? "unset, or " + inner : inner;
    } else {
        info.range = describe_option_range(m.range, std::is_integral_v<V>);
    }
    return info;
}

} // namespace hven::detail
