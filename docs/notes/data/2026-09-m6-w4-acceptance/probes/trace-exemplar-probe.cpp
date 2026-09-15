// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// trace-exemplar-probe.cpp -- M6 W4 T5's evidence instrument.
//
// Produces, in one solo run:
//
//   * the FOUR v0 exemplar streams named by the T5 brief -- HS24 at kWalk,
//     HS33 at kSsn, HS38 at kIpm, and HS38 on the INTERIOR-POINT driver -- each
//     as JSON lines behind a `#`-prefixed provenance header;
//
//   * the telemetry CENSUS over the HS battery at all three QP modes: one row
//     per major with the six W4 T3 activity fields, plus the four folds and the
//     per-`site` `qp.mode` line counts, as a CSV behind the same header.
//
// NOT A CMAKE TARGET, on the W2 acceptance artifact's own precedent: it is a
// scratch instrument committed so the numbers are re-derivable. The compile
// line is in that directory's README.md.
//
// The HS fixtures are not copied: this file INCLUDES
// tests/sqp/support/hs_problems.h.
//
// The interior-point cell is the one exception. `NLPProblem` and `NlpModel` are
// different contracts with no adapter, so HS38 is hand-transcribed for it.
//
// That transcription is CHECKED against the header's own `Hs38Model` -- f, grad
// f and the box -- before the probe runs, so a slip fails loudly rather than
// producing an exemplar of a different problem.

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <Eigen/Core>

#include <hven/drivers/interior_point_solver.h>
#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/trace_writer.h>
#include <hven/model/nlp_model.h>
#include <hven/model/nlp_problem.h>
#include <hven/model/nlp_solver.h>

#include "support/hs_problems.h"

using hven::ConvergenceFlags;
using hven::Index;
using hven::Vec;
using hven::solvers::IpqpTraceSink;
using hven::solvers::JsonLinesTraceSink;
using hven::solvers::NLPProblem;
using hven::solvers::NLPSolver;
using hven::solvers::QpMode;
using hven::solvers::SqpDriver;
using hven::solvers::SqpOptions;
using hven::solvers::SqpSolution;
using hven::solvers::test_support::HsProblem;
using hven::solvers::test_support::make_hs;

namespace {

constexpr double kInfinity = std::numeric_limits<double>::infinity();

// --- HS38 for the interior-point contract ---------------------------------
//
// n = 4, NO constraints, box [-10, 10]^4, x0 = (-3, -1, -3, -1).
// The Hessian's LOWER triangle, row-major: (0,0) (1,0) (1,1) (2,2) (3,1) (3,2)
// (3,3) -- seven entries, the 19.8 coupling included.
struct Hs38Problem : NLPProblem {
    int num_vars() const override { return 4; }
    int num_cons() const override { return 0; }
    int num_jac_nonzeros() const override { return 0; }
    int num_hess_nonzeros() const override { return 7; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>) const override {
        xl.setConstant(-10.0);
        xu.setConstant(10.0);
    }
    void eval_f(hven::ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        const double a = x[1] - x[0] * x[0], b = 1.0 - x[0];
        const double c = x[3] - x[2] * x[2], d = 1.0 - x[2];
        const double e = x[1] - 1.0, g = x[3] - 1.0;
        f = 100.0 * a * a + b * b + 90.0 * c * c + d * d + 10.1 * (e * e + g * g) + 19.8 * e * g;
    }
    void eval_grad_f(hven::ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        const double a = x[1] - x[0] * x[0];
        const double c = x[3] - x[2] * x[2];
        g[0] = -400.0 * x[0] * a - 2.0 * (1.0 - x[0]);
        g[1] = 200.0 * a + 20.2 * (x[1] - 1.0) + 19.8 * (x[3] - 1.0);
        g[2] = -360.0 * x[2] * c - 2.0 * (1.0 - x[2]);
        g[3] = 180.0 * c + 20.2 * (x[3] - 1.0) + 19.8 * (x[1] - 1.0);
    }
    void eval_g(hven::ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>) const override {}
    void jac_structure(Eigen::Ref<Eigen::VectorXi>, Eigen::Ref<Eigen::VectorXi>) const override {}
    void eval_jac(hven::ConstEigenRef<Eigen::VectorXd>,
                  Eigen::Ref<Eigen::VectorXd>) const override {}
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1, 1, 2, 3, 3, 3;
        c << 0, 0, 1, 2, 1, 2, 3;
    }
    void eval_hess(hven::ConstEigenRef<Eigen::VectorXd> x, double obj_factor,
                   hven::ConstEigenRef<Eigen::VectorXd>,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = obj_factor * (1200.0 * x[0] * x[0] - 400.0 * x[1] + 2.0);
        v[1] = obj_factor * (-400.0 * x[0]);
        v[2] = obj_factor * 220.2;
        v[3] = obj_factor * (1080.0 * x[2] * x[2] - 360.0 * x[3] + 2.0);
        v[4] = obj_factor * 19.8;
        v[5] = obj_factor * (-360.0 * x[2]);
        v[6] = obj_factor * 200.2;
    }
    std::string name() const override { return "Hs38Problem"; }
};

/// THE TRANSCRIPTION CHECK. `Hs38Problem` above is a hand copy of
/// `Hs38Model` under a different model contract; this compares the two at three
/// points, on f, on grad f, and on the declared box, and aborts on any
/// disagreement so the exemplar cannot describe a different problem.
void verify_hs38_transcription() {
    const HsProblem ref = make_hs(38);
    const Hs38Problem probe;
    const std::vector<Vec> pts{ref.model->start_point(), Vec::Constant(4, 1.0),
                               (Vec(4) << 0.5, -2.0, 3.0, 1.5).finished()};
    for (const Vec &x : pts) {
        double f = 0.0;
        probe.eval_f(x, f);
        const double fr = ref.model->eval_f(x);
        if (std::abs(f - fr) > 1e-12 * std::max(1.0, std::abs(fr))) {
            std::fprintf(stderr, "HS38 transcription: f mismatch %.17g vs %.17g\n", f, fr);
            std::abort();
        }
        Eigen::VectorXd g(4);
        probe.eval_grad_f(x, g);
        const Vec gr = ref.model->eval_grad(x);
        if ((g - gr).cwiseAbs().maxCoeff() > 1e-9 * std::max(1.0, gr.cwiseAbs().maxCoeff())) {
            std::fprintf(stderr, "HS38 transcription: grad mismatch\n");
            std::abort();
        }
    }
    Eigen::VectorXd xl(4), xu(4), gl(0), gu(0);
    probe.bounds(xl, xu, gl, gu);
    if ((xl - ref.model->lower()).cwiseAbs().maxCoeff() > 0.0 ||
        (xu - ref.model->upper()).cwiseAbs().maxCoeff() > 0.0) {
        std::fprintf(stderr, "HS38 transcription: box mismatch\n");
        std::abort();
    }
}

// --- the provenance header ------------------------------------------------

std::string g_header;

void write_with_header(const std::string &path, const std::string &body) {
    std::ofstream out(path, std::ios::trunc);
    out << g_header << body;
}

std::string mode_token(QpMode m) {
    switch (m) {
    case QpMode::kWalk:
        return "walk";
    case QpMode::kSsn:
        return "ssn";
    case QpMode::kIpm:
        return "ipm";
    default:
        return "?";
    }
}

std::vector<std::string> split_lines(const std::string &s) {
    std::vector<std::string> out;
    std::istringstream in(s);
    std::string l;
    while (std::getline(in, l)) {
        out.push_back(l);
    }
    return out;
}

std::string raw_field(const std::string &line, const std::string &key) {
    const std::string k = "\"" + key + "\":";
    const auto p = line.find(k);
    if (p == std::string::npos) {
        return {};
    }
    auto b = p + k.size();
    if (line[b] == '"') {
        const auto e = line.find('"', b + 1);
        return line.substr(b + 1, e - b - 1);
    }
    const auto e = line.find_first_of(",}", b);
    return line.substr(b, e - b);
}

// --- the exemplars --------------------------------------------------------

void write_sqp_exemplar(const std::string &path, int hs, QpMode mode) {
    SqpOptions opts;
    opts.qp_mode = mode;
    opts.max_iter = 60;
    const HsProblem p = make_hs(hs);
    SqpDriver driver(opts);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    driver.attach_trace(&json);
    const SqpSolution sol = driver.solve(*p.model);
    write_with_header(path, os.str());
    std::cout << path << ": status=" << static_cast<int>(sol.status)
              << " majors=" << sol.counters.major_iters << " lines=" << json.lines_written()
              << " failed=" << (json.failed() ? 1 : 0) << "\n";
}

void write_ipm_exemplar(const std::string &path) {
    NLPSolver solver(std::make_shared<Hs38Problem>());
    solver.optimizer_->set_print_level(0);
    std::ostringstream os;
    JsonLinesTraceSink json(os);
    solver.optimizer_->attach_trace(&json);
    Eigen::VectorXd x0(4);
    x0 << -3.0, -1.0, -3.0, -1.0;
    const ConvergenceFlags flag = solver.optimize(x0);
    write_with_header(path, os.str());
    std::cout << path << ": flag=" << static_cast<int>(flag)
              << " iters=" << solver.optimizer_->result().iter_num_
              << " lines=" << json.lines_written() << " failed=" << (json.failed() ? 1 : 0) << "\n";
}

// --- the telemetry census -------------------------------------------------

const int kBatteryHs[] = {1,  3,  5,  6,  7,  10, 11, 12, 14, 15, 22, 24, 25, 26,
                          27, 28, 30, 33, 35, 38, 39, 40, 43, 45, 76, 77, 79};

void write_census(const std::string &per_row_path, const std::string &per_cell_path) {
    std::ostringstream rows;
    std::ostringstream cells;
    rows << "cell,mode,major,qp_solved,tr_binding,verdict,active_set_delta,weak_active_rows,"
            "near_active_rows,active_rows,active_lower_sides,active_upper_sides\n";
    cells << "cell,mode,status,majors,active_set_delta_total,active_set_delta_peak,"
             "weak_active_peak,near_active_peak,qp_mode_dispatch,qp_mode_ssn_warm_grade,"
             "qp_mode_fallback_rung_b,qp_mode_elastic_rung,qp_mode_soc_resolve,"
             "elastic_activations,elastic_escalations,ipqp_to_ssn,ipqp_fallback_rung_b,soc_steps,"
             "restoration_iters,depth1_lines\n";

    for (int hs : kBatteryHs) {
        for (QpMode mode : {QpMode::kWalk, QpMode::kSsn, QpMode::kIpm}) {
            SqpOptions opts;
            opts.qp_mode = mode;
            opts.max_iter = 60;
            const HsProblem p = make_hs(hs);
            SqpDriver driver(opts);
            std::ostringstream os;
            JsonLinesTraceSink json(os);
            driver.attach_trace(&json);
            const SqpSolution sol = driver.solve(*p.model);

            const std::string cell = "hs" + std::to_string(hs);
            const std::string m = mode_token(mode);
            for (std::size_t k = 0; k < sol.history.size(); ++k) {
                const auto &r = sol.history[k];
                rows << cell << "," << m << "," << k << "," << (r.qp_solved ? 1 : 0) << ","
                     << (r.tr_binding ? 1 : 0) << "," << static_cast<int>(r.verdict) << ","
                     << r.active_set_delta << "," << r.weak_active_rows << "," << r.near_active_rows
                     << "," << r.active_rows << "," << r.active_lower_sides << ","
                     << r.active_upper_sides << "\n";
            }

            std::map<std::string, Index> site;
            Index depth1 = 0;
            for (const std::string &l : split_lines(os.str())) {
                depth1 += (l.find("\"depth\":1") != std::string::npos) ? 1 : 0;
                if (l.find("\"ev\":\"qp.mode\"") == std::string::npos) {
                    continue;
                }
                ++site[raw_field(l, "site")];
            }
            const auto at = [&](const char *k) {
                const auto it = site.find(k);
                return it == site.end() ? Index{0} : it->second;
            };
            const auto &c = sol.counters;
            cells << cell << "," << m << "," << static_cast<int>(sol.status) << "," << c.major_iters
                  << "," << c.active_set_delta_total << "," << c.active_set_delta_peak << ","
                  << c.weak_active_peak << "," << c.near_active_peak << "," << at("dispatch") << ","
                  << at("ssn_warm_grade") << "," << at("fallback_rung_b") << ","
                  << at("elastic_rung") << "," << at("soc_resolve") << "," << c.elastic_activations
                  << "," << c.elastic_escalations << "," << c.ipqp.ipqp_to_ssn << ","
                  << c.ipqp_fallback_rung_b << "," << c.soc_steps << "," << c.restoration_iters
                  << "," << depth1 << "\n";
        }
    }
    write_with_header(per_row_path, rows.str());
    write_with_header(per_cell_path, cells.str());
    std::cout << per_row_path << " and " << per_cell_path << " written\n";
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: trace-exemplar-probe <out-dir> <provenance-header-file>\n");
        return 2;
    }
    const std::string dir = argv[1];
    {
        std::ifstream h(argv[2]);
        std::ostringstream b;
        b << h.rdbuf();
        g_header = b.str();
    }
    verify_hs38_transcription();
    write_sqp_exemplar(dir + "/exemplars/hs24-walk.jsonl", 24, QpMode::kWalk);
    write_sqp_exemplar(dir + "/exemplars/hs33-ssn.jsonl", 33, QpMode::kSsn);
    write_sqp_exemplar(dir + "/exemplars/hs38-ipqp.jsonl", 38, QpMode::kIpm);
    write_ipm_exemplar(dir + "/exemplars/hs38-interior-point.jsonl");
    write_census(dir + "/telemetry-census-rows.csv", dir + "/telemetry-census-cells.csv");
    return 0;
}
