// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief The one validator of `CommonOptions`, shared by both engines.

#include <hven/core/types.h>
#include <hven/drivers/common_options.h>

namespace hven::detail {

/// @brief The bounds on which the two engines differ.
struct CommonOptionsFloors {
    /// Smallest accepted `max_iterations`.
    Index max_iterations;
    /// Smallest accepted `threads`.
    int threads;
};

/// @brief Refuses an invalid `CommonOptions`, naming the first offending field.
///
/// One field per message, in the form `common.<field> must ..., got <value>`.
/// A NaN is refused wherever a number is checked. `max_iterations` must also not
/// exceed INT_MAX, which both engines narrow to `int`.
///
/// @throws std::invalid_argument
void validate_common_options(const CommonOptions &c, CommonOptionsFloors floors);

} // namespace hven::detail
