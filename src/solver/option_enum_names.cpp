// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The name tables of the option enums the shared tables use (detail/solver/
// common_option_enum_names.h). ENUMERATOR NAMES, not display strings: to_string(StartLevel)
// (core/enum_names.cpp) spells "Warm", while a recorded run has to say `kWarm`, the token a
// reader can paste back into source. Each switch is exhaustive with no default label, so a new
// enumerator warns here; the trailing return covers a value that is in range for no enumerator.
//
// It also holds the registry's two numeric spellings (detail/core/options_registry.h declares
// them), fmt's: an integer in decimal, a double at its shortest round-trip form (`1e-06`). fmt's
// output is locale-independent by specification and this library compiles it with
// FMT_USE_LOCALE=0, which is what the options dump's stream-state independence rests on.

#include <string>

#include <fmt/format.h>

#include <hven/detail/solver/common_option_enum_names.h>

namespace hven::detail {

std::string format_double(double v) { return fmt::format("{}", v); }
std::string format_int(long long v) { return fmt::format("{}", v); }

const char *EnumTraits<hven::Verbosity>::name(hven::Verbosity v) {
    using E = hven::Verbosity;
    switch (v) {
    case E::kSilent:
        return "kSilent";
    case E::kWarnings:
        return "kWarnings";
    case E::kSummary:
        return "kSummary";
    case E::kIterations:
        return "kIterations";
    case E::kDetail:
        return "kDetail";
    }
    return "unknown";
}

const char *EnumTraits<hven::ReturnBest>::name(hven::ReturnBest v) {
    using E = hven::ReturnBest;
    switch (v) {
    case E::kNever:
        return "kNever";
    case E::kOnLimit:
        return "kOnLimit";
    case E::kOnAnyNonOptimal:
        return "kOnAnyNonOptimal";
    }
    return "unknown";
}

const char *EnumTraits<hven::StartLevel>::name(hven::StartLevel v) {
    using E = hven::StartLevel;
    switch (v) {
    case E::kCold:
        return "kCold";
    case E::kSeeded:
        return "kSeeded";
    case E::kWarm:
        return "kWarm";
    case E::kHot:
        return "kHot";
    }
    return "unknown";
}

const char *EnumTraits<hven::linear::DensePolicy>::name(hven::linear::DensePolicy v) {
    using E = hven::linear::DensePolicy;
    switch (v) {
    case E::kNever:
        return "kNever";
    case E::kAlways:
        return "kAlways";
    case E::kAuto:
        return "kAuto";
    }
    return "unknown";
}

const char *EnumTraits<hven::QpMode>::name(hven::QpMode v) {
    using E = hven::QpMode;
    switch (v) {
    case E::kWalk:
        return "kWalk";
    case E::kSsn:
        return "kSsn";
    case E::kIpqp:
        return "kIpqp";
    }
    return "unknown";
}

const char *EnumTraits<hven::QpDumpPolicy>::name(hven::QpDumpPolicy v) {
    using E = hven::QpDumpPolicy;
    switch (v) {
    case E::kOff:
        return "kOff";
    case E::kFailed:
        return "kFailed";
    case E::kAll:
        return "kAll";
    }
    return "unknown";
}

const char *EnumTraits<hven::QpWorkingSetAlgebra>::name(hven::QpWorkingSetAlgebra v) {
    using E = hven::QpWorkingSetAlgebra;
    switch (v) {
    case E::kRefactorize:
        return "kRefactorize";
    case E::kSchurBorder:
        return "kSchurBorder";
    }
    return "unknown";
}

const char *EnumTraits<hven::SsnSigmaRule>::name(hven::SsnSigmaRule v) {
    using E = hven::SsnSigmaRule;
    switch (v) {
    case E::kLadder:
        return "kLadder";
    case E::kResidualArmed:
        return "kResidualArmed";
    case E::kResidualAlways:
        return "kResidualAlways";
    }
    return "unknown";
}

const char *EnumTraits<hven::SsnHintRule>::name(hven::SsnHintRule v) {
    using E = hven::SsnHintRule;
    switch (v) {
    case E::kIterationZeroFree:
        return "kIterationZeroFree";
    case E::kWatchdog:
        return "kWatchdog";
    }
    return "unknown";
}

const char *EnumTraits<hven::SsnInfeasibilityRule>::name(hven::SsnInfeasibilityRule v) {
    using E = hven::SsnInfeasibilityRule;
    switch (v) {
    case E::kSymptoms:
        return "kSymptoms";
    case E::kFarkasGated:
        return "kFarkasGated";
    }
    return "unknown";
}

const char *EnumTraits<hven::IpqpBarrierInitialRule>::name(hven::IpqpBarrierInitialRule v) {
    using E = hven::IpqpBarrierInitialRule;
    switch (v) {
    case E::kFixed:
        return "kFixed";
    case E::kComplementarity:
        return "kComplementarity";
    }
    return "unknown";
}

} // namespace hven::detail
