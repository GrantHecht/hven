// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// warm_start_access.h -- the library's door into WarmStart's engine-split blocks.
//
// WarmStart (warmstart/warm_start_data.h) keeps its four blocks, its stamp and
// its extensions private: a consumer reads the primal point, z and the stamp,
// and reads or builds the multipliers in the problem's ROW SPACE. The engines,
// the byte form and the crossover work in the engine split (equality block,
// inequality block), declared space, and reach it here.

#include "hven/warmstart/warm_start_data.h"

namespace hven::detail {

struct WarmStartAccess {
    static Vec &primal(WarmStart &w) noexcept { return w.primal_; }
    static const Vec &primal(const WarmStart &w) noexcept { return w.primal_; }
    static Vec &eq(WarmStart &w) noexcept { return w.eq_lmults_; }
    static const Vec &eq(const WarmStart &w) noexcept { return w.eq_lmults_; }
    static Vec &iq(WarmStart &w) noexcept { return w.iq_lmults_; }
    static const Vec &iq(const WarmStart &w) noexcept { return w.iq_lmults_; }
    static Vec &bound(WarmStart &w) noexcept { return w.bound_lmults_; }
    static const Vec &bound(const WarmStart &w) noexcept { return w.bound_lmults_; }
    static DeclarationKey &key(WarmStart &w) noexcept { return w.structure_key_; }
    static const DeclarationKey &key(const WarmStart &w) noexcept { return w.structure_key_; }
    static std::vector<WarmExtension> &extensions(WarmStart &w) noexcept { return w.extensions_; }
    static const std::vector<WarmExtension> &extensions(const WarmStart &w) noexcept {
        return w.extensions_;
    }
};

} // namespace hven::detail
