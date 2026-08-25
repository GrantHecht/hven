// Derived from ASSET (AlabamaASRL/asset_asrl), https://github.com/AlabamaASRL/asset_asrl
// Copyright 2020-present The University of Alabama-Astrodynamics and Space Research Lab.
// Original developer: James B. Pezent. Licensed under the Apache License, Version 2.0
// (notices/asset-apache2.txt).
//
// Modified in hven. Copyright 2026-present Grant R. Hecht. Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <type_traits>

namespace hven::utils {

/// @internal
/// @brief Type trait that strips both `const` and reference qualifiers from T
/// in a single step (equivalent to `std::remove_const_t<std::remove_reference_t<T>>`).
template <class T> struct remove_const_reference {
    /// @internal @brief Resolved type with const and reference removed.
    using type = typename std::remove_const<typename std::remove_reference<T>::type>::type;
};

} // namespace hven::utils
