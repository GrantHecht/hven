// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// qp_dump_writer.h -- the QP dump writer behind SqpAdvancedOptions::qp_dump_dir.
//
// One text file per walk solve, in the "D4QP 1" format (tests/qp/fixtures/e2-d4/FORMAT.txt;
// loader tests/qp/support/qp_dump.h). The format is unchanged: the major and the site are
// carried in the FILE NAME, `qp<seq>_d<depth>_m<major>_<site>.txt`, `seq` process-wide.
// The dump holds the QP, the engine's options, the solve's overrides and the seed; it does
// not record `QpProblem::infinite_bound` (the loader's default applies).
//
// Library code does not print: a directory that cannot be created or a file that cannot be
// written throws std::runtime_error naming the path.

#include <memory>
#include <string>

#include <hven/core/solver_counters.h>
#include <hven/detail/qp/qp_problem.h>
#include <hven/qp/qp_types.h>

namespace hven::detail {

/// @brief Which walk solves a dump directory receives.
enum class QpDumpPolicy { kOff, kFailed, kAll };

/// @brief Where dumps go and which solves qualify. `dir` empty or `policy == kOff` is off.
struct QpDumpConfig {
    std::string dir;
    QpDumpPolicy policy = QpDumpPolicy::kOff;
    bool enabled() const noexcept { return !dir.empty() && policy != QpDumpPolicy::kOff; }
};

/// @brief The file-name tag of the next walk solve; set by the driver before each solve.
struct QpDumpTag {
    Index depth = 0;         ///< 0 the primary driver, 1 the restoration sub-driver.
    Index major = 0;         ///< The driver's major iteration.
    std::string site = "qp"; ///< `main`, `soc`, `rung<k>`, ...: [A-Za-z0-9_] only.
};

/// @brief One finished solve, as the writer needs it.
struct QpDumpSolve {
    const QpProblem *qp = nullptr;
    const QpOptions *opts = nullptr; ///< The engine's options as constructed.
    int threads = 0;
    const QpSolveOverrides *overrides = nullptr;
    bool warm = false;
    const QpSolution *seed = nullptr; ///< Null on a cold solve.
    bool hot = false;
    QpStatus status = QpStatus::kOptimal;
    const QpCounters *counters = nullptr;
    Index max_iter = 0; ///< The cap the solve ran at.
    const Vec *x_final = nullptr;
};

/// @brief True iff @p cfg asks for a dump of a solve that ended @p status.
bool qp_dump_wanted(const QpDumpConfig &cfg, QpStatus status) noexcept;

/// @brief Writes the dump file; returns its path.
/// @throws std::runtime_error if the directory or file cannot be created or written.
std::string write_qp_dump(const QpDumpConfig &cfg, const QpDumpTag &tag, const QpDumpSolve &s);

} // namespace hven::detail
