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
        "solve's first major, on an empty path-constraint window. That cell's generator lives "
        "in the sqp checkout's bench/test-support tree and produces its matrix only by running "
        "that engine's own walk, which would make this trace unavailable on the default build "
        "that has no such checkout -- so this recipe builds the STRUCTURAL CLASS the authority "
        "describes (trapezoidal chain, 3 states + 2 controls per node, defect rows coupling "
        "consecutive nodes, an initial-condition block, no inequality in the working set) at a "
        "rig-fixed node count, and the scale difference is recorded rather than hidden.");
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
        "sparsity pattern fixed across iterates.");
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
        "class carries the same coefficients and the same optimal value. WHAT IS NOT "
        "REPRODUCED: the authority names a walk trace with a known admission sequence, and the "
        "walk's pinned minor and factorization counters are a property of that engine's "
        "active-set search, not of the linear seam -- they are unreachable at this level and "
        "are not claimed. The working set below is a fixed, documented one, and what the trace "
        "pins is the seam-level clause: a factorization stays valid and unperturbed across "
        "another factor's whole lifecycle." +
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
    f.provenance = "the indefinite two-variable fixture from the sqp checkout's own solver "
                   "fixture header, transcribed: a Hessian of diag(1, -2) with no constraint in "
                   "the working set. Its residual vanishes at both a genuine minimizer and an "
                   "interior saddle, which is exactly why a verdict has to come from the "
                   "inertia rather than from the residual." +
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
        "regularization applied on top -- which is the property a boundary case is for. Flagged "
        "in the task report as a trace whose authority text was not implementable as written." +
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
                   "pins is the evidence the engine reads off the factorization." +
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
        "climbs." +
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
        "for." +
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
        "the brutally-scaled feasible fixture from the sqp checkout's solver fixture header, "
        "transcribed coefficient for coefficient (Hessian eigenvalues spanning about five "
        "orders, objective gradient at 1e9), with both inequality rows taken into the working "
        "set. The gradient reaches the linear system through the right-hand side, which is "
        "where iterative refinement and pivot perturbation have something to respond to. "
        "WHETHER this matrix actually perturbs a pivot on a given backend is an OBSERVATION "
        "the derivation records, not an assumption this recipe makes." +
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
