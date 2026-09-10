// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#ifndef HVEN_TESTS_SUPPORT_CONSOLE_CAPTURE_H
#define HVEN_TESTS_SUPPORT_CONSOLE_CAPTURE_H

// The console-capture helper the live console pins share (M6 W5 T8.7 fix1,
// astra's I2).
//
// THE DIRECTORY IS `tests/common_support/`, NOT `tests/support/` (M6 W5 T8.7b,
// the lane's third rider). `tests/sqp/` has a `support/` of its own, so a
// quoted `"support/console_capture.h"` written from a file in that directory
// resolves to a DIFFERENT tree than the same spelling written from
// `tests/interior/`. One unambiguous name removes the trap without a CMake
// include-directory change in every test target: every suite reaches this file
// as `"../common_support/console_capture.h"`, which resolves to exactly one
// place from anywhere under `tests/`.
//
// WHY IT EXISTS. Both console-capture suites -- tests/sqp/test_trace_writer.cpp
// and tests/interior/test_ipm_trace.cpp -- had a private copy of the same
// class, and each copy included <unistd.h> and called ::dup/::dup2/::fileno
// unconditionally. Both targets are built on Windows, where that header does
// not exist and those names are spelled with a leading underscore in <io.h>, so
// the suites would not COMPILE there. "Windows is UNOBSERVED" is a statement
// about values nobody has measured on that platform; it is not a licence to
// write source that cannot be built on it. This file is the one copy, and it is
// spelled portably.
//
// WHAT TO REACH FOR, IN ORDER.
//
//   1. A `std::FILE *` from `std::tmpfile()`, handed to the sink. Both
//      `ConsoleTraceSink` and `JsonLinesTraceSink` take their stream as a
//      constructor argument, so a test that builds its own sink needs no
//      descriptor games at all and must not use this class.
//   2. `StdoutCapture`, below -- and ONLY for what a test cannot reach any
//      other way: the output of a console a DRIVER built for itself, which by
//      construction writes to the process's `stdout`, and the interior-point
//      engine's remaining direct prints (the per-phase lines, the KKT-analysis
//      block, the warnings, the exit verdict), which write there too.
//
// WINDOWS IS STILL UNOBSERVED. Nothing here has been executed on Windows or on
// macOS; what this file changes is that the source is now WRITABLE there --
// `_dup`/`_dup2`/`_fileno`/`_close` from <io.h> are the documented MSVC
// spellings of the four POSIX calls used, with the same semantics for this
// use. Whether the suites PASS on Windows remains unobserved and is reported as
// such.

#include <cstdio>
#include <string>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace hven::testing {

/// @brief Redirects the process's `stdout` into a temporary file for the
///        lifetime of the object, and hands back everything written to it.
///
/// The original `stdout` is restored on destruction, an exception path
/// included. Not copyable: it owns a duplicated descriptor and a `FILE *`.
class StdoutCapture {
  public:
    StdoutCapture() : file_(std::tmpfile()) {
        std::fflush(stdout);
        if (file_ != nullptr) {
            saved_ = dup_fd(fileno_of(stdout));
            dup2_fd(fileno_of(file_), fileno_of(stdout));
        }
    }

    ~StdoutCapture() {
        std::fflush(stdout);
        if (saved_ >= 0) {
            dup2_fd(saved_, fileno_of(stdout));
            close_fd(saved_);
        }
        if (file_ != nullptr) {
            std::fclose(file_);
        }
    }

    StdoutCapture(const StdoutCapture &) = delete;
    StdoutCapture &operator=(const StdoutCapture &) = delete;
    StdoutCapture(StdoutCapture &&) = delete;
    StdoutCapture &operator=(StdoutCapture &&) = delete;

    /// @brief Everything written to `stdout` since construction.
    ///
    /// Repeatable: the read position is left at the end, so a later call sees
    /// the whole capture again rather than only what arrived since.
    std::string text() {
        if (file_ == nullptr) {
            return std::string();
        }
        std::fflush(stdout);
        std::fseek(file_, 0, SEEK_END);
        const long n = std::ftell(file_);
        std::string out(static_cast<std::size_t>(n < 0 ? 0 : n), '\0');
        std::fseek(file_, 0, SEEK_SET);
        const std::size_t got = std::fread(out.data(), 1, out.size(), file_);
        out.resize(got);
        std::fseek(file_, 0, SEEK_END);
        return out;
    }

    /// @brief True when the redirection is actually in place; false only if the
    ///        platform refused a temporary file.
    ///
    /// EVERY CAPTURE SITE CHECKS IT (M6 W5 T8.7b, the lane's second rider). The
    /// doc used to say callers "should" and none did, so a platform that
    /// refused `std::tmpfile()` would have turned every console pin into a
    /// comparison of two empty strings that passes. The check is `EXPECT_TRUE`
    /// rather than `ASSERT_TRUE` because most of those sites sit inside lambdas
    /// that return a value, where `ASSERT_` does not compile.
    bool active() const { return file_ != nullptr && saved_ >= 0; }

  private:
    // THE FOUR CALLS, IN ONE PLACE. MSVC's CRT deprecates the POSIX spellings
    // and provides the underscore-prefixed ones; every other platform this
    // project builds on provides the POSIX ones through <unistd.h>.
    static int fileno_of(std::FILE *f) {
#ifdef _WIN32
        return _fileno(f);
#else
        return ::fileno(f);
#endif
    }
    static int dup_fd(int fd) {
#ifdef _WIN32
        return _dup(fd);
#else
        return ::dup(fd);
#endif
    }
    static int dup2_fd(int from, int to) {
#ifdef _WIN32
        return _dup2(from, to);
#else
        return ::dup2(from, to);
#endif
    }
    static void close_fd(int fd) {
#ifdef _WIN32
        _close(fd);
#else
        ::close(fd);
#endif
    }

    std::FILE *file_ = nullptr;
    int saved_ = -1;
};

} // namespace hven::testing

#endif // HVEN_TESTS_SUPPORT_CONSOLE_CAPTURE_H
