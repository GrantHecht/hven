// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// stop_poll.h -- the intra-iteration stop polls (M8 AP-5 G4): the IPM inertia
// ladder between attempts and the SQP main subproblem's minor loops (walk, SSN,
// IPQP). Each engine holds `detail::stop_flag(budget.stop)`, which is nullptr
// unless the caller handed the solve a token, so an unarmed poll is one null
// test. Under HVEN_TESTING an armed poll also reports to StopPollSeam
// (docs/testing.md), which can request the stop at a chosen poll.

#include <atomic>

#ifdef HVEN_TESTING
#include <vector>

#include <hven/core/stop.h>
#endif

namespace hven::detail {

enum class StopPollSite { kIpmLadder = 0, kWalk = 1, kSsn = 2, kIpqp = 3 };

#ifdef HVEN_TESTING
namespace testing {

/// Counts armed polls per site and, at the chosen one, requests the stop on
/// `source` just before the poll reads the flag. Inert while `source` is null.
struct StopPollSeam {
    static inline StopSource *source = nullptr;
    /// Armed polls seen, per site (StopPollSite order).
    static inline int polls[4] = {0, 0, 0, 0};
    /// Armed kernel solves begun, per site (walk, SSN, IPQP; the ladder has none).
    static inline int solves[4] = {0, 0, 0, 0};
    /// The site to fire at, and where: the `fire_at`-th armed poll (1-based), or the
    /// `fire_index`-th poll (0-based) of the `fire_solve`-th armed solve (1-based).
    static inline int fire_site = -1;
    static inline int fire_at = 0;
    static inline int fire_solve = 0;
    static inline int fire_index = 0;
    /// The site's poll count at which the stop was requested (-1: not yet), and every
    /// site's count at that moment (a kernel that honours the stop polls no more).
    static inline int fired_at = -1;
    static inline int polls_at_fire[4] = {0, 0, 0, 0};
    /// Non-null: one entry per armed solve of `record_site`, its poll count.
    static inline std::vector<int> *record_polls_per_solve = nullptr;
    static inline int record_site = -1;
    static inline int index_in_solve[4] = {0, 0, 0, 0};

    static void reset() {
        source = nullptr;
        for (int i = 0; i < 4; ++i) {
            polls[i] = 0;
            solves[i] = 0;
            index_in_solve[i] = 0;
            polls_at_fire[i] = 0;
        }
        fire_site = -1;
        fire_at = 0;
        fire_solve = 0;
        fire_index = 0;
        fired_at = -1;
        record_polls_per_solve = nullptr;
        record_site = -1;
    }
    static void begin_solve(StopPollSite site) {
        const int s = static_cast<int>(site);
        ++solves[s];
        index_in_solve[s] = 0;
        if (record_polls_per_solve != nullptr && s == record_site) {
            record_polls_per_solve->push_back(0);
        }
    }
    static void poll(StopPollSite site) {
        const int s = static_cast<int>(site);
        const int n = ++polls[s];
        const int idx = index_in_solve[s]++;
        if (record_polls_per_solve != nullptr && s == record_site &&
            !record_polls_per_solve->empty()) {
            ++record_polls_per_solve->back();
        }
        if (source == nullptr || fired_at >= 0 || s != fire_site) {
            return;
        }
        if ((fire_at > 0 && n == fire_at) ||
            (fire_solve > 0 && solves[s] == fire_solve && idx == fire_index)) {
            fired_at = n;
            for (int i = 0; i < 4; ++i) {
                polls_at_fire[i] = polls[i];
            }
            source->request_stop();
        }
    }
};

} // namespace testing
#endif

/// True when `flag` is non-null and a stop has been requested on it. Not a poll
/// site: the driver's read after a kernel returns.
inline bool stop_flag_set(const std::atomic<bool> *flag) noexcept {
    return flag != nullptr && flag->load(std::memory_order_acquire);
}

/// As stop_flag_set, at a poll site (the seam counts it under HVEN_TESTING).
inline bool poll_stop(const std::atomic<bool> *flag, [[maybe_unused]] StopPollSite site) noexcept {
    if (flag == nullptr) [[likely]] {
        return false;
    }
#ifdef HVEN_TESTING
    testing::StopPollSeam::poll(site);
#endif
    return flag->load(std::memory_order_acquire);
}

/// Marks the start of an armed kernel solve for the seam; nothing in production.
inline void poll_stop_begin_solve([[maybe_unused]] const std::atomic<bool> *flag,
                                  [[maybe_unused]] StopPollSite site) noexcept {
#ifdef HVEN_TESTING
    if (flag != nullptr) {
        testing::StopPollSeam::begin_solve(site);
    }
#endif
}

} // namespace hven::detail
