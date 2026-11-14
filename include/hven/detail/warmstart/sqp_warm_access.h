// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// sqp_warm_access.h -- the library's door into the opaque SqpWarmStart.
//
// The public SqpWarmStart (warmstart/sqp_warm_start.h) is a handle to an immutable
// shared state. Library internals, the continuation and mesh-transfer machinery and
// the tests that build native states work on the state itself
// (detail/warmstart/sqp_warm_state.h) and wrap or unwrap only at the public
// boundary, here.

#include <memory>
#include <utility>

#include "hven/detail/warmstart/sqp_warm_state.h"
#include "hven/warmstart/sqp_warm_start.h"

namespace hven::detail {

struct SqpWarmAccess {
    /// The cold value: nothing captured (valid() is false). The route a caller
    /// that passes a native warm start but means "none" takes, and the value a
    /// default-initialised SqpResult starts from.
    static SqpWarmStart cold() noexcept { return SqpWarmStart(SqpWarmStart::Cold{}); }

    /// The state, read-only. A cold handle reads as the one static empty state
    /// (valid == false), so a caller never branches on null.
    static const SqpWarmState &state(const SqpWarmStart &warm) noexcept {
        static const SqpWarmState kEmpty{};
        return warm.state_ != nullptr ? *warm.state_ : kEmpty;
    }

    /// The state, writable. Copy-on-write: a shared (or null) handle is first
    /// replaced by a private copy, so a write never reaches another handle that
    /// shares the state.
    static SqpWarmState &mutable_state(SqpWarmStart &warm) {
        if (warm.state_ == nullptr) {
            warm.state_ = std::make_shared<SqpWarmState>();
        } else if (warm.state_.use_count() != 1) {
            warm.state_ = std::make_shared<SqpWarmState>(*warm.state_);
        }
        // Held by a pointer to non-const (make_shared<SqpWarmState> above), sole
        // owner here: writing through it is sound.
        return const_cast<SqpWarmState &>(*warm.state_);
    }

    /// Wraps a state in the public handle.
    static SqpWarmStart wrap(SqpWarmState state) {
        SqpWarmStart out(SqpWarmStart::Cold{});
        out.state_ = std::make_shared<SqpWarmState>(std::move(state));
        return out;
    }
};

} // namespace hven::detail
