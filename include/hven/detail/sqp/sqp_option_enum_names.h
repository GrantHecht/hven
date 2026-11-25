// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief The name tables of the SQP option enums that `sqp/sqp_options.h` declares. Defined in
///        src/sqp/sqp_option_enum_names.cpp.

#include <hven/detail/core/options_registry.h>
#include <hven/sqp/sqp_options.h>

namespace hven::detail {

HVEN_DECLARE_OPTION_ENUM(hven::SqpElasticRungEngine, "SqpElasticRungEngine")
HVEN_DECLARE_OPTION_ENUM(hven::SqpHessianMode, "SqpHessianMode")
HVEN_DECLARE_OPTION_ENUM(hven::SqpColumnScaling, "SqpColumnScaling")
HVEN_DECLARE_OPTION_ENUM(hven::SqpRestorationCurvature, "SqpRestorationCurvature")
HVEN_DECLARE_OPTION_ENUM(hven::SqpTrustRegionGrowth, "SqpTrustRegionGrowth")

} // namespace hven::detail
