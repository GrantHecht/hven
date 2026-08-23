// Derived from ASSET (AlabamaASRL/asset_asrl), https://github.com/AlabamaASRL/asset_asrl
// Copyright 2020-present The University of Alabama-Astrodynamics and Space Research Lab.
// Original developer: James B. Pezent. Licensed under the Apache License, Version 2.0
// (notices/asset-apache2.txt).
//
// Modified in hven. Copyright 2026-present Grant R. Hecht. Apache License, Version 2.0
// (see LICENSE).

// Defines the type erasure spec SizableSpec defining the ability to query
// the Input/Output rows of a type-erased vectorfunction as well as its name
// and thread safety.
//
// Modified in Tycho, then in hven (Copyright 2026-present Grant R. Hecht,
//   Apache 2.0 — see LICENSE.txt):
//   - Namespace: asset -> tycho -> hven
//   - Python binding methods moved to src/bindings/ (nanobind)
//   - PR 9: Removed dead Model<>/ExternalInterface<> boilerplate
// =============================================================================

#pragma once

#include <algorithm>
#include <array>
#include <functional>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <variant>
#include <vector>

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <Eigen/Sparse>

#include "hven/detail/interior/typedefs/eigen_types.h"
#include "hven/detail/interior/utils/flat_map.h"
#include "hven/detail/interior/utils/function_return_type.h"
#include "hven/detail/interior/utils/get_core_count.h"
#include "hven/detail/interior/utils/math_functions.h"
#include "hven/detail/interior/utils/sizing_helpers.h"
#include "hven/detail/interior/utils/std_extensions.h"
#include "hven/detail/interior/utils/thread_pool.h"
#include "hven/detail/interior/utils/type_name.h"
#include "hven/detail/interior/utils/type_storage.h"

namespace hven::solvers {

struct SizableSpec {
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    struct Concept { // abstract base class for model.
        virtual ~Concept() = default;
        // Your (internal) interface goes here.
        virtual std::string name() const = 0;

        virtual int input_rows() const = 0;
        virtual int output_rows() const = 0;
        virtual bool thread_safe() const = 0;
    };
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
};

} // namespace hven::solvers
