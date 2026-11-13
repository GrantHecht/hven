// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// stop.h -- StopSource and StopToken, the cooperative stop request a caller
// hands to one solve through `SolveBudget::stop`.
//
// hven's own type rather than std::stop_token: libc++ kept <stop_token> behind
// -fexperimental-library until LLVM 20, and a public header that needed it
// would carry that flag into every consumer. Nothing here includes it.

#include <atomic>
#include <memory>

namespace hven {

class StopSource;

/// @brief A read-only view of a StopSource's flag. Copyable; every copy reads
///        the same flag.
///
/// A default-constructed token has no source: `stop_possible()` is false and
/// `stop_requested()` is a null-pointer test that returns false. That is the
/// `SolveBudget::stop` default, so a solve that is never handed a token pays one
/// null test per iteration.
class StopToken {
  public:
    StopToken() noexcept = default;

    /// @brief True when the token was taken from a StopSource.
    bool stop_possible() const noexcept { return flag_ != nullptr; }

    /// @brief True once the source's `request_stop()` has been called. One
    ///        acquire load; false on a token with no source.
    bool stop_requested() const noexcept {
        return flag_ != nullptr && flag_->load(std::memory_order_acquire);
    }

  private:
    friend class StopSource;
    explicit StopToken(std::shared_ptr<const std::atomic<bool>> flag) noexcept
        : flag_(std::move(flag)) {}
    std::shared_ptr<const std::atomic<bool>> flag_;
};

/// @brief The owner of a stop flag. `request_stop()` may be called from any
///        thread, with no lock and no callback installed.
///
/// A TOKEN IS PER SOLVE. A source whose stop was already requested stops the
/// next solve it is handed to at that solve's first safe point; there is no
/// reset. Take a new StopSource for each solve. N concurrent solves take N
/// independent sources.
///
/// The solver never writes the flag.
class StopSource {
  public:
    /// @brief Allocates the shared flag, initially clear.
    StopSource() : flag_(std::make_shared<std::atomic<bool>>(false)) {}

    /// @brief A token that reads this source's flag.
    StopToken token() const noexcept { return StopToken(flag_); }

    /// @brief Requests the stop. One release store to a lock-free atomic: no
    ///        allocation and no lock, so it is safe from a GUI thread and from
    ///        an async-signal handler that uses an EXISTING source (not one
    ///        that copy-constructs a source).
    void request_stop() noexcept { flag_->store(true, std::memory_order_release); }

    /// @brief True once `request_stop()` has been called on this source.
    bool stop_requested() const noexcept { return flag_->load(std::memory_order_acquire); }

  private:
    std::shared_ptr<std::atomic<bool>> flag_;
};

static_assert(std::atomic<bool>::is_always_lock_free,
              "StopSource::request_stop() is documented lock-free and async-signal-safe");

} // namespace hven
