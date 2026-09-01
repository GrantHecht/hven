// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// ipqp_e1_arm -- CLI glue for spec section 8's A4 (the tier on E1's 29 cells),
// the Q4 `ipqp_init_mu` sweep, and the A13/A14 wall legs.
// The arm itself is bench/ipqp_e1_arm.h; its gate is test_ipqp_acceptance.cpp.

// NOT ctest-registered: one A4 pass is tens of minutes at `nx = 1e5`. Every
// mode writes a provenance-stamped CSV and each run's terms are the caller's
// (`taskset`, `MKL_NUM_THREADS`), stamped into the header rather than assumed.

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <fmt/format.h>

#include <hven/drivers/sqp_driver.h>

#include "bench_cli.h"
#include "corpus_cells.h"
#include "ipqp_e1_arm.h"
#include "support/hs_problems.h"
#include "support/indefinite_fixtures.h"

namespace {

using namespace hven;
using namespace hven::solvers;

constexpr const char *kUsage =
    "usage: hven_sqp_ipqp_e1_arm <--regen|--sweep|--mu-sweep|--envelope> --out FILE\n"
    "                            [--mu VALUE] [--sizes 20000,100000] [--nodes N] [--p P]\n";

std::string utc_now() {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

std::string env_or(const char *key, const char *fallback) {
    const char *v = std::getenv(key);
    return v != nullptr ? std::string(v) : std::string(fallback);
}

/// Every CSV this binary writes says which binary, which commit and under what
/// thread/affinity terms it was produced -- CLAUDE.md section 7's stamp.
void stamp(std::ostream &os, const std::string &mode, const std::string &extra) {
    os << "# artifact: hven IPQP E1 acceptance arm (M6 W1 T10, spec section 8 A4/Q4/A13/A14)\n";
    os << "# producer: bench/ipqp_e1_arm.cpp (hven_sqp_ipqp_e1_arm)\n";
    os << "# git describe: " << HVEN_SQP_E1_ARM_GIT_DESCRIBE << "\n";
    os << "# mode: " << mode << "\n";
    os << "# date (UTC): " << utc_now() << "\n";
    os << "# MKL_NUM_THREADS: " << env_or("MKL_NUM_THREADS", "<unset>") << "\n";
    os << "# OMP_NUM_THREADS: " << env_or("OMP_NUM_THREADS", "<unset>") << "\n";
    os << "# cell recipe: docs/notes/data/2026-08-m6-e1-acquisition/generator/e1_generate.cpp\n";
    os << "# PIQP: NEVER run, linked or read as source here; its committed CSVs are the\n";
    os << "#       oracle this arm is compared against (CLAUDE.md section 6).\n";
    if (!extra.empty()) {
        os << extra;
    }
}

std::string fnum(double v) { return fmt::format("{:.6e}", v); }

const char *status_name(QpStatus s) {
    switch (s) {
    case QpStatus::kOptimal:
        return "optimal";
    case QpStatus::kMaxIter:
        return "max_iter";
    case QpStatus::kInfeasible:
        return "infeasible";
    case QpStatus::kNumericalError:
        return "numerical_error";
    }
    return "unknown";
}

const char *sqp_status_name(SqpStatus s) {
    return s == SqpStatus::kOptimal ? "optimal" : "not_optimal";
}

/// The 39 `IpqpCounters` fields, in declaration order, as CSV.
std::string counters_header() {
    return "ipqp_iters,ipqp_factorizations,ipqp_symbolic_analyses,ipqp_solves,"
           "ipqp_pattern_verifies,ipqp_rho_demanded_max,ipqp_rho_demanded_last,"
           "ipqp_inertia_retries,ipqp_iters_at_elevated_rho,ipqp_ladder_reclimbs,"
           "ipqp_pivot_reroute_primal,ipqp_pivot_reroute_dual_fallback,"
           "ipqp_iters_ladder_armed_no_advance,ipqp_final_inertia_read,ipqp_reg_decreases,"
           "ipqp_reg_increases,ipqp_prox_center_updates,ipqp_restart_repairs,"
           "ipqp_restart_shift_max,ipqp_mu_adopted,ipqp_warm_restart_abandoned,"
           "ipqp_declined_pinned,ipqp_tier_retired_after,ipqp_face_uncertain,"
           "ipqp_refine_accepted,ipqp_refine_refused,ipqp_to_refine,ipqp_to_ssn,ipqp_to_walk,"
           "ipqp_escapes,ipqp_escape_budget,ipqp_escape_stall,ipqp_escape_indefinite,"
           "ipqp_escape_numerical,ipqp_escape_infeasible_suspect,ipqp_alpha_p_min,"
           "ipqp_alpha_d_min,ipqp_read_kept_tight_sides,ipqp_read_barrier_noise_sides";
}

std::string counters_row(const IpqpCounters &c) {
    return fmt::format(
        "{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},"
        "{},{},{},{},{},{},{},{},{},{}",
        c.ipqp_iters, c.ipqp_factorizations, c.ipqp_symbolic_analyses, c.ipqp_solves,
        c.ipqp_pattern_verifies, fnum(c.ipqp_rho_demanded_max), fnum(c.ipqp_rho_demanded_last),
        c.ipqp_inertia_retries, c.ipqp_iters_at_elevated_rho, c.ipqp_ladder_reclimbs,
        c.ipqp_pivot_reroute_primal, c.ipqp_pivot_reroute_dual_fallback,
        c.ipqp_iters_ladder_armed_no_advance, c.ipqp_final_inertia_read, c.ipqp_reg_decreases,
        c.ipqp_reg_increases, c.ipqp_prox_center_updates, c.ipqp_restart_repairs,
        fnum(c.ipqp_restart_shift_max), c.ipqp_mu_adopted, c.ipqp_warm_restart_abandoned,
        c.ipqp_declined_pinned, c.ipqp_tier_retired_after, c.ipqp_face_uncertain,
        c.ipqp_refine_accepted, c.ipqp_refine_refused, c.ipqp_to_refine, c.ipqp_to_ssn,
        c.ipqp_to_walk, c.ipqp_escapes, c.ipqp_escape_budget, c.ipqp_escape_stall,
        c.ipqp_escape_indefinite, c.ipqp_escape_numerical, c.ipqp_escape_infeasible_suspect,
        fnum(c.ipqp_alpha_p_min), fnum(c.ipqp_alpha_d_min), c.ipqp_read_kept_tight_sides,
        c.ipqp_read_barrier_noise_sides);
}

std::vector<e1arm::CellSpec> selected(const std::vector<Index> &sizes) {
    std::vector<e1arm::CellSpec> out;
    for (const e1arm::CellSpec &s : e1arm::taxonomy()) {
        const Index n = s.nodes * (e1arm::kStates + e1arm::kControls);
        if (sizes.empty() || std::find(sizes.begin(), sizes.end(), n) != sizes.end()) {
            out.push_back(s);
        }
    }
    return out;
}

int run_regen(std::ostream &os, const std::vector<Index> &sizes) {
    stamp(os, "regen",
          "# each column is the generator's own verifier quantity, %.6e, so the\n"
          "# artifact's KKT-VERIFICATION logs can be diffed against this file.\n");
    os << "id,layout,active_offset,stat_inf,stat_scale,min_inactive_slack_rel,"
          "min_active_multiplier,decile,min_box_slack,licq_d_min,licq_d_max\n";
    for (const e1arm::CellSpec &s : selected(sizes)) {
        if (s.layout == e1arm::Layout::kAnchor) {
            continue;
        }
        const e1arm::Cell cell = e1arm::build(s);
        const e1arm::RegenCertificate c = e1arm::certify(cell);
        os << fmt::format("{},{},{},{},{},{},{},{},{},{},{}\n", s.id,
                          s.layout == e1arm::Layout::kContiguous ? "contiguous" : "scattered",
                          c.active_offset, fnum(c.stat_inf), fnum(c.stat_scale),
                          fnum(c.min_inactive_slack_rel), fnum(c.min_active_multiplier),
                          fnum(c.decile), fnum(c.min_box_slack), fnum(c.licq_d_min),
                          fnum(c.licq_d_max));
        os.flush();
        std::fprintf(stderr, "regen %s done\n", s.id.c_str());
    }
    return 0;
}

int run_sweep(std::ostream &os, const std::vector<Index> &sizes, double mu, Index hard_cap,
              double converge_slack) {
    IpqpOptions iopts;
    if (mu > 0.0) {
        iopts.ipqp_init_mu = mu;
    }
    if (converge_slack > 0.0) {
        iopts.ipqp_converge_slack = converge_slack;
    }
    if (hard_cap > 0) {
        iopts.ipqp_hard_iter_cap = hard_cap;
        iopts.ipqp_max_iter = hard_cap;
        iopts.ipqp_max_factorizations = 4 * hard_cap;
    }
    stamp(os, "sweep",
          fmt::format("# ipqp_init_mu: {}\n# ipqp_hard_iter_cap: {}\n"
                      "# ipqp_converge_slack: {}\n"
                      "# wall_s is INFORMATIONAL (CLAUDE.md section 7).\n",
                      fnum(iopts.ipqp_init_mu), iopts.ipqp_hard_iter_cap,
                      fnum(iopts.ipqp_converge_slack)));
    os << "id,layout,n,me,mi,active_fraction,margin_class,status,active_true,active_found,"
          "misclassified,uncertain,rule_a,rule_b,rule_a_missed,rule_a_false_positive,"
          "res_primal,res_dual,res_comp,x_err_inf,wall_s,"
       << counters_header() << "\n";
    for (const e1arm::CellSpec &s : selected(sizes)) {
        const e1arm::Cell cell = e1arm::build(s);
        const e1arm::SolveRow r = e1arm::solve(cell, iopts);
        os << fmt::format(
            "{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}\n", r.id,
            s.layout == e1arm::Layout::kAnchor
                ? "anchor"
                : (s.layout == e1arm::Layout::kContiguous ? "contiguous" : "scattered"),
            r.n, r.me, r.mi, fnum(s.active_fraction), fnum(s.margin), status_name(r.status),
            r.active_true, r.active_found, r.misclassified, r.uncertain, r.rule_a, r.rule_b,
            r.rule_a_missed, r.rule_a_false_positive, fnum(r.res_primal), fnum(r.res_dual),
            fnum(r.res_comp), fnum(r.x_err_inf), fnum(r.wall_s), counters_row(r.counters));
        os.flush();
        std::fprintf(stderr, "sweep %s: %s iters=%lld facts=%lld wall=%.2fs\n", r.id.c_str(),
                     status_name(r.status), static_cast<long long>(r.counters.ipqp_iters),
                     static_cast<long long>(r.counters.ipqp_factorizations), r.wall_s);
    }
    return 0;
}

/// The nonconvex family and the two `path_warm` corpus cells, which the mu
/// decision is taken on beside the A4 cells (settler amendment (b)).
int run_mu_sweep(std::ostream &os, const std::vector<Index> &sizes, Index hard_cap) {
    const double mus[4] = {1e-3, 1e-2, 1e-1, 1.0};
    stamp(os, "mu-sweep",
          "# family: e1 = tier-direct A4 cell; hs/corpus = SqpDriver under kIpm;\n"
          "# indefinite = tier-direct nonconvex QP fixture.\n");
    os << "mu,family,id,status,wall_s," << counters_header() << "\n";

    const auto opts_at = [&](double mu) {
        IpqpOptions o;
        o.ipqp_init_mu = mu;
        if (hard_cap > 0) {
            o.ipqp_hard_iter_cap = hard_cap;
            o.ipqp_max_iter = hard_cap;
            o.ipqp_max_factorizations = 4 * hard_cap;
        }
        return o;
    };

    // EACH CELL IS BUILT ONCE and solved at all four levels: construction
    // dominates the arm's wall at `nx = 1e5`, and building it four times would
    // buy nothing -- the seeds make every build identical anyway.
    for (const e1arm::CellSpec &spec : selected(sizes)) {
        const e1arm::Cell cell = e1arm::build(spec);
        for (const double mu : mus) {
            const e1arm::SolveRow r = e1arm::solve(cell, opts_at(mu));
            os << fmt::format("{},e1,{},{},{},{}\n", fnum(mu), r.id, status_name(r.status),
                              fnum(r.wall_s), counters_row(r.counters));
            os.flush();
        }
        std::fprintf(stderr, "mu-sweep e1 %s done\n", spec.id.c_str());
    }

    for (const double mu : mus) {
        const IpqpOptions iopts = opts_at(mu);
        for (const int number : {10, 24, 33}) {
            const test_support::HsProblem p = test_support::make_hs(number);
            SqpOptions o;
            o.qp_mode = QpMode::kIpm;
            o.max_iter = 60;
            o.ipqp = iopts;
            SqpDriver driver(o);
            const auto t0 = std::chrono::steady_clock::now();
            const SqpSolution sol = driver.solve(*p.model);
            const double wall =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            os << fmt::format("{},hs,hs{},{},{},{}\n", fnum(mu), number,
                              sqp_status_name(sol.status), fnum(wall),
                              counters_row(sol.counters.ipqp));
            os.flush();
        }
        struct Fixture {
            const char *name;
            QpProblem qp;
        };
        std::vector<Fixture> fixtures;
        fixtures.push_back({"indefinite_equality", test_support::indefinite_equality_qp()});
        fixtures.push_back(
            {"indefinite_equality_and_row", test_support::indefinite_equality_and_row_qp()});
        fixtures.push_back(
            {"two_negative_eigenvalue_row", test_support::two_negative_eigenvalue_row_qp()});
        for (Fixture &f : fixtures) {
            QpOptions qopts;
            qopts.tr_radius = std::numeric_limits<double>::infinity();
            IpqpEngine tier(qopts);
            const auto t0 = std::chrono::steady_clock::now();
            const IpqpResult r = tier.solve(f.qp, nullptr, iopts, SolveOverrides{});
            const double wall =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            os << fmt::format("{},indefinite,{},{},{},{}\n", fnum(mu), f.name,
                              status_name(r.status), fnum(wall), counters_row(r.counters));
            os.flush();
        }
        for (const char *id : {"f7_n800_path_warm", "f7_n1000_path_warm"}) {
            const corpus::CorpusCell *cell = corpus::find_cell(id);
            if (cell == nullptr) {
                throw std::invalid_argument(fmt::format("mu-sweep: no corpus cell '{}'", id));
            }
            corpus::detail::EngineConfig cfg;
            cfg.ipqp = iopts;
            const corpus::CorpusRow row = corpus::run_cell(*cell, "ipm", {}, cfg);
            os << fmt::format("{},corpus,{},{},{},{}\n", fnum(mu), id, sqp_status_name(row.status),
                              fnum(row.wall_s), counters_row(row.ipqp));
            os.flush();
        }
        std::fprintf(stderr, "mu-sweep families mu=%g done\n", mu);
    }
    return 0;
}

/// A13/A14's wall leg: the same F7 cell solved by the walk and by the tier on
/// one occasion. WALL-ASSERTING -- the caller must run it solo and serialized.
int run_envelope(std::ostream &os, Index nodes, double p) {
    stamp(os, "envelope",
          "# WALL-ASSERTING leg: run SOLO, serialized, one solve at a time.\n"
          "# The engine column is the only difference between the two rows.\n");
    os << "engine,nodes,n,p,status,majors,wall_s,qp_iters,ipqp_iters,ipqp_factorizations\n";
    for (const char *engine : {"walk", "ipm"}) {
        test_support::F7CollocationChain model(nodes, e1arm::kStates, e1arm::kControls, p,
                                               e1arm::kRadius);
        model.set_parameters(Vec::Constant(1, p));
        SqpOptions o;
        o.max_iter = 60;
        o.qp_mode = std::string(engine) == "ipm" ? QpMode::kIpm : QpMode::kWalk;
        SqpDriver driver(o);
        const auto t0 = std::chrono::steady_clock::now();
        const SqpSolution sol = driver.solve(model);
        const double wall =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        os << fmt::format("{},{},{},{},{},{},{},{},{},{}\n", engine, nodes, model.n(), fnum(p),
                          sqp_status_name(sol.status), sol.counters.major_iters, fnum(wall),
                          sol.counters.qp_minor_iters, sol.counters.ipqp.ipqp_iters,
                          sol.counters.ipqp.ipqp_factorizations);
        os.flush();
        std::fprintf(stderr, "envelope %s: wall=%.3f s\n", engine, wall);
    }
    return 0;
}

} // namespace

int main(int argc, char **argv) {
    try {
        std::string mode, out;
        std::vector<Index> sizes;
        double mu = -1.0;
        Index nodes = 20000;
        Index hard_cap = 0;
        double converge_slack = -1.0;
        double p_window = e1arm::kBoundArcP;
        for (int i = 1; i < argc; ++i) {
            const std::string a = argv[i];
            const auto next = [&](const char *what) {
                if (i + 1 >= argc) {
                    throw std::invalid_argument(fmt::format("{}: {} needs a value", kUsage, what));
                }
                return std::string(argv[++i]);
            };
            if (a == "--regen" || a == "--sweep" || a == "--mu-sweep" || a == "--envelope") {
                mode = a.substr(2);
            } else if (a == "--out") {
                out = next("--out");
            } else if (a == "--mu") {
                mu = hven::solvers::bench_cli::parse_double(kUsage, "--mu", next("--mu"));
            } else if (a == "--hard-cap") {
                hard_cap = static_cast<Index>(
                    hven::solvers::bench_cli::parse_ll(kUsage, "--hard-cap", next("--hard-cap")));
            } else if (a == "--p") {
                p_window = hven::solvers::bench_cli::parse_double(kUsage, "--p", next("--p"));
            } else if (a == "--converge-slack") {
                converge_slack = hven::solvers::bench_cli::parse_double(kUsage, "--converge-slack",
                                                                        next("--converge-slack"));
            } else if (a == "--nodes") {
                nodes = static_cast<Index>(
                    hven::solvers::bench_cli::parse_ll(kUsage, "--nodes", next("--nodes")));
            } else if (a == "--sizes") {
                std::string spec = next("--sizes");
                std::size_t pos = 0;
                while (pos <= spec.size()) {
                    const std::size_t comma = spec.find(',', pos);
                    const std::string tok = spec.substr(pos, comma - pos);
                    if (!tok.empty()) {
                        sizes.push_back(static_cast<Index>(
                            hven::solvers::bench_cli::parse_ll(kUsage, "--sizes", tok)));
                    }
                    if (comma == std::string::npos) {
                        break;
                    }
                    pos = comma + 1;
                }
            } else {
                throw std::invalid_argument(fmt::format("{}: unknown argument '{}'", kUsage, a));
            }
        }
        if (mode.empty() || out.empty()) {
            std::fputs(kUsage, stderr);
            return 2;
        }
        std::ofstream os = hven::solvers::bench_cli::open_output_or_throw(kUsage, "--out", out);
        if (mode == "regen") {
            return run_regen(os, sizes);
        }
        if (mode == "sweep") {
            return run_sweep(os, sizes, mu, hard_cap, converge_slack);
        }
        if (mode == "mu-sweep") {
            return run_mu_sweep(os, sizes, hard_cap);
        }
        return run_envelope(os, nodes, p_window);
    } catch (const std::exception &e) {
        std::fprintf(stderr, "hven_sqp_ipqp_e1_arm: %s\n", e.what());
        return 1;
    }
}
