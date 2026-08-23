// Derived from ASSET (AlabamaASRL/asset_asrl), https://github.com/AlabamaASRL/asset_asrl
// Copyright 2020-present The University of Alabama-Astrodynamics and Space Research Lab.
// Original developer: James B. Pezent. Licensed under the Apache License, Version 2.0
// (notices/asset-apache2.txt).
//
// Modified in hven. Copyright 2026-present Grant R. Hecht. Apache License, Version 2.0
// (see LICENSE).

#pragma once

namespace hven::solvers {

/// @brief Classifies how a parsed input/output slot maps into a VF's storage.
/// @ingroup vf
enum class ParsedIOFlags {
    HiddenInput = -2,  ///< @brief Slot is an input not exposed to the caller.
    IngoreOutput = -1, ///< @brief Slot is an output that should be ignored.
    NotContiguous,     ///< @brief Slot maps to a non-contiguous range.
    Contiguous,        ///< @brief Slot maps to a contiguous range.
};

} // namespace hven::solvers
