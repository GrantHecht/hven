// hven_sqp_ssn_safeguard_probe -- the reproduction vehicle for
// docs/notes/2026-08-07-ssn-safeguards.md (Phase-7 Task 4, review fix round 1,
// finding I5; Fable review F1/F3).
//
// WHY IT EXISTS. Every load-bearing population in that note -- the 2.4M-QP
// counter-example search (section 3.3), the PDAS cycle census (3.2), the
// 682k-cell `uncertain_tol` null result (4.2), the 60 794-cell band study
// (4.3) and the 29 903-QP infeasible-route census (10.1) -- was produced by a
// harness that was never committed. The phase's standing rule is that a
// measurement note names its executable vehicle; without one, the Task-6 / Grant
// ratification scheduled on the first of those numbers would have rested on
// figures nobody could re-derive. This program is that vehicle.
//
// PLACEMENT. A normal CMake target linked against `hven::hven` and BUILT IN BOTH
// CONFIGURATIONS, for the reason bench/CMakeLists.txt already states about
// hven_sqp_f7_cold: "an uncompiled probe rots silently." Deliberately NOT
// ctest-registered -- it is a measurement instrument whose longest documented
// invocation runs for many minutes, not a per-commit assertion. Its fixtures
// are tests/support/ssn_fixtures.h, the SAME file tests/test_ssn_engine.cpp
// includes, so a fixture cannot drift between the note and the test suite.
//
// INVOCATIONS (from the repo root, after `cmake --build build`; this project's
// authoritative configuration is clang++ / MKL Pardiso, and wall-clock figures
// are quoted under MKL_NUM_THREADS=1):
//
//   ./build/bench/hven_sqp_ssn_safeguard_probe trace
//   ./build/bench/hven_sqp_ssn_safeguard_probe search   [qps-per-family]
//   ./build/bench/hven_sqp_ssn_safeguard_probe pdas     [qps]
//   ./build/bench/hven_sqp_ssn_safeguard_probe tau      [qps]
//   ./build/bench/hven_sqp_ssn_safeguard_probe tausweep
//   ./build/bench/hven_sqp_ssn_safeguard_probe ablate
//   ./build/bench/hven_sqp_ssn_safeguard_probe census   [qps]
//   ./build/bench/hven_sqp_ssn_safeguard_probe band     [qps]
//   ./build/bench/hven_sqp_ssn_safeguard_probe routes   [qps]
//
// **THE COUNT ARGUMENT IS QPs, NOT CELLS** (review fix round 2, finding N7; the
// banner used to say "cells-per-family" while every mode consumes it as QPs).
// A family with `starts` start points reports `cells = QPs x starts` minus the
// draws the walk declined, so re-running a population from a cell count in the
// note means dividing by that family's start count first.
//
// `tausweep` and `ablate` take no count: they run the NAMED analytic fixtures
// (sections 4.1 and 5), which are a fixed list, not a population.
//
// A trailing integer argument after the count overrides the RNG seed (default
// kDefaultSeed below). Every mode is deterministic in (mode, count, seed): the
// generator is a seeded std::mt19937_64 drawn in a fixed order, so two runs of
// the same command line print the same rows on the same machine.
//
// WHAT THE `band` MODE CAN AND CANNOT DO. `uncertain_tol` is a RUNTIME option,
// so the tau sweep and the tau null result are ordinary invocations. The band's
// two INTERNALS -- the hysteresis ratio detail::kSsnUncertainLeaveRatio and the
// symmetry of the damped element -- are `inline constexpr` / straight-line code
// in ssn_engine.h, so comparing the shipped classification against a variant of
// either is a PATCHED-HEADER RECOMPILE outside this build, exactly as
// bench/tau_bar_sweep_probe.cpp documents for kFunnelTauBar. `band` prints the
// per-tau aggregate that such a diff is taken on. EXACT INVOCATION for the
// no-hysteresis variant (the alpha-only-damping variant is the same recipe with
// the other sed):
//
//   rm -rf /tmp/ssn_patch && mkdir -p /tmp/ssn_patch/hven/detail/sqp
//   cp include/hven/detail/sqp/ssn_engine.h /tmp/ssn_patch/hven/detail/sqp/
//   F=/tmp/ssn_patch/hven/detail/sqp/ssn_engine.h
//   # no hysteresis: the leave gate becomes the enter gate
//   sed -i 's/was_uncertain ? tau \* detail::kSsnUncertainLeaveRatio : tau/tau/' "$F"
//   # (alpha-only damping instead -- a sed on the beta assignment alone would
//   #  also hit the rho-floor branch, which is a DIFFERENT element selection,
//   #  so this variant is patched by matching the whole uncertain block:)
//   # python3 -c "p='$F';s=open(p).read();open(p,'w').write(s.replace(
//   #   'kUncertain) {\n            alpha[k] = detail::kSsnDegenerateFbDeriv;\n'
//   #   '            beta[k] = detail::kSsnDegenerateFbDeriv;',
//   #   'kUncertain) {\n            alpha[k] = detail::kSsnDegenerateFbDeriv;'))"
//   clang++ -DFMT_HEADER_ONLY -DMKL_LP64 -m64 -O3 -DNDEBUG -std=c++20 \
//     -I /tmp/ssn_patch -I include -I tests/sqp \
//     -isystem dep/eigen -isystem dep/fmt/include \
//     -isystem /opt/intel/oneapi/mkl/latest/include \
//     bench/ssn_safeguard_probe.cpp -o /tmp/ssn_probe \
//     -Wl,--start-group /opt/intel/oneapi/mkl/latest/lib/libmkl_intel_lp64.a \
//     /opt/intel/oneapi/mkl/latest/lib/libmkl_intel_thread.a \
//     /opt/intel/oneapi/mkl/latest/lib/libmkl_core.a -Wl,--end-group \
//     /opt/intel/oneapi/compiler/latest/lib/libiomp5.so -lpthread -ldl -lm
//   MKL_NUM_THREADS=1 LD_LIBRARY_PATH=/opt/intel/oneapi/compiler/latest/lib \
//     /tmp/ssn_probe band 60794
//
// The `-I /tmp/ssn_patch` ahead of `-I include` resolves ONLY ssn_engine.h to
// the patched copy; nothing in the repository is modified, and no CMake option
// here can build a patched engine.
//
// THE SAME RECOMPILE SERVES TWO MORE ARMS (added in review fix round 2):
//
//   * `ablate`'s projection-OFF column -- sed
//     's/^        if (project) {/        if (false) {/' on the patched copy,
//     which is unique in ssn_engine.h; and
//   * `census`'s PRE-FIX arm -- instead of a sed, seed the patched copy from
//     the pre-fix engine itself (the archived sandbox's commit f20dcfe, its
//     copy of ssn_engine.h -- see section 12.2 below), and add
//     -DHVEN_SQP_PROBE_PREFIX_ENGINE so this file's escape-name switch omits
//     the one enumerator fix round 1 introduced. That define exists for this
//     arm alone; no CMake target sets it.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include <fmt/format.h>

#include <hven/detail/sqp/qp_engine.h>
#include <hven/detail/sqp/ssn_engine.h>

#include "support/ssn_fixtures.h"

using namespace hven::solvers;
using namespace hven::solvers::test_support;

namespace {

constexpr std::uint64_t kDefaultSeed = 20260807ULL;

QpSolution walk_answer(const QpProblem &qp) {
    QpEngine engine{QpOptions{}};
    return engine.solve(qp);
}

SsnOptions bare() {
    SsnOptions s;
    s.safeguards = SsnSafeguards::kBare;
    return s;
}

const char *status_name(QpStatus s) {
    switch (s) {
    case QpStatus::kOptimal:
        return "kOptimal";
    case QpStatus::kMaxIter:
        return "kMaxIter";
    case QpStatus::kInfeasible:
        return "kInfeasible";
    case QpStatus::kNumericalError:
        return "kNumericalError";
    }
    return "?";
}

const char *escape_name(SsnEscape e) {
    switch (e) {
    case SsnEscape::kNone:
        return "kNone";
    case SsnEscape::kBudget:
        return "kBudget";
    case SsnEscape::kSingular:
        return "kSingular";
    case SsnEscape::kNoContraction:
        return "kNoContraction";
    case SsnEscape::kInfeasibleSuspect:
        return "kInfeasibleSuspect";
#ifndef HVEN_SQP_PROBE_PREFIX_ENGINE
    // The ONE enumerator fix round 1 added. Guarded so that the pre-fix
    // A/B arm of section 12.2 -- this same file compiled against the
    // archived sandbox's pre-fix ssn_engine.h (its commit f20dcfe) -- is a
    // documented recompile rather than a source edit. The define is never
    // set by any CMake target here; both configurations build the shipped
    // branch.
    case SsnEscape::kIndefinite:
        return "kIndefinite";
#endif
    }
    return "?";
}

double dual_inf_norm(const SsnResult &r) {
    double m = 0.0;
    for (Index i = 0; i < r.lambda_e.size(); ++i) {
        m = std::max(m, std::abs(r.lambda_e(i)));
    }
    for (Index i = 0; i < r.lambda_i.size(); ++i) {
        m = std::max(m, std::abs(r.lambda_i(i)));
    }
    for (Index i = 0; i < r.z.size(); ++i) {
        m = std::max(m, std::abs(r.z(i)));
    }
    return m;
}

// =============================================================================
// The random QP generator
// =============================================================================
//
// The shape the note's section 3.3 families are drawn from: symmetric positive
// definite but deliberately NOT an M-matrix (off-diagonals of either sign, and
// often large), which is the family in which plain PDAS has no convergence
// theory. Feasibility is guaranteed BY CONSTRUCTION -- every right-hand side is
// priced against a point drawn inside the bounds -- so a bare-mode
// non-convergence is never a disguised infeasibility, and the walk is run as an
// independent oracle on top of that.
struct GenSpec {
    Index n_lo = 2;
    Index n_hi = 4;
    Index rows = 2;               // rows of Ai; 0 for none
    bool two_sided_bounds = true; // [-3, 3] on every variable
    bool lcp_bounds = false;      // x >= 0 only (single-sided), overrides above
    double decades = 0.0;         // H row/col scaling over 10^{+-decades}
    bool force_weak = false;      // force row 0 weakly active at the solution

    // --- fields added in review fix round 2 -----------------------------------
    //
    // Both default to the pre-existing behaviour EXACTLY, and both are read
    // AFTER the draws that the fix-round-1 families consumed, so no family that
    // leaves them alone sees a different random stream. That is the whole reason
    // they are appended rather than folded into the shape above: section 12.4's
    // A-E/G rows had to stay reproducible while family F was added beside them.

    // The n-DRAW as an explicit set rather than a contiguous range, which is
    // what section 3.3's family F ({2, 9, 16, 23, 30}) needs and the range
    // cannot express. Empty means "use [n_lo, n_hi]".
    std::vector<Index> n_choices{};

    // A UNIFORM SCALING OF THE OBJECTIVE (H and g together), which leaves the
    // solution set and the feasible set untouched and multiplies every
    // multiplier by the same factor -- i.e. it is a pure conditioning stress on
    // the divergence telemetry, never a change of answer. Section 12.2's false
    // kInfeasible censuses are drawn at 1e6 and 1e9.
    double obj_scale = 1.0;
};

QpProblem generate(std::mt19937_64 &rng, const GenSpec &spec, Index *n_out) {
    const bool n_from_set = !spec.n_choices.empty();
    std::uniform_int_distribution<int> ndist(
        n_from_set ? 0 : static_cast<int>(spec.n_lo),
        n_from_set ? static_cast<int>(spec.n_choices.size()) - 1 : static_cast<int>(spec.n_hi));
    std::uniform_real_distribution<double> u11(-1.0, 1.0);
    std::uniform_real_distribution<double> u22(-2.0, 2.0);
    std::uniform_real_distribution<double> u33(-3.0, 3.0);
    std::uniform_real_distribution<double> slackd(0.05, 2.0);

    const int ndraw = ndist(rng);
    const Index n =
        n_from_set ? spec.n_choices[static_cast<std::size_t>(ndraw)] : static_cast<Index>(ndraw);
    *n_out = n;

    Eigen::MatrixXd S(n, n);
    for (Index i = 0; i < n; ++i) {
        for (Index j = 0; j <= i; ++j) {
            const double v = 2.0 * u11(rng);
            S(i, j) = v;
            S(j, i) = v;
        }
    }
    // Shift onto the positive-definite cone without touching the off-diagonal
    // signs, which is what keeps the family away from the M-matrix hypothesis.
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(S);
    const double lo = es.eigenvalues().minCoeff();
    for (Index i = 0; i < n; ++i) {
        S(i, i) += std::max(0.0, -lo) + 0.5;
    }
    if (spec.decades > 0.0) {
        std::uniform_real_distribution<double> ud(-spec.decades, spec.decades);
        Vec d(n);
        for (Index i = 0; i < n; ++i) {
            d(i) = std::pow(10.0, ud(rng));
        }
        for (Index i = 0; i < n; ++i) {
            for (Index j = 0; j < n; ++j) {
                S(i, j) *= d(i) * d(j);
            }
        }
    }

    QpProblem qp;
    qp.H = S.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(n);
    for (Index i = 0; i < n; ++i) {
        qp.g(i) = u33(rng);
    }
    // Applied AFTER the last draw that touches the objective, so a spec that
    // leaves obj_scale at 1 consumes the identical random stream.
    if (spec.obj_scale != 1.0) {
        qp.H = (qp.H * spec.obj_scale).eval();
        qp.g *= spec.obj_scale;
    }
    qp.Ae.resize(0, n);
    qp.be = Vec(0);

    if (spec.lcp_bounds) {
        qp.lower = Vec::Zero(n);
        qp.upper = Vec::Constant(n, 1e20);
    } else if (spec.two_sided_bounds) {
        qp.lower = Vec::Constant(n, -3.0);
        qp.upper = Vec::Constant(n, 3.0);
    } else {
        qp.lower = Vec::Constant(n, -1e20);
        qp.upper = Vec::Constant(n, 1e20);
    }

    // A point strictly inside the bounds, used only to price bi.
    Vec xf(n);
    for (Index i = 0; i < n; ++i) {
        xf(i) = spec.lcp_bounds ? std::abs(u22(rng)) : u22(rng);
    }

    if (spec.rows > 0) {
        Eigen::MatrixXd Aid(spec.rows, n);
        for (Index r = 0; r < spec.rows; ++r) {
            for (Index c = 0; c < n; ++c) {
                Aid(r, c) = u22(rng);
            }
        }
        qp.Ai = Aid.sparseView();
        qp.bi = Vec(spec.rows);
        for (Index r = 0; r < spec.rows; ++r) {
            qp.bi(r) = Aid.row(r).dot(xf) + slackd(rng);
        }
    } else {
        qp.Ai.resize(0, n);
        qp.bi = Vec(0);
    }

    if (spec.force_weak && spec.rows > 0) {
        // Solve WITHOUT row 0, then make row 0 exactly tight at that solution:
        // it is then active with multiplier 0, i.e. weakly active, and strict
        // complementarity fails there by construction.
        QpProblem red = qp;
        Eigen::MatrixXd rest(spec.rows - 1, n);
        for (Index r = 1; r < spec.rows; ++r) {
            rest.row(r - 1) = Eigen::MatrixXd(qp.Ai).row(r);
        }
        red.Ai = rest.sparseView();
        red.bi = qp.bi.tail(spec.rows - 1);
        const QpSolution s = walk_answer(red);
        if (s.status == QpStatus::kOptimal) {
            qp.bi(0) = Eigen::MatrixXd(qp.Ai).row(0).dot(s.x);
        }
    }
    return qp;
}

// The four/six start points the note's searches use. `k` selects one.
SsnStart make_start(Index k, const QpProblem &qp, const QpSolution &walk) {
    SsnStart st;
    const Index n = qp.n();
    switch (k) {
    case 0: // cold
        break;
    case 1: // cold primal, CORRECT activity hint (one PDAS step from the answer)
        st.activity_hint.ineq.assign(walk.ineq_active.begin(), walk.ineq_active.end());
        st.activity_hint.bounds = walk.bound_state;
        break;
    case 2: { // cold primal, WRONG hint (every row's activity flipped)
        std::vector<bool> flip(static_cast<std::size_t>(qp.mi()), false);
        for (std::size_t i = 0; i < flip.size(); ++i) {
            flip[i] = !walk.ineq_active[i];
        }
        st.activity_hint.ineq = flip;
        std::vector<BoundState> bs(static_cast<std::size_t>(n), BoundState::kFree);
        for (std::size_t i = 0; i < bs.size(); ++i) {
            bs[i] =
                walk.bound_state[i] == BoundState::kFree ? BoundState::kAtLower : BoundState::kFree;
        }
        st.activity_hint.bounds = bs;
        break;
    }
    case 3: // warm primal at the walk's own answer, no hint
        st.x = walk.x;
        break;
    case 4: // warm primal, WRONG hint
        st.x = walk.x;
        st.activity_hint.ineq.assign(static_cast<std::size_t>(qp.mi()), true);
        break;
    default: // a scrambled interior primal
        st.x = Vec::Constant(n, 0.75);
        break;
    }
    return st;
}

bool agrees_with_walk(const SsnResult &res, const QpSolution &walk, double tol) {
    if (res.x.size() != walk.x.size()) {
        return false;
    }
    return (res.x - walk.x).cwiseAbs().maxCoeff() <= tol;
}

// =============================================================================
// Modes
// =============================================================================

// The TRAJECTORY of a named fixture, read the only way an external program can
// read one: re-solve at hard_budget = 0, 1, 2, ... and report where each stops.
// This is the vehicle for the note's sections 3.4 (the overshoot excursion), 6
// (the indefinite fixture's ladder) and 10.1's per-fixture counts.
void mode_trace() {
    struct Cell {
        const char *name;
        QpProblem qp;
        SsnStart start;
    };
    std::vector<Cell> cells;
    // **cycling_start_2var(), NOT a walk-derived flipped hint** (review fix
    // round 2). Until this was corrected, `trace` ran this fixture from
    // make_start(2, ...) -- a DIFFERENT wrong hint, on which the bare kernel
    // converges in 10 steps -- so the mode named as the vehicle for section
    // 3.4's overshoot trajectory did not in fact produce it. ssn_fixtures.h
    // exports the fixture's stated start precisely so the test and this file
    // cannot drift apart; the drift happened anyway, in the one cell that did
    // not use the export.
    cells.push_back({"cycling_qp_2var wrong hint", cycling_qp_2var(), cycling_start_2var()});
    cells.push_back({"cycling_qp_3var cold", cycling_qp_3var(), SsnStart{}});
    cells.push_back({"contradictory_qp cold", contradictory_qp(), SsnStart{}});
    cells.push_back({"inconsistent_equality_qp cold", inconsistent_equality_qp(), SsnStart{}});
    cells.push_back({"slow_infeasible_qp cold", slow_infeasible_qp(), SsnStart{}});
    cells.push_back({"indefinite_qp cold", indefinite_qp(), SsnStart{}});
    {
        // The C1 fixture: seeded ON the saddle the residual cannot see.
        Cell c{"indefinite_qp SEEDED AT THE SADDLE", indefinite_qp(), SsnStart{}};
        c.start.x = Vec(2);
        c.start.x << 0.5, 0.125;
        cells.push_back(c);
    }

    for (const Cell &c : cells) {
        std::printf("=== %s ===\n", c.name);
        for (const bool guarded : {false, true}) {
            std::printf("  --- %s ---\n", guarded ? "kFull" : "kBare");
            for (Index budget = 0; budget <= 25; ++budget) {
                SsnEngine engine{QpOptions{}};
                SsnOptions s = guarded ? SsnOptions{} : bare();
                s.hard_budget = budget;
                SsnResult res;
                engine.solve(c.qp, c.start, s, &res);
                std::printf("    budget=%2lld iters=%2lld fact=%2lld back=%3lld prox=%2lld "
                            "sigma=%-10.4g ||F||=%.6e ||lam||=%.4e %s/%s\n",
                            (long long)budget, (long long)res.iters, (long long)res.factorizations,
                            (long long)res.counters.ssn_backtracks,
                            (long long)res.counters.ssn_prox_updates, res.prox_sigma,
                            res.fb_residual, dual_inf_norm(res), status_name(res.status),
                            escape_name(res.escape_reason));
                if (res.escape_reason == SsnEscape::kNone && budget > 0) {
                    break;
                }
            }
        }
    }
}

// The note's section 3.3 family census, PLUS family G -- the two-sided-box-only
// control the Fable review's F1 found missing (search A was the SINGLE-SIDED
// LCP, whose bound-row gradients are pairwise orthonormal and so structurally
// cannot form the dependent implied-active pair the overshoot mechanism needs).
void mode_search(long long per_family, std::uint64_t seed) {
    struct Family {
        const char *name;
        GenSpec spec;
        Index starts;
    };
    const std::vector<Family> fams = {
        {"A  LCP x>=0, no rows", GenSpec{2, 4, 0, false, true, 0.0, false}, 1},
        {"B  rows, NO bounds", GenSpec{2, 4, 2, false, false, 0.0, false}, 4},
        {"C  rows AND box [-3,3]", GenSpec{2, 6, 2, true, false, 0.0, false}, 6},
        {"D  as C, row 0 weakly active", GenSpec{2, 6, 2, true, false, 0.0, true}, 6},
        {"E  as C, H scaled 10^{+-3}", GenSpec{2, 6, 2, true, false, 3.0, false}, 6},
        // F was in section 3.3's table and had NO counterpart here until review
        // fix round 2 (finding N1): the note promised a re-derivation of a
        // six-row table through a mode that defined five of its rows.
        {"F  as C, n in {2,9,16,23,30}",
         GenSpec{2, 6, 2, true, false, 0.0, false, {2, 9, 16, 23, 30}, 1.0}, 6},
        {"G  TWO-SIDED BOX ONLY, no rows", GenSpec{2, 6, 0, true, false, 0.0, false}, 6},
    };
    for (const Family &f : fams) {
        std::mt19937_64 rng(seed);
        long long qps = 0, cells = 0, failures = 0, walk_skips = 0;
        for (long long t = 0; t < per_family; ++t) {
            Index n = 0;
            const QpProblem qp = generate(rng, f.spec, &n);
            const QpSolution walk = walk_answer(qp);
            if (walk.status != QpStatus::kOptimal) {
                ++walk_skips;
                continue;
            }
            ++qps;
            for (Index k = 0; k < f.starts; ++k) {
                ++cells;
                SsnEngine engine{QpOptions{}};
                SsnResult res;
                engine.solve(qp, make_start(k, qp, walk), bare(), &res);
                if (res.status != QpStatus::kOptimal || !agrees_with_walk(res, walk, 1e-5)) {
                    ++failures;
                }
            }
        }
        std::printf("%-34s qps=%-8lld cells=%-9lld BARE FAILURES=%-6lld (walk-skipped %lld)\n",
                    f.name, qps, cells, failures, walk_skips);
    }
}

// =============================================================================
// THE NAMED-FIXTURE CELL SET (review fix round 2)
// =============================================================================
//
// The twelve-ish fixture/start cells that sections 4.1 (the `uncertain_tol`
// sweep that DERIVES the shipped kSsnUncertainEnter) and 5 (the dual-projection
// ablation) are tabulated over. Fix round 1 gave neither section a vehicle,
// which mattered most for 4.1 because a SHIPPED CONSTANT rests on it.
//
// **THE CELL DEFINITIONS ARE THIS FUNCTION, not the note's row labels.** The
// original harness was never committed, so where a row below disagrees with the
// note the note is corrected to what this code prints (section 13), never the
// other way round. Two rows the note's two tables label identically are split
// here because they are demonstrably different cells: `weakly active` is run
// both cold and from the x0 = (6, -5) start section 4.2 describes.
struct NamedCell {
    const char *name;
    QpProblem qp;
    SsnStart start;
};

std::vector<NamedCell> named_cells() {
    std::vector<NamedCell> cells;

    cells.push_back({"two_row cold", two_row_qp(), SsnStart{}});
    {
        // The wrong hint of test_ssn_engine.cpp's case (b): row 0 inactive,
        // row 1 active, which is the exact reverse of the truth.
        SsnStart s;
        s.activity_hint.ineq = {false, true};
        cells.push_back({"two_row wrong hint", two_row_qp(), s});
    }
    {
        SsnStart s;
        s.activity_hint.ineq = {true, false};
        cells.push_back({"two_row correct hint", two_row_qp(), s});
    }
    {
        // BoxQpFromScrambledHint's deterministic j % 3 scramble, verbatim.
        const Index n = 50;
        SsnStart s;
        s.activity_hint.bounds.assign(static_cast<std::size_t>(n), BoundState::kFree);
        for (Index j = 0; j < n; ++j) {
            const int r = static_cast<int>(j % 3);
            s.activity_hint.bounds[static_cast<std::size_t>(j)] =
                r == 0 ? BoundState::kAtLower : (r == 1 ? BoundState::kAtUpper : BoundState::kFree);
        }
        cells.push_back({"box50 scrambled hint", box_qp(50), s});
    }
    cells.push_back({"box50 cold", box_qp(50), SsnStart{}});
    cells.push_back({"box400 cold", box_qp(400), SsnStart{}});
    cells.push_back({"mixed cold", mixed_block_qp(), SsnStart{}});
    cells.push_back({"cycling 2var wrong hint", cycling_qp_2var(), cycling_start_2var()});
    cells.push_back({"cycling 3var cold", cycling_qp_3var(), SsnStart{}});
    cells.push_back({"contradictory cold", contradictory_qp(), SsnStart{}});
    cells.push_back({"weakly active cold", weakly_active_qp(), SsnStart{}});
    {
        SsnStart s;
        s.x = Vec(2);
        s.x << 6.0, -5.0;
        cells.push_back({"weakly active x0=(6,-5)", weakly_active_qp(), s});
    }
    cells.push_back({"indefinite cold", indefinite_qp(), SsnStart{}});
    return cells;
}

// Section 4.1: the sweep the SHIPPED kSsnUncertainEnter = 0.1 is derived from.
// The claim it has to support is "0.1 is the largest swept value that changes
// nothing on the benign set, and 0.5 is where it starts costing outcomes", so
// the printed row is the accepted-step count per tau with the exit beside it --
// a column that goes from a count to an escape is what "costing outcomes"
// means, and a column that does not move is what "free" means.
void mode_tausweep() {
    const std::vector<double> taus = {0.0, 0.05, 0.1, 0.2, 0.5, 0.9};
    std::printf("# tau sweep over the named fixture cells: 'iters' on a certifying exit, "
                "'ESC:<reason>@iters' otherwise\n");
    std::printf("%-26s", "cell");
    for (const double tau : taus) {
        std::printf(" %-22.2f", tau);
    }
    std::printf("\n");
    for (const NamedCell &c : named_cells()) {
        std::printf("%-26s", c.name);
        for (const double tau : taus) {
            SsnEngine e{QpOptions{}};
            SsnOptions s;
            s.uncertain_tol = tau;
            SsnResult res;
            e.solve(c.qp, c.start, s, &res);
            char buf[64];
            if (res.escape_reason == SsnEscape::kNone) {
                std::snprintf(buf, sizeof(buf), "%lld", (long long)res.iters);
            } else {
                std::snprintf(buf, sizeof(buf), "ESC:%s@%lld", escape_name(res.escape_reason),
                              (long long)res.iters);
            }
            std::printf(" %-22s", buf);
        }
        std::printf("\n");
    }
}

// Section 5: the dual-projection ablation. The projection is STRAIGHT-LINE CODE
// inside SsnEngine::trial_point, not an option -- kBare would switch off the
// line search and the ladder along with it -- so the OFF column is a
// PATCHED-HEADER RECOMPILE exactly as the band variants are (see the banner),
// with the one-line sed
//
//   sed -i 's/^        if (project) {/        if (false) {/' "$F"
//
// which is unique in the file. This mode prints the column; the ablation is the
// diff of two runs of it.
void mode_ablate() {
    std::printf("# dual-projection ablation column: run once shipped, once with the "
                "patched header (banner)\n");
    std::printf("%-26s %-9s %-6s %-6s %-6s %-12s %s\n", "cell", "iters", "fact", "back", "prox",
                "sigma", "status/escape");
    for (const NamedCell &c : named_cells()) {
        SsnEngine e{QpOptions{}};
        SsnResult res;
        e.solve(c.qp, c.start, SsnOptions{}, &res);
        std::printf("%-26s %-9lld %-6lld %-6lld %-6lld %-12.4g %s/%s\n", c.name,
                    (long long)res.iters, (long long)res.factorizations,
                    (long long)res.counters.ssn_backtracks,
                    (long long)res.counters.ssn_prox_updates, res.prox_sigma,
                    status_name(res.status), escape_name(res.escape_reason));
    }
}

// Section 12.2: the FALSE-kInfeasible census -- the A/B whose two headline rows
// (48.3% -> 0.0% and 58.9% -> 0.0%) fix round 1 produced from two uncommitted
// scratch programs, which is the very defect finding I5 raised. This is the
// committed half; the PRE-fix arm is the same binary rebuilt against the
// pre-fix engine header (the archived sandbox's commit f20dcfe), resolved
// ahead of include/ exactly as the band variants are, with
// -DHVEN_SQP_PROBE_PREFIX_ENGINE so this file's escape-name switch does not
// name the enumerator that fix round 1 added:
//
//   mkdir -p /tmp/ssn_prefix/hven/detail/sqp
//   # in the archived sandbox checkout: git show f20dcfe:<its ssn_engine.h>
//   #   > /tmp/ssn_prefix/hven/detail/sqp/ssn_engine.h
//   clang++ ... -DHVEN_SQP_PROBE_PREFIX_ENGINE -I /tmp/ssn_prefix -I include -I tests/sqp ...
//
// EVERY QP HERE IS FEASIBLE BY CONSTRUCTION and confirmed so by the walk, so
// every kInfeasible counted below is a FALSE POSITIVE -- the one status a driver
// answers with restoration rather than with a budget bump.
void mode_census(long long qps, std::uint64_t seed) {
    struct Pop {
        const char *name;
        GenSpec spec;
    };
    const std::vector<Pop> pops = {
        {"feasible, objective x 1e6", GenSpec{2, 4, 2, true, false, 0.0, false, {}, 1e6}},
        {"feasible, objective x 1e9", GenSpec{2, 4, 2, true, false, 0.0, false, {}, 1e9}},
        {"feasible, obj x 1e9 AND H over 10^{+-3}",
         GenSpec{2, 4, 2, true, false, 3.0, false, {}, 1e9}},
    };
    for (const Pop &p : pops) {
        std::mt19937_64 rng(seed);
        long long seen = 0, false_infeas = 0, escapes = 0, walk_skips = 0;
        for (long long t = 0; t < qps; ++t) {
            Index n = 0;
            const QpProblem qp = generate(rng, p.spec, &n);
            const QpSolution walk = walk_answer(qp);
            if (walk.status != QpStatus::kOptimal) {
                ++walk_skips;
                continue;
            }
            ++seen;
            SsnEngine e{QpOptions{}};
            SsnResult res;
            e.solve(qp, SsnStart{}, SsnOptions{}, &res);
            if (res.status == QpStatus::kInfeasible) {
                ++false_infeas;
            }
            if (res.escape_reason != SsnEscape::kNone) {
                ++escapes;
            }
        }
        std::printf("%-40s feasible QPs=%-7lld FALSE kInfeasible=%-6lld (%5.2f%%)  escapes of any "
                    "kind=%-6lld (walk-skipped %lld)\n",
                    p.name, seen, false_infeas,
                    seen > 0 ? 100.0 * static_cast<double>(false_infeas) / static_cast<double>(seen)
                             : 0.0,
                    escapes, walk_skips);
    }
}

// Section 3.2: does the PLAIN PDAS iteration cycle, and does the bare FB kernel
// survive the instances on which it does? The plain iteration is realised
// through the engine's own hint seam -- partition_{k+1} = activity(one hinted
// step on partition_k), i.e. hard_budget = 1 with the previous export fed back
// -- which is CHR's iteration exactly, and is also the loop Task 5 builds.
void mode_pdas(long long qps, std::uint64_t seed) {
    const std::vector<std::pair<const char *, GenSpec>> fams = {
        {"LCP x>=0", GenSpec{2, 4, 0, false, true, 0.0, false}},
        {"general Ai rows", GenSpec{2, 4, 2, false, false, 0.0, false}},
    };
    for (const auto &f : fams) {
        std::mt19937_64 rng(seed);
        long long seen = 0, cycles = 0, bare_ok = 0;
        for (long long t = 0; t < qps; ++t) {
            Index n = 0;
            const QpProblem qp = generate(rng, f.second, &n);
            const QpSolution walk = walk_answer(qp);
            if (walk.status != QpStatus::kOptimal) {
                continue;
            }
            ++seen;
            SsnEngine engine{QpOptions{}};
            SsnOptions one = bare();
            one.hard_budget = 1;
            SsnStart st;
            std::vector<std::string> seen_parts;
            bool cycled = false;
            for (Index step = 0; step < 12; ++step) {
                SsnResult res;
                engine.solve(qp, st, one, &res);
                if (res.fb_residual <= 1e-6) {
                    break;
                }
                std::string p;
                for (const bool a : res.ineq_active) {
                    p += a ? 'A' : '.';
                }
                p += '|';
                for (const BoundState b : res.bound_state) {
                    p += b == BoundState::kAtLower   ? 'L'
                         : b == BoundState::kAtUpper ? 'U'
                         : b == BoundState::kFixed   ? 'X'
                                                     : 'F';
                }
                // A CYCLE is a repeat at separation >= 2: the partition equals
                // one the iteration already visited, and not merely the one it
                // visited last (which is a fixed point, not an orbit). Same
                // rule tests/test_ssn_engine.cpp's cycling assertion uses.
                for (std::size_t i = 0; i + 1 < seen_parts.size(); ++i) {
                    cycled = cycled || seen_parts[i] == p;
                }
                seen_parts.push_back(p);
                st = SsnStart{};
                st.activity_hint.ineq = res.ineq_active;
                st.activity_hint.bounds = res.bound_state;
            }
            if (cycled) {
                ++cycles;
                SsnEngine e2{QpOptions{}};
                SsnResult r2;
                e2.solve(qp, SsnStart{}, bare(), &r2);
                if (r2.status == QpStatus::kOptimal) {
                    ++bare_ok;
                }
            }
        }
        std::printf("%-18s qps=%-9lld PDAS cycles=%-5lld bare FB kernel converges on %lld of "
                    "them\n",
                    f.first, seen, cycles, bare_ok);
    }
}

// Section 4.2: the `uncertain_tol` null result. tau = 0 against the shipped
// tau = 0.1 over a random population, reporting the two things that could make
// the uncertain set earn its place on the ITERATION axis -- a step-count
// difference and a status difference -- and the one thing it demonstrably does
// (a non-empty uncertain set at all).
void mode_tau(long long qps, std::uint64_t seed) {
    const std::vector<std::pair<const char *, GenSpec>> pops = {
        {"generic (Ai + bounds)", GenSpec{2, 6, 2, true, false, 0.0, false}},
        {"one row forced weakly active", GenSpec{2, 6, 2, true, false, 0.0, true}},
        {"ill-conditioned H (10^{+-3})", GenSpec{2, 6, 2, true, false, 3.0, false}},
    };
    for (const auto &p : pops) {
        std::mt19937_64 rng(seed);
        long long cells = 0, unc_nonempty = 0, count_diff = 0, status_diff = 0, favour_zero = 0;
        long long status_favour_tau = 0, status_favour_zero = 0;
        for (long long t = 0; t < qps; ++t) {
            Index n = 0;
            const QpProblem qp = generate(rng, p.second, &n);
            const QpSolution walk = walk_answer(qp);
            if (walk.status != QpStatus::kOptimal) {
                continue;
            }
            for (Index k = 0; k < 4; ++k) {
                ++cells;
                const SsnStart st = make_start(k, qp, walk);
                SsnResult r0, r1;
                {
                    SsnEngine e{QpOptions{}};
                    SsnOptions s;
                    s.uncertain_tol = 0.0;
                    e.solve(qp, st, s, &r0);
                }
                {
                    SsnEngine e{QpOptions{}};
                    e.solve(qp, st, SsnOptions{}, &r1);
                }
                if (r1.counters.ssn_uncertain_peak > 0) {
                    ++unc_nonempty;
                }
                if (r0.iters != r1.iters) {
                    ++count_diff;
                    if (r0.iters < r1.iters) {
                        ++favour_zero;
                    }
                }
                if (r0.status != r1.status) {
                    ++status_diff;
                    // THE DIRECTION IS THE WHOLE POINT of the null result: a
                    // cell that the shipped tau solves and tau = 0 does not
                    // would be the uncertain set's first iteration-axis
                    // POSITIVE, and the note has to report either way.
                    if (r1.status == QpStatus::kOptimal && r0.status != QpStatus::kOptimal) {
                        ++status_favour_tau;
                    } else if (r0.status == QpStatus::kOptimal && r1.status != QpStatus::kOptimal) {
                        ++status_favour_zero;
                    }
                }
            }
        }
        std::printf("%-30s cells=%-9lld uncertain non-empty=%-8lld count differs=%-5lld "
                    "(%lld favour tau=0)  status differs=%-4lld (%lld favour the shipped tau, "
                    "%lld favour tau=0)\n",
                    p.first, cells, unc_nonempty, count_diff, favour_zero, status_diff,
                    status_favour_tau, status_favour_zero);
    }
}

// Section 4.3: the aggregate a patched-header diff is taken on (see the banner).
void mode_band(long long qps, std::uint64_t seed) {
    for (const double tau : {0.1, 0.3, 0.9}) {
        std::mt19937_64 rng(seed);
        long long cells = 0, optimal = 0, iters = 0, flips = 0, peak = 0;
        std::uint64_t sig = 1469598103934665603ULL;
        for (long long t = 0; t < qps; ++t) {
            Index n = 0;
            const QpProblem qp = generate(rng, GenSpec{2, 6, 2, true, false, 0.0, false}, &n);
            const QpSolution walk = walk_answer(qp);
            if (walk.status != QpStatus::kOptimal) {
                continue;
            }
            for (Index k = 0; k < 4; ++k) {
                ++cells;
                SsnEngine e{QpOptions{}};
                SsnOptions s;
                s.uncertain_tol = tau;
                SsnResult res;
                e.solve(qp, make_start(k, qp, walk), s, &res);
                optimal += res.status == QpStatus::kOptimal ? 1 : 0;
                iters += res.iters;
                flips += res.counters.ssn_bulk_flips;
                peak += res.counters.ssn_uncertain_peak;
                // A cheap order-sensitive digest, so a patched-header run that
                // differs on ONE cell differs here too.
                sig = (sig ^ static_cast<std::uint64_t>(res.iters * 131 +
                                                        res.counters.ssn_bulk_flips * 17 +
                                                        static_cast<int>(res.status))) *
                      1099511628211ULL;
            }
        }
        std::printf("tau=%.1f cells=%-8lld optimal=%-8lld iters=%-9lld bulk_flips=%-8lld "
                    "uncertain_peak=%-7lld digest=%016llx\n",
                    tau, cells, optimal, iters, flips, peak, (unsigned long long)sig);
    }
}

// Section 10.1: which of the two entry points into kInfeasibleSuspect an
// infeasible QP takes. The generator makes the LAST row contradict the first by
// construction (a_last = -a_0 with a right-hand side that leaves a positive
// gap), which is the shape an SQP linearization produces.
void mode_routes(long long qps, std::uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> gap(0.2, 2.0);
    long long infeasible = 0, standing = 0, exhaustion = 0, other = 0;
    for (long long t = 0; t < qps; ++t) {
        Index n = 0;
        QpProblem qp = generate(rng, GenSpec{2, 4, 2, true, false, 0.0, false}, &n);
        Eigen::MatrixXd A(qp.Ai);
        A.row(1) = -A.row(0);
        qp.Ai = A.sparseView();
        qp.bi(1) = -qp.bi(0) - gap(rng); // a_0 x <= b_0 and a_0 x >= b_0 + gap
        SsnEngine e{QpOptions{}};
        SsnResult res;
        e.solve(qp, SsnStart{}, SsnOptions{}, &res);
        if (res.escape_reason != SsnEscape::kInfeasibleSuspect) {
            ++other;
            continue;
        }
        ++infeasible;
        if (res.escape_detail.find("accepted steps that did not improve") != std::string::npos) {
            ++standing;
        } else if (res.escape_detail.find("found no descent at all") != std::string::npos) {
            ++exhaustion;
        }
    }
    std::printf("infeasible QPs=%lld  diagnosed=%lld  STANDING route=%lld (%.1f%%)  EXHAUSTION "
                "route=%lld  not diagnosed=%lld\n",
                qps, infeasible, standing,
                infeasible > 0
                    ? 100.0 * static_cast<double>(standing) / static_cast<double>(infeasible)
                    : 0.0,
                exhaustion, other);
}

// =============================================================================
// PHASE-7 TASK 6b PHASE B -- THE FOUR RESEARCH LEVERS' MEASUREMENT ARMS
// =============================================================================
//
// Each mode below is ONE lever's deciding measurement as the research pass
// (docs/notes/research/fable_fb_ssn_globalization_second_pass_claude.md) states
// it, run through the SAME generator and the SAME named fixture cells the
// shipped safeguards were derived on -- so an arm is a diff against a column
// this file already prints rather than a new population nobody has calibrated.
//
// **THE CORPUS HAS NO ILL-CONDITIONED FAMILY AND THIS IS SAID OUT LOUD.** The
// 57 scale cells read dual_scale = 1.0 on every gated row and contain no
// scaled-dual or ill-conditioned cell at all (battery note ADDENDUM section D's
// scope rider). R1's and R2's populations therefore come from THIS FILE's
// generator -- families D (a row forced weakly active, i.e. degenerate) and E
// (H scaled over 10^{+-3}, i.e. ill-conditioned) -- and every figure they
// produce is a figure about that generator, never about F7.

const char *sigma_rule_name(SsnSigmaRule r) {
    switch (r) {
    case SsnSigmaRule::kLadder:
        return "kLadder";
    case SsnSigmaRule::kResidualArmed:
        return "kResidualArmed";
    case SsnSigmaRule::kResidualAlways:
        return "kResidualAlways";
    }
    return "?";
}

// R1's deciding measurement: "ill-conditioned and degenerate populations, all
// five start arms, factorizations-to-exit vs the ladder".
void mode_lever_sigma(long long qps, std::uint64_t seed) {
    const std::vector<std::pair<const char *, GenSpec>> pops = {
        {"generic (Ai + box)", GenSpec{2, 6, 2, true, false, 0.0, false}},
        {"DEGENERATE (row 0 weakly active)", GenSpec{2, 6, 2, true, false, 0.0, true}},
        {"ILL-CONDITIONED (H 10^{+-3})", GenSpec{2, 6, 2, true, false, 3.0, false}},
        {"ILL-COND + obj x 1e6", GenSpec{2, 6, 2, true, false, 3.0, false, {}, 1e6}},
    };
    const std::vector<SsnSigmaRule> rules = {SsnSigmaRule::kLadder, SsnSigmaRule::kResidualArmed,
                                             SsnSigmaRule::kResidualAlways};
    std::printf("# R1: factorizations-to-exit vs the ladder, 6 start arms per QP\n");
    std::printf("%-34s %-16s %-9s %-11s %-9s %-9s %-9s\n", "population", "sigma_rule", "cells",
                "fact", "optimal", "escapes", "prox_upd");
    for (const auto &p : pops) {
        for (const SsnSigmaRule rule : rules) {
            std::mt19937_64 rng(seed);
            long long cells = 0, fact = 0, optimal = 0, escapes = 0, prox = 0;
            for (long long t = 0; t < qps; ++t) {
                Index n = 0;
                const QpProblem qp = generate(rng, p.second, &n);
                const QpSolution walk = walk_answer(qp);
                if (walk.status != QpStatus::kOptimal) {
                    continue;
                }
                for (Index k = 0; k < 6; ++k) {
                    ++cells;
                    SsnEngine e{QpOptions{}};
                    SsnOptions s;
                    s.sigma_rule = rule;
                    SsnResult res;
                    e.solve(qp, make_start(k, qp, walk), s, &res);
                    fact += res.factorizations;
                    optimal += res.status == QpStatus::kOptimal ? 1 : 0;
                    escapes += res.escape_reason != SsnEscape::kNone ? 1 : 0;
                    prox += res.counters.ssn_prox_updates;
                }
            }
            std::printf("%-34s %-16s %-9lld %-11lld %-9lld %-9lld %-9lld\n", p.first,
                        sigma_rule_name(rule), cells, fact, optimal, escapes, prox);
        }
    }
}

// The named-fixture column for R1 and R2 alike: a per-cell trajectory table, so
// a rule that changes an OUTCOME (rather than a count) is visible by name.
void mode_lever_cells() {
    struct Arm {
        const char *name;
        SsnOptions opts;
    };
    std::vector<Arm> arms;
    arms.push_back({"shipped", SsnOptions{}});
    {
        SsnOptions s;
        s.sigma_rule = SsnSigmaRule::kResidualArmed;
        arms.push_back({"R1 armed", s});
    }
    {
        SsnOptions s;
        s.sigma_rule = SsnSigmaRule::kResidualAlways;
        arms.push_back({"R1 always", s});
    }
    {
        SsnOptions s;
        s.hint_rule = SsnHintRule::kWatchdog;
        s.watchdog_q = 1;
        arms.push_back({"R2 wd q=1", s});
    }
    {
        SsnOptions s;
        s.hint_rule = SsnHintRule::kWatchdog;
        s.watchdog_q = 2;
        arms.push_back({"R2 wd q=2", s});
    }
    {
        SsnOptions s;
        s.infeasibility_rule = SsnInfeasibilityRule::kFarkasGated;
        arms.push_back({"R4 farkas", s});
    }
    std::printf("# per-cell: 'F<fact>/<iters>' on a certifying exit, "
                "'ESC:<reason>@F<fact>' otherwise; +wd<n> = watchdog returns\n");
    std::printf("%-26s", "cell");
    for (const Arm &a : arms) {
        std::printf(" %-24s", a.name);
    }
    std::printf("\n");
    for (const NamedCell &c : named_cells()) {
        std::printf("%-26s", c.name);
        for (const Arm &a : arms) {
            SsnEngine e{QpOptions{}};
            SsnResult res;
            e.solve(c.qp, c.start, a.opts, &res);
            char buf[80];
            char wd[16] = "";
            if (res.watchdog_returns > 0) {
                std::snprintf(wd, sizeof(wd), "+wd%lld", (long long)res.watchdog_returns);
            }
            if (res.escape_reason == SsnEscape::kNone) {
                std::snprintf(buf, sizeof(buf), "F%lld/%lld%s", (long long)res.factorizations,
                              (long long)res.iters, wd);
            } else {
                std::snprintf(buf, sizeof(buf), "ESC:%s@F%lld%s", escape_name(res.escape_reason),
                              (long long)res.factorizations, wd);
            }
            std::printf(" %-24s", buf);
        }
        std::printf("\n");
    }
}

// R2's deciding measurement: wrong-hint cells (factorizations unchanged or
// better) plus the COMMON-MODE cost the shipped banner names -- "1 wrongly
// hinted row costs 7 iterations, 100 wrongly hinted rows cost 8". The 100-row
// half of that identity is a box QP with every hint wrong, and it is built here
// because it exists in the shipped record only as a measurement.
void mode_lever_watchdog(long long qps, std::uint64_t seed) {
    struct Arm {
        const char *name;
        SsnOptions opts;
    };
    std::vector<Arm> arms;
    arms.push_back({"shipped exemption", SsnOptions{}});
    {
        SsnOptions s;
        s.hint_rule = SsnHintRule::kWatchdog;
        s.watchdog_q = 1;
        arms.push_back({"watchdog q=1", s});
    }
    {
        SsnOptions s;
        s.hint_rule = SsnHintRule::kWatchdog;
        s.watchdog_q = 2;
        arms.push_back({"watchdog q=2", s});
    }

    // --- (a) THE HALVING IDENTITY, both ends -------------------------------
    std::printf("# R2 (a) the common-mode identity: 1 wrong row vs 100 wrong rows\n");
    std::printf("%-20s %-22s %-8s %-8s %-8s %-8s %s\n", "arm", "fixture", "iters", "fact", "back",
                "wd_ret", "status");
    for (const Arm &a : arms) {
        // -1 = cold (no hint at all), 0 = the CORRECT hint, k > 0 = the correct
        // hint with its first k entries flipped. The pair (0, 1, 100) is what
        // the shipped banner's common-mode claim is about; the cold row is the
        // control that says what a hint is worth here at all.
        for (const Index wrong : {Index{-1}, Index{0}, Index{1}, Index{100}}) {
            const Index n = 100;
            const QpProblem qp = box_qp(n);
            SsnEngine e{QpOptions{}};
            SsnResult truth_res;
            e.solve(qp, SsnStart{}, SsnOptions{}, &truth_res);
            SsnStart st;
            if (wrong >= 0) {
                st.activity_hint.bounds = truth_res.bound_state;
                Index flipped = 0;
                for (std::size_t j = 0; j < st.activity_hint.bounds.size() && flipped < wrong;
                     ++j) {
                    st.activity_hint.bounds[j] = st.activity_hint.bounds[j] == BoundState::kFree
                                                     ? BoundState::kAtLower
                                                     : BoundState::kFree;
                    ++flipped;
                }
            }
            SsnEngine e2{QpOptions{}};
            SsnResult res;
            e2.solve(qp, st, a.opts, &res);
            const std::string label = wrong < 0 ? "box100, COLD (no hint)"
                                      : wrong == 0
                                          ? "box100, correct hint"
                                          : fmt::format("box100, {} wrong rows", (long long)wrong);
            std::printf("%-20s %-22s %-8lld %-8lld %-8lld %-8lld %s/%s\n", a.name, label.c_str(),
                        (long long)res.iters, (long long)res.factorizations,
                        (long long)res.counters.ssn_backtracks, (long long)res.watchdog_returns,
                        status_name(res.status), escape_name(res.escape_reason));
        }
    }

    // --- (b) THE WRONG-HINT POPULATION -------------------------------------
    //
    // Start arms 2 and 4 of make_start are the two WRONG-hint arms (every row's
    // activity flipped from a cold primal; every row asserted active from the
    // walk's own primal). Arm 1 is the CORRECT hint, carried as the control the
    // exemption exists to protect -- a rule that costs anything there has lost
    // the phase's warm-start claim.
    std::printf("\n# R2 (b) wrong-hint population (start arms 2 and 4) + the correct-hint control "
                "(arm 1)\n");
    std::printf("%-30s %-20s %-9s %-11s %-9s %-9s %-9s\n", "population", "arm", "cells", "fact",
                "optimal", "escapes", "wd_returns");
    const std::vector<std::pair<const char *, GenSpec>> pops = {
        {"generic (Ai + box)", GenSpec{2, 6, 2, true, false, 0.0, false}},
        {"DEGENERATE (weakly active)", GenSpec{2, 6, 2, true, false, 0.0, true}},
        {"ILL-CONDITIONED (10^{+-3})", GenSpec{2, 6, 2, true, false, 3.0, false}},
    };
    for (const auto &p : pops) {
        for (const Index k : {Index{1}, Index{2}, Index{4}}) {
            for (const Arm &a : arms) {
                std::mt19937_64 rng(seed);
                long long cells = 0, fact = 0, optimal = 0, escapes = 0, wdr = 0;
                for (long long t = 0; t < qps; ++t) {
                    Index n = 0;
                    const QpProblem qp = generate(rng, p.second, &n);
                    const QpSolution walk = walk_answer(qp);
                    if (walk.status != QpStatus::kOptimal) {
                        continue;
                    }
                    ++cells;
                    SsnEngine e{QpOptions{}};
                    SsnResult res;
                    e.solve(qp, make_start(k, qp, walk), a.opts, &res);
                    fact += res.factorizations;
                    optimal += res.status == QpStatus::kOptimal ? 1 : 0;
                    escapes += res.escape_reason != SsnEscape::kNone ? 1 : 0;
                    wdr += res.watchdog_returns;
                }
                std::printf("%-30s %-20s %-9lld %-11lld %-9lld %-9lld %-9lld\n", p.first,
                            fmt::format("start {} / {}", (long long)k, a.name).c_str(), cells, fact,
                            optimal, escapes, wdr);
            }
        }
    }
}

// R4's deciding measurement: recall on infeasible-tagged cells (vs the shipped
// 56.5%), false positives on badly-scaled FEASIBLE cells (vs 0.43%/0.42%), and
// factorizations burned before diagnosis (vs the up-to-25 budget).
void mode_lever_farkas(long long qps, std::uint64_t seed) {
    struct Arm {
        const char *name;
        SsnInfeasibilityRule rule;
    };
    const std::vector<Arm> arms = {{"shipped symptoms", SsnInfeasibilityRule::kSymptoms},
                                   {"R4 Farkas-gated", SsnInfeasibilityRule::kFarkasGated}};

    // --- (a) RECALL, on the routes generator's contradictory-pair QPs -------
    std::printf("# R4 (a) recall on INFEASIBLE-BY-CONSTRUCTION QPs (routes generator)\n");
    std::printf("%-20s %-9s %-13s %-9s %-11s %-11s %s\n", "arm", "QPs", "diagnosed", "recall",
                "fact(diag)", "fact(total)", "farkas refusals");
    for (const Arm &a : arms) {
        std::mt19937_64 rng(seed);
        std::uniform_real_distribution<double> gap(0.2, 2.0);
        long long infeasible = 0, fact_diag = 0, fact_total = 0, refusals = 0;
        for (long long t = 0; t < qps; ++t) {
            Index n = 0;
            QpProblem qp = generate(rng, GenSpec{2, 4, 2, true, false, 0.0, false}, &n);
            Eigen::MatrixXd A(qp.Ai);
            A.row(1) = -A.row(0);
            qp.Ai = A.sparseView();
            qp.bi(1) = -qp.bi(0) - gap(rng);
            SsnEngine e{QpOptions{}};
            SsnOptions s;
            s.infeasibility_rule = a.rule;
            SsnResult res;
            e.solve(qp, SsnStart{}, s, &res);
            fact_total += res.factorizations;
            refusals += res.farkas_refusals;
            if (res.escape_reason == SsnEscape::kInfeasibleSuspect) {
                ++infeasible;
                fact_diag += res.factorizations;
            }
        }
        std::printf(
            "%-20s %-9lld %-13lld %-9.1f%% %-11.2f %-11lld %lld\n", a.name, qps, infeasible,
            100.0 * static_cast<double>(infeasible) / static_cast<double>(qps),
            infeasible > 0 ? static_cast<double>(fact_diag) / static_cast<double>(infeasible) : 0.0,
            fact_total, refusals);
    }

    // --- (b) FALSE POSITIVES, on feasible-by-construction badly-scaled QPs --
    std::printf("\n# R4 (b) false kInfeasible on FEASIBLE-BY-CONSTRUCTION badly-scaled QPs\n");
    std::printf("%-42s %-20s %-9s %-13s %-9s %s\n", "population", "arm", "QPs", "false kInf",
                "rate", "farkas refusals");
    const std::vector<std::pair<const char *, GenSpec>> pops = {
        {"feasible, objective x 1e6", GenSpec{2, 4, 2, true, false, 0.0, false, {}, 1e6}},
        {"feasible, objective x 1e9", GenSpec{2, 4, 2, true, false, 0.0, false, {}, 1e9}},
        {"feasible, obj x 1e9 AND H over 10^{+-3}",
         GenSpec{2, 4, 2, true, false, 3.0, false, {}, 1e9}},
    };
    for (const auto &p : pops) {
        for (const Arm &a : arms) {
            std::mt19937_64 rng(seed);
            long long seen = 0, false_infeas = 0, refusals = 0;
            for (long long t = 0; t < qps; ++t) {
                Index n = 0;
                const QpProblem qp = generate(rng, p.second, &n);
                const QpSolution walk = walk_answer(qp);
                if (walk.status != QpStatus::kOptimal) {
                    continue;
                }
                ++seen;
                SsnEngine e{QpOptions{}};
                SsnOptions s;
                s.infeasibility_rule = a.rule;
                SsnResult res;
                e.solve(qp, SsnStart{}, s, &res);
                if (res.status == QpStatus::kInfeasible) {
                    ++false_infeas;
                }
                refusals += res.farkas_refusals;
            }
            std::printf(
                "%-42s %-20s %-9lld %-13lld %-9.2f%% %lld\n", p.first, a.name, seen, false_infeas,
                seen > 0 ? 100.0 * static_cast<double>(false_infeas) / static_cast<double>(seen)
                         : 0.0,
                refusals);
        }
    }
}

// M11's A/B, made reproducible from the tree (Phase-B review, findings F5 and
// Fable D). M11 is the ONE unkilled mutant in Task 6b Phase B's battery: it
// drops the `<b, y> < 0` half of `farkas_certificate`, keeping only the
// residual conjunct. The single justification for shipping it unkilled is a
// measurement -- that on every population this repository has, the residual
// conjunct alone accounts for 100% of the armed refusals -- and the review
// found that measurement had NO VEHICLE AND NO ARTIFACT in the tree. This mode
// is the vehicle.
//
// THE MEASUREMENT IS AN A/B ACROSS TWO BUILDS, not across two options, because
// the conjunct is straight-line code rather than a runtime rule. The
// residual-only column comes from a PATCHED-HEADER RECOMPILE of THIS FILE --
// the technique this banner already documents for `band` and `ablate`, and the
// exact commands are committed beside the artifact at
// docs/notes/data/2026-08-10-task6b-phaseB/m11_ab/run.sh. Each build prints
// the same three rows; the claim is that the two builds print the SAME
// NUMBERS.
//
// THE THREE CONSTRUCTIONS, and why they are these three:
//
//   1. GENERIC FEASIBLE at obj x 1e6 -- section 12.2's own false-kInfeasible
//      population, i.e. the one whose 0.43% the gate drives to zero. The A/B
//      asks whether the gap conjunct did any of that work.
//   2. FEASIBLE NARROW STRIP at obj x 1e6 -- the `routes` construction with
//      its gap POSITIVE instead of negative, so the two rows bound a non-empty
//      strip. This is the shape fixture (j) (`narrow_strip_feasible_qp`) is
//      drawn from, and it is the one where the two conjuncts CAN separate:
//      y = (1, 1) annihilates A' exactly, so the residual conjunct is
//      satisfied for free and only <b, y> -- the strip's own width, positive
//      exactly because the strip is non-empty -- can refuse.
//   3. DUPLICATED EQUALITY PAIR at obj x 1e6 -- a rank-deficient but CONSISTENT
//      equality block, the other way to make A' y = 0 free.
//
// Constructions 2 and 3 are the ones built to make the gap conjunct
// load-bearing. If it is inert even there, "inert on every population this
// repository has" is a measurement rather than an absence of looking.
//
// COUNTS. `count` is construction 1's and 2's draw count; construction 3 takes
// half of it, so the documented 500 000-draw invocation is `count = 200000`.
// Deterministic in (count, seed): each construction re-seeds from the literal
// `kDefaultSeed` (or the command line's override), and nothing here reads a
// clock or a random_device.
void mode_lever_farkas_m11(long long qps, std::uint64_t seed) {
    const GenSpec spec{2, 4, 2, true, false, 0.0, false, {}, 1e6};

    std::printf("# M11 A/B: does the <b, y> < 0 conjunct refuse anything the residual "
                "conjunct does not?\n");
    std::printf("# Run this binary AND the residual-only patched build (m11_ab/run.sh); the "
                "claim is that the rows match.\n");
    std::printf("%-44s %-9s %-9s %-16s %-16s %s\n", "construction", "draws", "cells",
                "armed refusals", "certificate fired", "kInfeasible reported");

    for (int c = 1; c <= 3; ++c) {
        const long long draws = c == 3 ? qps / 2 : qps;
        std::mt19937_64 rng(seed);
        std::uniform_real_distribution<double> width(0.2, 2.0);
        std::uniform_real_distribution<double> u22(-2.0, 2.0);
        long long cells = 0, refusals = 0, fired = 0, infeas = 0;
        for (long long t = 0; t < draws; ++t) {
            Index n = 0;
            QpProblem qp = generate(rng, spec, &n);
            if (c == 2) {
                // The `routes` pair with a POSITIVE width: a_0' x <= b_0 and
                // -a_0' x <= -b_0 + w, i.e. b_0 - w <= a_0' x <= b_0.
                Eigen::MatrixXd A(qp.Ai);
                A.row(1) = -A.row(0);
                qp.Ai = A.sparseView();
                qp.bi(1) = -qp.bi(0) + width(rng);
            } else if (c == 3) {
                // A consistent duplicated equality pair, priced at a point
                // drawn inside the bounds so the block itself is satisfiable.
                Eigen::MatrixXd Aed(2, n);
                Vec p(n);
                for (Index j = 0; j < n; ++j) {
                    p(j) = u22(rng);
                }
                for (Index j = 0; j < n; ++j) {
                    const double v = u22(rng);
                    Aed(0, j) = v;
                    Aed(1, j) = v;
                }
                qp.Ae = Aed.sparseView();
                qp.be = Vec(2);
                qp.be(0) = Aed.row(0).dot(p);
                qp.be(1) = qp.be(0);
            }
            // THE POPULATION'S OWN PREMISE: only cells the WALK certifies are
            // counted, so every cell below is FEASIBLE by an independent
            // kernel's word and a kInfeasible here is a false positive.
            const QpSolution walk = walk_answer(qp);
            if (walk.status != QpStatus::kOptimal) {
                continue;
            }
            ++cells;
            SsnEngine e{QpOptions{}};
            SsnOptions s;
            s.infeasibility_rule = SsnInfeasibilityRule::kFarkasGated;
            SsnResult res;
            e.solve(qp, SsnStart{}, s, &res);
            refusals += res.farkas_refusals;
            fired += res.farkas_fired;
            infeas += res.status == QpStatus::kInfeasible ? 1 : 0;
        }
        const char *name = c == 1   ? "generic feasible, obj x 1e6"
                           : c == 2 ? "feasible NARROW STRIP, obj x 1e6"
                                    : "duplicated EQUALITY PAIR, obj x 1e6";
        std::printf("%-44s %-9lld %-9lld %-16lld %-16lld %lld\n", name, draws, cells, refusals,
                    fired, infeas);
    }
}

} // namespace

int main(int argc, char **argv) {
    const std::string mode = argc > 1 ? argv[1] : "trace";
    const long long count = argc > 2 ? std::atoll(argv[2]) : 20000;
    const std::uint64_t seed =
        argc > 3 ? static_cast<std::uint64_t>(std::atoll(argv[3])) : kDefaultSeed;

    std::printf("# hven_sqp_ssn_safeguard_probe mode=%s count=%lld seed=%llu\n", mode.c_str(),
                count, (unsigned long long)seed);
    if (mode == "trace") {
        mode_trace();
    } else if (mode == "search") {
        mode_search(count, seed);
    } else if (mode == "pdas") {
        mode_pdas(count, seed);
    } else if (mode == "tau") {
        mode_tau(count, seed);
    } else if (mode == "tausweep") {
        mode_tausweep();
    } else if (mode == "ablate") {
        mode_ablate();
    } else if (mode == "census") {
        mode_census(count, seed);
    } else if (mode == "band") {
        mode_band(count, seed);
    } else if (mode == "routes") {
        mode_routes(count, seed);
    } else if (mode == "lever-sigma") {
        mode_lever_sigma(count, seed);
    } else if (mode == "lever-cells") {
        mode_lever_cells();
    } else if (mode == "lever-watchdog") {
        mode_lever_watchdog(count, seed);
    } else if (mode == "lever-farkas") {
        mode_lever_farkas(count, seed);
    } else if (mode == "lever-farkas-m11") {
        mode_lever_farkas_m11(count, seed);
    } else {
        std::fprintf(stderr,
                     "usage: %s {trace|search|pdas|tau|tausweep|ablate|census|band|routes|"
                     "lever-sigma|lever-cells|lever-watchdog|lever-farkas|"
                     "lever-farkas-m11} [count] [seed]\n",
                     argv[0]);
        return 2;
    }
    return 0;
}
