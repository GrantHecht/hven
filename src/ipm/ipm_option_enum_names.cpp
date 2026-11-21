// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The name tables of the interior-point option enums (detail/ipm/ipm_option_enum_names.h). Each
// switch is exhaustive with no default label, so a new enumerator warns here; the trailing
// return covers a value that is in range for no enumerator.

#include <hven/detail/ipm/ipm_option_enum_names.h>

namespace hven::detail {

const char *EnumTraits<hven::IpmPhase>::name(hven::IpmPhase v) {
    using E = hven::IpmPhase;
    switch (v) {
    case E::kOptimize:
        return "kOptimize";
    case E::kSolve:
        return "kSolve";
    }
    return "unknown";
}

const char *EnumTraits<hven::IpmFixedVariableTreatment>::name(hven::IpmFixedVariableTreatment v) {
    using E = hven::IpmFixedVariableTreatment;
    switch (v) {
    case E::kMakeParameter:
        return "kMakeParameter";
    case E::kMakeConstraint:
        return "kMakeConstraint";
    case E::kRelaxBounds:
        return "kRelaxBounds";
    }
    return "unknown";
}

const char *EnumTraits<hven::IpmOrdering>::name(hven::IpmOrdering v) {
    using E = hven::IpmOrdering;
    switch (v) {
    case E::kMinimumDegree:
        return "kMinimumDegree";
    case E::kNestedDissection:
        return "kNestedDissection";
    case E::kParallelNestedDissection:
        return "kParallelNestedDissection";
    }
    return "unknown";
}

const char *EnumTraits<hven::IpmPivotStrategy>::name(hven::IpmPivotStrategy v) {
    using E = hven::IpmPivotStrategy;
    switch (v) {
    case E::kOneByOne:
        return "kOneByOne";
    case E::kTwoByTwo:
        return "kTwoByTwo";
    }
    return "unknown";
}

const char *EnumTraits<hven::IpmSolveParallelism>::name(hven::IpmSolveParallelism v) {
    using E = hven::IpmSolveParallelism;
    switch (v) {
    case E::kAdaptivePartitioning:
        return "kAdaptivePartitioning";
    case E::kSequential:
        return "kSequential";
    }
    return "unknown";
}

const char *EnumTraits<hven::IpmSolvePhaseMode>::name(hven::IpmSolvePhaseMode v) {
    using E = hven::IpmSolvePhaseMode;
    switch (v) {
    case E::kOptimize:
        return "kOptimize";
    case E::kOptimizeWithoutObjective:
        return "kOptimizeWithoutObjective";
    case E::kSolveEquations:
        return "kSolveEquations";
    }
    return "unknown";
}

const char *EnumTraits<hven::IpmBarrierMode>::name(hven::IpmBarrierMode v) {
    using E = hven::IpmBarrierMode;
    switch (v) {
    case E::kProbe:
        return "kProbe";
    case E::kLoqo:
        return "kLoqo";
    }
    return "unknown";
}

const char *EnumTraits<hven::IpmLineSearchMode>::name(hven::IpmLineSearchMode v) {
    using E = hven::IpmLineSearchMode;
    switch (v) {
    case E::kAugmentedLagrangian:
        return "kAugmentedLagrangian";
    case E::kLagrangian:
        return "kLagrangian";
    case E::kL1:
        return "kL1";
    case E::kNone:
        return "kNone";
    }
    return "unknown";
}

const char *EnumTraits<hven::IpmStepLengthRule>::name(hven::IpmStepLengthRule v) {
    using E = hven::IpmStepLengthRule;
    switch (v) {
    case E::kEqualityMultipliersPrimal:
        return "kEqualityMultipliersPrimal";
    case E::kAllMinimum:
        return "kAllMinimum";
    case E::kEqualityMultipliersDual:
        return "kEqualityMultipliersDual";
    case E::kEqualityMultipliersMaximum:
        return "kEqualityMultipliersMaximum";
    }
    return "unknown";
}

const char *EnumTraits<hven::IpmAcceptanceMode>::name(hven::IpmAcceptanceMode v) {
    using E = hven::IpmAcceptanceMode;
    switch (v) {
    case E::kClassicMerit:
        return "kClassicMerit";
    case E::kMerit:
        return "kMerit";
    case E::kFunnel:
        return "kFunnel";
    case E::kFilter:
        return "kFilter";
    }
    return "unknown";
}

const char *EnumTraits<hven::IpmMeritPenaltyRule>::name(hven::IpmMeritPenaltyRule v) {
    using E = hven::IpmMeritPenaltyRule;
    switch (v) {
    case E::kSinglePenalty:
        return "kSinglePenalty";
    case E::kFlexible:
        return "kFlexible";
    }
    return "unknown";
}

const char *EnumTraits<hven::IpmBarrierGovernorMode>::name(hven::IpmBarrierGovernorMode v) {
    using E = hven::IpmBarrierGovernorMode;
    switch (v) {
    case E::kClassicAdaptive:
        return "kClassicAdaptive";
    case E::kMonitored:
        return "kMonitored";
    }
    return "unknown";
}

const char *EnumTraits<hven::IpmRestorationMode>::name(hven::IpmRestorationMode v) {
    using E = hven::IpmRestorationMode;
    switch (v) {
    case E::kOff:
        return "kOff";
    case E::kProximalSwitch:
        return "kProximalSwitch";
    case E::kL1Nested:
        return "kL1Nested";
    }
    return "unknown";
}

const char *EnumTraits<hven::IpmInertiaMode>::name(hven::IpmInertiaMode v) {
    using E = hven::IpmInertiaMode;
    switch (v) {
    case E::kClassic:
        return "kClassic";
    case E::kProximalRegularization:
        return "kProximalRegularization";
    }
    return "unknown";
}

} // namespace hven::detail
