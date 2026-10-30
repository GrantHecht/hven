// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// e1b_off_golden.h — M7 W2 E1b T5: the OFF byte-identity pin on a DEPENDENT QP.
// ONE source for both ends of the pin: the fixture at HEAD
// (test_sqp_equality_basis.cpp, T5) and the generator compiled against E1's
// pre-merge base 8fdeed3e (docs/notes/data/2026-09-m7-w2-e1-equality-basis/
// e1b-off-golden/), which predates SqpOptions::equality_basis: define
// HVEN_E1B_GOLDEN_AT_BASE there and the field is not named.
//
// The model: F1's two networks plus a duplicate of balance row 1 (a dependent and
// consistent row), 10 x 11. The fingerprint is status, six SqpCounters, and the
// hex bytes of x and lambda_e -- one line per quantity.

#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/Dense>
#include <fmt/format.h>

#include <hven/drivers/sqp_solver.h>
#include <hven/drivers/sqp_solver_types.h>

#include "qp_as_nlp_model.h"

namespace hven::solvers::e1b_golden {

inline QpProblem dependent_networks_qp() {
    Eigen::MatrixXd A = Eigen::MatrixXd::Zero(10, 11);
    const std::vector<std::pair<Index, Index>> net1 = {{0, 1}, {1, 2}, {2, 3}, {0, 2}, {1, 3}};
    const std::vector<std::pair<Index, Index>> net2 = {{0, 1}, {1, 2}, {2, 3},
                                                       {3, 4}, {0, 4}, {1, 3}};
    for (std::size_t a = 0; a < net1.size(); ++a) {
        A(net1[a].first, static_cast<Index>(a)) = 1.0;
        A(net1[a].second, static_cast<Index>(a)) = -1.0;
    }
    for (std::size_t a = 0; a < net2.size(); ++a) {
        A(4 + net2[a].first, 5 + static_cast<Index>(a)) = 1.0;
        A(4 + net2[a].second, 5 + static_cast<Index>(a)) = -1.0;
    }
    A.row(9) = A.row(1);
    Vec b(10);
    b << 1.0, 0.5, -0.25, -1.25, 2.0, -0.5, 0.0, -1.0, -0.5, 0.5;
    QpProblem qp;
    const Index n = 11;
    const Vec h = Vec::LinSpaced(n, 1.0, 3.0);
    qp.H = SpMatRM(n, n);
    std::vector<Eigen::Triplet<double>> t;
    for (Index i = 0; i < n; ++i) {
        t.emplace_back(i, i, h(i));
    }
    qp.H.setFromTriplets(t.begin(), t.end());
    qp.g = Vec::LinSpaced(n, -1.0, 1.0);
    qp.Ae = A.sparseView();
    qp.Ae.makeCompressed();
    qp.be = b;
    qp.Ai = SpMatRM(0, n);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(n, -50.0);
    qp.upper = Vec::Constant(n, 50.0);
    return qp;
}

inline std::string hex_of(const Vec &v) {
    std::string out;
    for (Index i = 0; i < v.size(); ++i) {
        std::uint64_t u = 0;
        const double d = v(i);
        std::memcpy(&u, &d, sizeof(u));
        out += fmt::format("{}{:016x}", i == 0 ? "" : " ", u);
    }
    return out;
}

// The fingerprint of one OFF solve under `mode`.
inline std::vector<std::string> off_fingerprint(QpMode mode) {
    const test_support::QpAsNlpModel model(dependent_networks_qp(), Vec::Zero(11));
    SqpOptions o;
    o.qp_mode = mode;
    o.max_iter = 100;
    o.common.threads = 1;
#ifndef HVEN_E1B_GOLDEN_AT_BASE
    o.equality_basis = false;
    // The base predates E2's independent working set: the pin is the old walk's.
    o.qp.independent_working_set = false;
#endif
    SqpSolver solver(o);
    const SqpResult r = solver.solve(model);
    const SqpCounters &c = r.counters;
    return {
        fmt::format("status {}", static_cast<int>(r.status)),
        fmt::format("counters {} {} {} {} {} {}", c.major_iters, c.qp_minor_iters, c.factorizations,
                    c.steps_accepted, c.rejected_steps, c.restoration_iters),
        fmt::format("x {}", hex_of(r.x)),
        fmt::format("lambda_e {}", hex_of(r.lambda_e)),
    };
}

} // namespace hven::solvers::e1b_golden
