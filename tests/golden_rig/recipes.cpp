// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// See recipes.h for the recipes-not-matrices rule these all obey.

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <Eigen/SparseCore>

#include "recipes.h"

namespace hven::rig {
namespace {

// A deterministic 64-bit linear congruential generator, written out here
// rather than taken from <random> so a recipe's values are bit-identical on
// every platform and every standard-library version. The rig's reproducibility
// claim is only as good as its weakest source of numbers.
class Lcg {
  public:
    explicit Lcg(unsigned seed)
        : state_(0x9E3779B97F4A7C15ULL ^ static_cast<std::uint64_t>(seed)) {}

    // A double in [-1, 1), from the high 53 bits so the low-order state bits
    // (which an LCG leaves badly distributed) never reach the output.
    double next() {
        state_ = state_ * 6364136223846793005ULL + 1442695040888963407ULL;
        const std::uint64_t bits = state_ >> 11;
        const double unit = static_cast<double>(bits) * (1.0 / 9007199254740992.0); // 2^-53
        return 2.0 * unit - 1.0;
    }

  private:
    std::uint64_t state_;
};

void push_upper(std::vector<Eigen::Triplet<double>> &trips, Index r, Index c, double v) {
    if (r <= c) {
        trips.emplace_back(static_cast<int>(r), static_cast<int>(c), v);
    } else {
        trips.emplace_back(static_cast<int>(c), static_cast<int>(r), v);
    }
}

SpMatRM from_triplets(Index dim, std::vector<Eigen::Triplet<double>> &trips) {
    SpMatRM K(dim, dim);
    K.setFromTriplets(trips.begin(), trips.end());
    K.makeCompressed();
    return K;
}

// A right-hand side that is deterministic, well-scaled, and not accidentally
// in the range of a single block: every trace that solves needs one, and a
// zero or constant vector would let a wrong solve look right.
Vec deterministic_rhs(Index dim, unsigned seed) {
    Lcg rng(seed);
    Vec b(dim);
    for (Index i = 0; i < dim; ++i) {
        b(i) = 0.5 + rng.next();
    }
    return b;
}

// The sentence every recipe that goes through assemble_kkt appends to its own
// provenance. It exists because those recipes describe themselves as
// "transcribed", and a reader is owed the one respect in which the assembled
// matrix is not literally the cited fixture's blocks.
const char *const kRegularizationNote =
    " NOTE ON WHAT IS ADDED: the assembly puts the same primal and dual "
    "regularization terms on the diagonals that the engines' own KKT assemblies "
    "put there, so the matrix is the cited fixture's blocks PLUS those terms, "
    "not the bare blocks. They are not decoration -- they are also what "
    "guarantees the structurally present diagonal every backend requires. The "
    "values are stated at each call site.";

// ---------------------------------------------------------------------------
// THE REACHABILITY RATIFICATION (2026-08-15).
//
// Every recipe below was written before either engine was in this repository,
// against a naming authority's DESCRIPTION of a fixture it could not read. Both
// engines are now migrated, so each recipe has been compared against the fixture
// its authority names, and the verdict is recorded in the provenance string the
// report prints. Three verdicts occur, and the wording distinguishes them
// because they are not the same claim:
//
//   VERIFIED AGAINST FIXTURE -- the migrated fixture exists and this recipe
//       matches it. The clause names the file it was checked against.
//   STRUCTURE VERIFIED AGAINST FIXTURE -- the migrated GENERATOR exists and the
//       structure matches clause for clause, but the named matrix still cannot
//       be produced without running a solve, so the scale (or the instant in a
//       solve) is not reproduced and stays declared.
//   NOT RATIFIABLE -- there is no migrated fixture to compare against, because
//       the authority named a code path rather than a matrix, or named a
//       fixture that never existed, or named one that neither engine migration
//       brought into this tree. The recipe is NOT upgraded, and the reason is
//       stated where a reader of the report will see it.
//
// The per-recipe table and the adjudication of each NOT RATIFIABLE case are in
// docs/testing.md, "The nine-trace reachability ratification". Nothing was
// changed on either side to make a comparison come out even.
// ---------------------------------------------------------------------------

constexpr Index kChainNodes = 200;
constexpr Index kChainStates = 3;
constexpr Index kChainControls = 2;

// The shared body of the two collocation recipes. `barrier` adds an
// interior-point barrier diagonal to the primal block (zero for the
// bound-eliminated form).
Fixture collocation_core(Index nodes, unsigned value_seed, const Vec *barrier, std::string name,
                         std::string provenance) {
    if (nodes < 2) {
        throw std::invalid_argument("collocation recipe: need at least two nodes");
    }
    const Index vars_per_node = kChainStates + kChainControls;
    const Index n_primal = nodes * vars_per_node;
    // Three defect rows per interval plus three initial-condition rows.
    const Index n_dual = kChainStates * (nodes - 1) + kChainStates;
    const Index dim = n_primal + n_dual;

    constexpr double kPrimalReg = 1e-8;
    constexpr double kDualReg = 1e-8;
    const double h = 1.0 / static_cast<double>(nodes - 1);

    Lcg rng(value_seed);
    std::vector<Eigen::Triplet<double>> trips;
    trips.reserve(static_cast<std::size_t>(dim * 12));

    // Primal block: one small symmetric positive-definite block per node
    // (diagonally dominant by construction, so the block class does not depend
    // on the seed) plus the regularization and any barrier diagonal.
    for (Index k = 0; k < nodes; ++k) {
        const Index base = k * vars_per_node;
        for (Index i = 0; i < vars_per_node; ++i) {
            double diag = 2.0 + 0.25 * static_cast<double>(i) + 0.5 * rng.next() + kPrimalReg;
            if (barrier != nullptr) {
                diag += (*barrier)(base + i);
            }
            push_upper(trips, base + i, base + i, diag);
            for (Index j = i + 1; j < vars_per_node; ++j) {
                push_upper(trips, base + i, base + j, 0.1 * rng.next());
            }
        }
    }

    // Defect rows: the trapezoidal rule's Jacobian, coupling node k to node
    // k+1 across all states and both controls.
    Index row = n_primal;
    for (Index k = 0; k + 1 < nodes; ++k) {
        const Index base = k * vars_per_node;
        const Index next = (k + 1) * vars_per_node;
        for (Index c = 0; c < kChainStates; ++c, ++row) {
            for (Index s = 0; s < kChainStates; ++s) {
                const double dfds = (s == c) ? (0.5 + 0.25 * static_cast<double>(c)) : 0.15;
                push_upper(trips, base + s, row, -(s == c ? 1.0 : 0.0) - 0.5 * h * dfds);
                push_upper(trips, next + s, row, (s == c ? 1.0 : 0.0) - 0.5 * h * dfds);
            }
            for (Index u = 0; u < kChainControls; ++u) {
                const double dfdu = 0.3 + 0.05 * static_cast<double>(u) + 0.02 * rng.next();
                push_upper(trips, base + kChainStates + u, row, -0.5 * h * dfdu);
                push_upper(trips, next + kChainStates + u, row, -0.5 * h * dfdu);
            }
            push_upper(trips, row, row, -kDualReg);
        }
    }

    // Initial-condition rows: the first node's states are fixed.
    for (Index c = 0; c < kChainStates; ++c, ++row) {
        push_upper(trips, c, row, 1.0);
        push_upper(trips, row, row, -kDualReg);
    }

    Fixture f;
    f.name = std::move(name);
    f.provenance = std::move(provenance);
    f.K = from_triplets(dim, trips);
    f.rhs = deterministic_rhs(dim, value_seed + 7919u);
    f.n_primal = n_primal;
    f.n_dual = n_dual;
    return f;
}

} // namespace

SpMatRM assemble_kkt(const Mat &H, const Mat &A, double delta, double mu) {
    const Index n = H.rows();
    if (H.cols() != n) {
        throw std::invalid_argument("assemble_kkt: H must be square");
    }
    if (A.size() != 0 && A.cols() != n) {
        throw std::invalid_argument("assemble_kkt: A's column count must match H's dimension");
    }
    const Index m = (A.size() == 0) ? 0 : A.rows();
    const Index dim = n + m;

    std::vector<Eigen::Triplet<double>> trips;
    for (Index i = 0; i < n; ++i) {
        push_upper(trips, i, i, H(i, i) + delta);
        for (Index j = i + 1; j < n; ++j) {
            if (H(i, j) != 0.0) {
                push_upper(trips, i, j, H(i, j));
            }
        }
    }
    for (Index r = 0; r < m; ++r) {
        for (Index j = 0; j < n; ++j) {
            if (A(r, j) != 0.0) {
                push_upper(trips, j, n + r, A(r, j));
            }
        }
        push_upper(trips, n + r, n + r, -mu);
    }
    return from_triplets(dim, trips);
}

Index chain_nodes() { return kChainNodes; }

Fixture collocation_chain_kkt(Index nodes, unsigned value_seed) {
    return collocation_core(
        nodes, value_seed, nullptr, "collocation_chain",
        "REGENERATED FROM THE RECIPE DESCRIPTION, not from the named cell. The naming "
        "authority points at a collocation corpus cell of 100000 primal variables taken at a "
        "solve's first major, on an empty path-constraint window. That cell's generator was "
        "written against the sqp checkout's bench/test-support tree and produced its matrix "
        "only by running that engine's own walk, which would make this trace unavailable on "
        "the default build "
        "that has no such checkout -- so this recipe builds the STRUCTURAL CLASS the authority "
        "describes (trapezoidal chain, 3 states + 2 controls per node, defect rows coupling "
        "consecutive nodes, an initial-condition block, no inequality in the working set) at a "
        "rig-fixed node count, and the scale difference is recorded rather than hidden. "
        "STRUCTURE VERIFIED AGAINST FIXTURE 2026-08-15: the migrated generator is "
        "tests/sqp/support/scale_problems.h's F7CollocationChain(nodes, states=3, controls=2, "
        "p, radius=1), driven by the corpus cell table in bench/corpus_cells.h. Every clause "
        "matched: the trapezoidal defect row y_(k+1) - y_k - (h/2)[F_k + F_(k+1)] at "
        "h = 1/(N-1) with the same -I - (h/2)A / +I - (h/2)A state coupling and the same "
        "-(h/2)C on BOTH nodes' controls, an initial-condition block of `states` rows at "
        "identity, and an empty path window (the bound-arc cells sit below the activation "
        "radius, so no inequality row is active). The regularization matched too, which had "
        "never been checked: this recipe's 1e-8 primal and dual terms are the engine's own "
        "QpOptions::primal_delta / dual_mu defaults. WHAT IS STILL NOT REPRODUCED, and why "
        "this stays a reconstruction: the named cell's K has no standalone builder even now -- "
        "the migrated tree can dump the first QP subproblem but never the assembled KKT, which "
        "exists only inside a running solve -- so the node count (and with it the scale) is the "
        "rig's, not the cell's.");
}

Fixture barrier_chain_kkt(Index nodes, Index iterate) {
    // A barrier diagonal that shrinks by an order of magnitude per rung, the
    // way an interior-point method's does as the barrier parameter falls. Only
    // the VALUES move: the sparsity pattern is identical at every rung, which
    // is exactly the property the iterate-loop trace pins.
    const Index vars_per_node = kChainStates + kChainControls;
    const Index n_primal = nodes * vars_per_node;
    const double mu = std::pow(10.0, -1.0 - static_cast<double>(iterate));
    Vec barrier(n_primal);
    for (Index i = 0; i < n_primal; ++i) {
        // sigma = z / d, the condensed bound-curvature term, with a spread of
        // distances-to-bound across the variables so the diagonal is not
        // uniform.
        const double distance = 0.05 + 0.9 * static_cast<double>(i % 17) / 17.0;
        barrier(i) = mu / (distance * distance);
    }
    return collocation_core(
        nodes, /*value_seed=*/1u, &barrier, "barrier_chain",
        "REGENERATED FROM THE RECIPE DESCRIPTION, not from the named corpus problem. The "
        "naming authority points at a brachistochrone KKT from the interior-point corpus at "
        "iterations {1, 2, k}; that corpus is a set of Python problem definitions solved "
        "end-to-end through the full modelling library, and no standalone builder for its KKT "
        "matrices exists anywhere in that tree -- the matrix only exists inside a running "
        "solve. This recipe therefore builds what the authority says distinguishes the class: "
        "a full interior-point KKT with a BARRIER DIAGONAL on the primal block, over a "
        "collocation structure, with the barrier parameter stepping down per iterate and the "
        "sparsity pattern fixed across iterates. NOT RATIFIABLE 2026-08-15: the named fixture "
        "is not in this repository and neither engine migration brought it here -- the "
        "brachistochrone half stayed with the modelling library it needs (recorded at "
        "tests/interior/solver_test_utils.h's migration note), and the corpus it belongs to is "
        "still Python problem definitions with no standalone KKT builder. So this recipe is NOT "
        "upgraded. What the migration DID make checkable is the mechanism it claims, and that "
        "mechanism holds: the migrated engine condenses the bound curvature sigma = z/d onto "
        "the primal diagonal (include/hven/detail/interior/barrier_math.h's "
        "accumulate_bound_sigma) without growing the KKT, and writes it into pre-allocated "
        "structural slots, so the pattern really is fixed across rungs. The recipe reproduces "
        "the right thing; what it cannot claim is that it reproduces the named matrix. PER C0.4 "
        "RULING 1: authority not reproducible in this repository (unmigrated modelling "
        "library); structural reconstruction, verified against no fixture -- C0.4 report, "
        "2026-08-15.");
}

Fixture hs76_kkt(const std::vector<Index> &active_rows) {
    // HS76, transcribed from its standard statement (the sqp checkout's own
    // model class carries the same data and the same optimal value):
    //
    //   min  x0^2 + 0.5 x1^2 + x2^2 + 0.5 x3^2 - x0 x2 + x2 x3
    //          - x0 - 3 x1 + x2 - x3
    //   s.t. x0 + 2 x1 +   x2 + x3 <= 5
    //        3 x0 +  x1 + 2 x2 - x3 <= 4
    //             -  x1 - 4 x2      <= -1.5
    //        x >= 0
    Mat H(4, 4);
    H << 2.0, 0.0, -1.0, 0.0, //
        0.0, 1.0, 0.0, 0.0,   //
        -1.0, 0.0, 2.0, 1.0,  //
        0.0, 0.0, 1.0, 1.0;
    Vec g(4);
    g << -1.0, -3.0, 1.0, -1.0;

    Mat Ai(3, 4);
    Ai << 1.0, 2.0, 1.0, 1.0, //
        3.0, 1.0, 2.0, -1.0,  //
        0.0, -1.0, -4.0, 0.0;
    Vec bi(3);
    bi << 5.0, 4.0, -1.5;

    const Index m = static_cast<Index>(active_rows.size());
    Mat A(m, 4);
    Vec b(m);
    for (Index k = 0; k < m; ++k) {
        const Index r = active_rows[static_cast<std::size_t>(k)];
        if (r < 0 || r >= 3) {
            throw std::invalid_argument("hs76_kkt: inequality row index out of range");
        }
        A.row(k) = Ai.row(r);
        b(k) = bi(r);
    }

    Fixture f;
    f.name = "hs76";
    f.provenance =
        "HS76's own data, transcribed from its standard statement; the sqp checkout's model "
        "class carried the same coefficients and the same optimal value. WHAT IS NOT "
        "REPRODUCED: the authority names a walk trace with a known admission sequence, and the "
        "walk's pinned minor and factorization counters are a property of that engine's "
        "active-set search, not of the linear seam -- they are unreachable at this level and "
        "are not claimed. The working set below is a fixed, documented one, and what the trace "
        "pins is the seam-level clause: a factorization stays valid and unperturbed across "
        "another factor's whole lifecycle. VERIFIED AGAINST FIXTURE 2026-08-15: "
        "tests/sqp/support/hs_problems.h's Hs76Model. Its constant Hessian upper triangle "
        "((0,0)=2, (0,2)=-1, (1,1)=1, (2,2)=2, (2,3)=1, (3,3)=1), its linear objective term "
        "(-1, -3, 1, -1), its three Jacobian rows ([1,2,1,1], [3,1,2,-1], [0,-1,-4,0]) and its "
        "x >= 0 bounds are entry for entry what this recipe states, and the pinned optimum it "
        "carries is the standard one. The not-reproduced clause above was re-checked at the "
        "same time and still holds: the migrated tests carry no pinned HS76 minor or "
        "factorization counters, only recorded observations, so there is nothing here that "
        "could have been claimed and was not." +
        std::string(kRegularizationNote);
    f.K = assemble_kkt(H, A, /*delta=*/1e-8, /*mu=*/1e-8);
    f.n_primal = 4;
    f.n_dual = m;
    f.rhs = Vec(4 + m);
    f.rhs.head(4) = -g;
    f.rhs.tail(m) = b;
    return f;
}

Fixture saddle_kkt() {
    Mat H(2, 2);
    H << 1.0, 0.0, 0.0, -2.0;
    Fixture f;
    f.name = "saddle";
    f.provenance = "the indefinite two-variable fixture, transcribed from the sqp checkout's own "
                   "solver fixture header: a Hessian of diag(1, -2) with no constraint in "
                   "the working set. Its residual vanishes at both a genuine minimizer and an "
                   "interior saddle, which is exactly why a verdict has to come from the "
                   "inertia rather than from the residual. VERIFIED AGAINST FIXTURE 2026-08-15: "
                   "tests/sqp/support/ssn_fixtures.h's indefinite_qp(), whose Hessian is "
                   "diag(1, -2) and whose linear term (-0.5, 0.25) is this recipe's right-hand "
                   "side negated -- entry for entry. It carries no equality and no inequality "
                   "row, so an empty working set is the fixture's own shape rather than a "
                   "simplification. ONE NUANCE WORTH THE RECORD: the fixture also carries a "
                   "+/-1 box, and its documented minimizer sits on that box, so the empty "
                   "working set this recipe factorizes is the INTERIOR configuration -- which "
                   "is the one the fixture's own note names as the ambiguity (the interior "
                   "saddle at (0.5, 0.125)) and therefore the one the inertia gate exists for." +
                   std::string(kRegularizationNote);
    f.K = assemble_kkt(H, Mat(0, 2), /*delta=*/1e-8, /*mu=*/0.0);
    f.n_primal = 2;
    f.n_dual = 0;
    f.rhs = Vec(2);
    f.rhs << 0.5, -0.25;
    return f;
}

Fixture semidefinite_boundary_kkt() {
    Mat H(2, 2);
    H << 1.0, 0.0, 0.0, 0.0;
    Fixture f;
    f.name = "semidefinite_boundary";
    f.provenance =
        "IMPLEMENTED AGAINST THE DESCRIPTION -- there is no fixture of this name. The naming "
        "authority calls for 'the semidefinite-boundary case from the review fixtures', and no "
        "such symbol exists anywhere in the sqp checkout (the phrase appears only in the "
        "authority itself). This recipe implements the boundary the phrase describes: a "
        "positive-SEMIdefinite Hessian with one exactly-zero eigenvalue, so that whether the "
        "factorization reports a zero class or a definite one is decided entirely by the "
        "regularization applied on top -- which is the property a boundary case is for. "
        "NOT RATIFIABLE 2026-08-15, AND NOW CORRECTED AT THE SOURCE: the re-check confirmed the "
        "original finding on both sides -- the phrase matches nothing in the migrated SQP tree "
        "and nothing in the archived engine tree either; it occurs only in the authority "
        "sentence itself. This is the T5(b) fixture errata the plan of record carried, and it "
        "is no longer only a note here: the authority's own T5 entry has been amended in place "
        "with a dated erratum, and docs/testing.md carries the correction for readers who have "
        "this repository and not that one. The recipe below is unchanged -- what changes is "
        "that the reference it could not satisfy no longer stands uncorrected. PER C0.4 RULING "
        "1: authority not reproducible in this repository (fixture never existed); structural "
        "reconstruction, verified against no fixture -- C0.4 report, 2026-08-15." +
        std::string(kRegularizationNote);
    f.K = assemble_kkt(H, Mat(0, 2), /*delta=*/0.0, /*mu=*/0.0);
    f.n_primal = 2;
    f.n_dual = 0;
    f.rhs = Vec(2);
    f.rhs << 1.0, 0.0;
    return f;
}

Fixture pd_on_face_kkt(Index n_free, Index m_face) {
    if (n_free <= 0 || m_face < 0 || m_face > n_free) {
        throw std::invalid_argument("pd_on_face_kkt: need 0 < m_face <= n_free");
    }
    Mat H = Mat::Zero(n_free, n_free);
    for (Index i = 0; i < n_free; ++i) {
        H(i, i) = 2.0 + 0.5 * static_cast<double>(i);
        if (i + 1 < n_free) {
            H(i, i + 1) = 0.3;
            H(i + 1, i) = 0.3;
        }
    }
    // A full-rank face: row k pairs variable k with variable k+1, so no row is
    // a combination of the others.
    Mat A = Mat::Zero(m_face, n_free);
    for (Index r = 0; r < m_face; ++r) {
        A(r, r) = 1.0;
        A(r, (r + 1) % n_free) = 0.5;
    }

    Fixture f;
    f.name = "pd_on_face";
    f.provenance = "the accepted case of the sqp checkout's on-face refinement path, "
                   "reconstructed at the seam level: a Hessian that is positive definite on the "
                   "face and a full-rank face block, whose correct inertia is therefore exactly "
                   "(free variables, face rows, 0). The path itself is engine logic; what this "
                   "pins is the evidence the engine reads off the factorization. "
                   "NOT RATIFIABLE 2026-08-15, for a reason worth stating precisely: the "
                   "authority named a PATH, not a matrix. QpEngine::refine_on_face migrated "
                   "(include/hven/detail/qp/qp_engine.h), and its accepted arm is under test "
                   "(tests/sqp/test_qp_engine.cpp), but that arm builds its face by hand on a "
                   "small box QP rather than instantiating any named fixture -- there is no "
                   "committed matrix for this recipe to have been transcribed from, then or "
                   "now. So the matrix below is an INDEPENDENT construction and is not upgraded "
                   "to verified. The PROPERTY it exists to pin does check out against the "
                   "migrated arm: on a face that is positive definite with a full-rank face "
                   "block, the accepted path's inertia is exactly (free variables, face rows, "
                   "0), which is what this recipe is built to produce and what T5's case c "
                   "asserts. PER C0.4 RULING 1: authority not reproducible in this repository "
                   "(authority names a code path, not a matrix); structural reconstruction, "
                   "verified against no fixture -- C0.4 report, 2026-08-15." +
                   std::string(kRegularizationNote);
    f.K = assemble_kkt(H, A, /*delta=*/1e-8, /*mu=*/1e-8);
    f.n_primal = n_free;
    f.n_dual = m_face;
    f.rhs = deterministic_rhs(n_free + m_face, 4241u);
    return f;
}

Fixture duplicated_equality_kkt(double primal_reg, double dual_reg) {
    // min x0^2 + x1^2 s.t. x0 + x1 = 1, stated TWICE.
    Mat H(2, 2);
    H << 2.0, 0.0, 0.0, 2.0;
    Mat A(2, 2);
    A << 1.0, 1.0, 1.0, 1.0;

    Fixture f;
    f.name = "duplicated_equality";
    f.provenance =
        "the rank-deficient KKT the interior-point engine's singular-routing fixtures are built "
        "on, transcribed from its own statement (minimize the squared norm subject to one "
        "equality stated twice). The fixture in that tree drives a full solve through the "
        "modelling library to reach this matrix; the matrix itself is small enough to state "
        "directly, so this recipe states it. With no dual regularization it is exactly "
        "singular; a positive dual regularization is the rung an inertia-correction ladder "
        "climbs. NOT RATIFIABLE 2026-08-15, on both of the authorities that point here. The "
        "interior-point half: the singular-routing fixtures named above are not in this "
        "repository -- the migrated interior-point suite reaches its singular states by "
        "INJECTION on a diag(2, -3) probe matrix, and its one equality-only problem is a "
        "single full-rank row, so there is no duplicated-equality fixture here to compare "
        "against. The SQP half (T6's authority names a rank pre-screen fixture): that "
        "pre-screen migrated and is under test, but it REFUSES an over-determined face without "
        "factorizing at all, so it never produces a KKT -- a matrix recipe cannot be checked "
        "against a path whose whole assertion is that no matrix was formed. Not upgraded. What "
        "did survive the re-check is that the ladder this recipe is built for is real: the "
        "migrated engine climbs dual regularization on demand, and P2 replays that as rungs on "
        "a pattern it asserts unchanged. PER C0.4 RULING 1, one marking per authority since the "
        "two halves fail for different reasons: for the interior-point half, authority not "
        "reproducible in this repository (unmigrated modelling library); structural "
        "reconstruction, verified against no fixture -- C0.4 report, 2026-08-15. For the SQP "
        "half, authority not reproducible in this repository (authority names a code path, not "
        "a matrix); structural reconstruction, verified against no fixture -- C0.4 report, "
        "2026-08-15." +
        std::string(kRegularizationNote);
    f.K = assemble_kkt(H, A, primal_reg, dual_reg);
    f.n_primal = 2;
    f.n_dual = 2;
    f.rhs = Vec(4);
    f.rhs << 0.0, 0.0, 1.0, 1.0;
    return f;
}

Fixture active_bound_curvature_kkt(double sigma) {
    // min (x0 - 2)^2 + x1^2 s.t. x0 + x1 = 1.5, with x0 held against a bound.
    // The condensed system carries the bound's curvature term sigma = z/d on
    // the first primal diagonal entry, and sigma blows up as the iterate
    // approaches the bound.
    Mat H(2, 2);
    H << 2.0 + sigma, 0.0, 0.0, 2.0;
    Mat A(1, 2);
    A << 1.0, 1.0;

    Fixture f;
    f.name = "active_bound_curvature";
    f.provenance =
        "the negative control from the interior-point engine's active-bound-curvature pin, "
        "transcribed from its own statement (a two-variable least-squares objective, one "
        "equality, one variable held at a bound). That fixture reads its curvature off a live "
        "solve through the modelling library; the condensed KKT it produces is this one, with "
        "the bound's curvature term on the first primal diagonal. Pinned to NOT read as "
        "singular however large that term gets, which is the misread the original pin exists "
        "for. NOT RATIFIABLE 2026-08-15: the named pin is not in this repository -- it belongs "
        "to the same modelling-library-dependent half of the interior-point suite that stayed "
        "behind, and the only sigma in the migrated interior-point tests is the objective "
        "factor, a different quantity with the same letter. Not upgraded. The MECHANISM the "
        "recipe reproduces is now checkable, and it checks out: the migrated engine accumulates "
        "sigma = z/d from the bound multipliers onto the primal diagonal "
        "(include/hven/detail/interior/barrier_math.h) and eliminates the bound-multiplier rows "
        "rather than carrying them, so a condensed KKT of exactly this shape -- the curvature "
        "term on a primal diagonal entry, no extra row -- is what that engine actually hands a "
        "factorization. PER C0.4 RULING 1: authority not reproducible in this repository "
        "(unmigrated modelling library); structural reconstruction, verified against no "
        "fixture -- C0.4 report, 2026-08-15." +
        std::string(kRegularizationNote);
    f.K = assemble_kkt(H, A, /*delta=*/1e-8, /*mu=*/1e-8);
    f.n_primal = 2;
    f.n_dual = 1;
    f.rhs = Vec(3);
    f.rhs << 4.0, 0.0, 1.5;
    return f;
}

Fixture brutally_scaled_kkt() {
    Mat H(2, 2);
    H << 1.9e-4, -2.18e-2, -2.18e-2, 12.56;
    Vec g(2);
    g << -2.9e9, 5.0e8;
    Mat A(2, 2);
    A << -1.2, 0.5, -0.3, -1.2;
    Vec b(2);
    b << -1.0, 1.3;

    Fixture f;
    f.name = "brutally_scaled";
    f.provenance =
        "the brutally-scaled feasible fixture, transcribed from the sqp checkout's solver "
        "fixture header coefficient for coefficient (Hessian eigenvalues spanning about five "
        "orders, objective gradient at 1e9), with both inequality rows taken into the working "
        "set. The gradient reaches the linear system through the right-hand side, which is "
        "where iterative refinement and pivot perturbation have something to respond to. "
        "WHETHER this matrix actually perturbs a pivot on a given backend is an OBSERVATION "
        "the derivation records, not an assumption this recipe makes. VERIFIED AGAINST FIXTURE "
        "2026-08-15: tests/sqp/support/ssn_fixtures.h's brutally_scaled_feasible_qp(). Hessian "
        "(1.9e-4, -2.18e-2; -2.18e-2, 12.56), objective gradient (-2.9e9, 5.0e8), inequality "
        "rows ([-1.2, 0.5], [-0.3, -1.2]) and right-hand side (-1.0, 1.3) all match entry for "
        "entry. The fixture additionally carries a +/-3 box whose bound is active at its "
        "documented solution, so taking both inequality rows into the working set -- as the "
        "line above already says this recipe does -- is a stated choice of configuration and "
        "not the walk's own." +
        std::string(kRegularizationNote);
    f.K = assemble_kkt(H, A, /*delta=*/1e-8, /*mu=*/1e-8);
    f.n_primal = 2;
    f.n_dual = 2;
    f.rhs = Vec(4);
    f.rhs.head(2) = -g;
    f.rhs.tail(2) = b;
    return f;
}

Fixture perturbing_singular_kkt() {
    // [[1, 0, 1], [0, 0, 0], [1, 0, 0]] -- analytic inertia (1, 1, 1).
    std::vector<Eigen::Triplet<double>> trips;
    push_upper(trips, 0, 0, 1.0);
    push_upper(trips, 0, 2, 1.0);
    push_upper(trips, 1, 1, 0.0); // structurally present, numerically zero
    push_upper(trips, 2, 2, 0.0);

    Fixture f;
    f.name = "perturbing_singular";
    f.provenance = "the exactly-singular three-by-three the library's own backend suite uses to "
                   "demonstrate that static pivot perturbation gets THROUGH a singular matrix "
                   "rather than reporting a zero class: analytic inertia (1, 1, 1), and a "
                   "factorization that returns success with a nonzero perturbed-pivot count. "
                   "The shortest path to a factorization whose partial solves must not be "
                   "trusted to compose.";
    f.K = from_triplets(3, trips);
    f.n_primal = 3;
    f.n_dual = 0;
    f.rhs = Vec(3);
    f.rhs << 1.0, 1.0, 1.0;
    return f;
}

Mat dense_border_block(Index m) {
    if (m <= 0) {
        throw std::invalid_argument("dense_border_block: need a positive dimension");
    }
    // Symmetric and genuinely indefinite (alternating diagonal signs), which
    // is what a working-set border's Schur complement can legitimately be.
    Mat B = Mat::Zero(m, m);
    for (Index i = 0; i < m; ++i) {
        B(i, i) = ((i % 2) == 0) ? (1.0 + 0.5 * static_cast<double>(i))
                                 : -(1.0 + 0.25 * static_cast<double>(i));
        if (i + 1 < m) {
            B(i, i + 1) = 0.4;
            B(i + 1, i) = 0.4;
        }
    }
    return B;
}

} // namespace hven::rig
