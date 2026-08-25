// Derived from ASSET (AlabamaASRL/asset_asrl), https://github.com/AlabamaASRL/asset_asrl
// Copyright 2020-present The University of Alabama-Astrodynamics and Space Research Lab.
// Original developer: James B. Pezent. Licensed under the Apache License, Version 2.0
// (notices/asset-apache2.txt).
//
// Modified in hven. Copyright 2026-present Grant R. Hecht. Apache License, Version 2.0
// (see LICENSE).

#pragma once
namespace hven::utils {

/// @brief Return the number of physical CPU cores on the current machine.
///
/// Uses platform-specific APIs (e.g. `sysconf`, `GetSystemInfo`) to query the
/// physical core count. Falls back to `std::thread::hardware_concurrency()`
/// when the platform query is unavailable or fails.
///
/// @return The physical core count, or 1 if detection fails.
int get_core_count();

} // namespace hven::utils
