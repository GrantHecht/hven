// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief The name tables of the option enums both engines' tables use: the `common.` enums, the
///        dense-arm policy, and the QP-layer enums. Defined in src/solver/option_enum_names.cpp.

#include <hven/core/start_level.h>
#include <hven/detail/core/options_registry.h>
#include <hven/linear/dense_policy.h>
#include <hven/qp/qp_options.h>
#include <hven/qp/qp_types.h>
#include <hven/solver/common_options.h>

namespace hven::detail {

HVEN_DECLARE_OPTION_ENUM(hven::Verbosity, "Verbosity")
HVEN_DECLARE_OPTION_ENUM(hven::ReturnBest, "ReturnBest")
HVEN_DECLARE_OPTION_ENUM(hven::StartLevel, "StartLevel")
HVEN_DECLARE_OPTION_ENUM(hven::linear::DensePolicy, "DensePolicy")
HVEN_DECLARE_OPTION_ENUM(hven::QpMode, "QpMode")
HVEN_DECLARE_OPTION_ENUM(hven::QpDumpPolicy, "QpDumpPolicy")
HVEN_DECLARE_OPTION_ENUM(hven::QpWorkingSetAlgebra, "QpWorkingSetAlgebra")
HVEN_DECLARE_OPTION_ENUM(hven::SsnSigmaRule, "SsnSigmaRule")
HVEN_DECLARE_OPTION_ENUM(hven::SsnHintRule, "SsnHintRule")
HVEN_DECLARE_OPTION_ENUM(hven::SsnInfeasibilityRule, "SsnInfeasibilityRule")
HVEN_DECLARE_OPTION_ENUM(hven::IpqpBarrierInitialRule, "IpqpBarrierInitialRule")

} // namespace hven::detail
