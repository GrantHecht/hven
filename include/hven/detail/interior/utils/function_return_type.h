// Derived from ASSET (AlabamaASRL/asset_asrl), https://github.com/AlabamaASRL/asset_asrl
// Copyright 2020-present The University of Alabama-Astrodynamics and Space Research Lab.
// Original developer: James B. Pezent. Licensed under the Apache License, Version 2.0
// (notices/asset-apache2.txt).
//
// Modified in hven. Copyright 2026-present Grant R. Hecht. Apache License, Version 2.0
// (see LICENSE).

#pragma once

namespace hven::utils {

/// @internal
/// @brief Primary template for extracting the return type from a callable
/// type. Specializations cover free functions, member functions, and
/// cv-qualified member functions.
template <typename T> struct return_type;

/// @internal
/// @brief Specialization for plain free-function pointers.
template <typename R, typename... Args> struct return_type<R (*)(Args...)> {
    using type = R; ///< @internal The return type of the callable.
};

/// @internal
/// @brief Specialization for non-const member-function pointers.
template <typename R, typename C, typename... Args> struct return_type<R (C::*)(Args...)> {
    using type = R; ///< @internal The return type of the callable.
};

/// @internal
/// @brief Specialization for const member-function pointers.
template <typename R, typename C, typename... Args> struct return_type<R (C::*)(Args...) const> {
    using type = R; ///< @internal The return type of the callable.
};

/// @internal
/// @brief Specialization for volatile member-function pointers.
template <typename R, typename C, typename... Args> struct return_type<R (C::*)(Args...) volatile> {
    using type = R; ///< @internal The return type of the callable.
};

/// @internal
/// @brief Specialization for const-volatile member-function pointers.
template <typename R, typename C, typename... Args>
struct return_type<R (C::*)(Args...) const volatile> {
    using type = R; ///< @internal The return type of the callable.
};

/// @internal
/// @brief Convenience alias for `return_type<T>::type`.
template <typename T> using return_type_t = typename return_type<T>::type;

} // namespace hven::utils
