// Copyright (C) 2001-2003
// William E. Kempf
// Copyright (C) 2007-8 Anthony Williams
// (C) Copyright 2011-2012 Vicente J. Botet Escriba
// (C) Copyright 2007 David Deakins
// (C) Copyright 2007 Anthony Williams
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <hven/detail/util/get_core_count.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <Windows.h>
#include <malloc.h>
#include <stdio.h>
#include <tchar.h>
#endif // _WIN32

#if defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/sysctl.h>
#include <sys/types.h>
#endif
#if defined(__linux__)
#include <unistd.h>

#endif

// trim from start (in place)
static inline void ltrim(std::string &s) {
    s.erase(s.begin(),
            std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); })
                .base(),
            s.end());
}

int hven::detail::get_core_count() {

    int tcount = std::thread::hardware_concurrency();

#if defined(_WIN32)

    auto Run = [tcount]() {
        try {
            unsigned cores = 0;
            DWORD size = 0;
            GetLogicalProcessorInformation(NULL, &size);
            if (ERROR_INSUFFICIENT_BUFFER != GetLastError())
                return tcount;
            const size_t Elements = size / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);

            std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buffer(Elements);
            if (GetLogicalProcessorInformation(&buffer.front(), &size) == FALSE)
                return tcount;

            for (size_t i = 0; i < Elements; ++i) {
                if (buffer[i].Relationship == RelationProcessorCore)
                    ++cores;
            }
            return (int(cores));

        } catch (...) {
            return tcount;
        }
    };

#elif defined(__linux__)

    auto Run = [tcount]() {
        // Adapted from boost::thread::physical_concurrency
        // (boostorg/thread, pthread/thread.cpp), modified to work without
        // boost.

        // Same answer as the line-by-line stream parse this replaced (every
        // "physical id" / "core id" pair counted once; a non-empty line with no
        // colon, or no core id anywhere, falls back to hardware_concurrency()),
        // but read as one buffer and scanned in place: the stream version copied
        // every line (the long "flags" lines included) and cost ~0.28M
        // instructions on a 16-thread machine.
        std::FILE *fp = std::fopen("/proc/cpuinfo", "r");
        if (fp == nullptr) {
            return tcount;
        }
        std::string text;
        {
            char buf[8192];
            std::size_t got = 0;
            while ((got = std::fread(buf, 1, sizeof(buf), fp)) > 0) {
                text.append(buf, got);
            }
            std::fclose(fp);
        }
        try {
            std::set<std::pair<int, int>> cores; // [physical id, core id]
            std::pair<int, int> current{0, 0};
            const char *const end = text.data() + text.size();
            const char *line = text.data();
            while (line < end) {
                const char *eol = static_cast<const char *>(
                    std::memchr(line, '\n', static_cast<std::size_t>(end - line)));
                if (eol == nullptr) {
                    eol = end;
                }
                const char *a = line;
                const char *z = eol;
                while (a < z && std::isspace(static_cast<unsigned char>(*a))) {
                    ++a;
                }
                // The stream version skipped only lines that were EMPTY; a line of
                // whitespace has no colon and so fell back, which this keeps.
                if (line == eol) {
                    line = eol + 1;
                    continue;
                }
                const char *colon = static_cast<const char *>(
                    std::memchr(line, ':', static_cast<std::size_t>(eol - line)));
                if (colon == nullptr) {
                    return tcount;
                }
                const char *ke = colon;
                while (ke > a && std::isspace(static_cast<unsigned char>(*(ke - 1)))) {
                    --ke;
                }
                const auto klen = static_cast<std::size_t>(ke - a);
                const bool is_phys = klen == 11 && std::memcmp(a, "physical id", 11) == 0;
                const bool is_core = klen == 7 && std::memcmp(a, "core id", 7) == 0;
                if (is_phys || is_core) {
                    std::string value(colon + 1, eol);
                    const int v = std::atoi(
                        value.c_str()); // atoi skips leading space and stops at the first non-digit
                    if (is_phys) {
                        current.first = v;
                    } else {
                        current.second = v;
                        cores.insert(current);
                    }
                }
                line = eol + 1;
            }
            return cores.size() != 0 ? int(cores.size()) : tcount;
        } catch (...) {
            return tcount;
        }
    };

#elif defined(__APPLE__)
    auto Run = [tcount]() {
        int count;
        size_t size = sizeof(count);
        int result = sysctlbyname("hw.physicalcpu", &count, &size, NULL, 0);
        if (result == 0)
            return count;
        return tcount;
    };

#else
    auto Run = [tcount]() { return tcount; };
#endif

    // The physical-core probe (Run) reads /proc/cpuinfo or asks the OS, and the
    // answer is a property of the machine, not of the call: it is taken once per
    // process. tcount is NOT cached (hardware_concurrency follows the process's
    // affinity mask, which the caller may change), so the result is the same
    // min(tcount, cores) every call computed before the probe was cached. The
    // interior-point constructor calls this per solver, and the probe was ~0.28M
    // instructions of the ~0.65M a tiny solve costs (PF-7).
    static const int probed = std::max<int>(1, Run());
    int ccount = probed;
    if (tcount > 0)
        ccount = std::min<int>(tcount, ccount);
    return std::max<int>(1, ccount);
}
