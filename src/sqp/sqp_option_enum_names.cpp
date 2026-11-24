// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The name tables of the SQP option enums that sqp/sqp_options.h declares
// (detail/sqp/sqp_option_enum_names.h). Each switch is exhaustive with no default label, so a new
// enumerator warns here; the trailing return covers a value that is in range for no enumerator.

#include <hven/detail/sqp/sqp_option_enum_names.h>

namespace hven::detail {

const char *EnumTraits<hven::SqpElasticRungEngine>::name(hven::SqpElasticRungEngine v) {
    using E = hven::SqpElasticRungEngine;
    switch (v) {
    case E::kWalk:
        return "kWalk";
    case E::kTier:
        return "kTier";
    }
    return "unknown";
}

const char *EnumTraits<hven::SqpHessianMode>::name(hven::SqpHessianMode v) {
    using E = hven::SqpHessianMode;
    switch (v) {
    case E::kExact:
        return "kExact";
    case E::kLbfgsDamped:
        return "kLbfgsDamped";
    }
    return "unknown";
}

const char *EnumTraits<hven::SqpColumnScaling>::name(hven::SqpColumnScaling v) {
    using E = hven::SqpColumnScaling;
    switch (v) {
    case E::kOff:
        return "kOff";
    case E::kAlways:
        return "kAlways";
    case E::kAuto:
        return "kAuto";
    }
    return "unknown";
}

const char *EnumTraits<hven::SqpRestorationCurvature>::name(hven::SqpRestorationCurvature v) {
    using E = hven::SqpRestorationCurvature;
    switch (v) {
    case E::kModel:
        return "kModel";
    case E::kRegularized:
        return "kRegularized";
    case E::kModelThenRegularized:
        return "kModelThenRegularized";
    }
    return "unknown";
}

const char *EnumTraits<hven::SqpTrustRegionGrowth>::name(hven::SqpTrustRegionGrowth v) {
    using E = hven::SqpTrustRegionGrowth;
    switch (v) {
    case E::kRatio:
        return "kRatio";
    case E::kRatioOrFeasibilityStep:
        return "kRatioOrFeasibilityStep";
    case E::kAnyAccepted:
        return "kAnyAccepted";
    case E::kStepTypeRatio:
        return "kStepTypeRatio";
    }
    return "unknown";
}

} // namespace hven::detail
