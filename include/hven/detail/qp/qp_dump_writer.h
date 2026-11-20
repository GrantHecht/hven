// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// qp_dump_writer.h -- the QP dump writer behind SqpAdvancedOptions::qp_dump_dir.
//
// One text file per qualifying walk solve, format and file name in docs/qp-dump-format.md.
// A file is written to a temporary name and linked into place; an existing file of the final
// name is never replaced. Library code does not print: a directory that cannot be created or a
// file that cannot be written or already exists throws std::runtime_error naming the path.

#include <cstdint>
#include <string>

#include <hven/core/solver_counters.h>
#include <hven/detail/qp/qp_problem.h>
#include <hven/qp/qp_options.h>
#include <hven/qp/qp_types.h>

namespace hven::detail {

/// @brief Where dumps go and which solves qualify. `dir` empty or `policy == kOff` is off.
struct QpDumpConfig {
    std::string dir;
    QpDumpPolicy policy = QpDumpPolicy::kOff;
    std::uint64_t solver_id = 0; ///< The driver's token in the file name.
    bool enabled() const noexcept { return !dir.empty() && policy != QpDumpPolicy::kOff; }
};

/// @brief The file-name tag of the next walk solve; set by the driver before each solve, its
///        `site` reset to the default after it.
struct QpDumpTag {
    Index depth = 0;         ///< 0 the primary driver, 1 the restoration sub-driver.
    Index major = -1;        ///< The driver's major iteration; -1 untagged.
    std::string site = "qp"; ///< `main`, `soc`, `retry`, `fallback`, `rung<k>`: [A-Za-z0-9_].
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

/// @brief A process-unique driver token for QpDumpConfig::solver_id.
std::uint64_t next_qp_dump_solver_id() noexcept;

/// @brief True iff @p cfg asks for a dump of a solve that ended @p status.
bool qp_dump_wanted(const QpDumpConfig &cfg, QpStatus status) noexcept;

/// @brief Writes the dump file; returns its path.
/// @throws std::runtime_error if the directory cannot be created, the file cannot be written,
///         or a file of that name exists.
std::string write_qp_dump(const QpDumpConfig &cfg, const QpDumpTag &tag, const QpDumpSolve &s);

} // namespace hven::detail
