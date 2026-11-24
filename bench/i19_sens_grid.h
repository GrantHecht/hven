// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The I19-2a SENS-P grid (hven_sqp_i19_grid): the in-repo parametric families as cold SQP
// solves, one CSV row per cell. A header so tests/bench/test_i19_sens_grid.cpp exercises this
// implementation, not a copy. bench/i19_sens_grid.cpp is CLI glue only.
//
// Cell file: tab-separated, one header line, then one cell per line; '#' lines are comments.
//     cell_id  set  problem  base  family  size  p  caller_step_limit
// `set` is the cell's family label (sens-x, sens-q, senscap-x, senscap-q); `base` is `default`
// (kExact) or `sqp-qn` (kLbfgsDamped); `family` is one of F2CircleNlp, F3SpringChain,
// F4MovingConstraints, F6PathBoundQuadrature, F7CollocationChain; `size` is the family's size
// (F3 chain length, F6 mesh nodes, F7 collocation nodes; ignored by F2 and F4); `p` is the
// design point.
//
// The `sensfix` family (sets sensfix-x, sensfix-q): `family` is one of F-CYC, F-H, F-SOC, F-CAP,
// the four radius fixtures of tests/sqp/support/i19_radius_fixtures.h, each solved at the
// parameters pinned there (objective scale, SOC, iteration limit, cap, start point). `p` is the
// fixture's radius rho and `caller_step_limit` its cap, both written in the file for the reader
// and checked against the fixture (a drifted file is refused); `size` is 0.

#include <cmath>
#include <cstdlib>
#include <istream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <fmt/format.h>

#include <hven/detail/model/problem_access.h>
#include <hven/hven.h>

#include "model/support/parametric_families.h"
#include "model/support/scale_problems.h"
#include "sqp/support/i19_radius_fixtures.h"

namespace hven::bench::i19 {

struct SensCell {
    std::string cell_id;
    std::string set;
    std::string problem;
    std::string base;
    std::string family;
    Index size = 0;
    double p = 0.0;
    double caller_step_limit = 0.0;
};

inline std::vector<std::string> split_tabs(const std::string &line) {
    std::vector<std::string> out;
    std::string field;
    std::istringstream is(line);
    while (std::getline(is, field, '\t')) {
        out.push_back(field);
    }
    return out;
}

inline double parse_double(const std::string &s, const std::string &what) {
    if (s == "inf" || s == "+inf") {
        return std::numeric_limits<double>::infinity();
    }
    char *end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (s.empty() || end != s.c_str() + s.size() || !std::isfinite(v)) {
        throw std::invalid_argument(
            fmt::format("i19 grid: {} '{}' is not a finite number", what, s));
    }
    return v;
}

inline Index parse_index(const std::string &s, const std::string &what) {
    char *end = nullptr;
    const long long v = std::strtoll(s.c_str(), &end, 10);
    if (s.empty() || end != s.c_str() + s.size()) {
        throw std::invalid_argument(fmt::format("i19 grid: {} '{}' is not an integer", what, s));
    }
    return static_cast<Index>(v);
}

inline const std::vector<std::string> &cell_columns() {
    static const std::vector<std::string> cols = {
        "cell_id", "set", "problem", "base", "family", "size", "p", "caller_step_limit"};
    return cols;
}

inline std::vector<SensCell> parse_cells(std::istream &in) {
    std::vector<SensCell> cells;
    std::string line;
    bool header = false;
    std::map<std::string, bool> ids;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const std::vector<std::string> f = split_tabs(line);
        if (!header) {
            if (f != cell_columns()) {
                throw std::invalid_argument("i19 grid: the cell file's header is not "
                                            "cell_id/set/problem/base/family/size/p/"
                                            "caller_step_limit, tab-separated");
            }
            header = true;
            continue;
        }
        if (f.size() != cell_columns().size()) {
            throw std::invalid_argument(
                fmt::format("i19 grid: a cell line has {} fields, not 8: '{}'", f.size(), line));
        }
        SensCell c{f[0],
                   f[1],
                   f[2],
                   f[3],
                   f[4],
                   parse_index(f[5], "size"),
                   parse_double(f[6], "p"),
                   parse_double(f[7], "caller_step_limit")};
        if (c.base != "default" && c.base != "sqp-qn") {
            throw std::invalid_argument(fmt::format(
                "i19 grid: cell {} base '{}' is not default or sqp-qn", c.cell_id, c.base));
        }
        if (ids.count(c.cell_id) != 0) {
            throw std::invalid_argument(fmt::format("i19 grid: duplicate cell id {}", c.cell_id));
        }
        ids[c.cell_id] = true;
        cells.push_back(std::move(c));
    }
    if (!header) {
        throw std::invalid_argument("i19 grid: the cell file has no header line");
    }
    return cells;
}

/// The in-repo radius fixtures (the `sensfix` family), by the `family` column.
inline bool is_fixture(const SensCell &c) {
    return c.family == "F-CYC" || c.family == "F-H" || c.family == "F-SOC" || c.family == "F-CAP";
}

/// The fixture a `sensfix` cell names, in the cell's Hessian mode. Throws when the cell's `p`
/// (rho) or `caller_step_limit` disagrees with the pinned fixture.
inline hven::test_support::I19Fixture make_fixture(const SensCell &c) {
    using namespace hven::test_support;
    const SqpHessianMode mode =
        c.base == "sqp-qn" ? SqpHessianMode::kLbfgsDamped : SqpHessianMode::kExact;
    I19Fixture f = c.family == "F-CYC"   ? f_cyc(mode)
                   : c.family == "F-H"   ? f_h(mode)
                   : c.family == "F-SOC" ? f_soc(mode)
                                         : f_cap(mode);
    if (c.p != f.model->rho || c.caller_step_limit != f.options.caller_step_limit) {
        throw std::invalid_argument(fmt::format(
            "i19 grid: cell {} (p {}, cap {}) disagrees with the pinned {} (rho {}, cap {})",
            c.cell_id, c.p, c.caller_step_limit, c.family, f.model->rho,
            f.options.caller_step_limit));
    }
    return f;
}

inline std::unique_ptr<detail::ParametricNlpModel> make_model(const SensCell &c) {
    using namespace hven::test_support;
    if (is_fixture(c)) {
        (void)make_fixture(c); // validation only: the fixture is solved through run_cell
        return nullptr;
    }
    if (c.family == "F2CircleNlp") {
        return std::make_unique<F2CircleNlp>(c.p);
    }
    if (c.family == "F3SpringChain") {
        return std::make_unique<F3SpringChain>(c.size, 0.5, c.p);
    }
    if (c.family == "F4MovingConstraints") {
        return std::make_unique<F4MovingConstraints>(c.p, c.p);
    }
    if (c.family == "F6PathBoundQuadrature") {
        const Vec nodes = uniform_nodes(c.size);
        return std::make_unique<F6PathBoundQuadrature>(nodes, trapezoid_weights(nodes), c.p);
    }
    if (c.family == "F7CollocationChain") {
        return std::make_unique<F7CollocationChain>(c.size, 3, 2, c.p);
    }
    throw std::invalid_argument(
        fmt::format("i19 grid: cell {} names an unknown family '{}'", c.cell_id, c.family));
}

inline bool parse_bool(const std::string &key, const std::string &v) {
    if (v == "true") {
        return true;
    }
    if (v == "false") {
        return false;
    }
    throw std::invalid_argument(fmt::format("i19 grid: --set {}={}: not true/false", key, v));
}

/// One `--set key=value`, the declared I19-2a arm keys only; anything else is refused.
inline void apply_set(SqpOptions &o, const std::string &kv) {
    const auto eq = kv.find('=');
    if (eq == std::string::npos) {
        throw std::invalid_argument(fmt::format("i19 grid: --set '{}' is not key=value", kv));
    }
    const std::string k = kv.substr(0, eq);
    const std::string v = kv.substr(eq + 1);
    SqpAdvancedOptions &a = o.advanced;
    if (k == "advanced.trust_region_growth") {
        if (v == "kRatio") {
            a.trust_region_growth = SqpTrustRegionGrowth::kRatio;
        } else if (v == "kRatioOrFeasibilityStep") {
            a.trust_region_growth = SqpTrustRegionGrowth::kRatioOrFeasibilityStep;
        } else if (v == "kAnyAccepted") {
            a.trust_region_growth = SqpTrustRegionGrowth::kAnyAccepted;
        } else if (v == "kRatioByStepType") {
            a.trust_region_growth = SqpTrustRegionGrowth::kRatioByStepType;
        } else {
            throw std::invalid_argument(fmt::format("i19 grid: --set {}={}: unknown value", k, v));
        }
    } else if (k == "advanced.trust_region_growth_after_soc") {
        a.trust_region_growth_after_soc = parse_bool(k, v);
    } else if (k == "advanced.trust_region_rejection_memory") {
        a.trust_region_rejection_memory = parse_index(v, k);
    } else if (k == "advanced.trust_region_growth_ratio_max") {
        a.trust_region_growth_ratio_max = parse_double(v, k);
    } else if (k == "advanced.trust_region_shrink_to_step") {
        a.trust_region_shrink_to_step = parse_bool(k, v);
    } else if (k == "advanced.trust_region_growth_on_step_limit") {
        a.trust_region_growth_on_step_limit = parse_bool(k, v);
    } else if (k == "advanced.trust_region_growth_funnel_guard") {
        a.trust_region_growth_funnel_guard = parse_bool(k, v);
    } else if (k == "advanced.second_order_correction") {
        a.second_order_correction = parse_bool(k, v);
    } else if (k == "trust_region_initial") {
        o.trust_region_initial = parse_double(v, k);
    } else if (k == "caller_step_limit") {
        o.caller_step_limit = parse_double(v, k);
    } else {
        throw std::invalid_argument(
            fmt::format("i19 grid: --set {}: not a declared I19-2a arm key", k));
    }
}

/// The cell's options: silent, the base's Hessian mode, the cell's cap, then every `--set` in
/// order (a `--set caller_step_limit` overrides the cell's). Validated.
inline SqpOptions cell_options(const SensCell &c, const std::vector<std::string> &sets) {
    SqpOptions o = is_fixture(c) ? make_fixture(c).options : SqpOptions{};
    o.common.verbosity = Verbosity::kSilent;
    o.hessian_mode = c.base == "sqp-qn" ? SqpHessianMode::kLbfgsDamped : SqpHessianMode::kExact;
    o.caller_step_limit = c.caller_step_limit;
    for (const std::string &kv : sets) {
        apply_set(o, kv);
    }
    validate(o);
    return o;
}

inline std::string join_sets(const std::vector<std::string> &sets) {
    std::string out;
    for (const std::string &s : sets) {
        out += (out.empty() ? "" : "+") + s;
    }
    return out;
}

/// The fixed leading columns; dump_counters' lines follow, one column each, in its order.
inline const std::vector<std::string> &row_columns() {
    static const std::vector<std::string> cols = {"cell_id",
                                                  "set",
                                                  "problem",
                                                  "base",
                                                  "overrides",
                                                  "status",
                                                  "factorizations",
                                                  "eval_f",
                                                  "eval_jac",
                                                  "major_iters",
                                                  "rejected_steps",
                                                  "reject_funnel_ceiling",
                                                  "trust_region_growths",
                                                  "trust_region_growth_refusals",
                                                  "trust_region_shrinks_to_step",
                                                  "trust_region_grown_step_rejections",
                                                  "trust_region_unbound_rejections",
                                                  "step_limit_binding_majors",
                                                  "objective"};
    return cols;
}

/// dump_counters' `name = value` lines, in order.
inline std::vector<std::pair<std::string, std::string>> counter_columns(const SqpResult &r) {
    std::ostringstream os;
    dump_counters(r, os);
    std::istringstream is(os.str());
    std::vector<std::pair<std::string, std::string>> out;
    std::string line;
    while (std::getline(is, line)) {
        const auto at = line.find(" = ");
        if (at == std::string::npos) {
            throw std::runtime_error("i19 grid: dump_counters line without ' = ': " + line);
        }
        // A list value ([a, b, ...]) is written with ';' between entries: one CSV cell.
        std::string v = line.substr(at + 3);
        std::string cell;
        for (const char ch : v) {
            if (ch == ',') {
                cell += ';';
            } else if (ch != ' ') {
                cell += ch;
            }
        }
        out.emplace_back("counter." + line.substr(0, at), cell);
    }
    return out;
}

inline std::string csv_header(const SqpResult &any) {
    std::string h;
    for (const std::string &c : row_columns()) {
        h += (h.empty() ? "" : ",") + c;
    }
    for (const auto &[k, v] : counter_columns(any)) {
        h += "," + k;
    }
    return h;
}

inline std::string csv_row(const SensCell &c, const std::vector<std::string> &sets,
                           const SqpResult &r) {
    const SqpCounters &k = r.counters;
    std::string row = fmt::format(
        "{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{:.17g}", c.cell_id, c.set,
        c.problem, c.base, join_sets(sets), to_string(r.status), r.work.factorizations,
        r.work.objective_evals, r.work.jacobian_evals, k.major_iters, k.rejected_steps,
        k.reject_funnel_ceiling, k.trust_region_growths, k.trust_region_growth_refusals,
        k.trust_region_shrinks_to_step, k.trust_region_grown_step_rejections,
        k.trust_region_unbound_rejections, k.step_limit_binding_majors, r.f);
    for (const auto &[name, v] : counter_columns(r)) {
        row += "," + v;
    }
    return row;
}

/// One cold solve of one cell.
inline SqpResult run_cell(const SensCell &c, const std::vector<std::string> &sets) {
    const SqpOptions o = cell_options(c, sets);
    if (is_fixture(c)) {
        const hven::test_support::I19Fixture f = make_fixture(c);
        Problem p(f.model);
        return SqpSolver(o).solve(p, f.x0);
    }
    std::unique_ptr<detail::ParametricNlpModel> model = make_model(c);
    return SqpSolver(o).solve(detail::problem_ref(*model), model->start_point());
}

} // namespace hven::bench::i19
