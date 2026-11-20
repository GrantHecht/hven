// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// get_core_count (hven/detail/util/get_core_count.h): the physical-core probe is taken once per
// process, so every call -- on any thread -- returns the same value, and that value is a usable
// thread cap: at least 1, at most the logical concurrency.

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <hven/detail/util/get_core_count.h>

namespace {

TEST(CoreCount, IsAtLeastOneAndNeverAboveTheLogicalConcurrency) {
    const int n = hven::detail::get_core_count();
    EXPECT_GE(n, 1);
    const auto logical = static_cast<int>(std::thread::hardware_concurrency());
    if (logical > 0) {
        EXPECT_LE(n, logical);
    }
}

#if defined(__linux__)
// The line-by-line stream parse the production probe replaced, kept here as the reference: the
// buffered scan must return exactly what it returned on this machine.
int reference_linux_core_count() {
    const int tcount = static_cast<int>(std::thread::hardware_concurrency());
    const auto trim = [](std::string &s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                        [](unsigned char ch) { return !std::isspace(ch); }));
        s.erase(
            std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); })
                .base(),
            s.end());
    };
    std::ifstream in("/proc/cpuinfo");
    std::set<std::pair<int, int>> cores;
    std::pair<int, int> current{0, 0};
    std::string line;
    int physical = 0;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        const auto pos = line.find(':');
        if (pos == std::string::npos) {
            physical = tcount;
            break;
        }
        std::string key = line.substr(0, pos);
        std::string value = line.erase(0, pos + 1);
        trim(key);
        trim(value);
        if (key == "physical id") {
            current.first = std::atoi(value.c_str());
        } else if (key == "core id") {
            current.second = std::atoi(value.c_str());
            cores.insert(current);
        }
    }
    if (physical == 0) {
        physical = cores.empty() ? tcount : static_cast<int>(cores.size());
    }
    int c = std::max(1, physical);
    if (tcount > 0) {
        c = std::min(tcount, c);
    }
    return std::max(1, c);
}

TEST(CoreCount, MatchesTheStreamParseOfProcCpuinfoOnThisMachine) {
    EXPECT_EQ(hven::detail::get_core_count(), reference_linux_core_count());
}
#endif

TEST(CoreCount, RepeatedCallsAndConcurrentCallsAgree) {
    const int first = hven::detail::get_core_count();
    for (int i = 0; i < 50; ++i) {
        EXPECT_EQ(hven::detail::get_core_count(), first);
    }
    std::vector<int> seen(8, 0);
    std::vector<std::thread> threads;
    threads.reserve(seen.size());
    for (std::size_t t = 0; t < seen.size(); ++t) {
        threads.emplace_back([&seen, t] { seen[t] = hven::detail::get_core_count(); });
    }
    for (auto &th : threads) {
        th.join();
    }
    for (const int v : seen) {
        EXPECT_EQ(v, first);
    }
}

} // namespace
