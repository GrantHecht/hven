// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// compiler.h -- the compiler-specific pragmas hven needs in portable form.
//
// HVEN_SUPPRESS_DEPRECATED_BEGIN / _END bracket a region in which calling a
// [[deprecated]] hven entry point is intentional and must not warn. hven uses
// it in exactly one place: NlpModel's in-place evaluation defaults, which
// delegate to the deprecated by-value forms by design (see nlp_model.h). It is
// public because a consumer mid-migration needs the same bracket -- a model
// that overrides the by-value forms and calls them itself, or a test that keeps
// a by-value evaluation as an independent oracle of the in-place path.
//
// The pair is scoped: push/pop, so it restores whatever the surrounding
// translation unit had. Wrap the smallest region that needs it.

#if defined(__clang__)
#define HVEN_SUPPRESS_DEPRECATED_BEGIN                                                             \
    _Pragma("clang diagnostic push")                                                               \
        _Pragma("clang diagnostic ignored \"-Wdeprecated-declarations\"")
#define HVEN_SUPPRESS_DEPRECATED_END _Pragma("clang diagnostic pop")
#elif defined(__GNUC__)
#define HVEN_SUPPRESS_DEPRECATED_BEGIN                                                             \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#define HVEN_SUPPRESS_DEPRECATED_END _Pragma("GCC diagnostic pop")
#elif defined(_MSC_VER)
#define HVEN_SUPPRESS_DEPRECATED_BEGIN __pragma(warning(push)) __pragma(warning(disable : 4996))
#define HVEN_SUPPRESS_DEPRECATED_END __pragma(warning(pop))
#else
#define HVEN_SUPPRESS_DEPRECATED_BEGIN
#define HVEN_SUPPRESS_DEPRECATED_END
#endif
