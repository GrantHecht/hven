// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// Four small Hock-Schittkowski problems as `Nlp`s, solved on either engine, plus a recording
// sink. The trace-v1 contract tests and the human-trace goldens share them.
//
//   hs006  min (1-x1)^2 s.t. 10(x2-x1^2) = 0                    start (-1.2, 1)
//   hs035  min 9-8x1-6x2-4x3+2x1^2+2x2^2+x3^2+2x1x2+2x1x3
//          s.t. x1+x2+2x3 <= 3, x >= 0                         start (0.5, 0.5, 0.5)
//   hs071  see hs071_problem.h
//   rosen  Rosenbrock, x1 <= 0.5 (active), -2 <= x2 <= 2        start (-1.2, 1)

#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <Eigen/Core>

#include "model/support/hs071_problem.h"
#include "hven/trace/trace.h"
#include "hven/model/nlp.h"

namespace hven_drivers_tests {

struct Hs006 final : hven::Nlp {
    hven::NlpDimensions dimensions() const override { return {2, 1, 2, 1}; }
    void bounds(hven::VecRef xl, hven::VecRef xu, hven::VecRef gl, hven::VecRef gu) const override {
        const double inf = std::numeric_limits<double>::infinity();
        xl.setConstant(-inf);
        xu.setConstant(inf);
        gl << 0.0;
        gu << 0.0;
    }
    double eval_f(hven::ConstVecRef x) const override { return (1.0 - x[0]) * (1.0 - x[0]); }
    void eval_grad_f(hven::ConstVecRef x, hven::VecRef g) const override {
        g << -2.0 * (1.0 - x[0]), 0.0;
    }
    void eval_g(hven::ConstVecRef x, hven::VecRef g) const override {
        g << 10.0 * (x[1] - x[0] * x[0]);
    }
    void jacobian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        r << 0, 0;
        c << 0, 1;
    }
    void eval_jacobian(hven::ConstVecRef x, hven::VecRef v) const override {
        v << -20.0 * x[0], 10.0;
    }
    void hessian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        r << 0;
        c << 0;
    }
    void eval_hessian(hven::ConstVecRef, double obj_factor, hven::ConstVecRef lambda,
                      hven::VecRef v) const override {
        v << 2.0 * obj_factor - 20.0 * lambda[0];
    }
    std::string name() const override { return "Hs006"; }
};

struct Hs035 final : hven::Nlp {
    hven::NlpDimensions dimensions() const override { return {3, 1, 3, 5}; }
    void bounds(hven::VecRef xl, hven::VecRef xu, hven::VecRef gl, hven::VecRef gu) const override {
        const double inf = std::numeric_limits<double>::infinity();
        xl.setZero();
        xu.setConstant(inf);
        gl << -inf;
        gu << 3.0;
    }
    double eval_f(hven::ConstVecRef x) const override {
        return 9.0 - 8.0 * x[0] - 6.0 * x[1] - 4.0 * x[2] + 2.0 * x[0] * x[0] + 2.0 * x[1] * x[1] +
               x[2] * x[2] + 2.0 * x[0] * x[1] + 2.0 * x[0] * x[2];
    }
    void eval_grad_f(hven::ConstVecRef x, hven::VecRef g) const override {
        g << -8.0 + 4.0 * x[0] + 2.0 * x[1] + 2.0 * x[2], -6.0 + 4.0 * x[1] + 2.0 * x[0],
            -4.0 + 2.0 * x[2] + 2.0 * x[0];
    }
    void eval_g(hven::ConstVecRef x, hven::VecRef g) const override {
        g << x[0] + x[1] + 2.0 * x[2];
    }
    void jacobian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        r << 0, 0, 0;
        c << 0, 1, 2;
    }
    void eval_jacobian(hven::ConstVecRef, hven::VecRef v) const override { v << 1.0, 1.0, 2.0; }
    void hessian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        r << 0, 1, 1, 2, 2;
        c << 0, 0, 1, 0, 2;
    }
    void eval_hessian(hven::ConstVecRef, double obj_factor, hven::ConstVecRef,
                      hven::VecRef v) const override {
        v << 4.0 * obj_factor, 2.0 * obj_factor, 4.0 * obj_factor, 2.0 * obj_factor,
            2.0 * obj_factor;
    }
    std::string name() const override { return "Hs035"; }
};

struct BoundedRosenbrock final : hven::Nlp {
    hven::NlpDimensions dimensions() const override { return {2, 0, 0, 3}; }
    void bounds(hven::VecRef xl, hven::VecRef xu, hven::VecRef, hven::VecRef) const override {
        xl << -2.0, -2.0;
        xu << 0.5, 2.0;
    }
    double eval_f(hven::ConstVecRef x) const override {
        const double a = 1.0 - x[0];
        const double b = x[1] - x[0] * x[0];
        return a * a + 100.0 * b * b;
    }
    void eval_grad_f(hven::ConstVecRef x, hven::VecRef g) const override {
        const double b = x[1] - x[0] * x[0];
        g << -2.0 * (1.0 - x[0]) - 400.0 * x[0] * b, 200.0 * b;
    }
    void eval_g(hven::ConstVecRef, hven::VecRef) const override {}
    void jacobian_structure(hven::IndexVecRef, hven::IndexVecRef) const override {}
    void eval_jacobian(hven::ConstVecRef, hven::VecRef) const override {}
    void hessian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        r << 0, 1, 1;
        c << 0, 0, 1;
    }
    void eval_hessian(hven::ConstVecRef x, double obj_factor, hven::ConstVecRef,
                      hven::VecRef v) const override {
        v << obj_factor * (2.0 - 400.0 * x[1] + 1200.0 * x[0] * x[0]), obj_factor * -400.0 * x[0],
            obj_factor * 200.0;
    }
    std::string name() const override { return "BoundedRosenbrock"; }
};

struct BatteryCase {
    const char *name;
    std::shared_ptr<hven::Nlp> (*make)();
    hven::Vec (*start)();
};

inline hven::Vec vec2(double a, double b) {
    hven::Vec x(2);
    x << a, b;
    return x;
}

/// The four-problem battery.
inline const std::vector<BatteryCase> &battery() {
    static const std::vector<BatteryCase> cases = {
        {"hs006", [] { return std::shared_ptr<hven::Nlp>(std::make_shared<Hs006>()); },
         [] { return vec2(-1.2, 1.0); }},
        {"hs035", [] { return std::shared_ptr<hven::Nlp>(std::make_shared<Hs035>()); },
         [] {
             hven::Vec x(3);
             x << 0.5, 0.5, 0.5;
             return x;
         }},
        {"hs071", [] { return std::shared_ptr<hven::Nlp>(std::make_shared<Hs071Problem>()); },
         [] { return hven::Vec(hs071_start()); }},
        {"rosen", [] { return std::shared_ptr<hven::Nlp>(std::make_shared<BoundedRosenbrock>()); },
         [] { return vec2(-1.2, 1.0); }},
    };
    return cases;
}

/// One captured `iter` row with the nesting depth it was written at.
struct CapturedRow {
    hven::IterationRecord record;
    hven::Index depth = 0;
};

/// The facts of one `solve.end` that outlive the call (the event borrows).
struct CapturedEnd {
    hven::SolveStatus status = hven::SolveStatus::kOptimal;
    hven::Index iterations = 0;
    bool returned_best_iterate = false;
    double f = 0.0;
    std::optional<hven::WorkCounters> work;
    std::optional<hven::IpmConvergence> ipm_convergence;
    std::optional<hven::SqpConvergence> sqp_convergence;
    hven::Index depth = 0;
};

/// Records every row, message, begin and end of a solve.
class CaptureSink final : public hven::TraceSink {
  public:
    std::vector<CapturedRow> rows;
    std::vector<hven::MessageTraceEvent> messages;
    std::vector<hven::SolveBeginTraceEvent> begins;
    std::vector<CapturedEnd> ends;

    hven::Index depth0_rows() const {
        hven::Index n = 0;
        for (const CapturedRow &r : rows) {
            n += r.depth == 0 ? 1 : 0;
        }
        return n;
    }

    void on_solve_begin(const hven::SolveBeginTraceEvent &e) override {
        if (open_ > 0) {
            ++depth_;
        }
        ++open_;
        begins.push_back(e);
    }
    void on_iter(const hven::IterTraceEvent &e) override { rows.push_back({e.record, depth_}); }
    void on_message(const hven::MessageTraceEvent &e) override { messages.push_back(e); }
    void on_solve_end(const hven::SolveEndTraceEvent &e) override {
        CapturedEnd c;
        c.status = e.status;
        c.iterations = e.iterations;
        c.returned_best_iterate = e.returned_best_iterate;
        c.f = e.f;
        c.work = e.work;
        c.depth = depth_;
        if (e.ipm) {
            c.ipm_convergence = e.ipm->convergence;
        }
        if (e.sqp) {
            c.sqp_convergence = e.sqp->convergence;
        }
        ends.push_back(c);
        if (open_ > 0) {
            --open_;
        }
        if (open_ > 0 && depth_ > 0) {
            --depth_;
        }
    }

    void on_solve_abort() noexcept override {
        if (open_ > 0) {
            --open_;
        }
        if (open_ > 0 && depth_ > 0) {
            --depth_;
        }
    }

  private:
    hven::Index open_ = 0;
    hven::Index depth_ = 0;
};

} // namespace hven_drivers_tests
