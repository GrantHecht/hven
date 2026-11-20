// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// qp_dump_writer.cpp -- implements hven/detail/qp/qp_dump_writer.h. Not on any hot path: it runs
// once per qualifying walk solve and only when a dump directory is configured.

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include <fmt/format.h>

#include <hven/detail/qp/qp_dump_writer.h>

namespace hven::detail {

namespace {

std::atomic<std::uint64_t> g_dump_seq{0};
std::atomic<std::uint64_t> g_solver_id{0};

long process_token() noexcept {
#ifdef _WIN32
    return static_cast<long>(_getpid());
#else
    return static_cast<long>(getpid());
#endif
}

void put_vec(std::string &o, const char *name, const Vec &v) {
    o += fmt::format("{} {}\n", name, v.size());
    for (Index i = 0; i < v.size(); ++i) {
        o += fmt::format("{:.17g}\n", v(i));
    }
}

void put_mat(std::string &o, const char *name, const SpMatRM &A) {
    o += fmt::format("{} {} {} {}\n", name, A.rows(), A.cols(), A.nonZeros());
    for (Index k = 0; k < A.outerSize(); ++k) {
        for (SpMatRM::InnerIterator it(A, k); it; ++it) {
            o += fmt::format("{} {} {:.17g}\n", it.row(), it.col(), it.value());
        }
    }
}

void put_ints(std::string &o, const char *name, const std::vector<long> &v) {
    o += fmt::format("{} {}\n", name, v.size());
    for (const long x : v) {
        o += fmt::format("{}\n", x);
    }
}

} // namespace

std::uint64_t next_qp_dump_solver_id() noexcept { return g_solver_id.fetch_add(1) + 1; }

bool qp_dump_wanted(const QpDumpConfig &cfg, QpStatus status) noexcept {
    return cfg.enabled() && (cfg.policy == QpDumpPolicy::kAll || status != QpStatus::kOptimal);
}

std::string write_qp_dump(const QpDumpConfig &cfg, const QpDumpTag &tag, const QpDumpSolve &s) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(cfg.dir, ec);
    if (ec) {
        throw std::runtime_error(
            fmt::format("QP dump: cannot create directory '{}': {}", cfg.dir, ec.message()));
    }
    const QpProblem &qp = *s.qp;
    const QpOptions &op = *s.opts;
    const QpSolveOverrides &ov = *s.overrides;
    const QpCounters &c = *s.counters;
    std::string o;
    o += "D4QP 1\n";
    o += fmt::format("dims {} {} {}\n", qp.n(), qp.me(), qp.mi());
    o += fmt::format("infinite_bound {:.17g}\n", qp.infinite_bound);
    o += fmt::format("opts {:.17g} {:.17g} {:.17g} {:.17g} {} {} {:.17g} {} {:.17g} {} {}\n",
                     op.primal_regularization, op.dual_regularization, op.feasibility_tol,
                     op.optimality_tol, s.max_iter, op.max_schur_border_dimension,
                     op.schur_condition_max,
                     op.working_set_algebra == QpWorkingSetAlgebra::kRefactorize ? 0 : 1,
                     op.trust_region_radius, op.independent_working_set ? 1 : 0, s.threads);
    o += fmt::format("overrides {:.17g} {:.17g} {:.17g}\n", ov.trust_region_radius,
                     ov.primal_regularization, ov.dual_regularization);
    o += fmt::format("warm {} seed {} hot {} k0_reused {}\n", s.warm ? 1 : 0,
                     s.seed != nullptr ? 1 : 0, s.hot ? 1 : 0, static_cast<long>(c.k0_reused));
    o += fmt::format("result status={} minors={} facts={} hkc={} maxiter={}\n",
                     static_cast<int>(s.status), static_cast<long>(c.minor_iters),
                     static_cast<long>(c.factorizations), op.hessian_known_convex ? 1 : 0,
                     s.max_iter);
    put_mat(o, "H", qp.H);
    put_vec(o, "g", qp.g);
    put_mat(o, "Ae", qp.Ae);
    put_vec(o, "be", qp.be);
    put_mat(o, "Ai", qp.Ai);
    put_vec(o, "bi", qp.bi);
    put_vec(o, "lower", qp.lower);
    put_vec(o, "upper", qp.upper);
    if (s.seed != nullptr) {
        put_vec(o, "seed_x", s.seed->x);
        std::vector<long> sb;
        std::vector<long> sa;
        for (const BoundState b : s.seed->bound_state) {
            sb.push_back(static_cast<long>(b));
        }
        for (const bool a : s.seed->ineq_active) {
            sa.push_back(a ? 1 : 0);
        }
        put_ints(o, "seed_bound_state", sb);
        put_ints(o, "seed_ineq_active", sa);
    }
    put_vec(o, "x_final", *s.x_final);

    const std::string name =
        fmt::format("qp_p{}_s{}_{:05d}_d{}_m{}_{}.txt", process_token(), cfg.solver_id,
                    g_dump_seq.fetch_add(1), tag.depth, tag.major, tag.site);
    const fs::path final_path = fs::path(cfg.dir) / name;
    const fs::path tmp_path = fs::path(cfg.dir) / (name + ".tmp");
    const std::string path = final_path.string();
    {
        std::ofstream f(tmp_path, std::ios::binary | std::ios::trunc);
        f << o;
        f.flush();
        if (!f) {
            fs::remove(tmp_path, ec);
            throw std::runtime_error(fmt::format("QP dump: cannot write '{}'", tmp_path.string()));
        }
    }
    // Linked into place, never renamed over: an existing file of the final name is an error.
    fs::create_hard_link(tmp_path, final_path, ec);
    std::error_code ec_rm;
    fs::remove(tmp_path, ec_rm);
    if (ec) {
        throw std::runtime_error(
            fmt::format("QP dump: cannot create '{}': {}", path,
                        fs::exists(final_path) ? "the file exists" : ec.message()));
    }
    return path;
}

} // namespace hven::detail
