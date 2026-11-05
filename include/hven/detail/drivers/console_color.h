// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief The interior-point console table's residual colouring. Internal to
///        the console sink; kept out of the public sink header so that header
///        does not expose fmt's styling types.

#include <fmt/color.h>

namespace hven {

/// @brief The interior-point iteration table's five-band residual colouring.
///
/// The bands are log-spaced between the CONVERGENCE tolerance and 1000x the
/// ACCEPTABLE one: below target is lime green, then yellow, orange, red, and
/// dark red beyond. A non-positive argument is floored at 1e-300 rather than
/// taking a logarithm of zero.
///
/// @param value      The residual to colour.
/// @param target     That column's convergence tolerance.
/// @param acceptable That column's acceptable-level tolerance.
/// @return An fmt foreground style.
fmt::text_style ipm_residual_color(double value, double target, double acceptable);

} // namespace hven
