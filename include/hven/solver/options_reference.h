// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief The options of both engines as data: one row per field, and the generated reference.
///
/// The rows are read off the same per-struct tables the options dump is written over, so a field
/// cannot be in one and missing from the other. `docs/options-reference.md` is
/// `write_options_reference()`'s output, checked in; a test fails when the two differ.

#include <iosfwd>
#include <string>
#include <vector>

namespace hven {

/// @brief How stable a field is.
enum class OptionTier {
    /// Documented and stable between break windows.
    kUser,
    /// The research tier (`advanced.`, `advanced.ipqp.`): may change in any milestone, without a
    /// break window.
    kAdvanced,
};

/// @brief One options field.
struct OptionInfo {
    /// The dumped name: `common.threads`, `advanced.ipqp.barrier_min`, `barrier_initial`.
    std::string name;
    /// `bool`, `int`, `Index`, `double`, an enum's name, `optional<T>`, `list<T>`, `string` or
    /// `callable`.
    std::string type;
    /// The default, in the options dump's spelling (`1e-06`, `kLoqo`, `unset`, `[kOptimize]`).
    std::string default_value;
    /// The accepted values, in words: `finite, > 0`, `(0, 1)`, `one of kA, kB`, `any`. An
    /// optional's range describes its engaged value and says `unset` is allowed.
    std::string range;
    /// A one-line description.
    std::string doc;
    OptionTier tier;
};

/// @brief Every field of `IpmOptions`, in declaration order, nested structs flattened under their
///        member names. Defaults are `IpmOptions{}`'s.
const std::vector<OptionInfo> &ipm_option_info();

/// @brief Every field of `SqpOptions`, in declaration order, nested structs flattened under their
///        member names. Defaults are `SqpOptions{}`'s.
const std::vector<OptionInfo> &sqp_option_info();

/// @brief Writes the options reference (Markdown): the shared `common.` options with each engine's
///        default, each engine's user and advanced options, and the enumerator legend.
///
/// The output is the contents of `docs/options-reference.md`. Deterministic, and independent of the
/// caller's stream state. Regenerate with the `options-reference` CMake target.
void write_options_reference(std::ostream &os);

} // namespace hven
