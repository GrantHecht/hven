// Derived from ASSET (AlabamaASRL/asset_asrl), https://github.com/AlabamaASRL/asset_asrl
// Copyright 2020-present The University of Alabama-Astrodynamics and Space Research Lab.
// Original developer: James B. Pezent. Licensed under the Apache License, Version 2.0
// (notices/asset-apache2.txt).
//
// Modified in hven. Copyright 2026-present Grant R. Hecht. Apache License, Version 2.0
// (see LICENSE).

#include "hven/detail/interior/utils/memory_management.h"

namespace hven::utils {

thread_local BumpAllocator::SuperScalarStackType BumpAllocator::SuperScalarStack =
    detail::BumpStack<hven::DefaultSuperScalar>(HVEN_DEFAULT_ARENA_SIZE);
thread_local BumpAllocator::ScalarStackType BumpAllocator::ScalarStack =
    detail::BumpStack<double>(HVEN_DEFAULT_ARENA_SIZE);

} // namespace hven::utils
