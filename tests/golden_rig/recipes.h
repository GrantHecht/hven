#pragma once

// RECIPES, NOT MATRICES. Every trace's matrix is REGENERATED here from a
// documented construction. No matrix is copied into this repository from
// either sibling checkout, and no matrix is read from a file: a trace's input
// is code with a provenance note, so it reproduces identically on any machine
// and stays available whether or not the old-seam checkouts are configured in.
//
// THAT LAST CLAUSE IS THE DESIGN CONSTRAINT. The rig's native arms run in the
// default test suite, on a build that has neither sibling checkout. A recipe
// that reached into one of those trees for its matrix would make the trace
// unrunnable there, so every recipe below is self-contained hven-side code.
// Where the naming authority points at a fixture that lives in one of those
// trees, the recipe transcribes that fixture's own construction and cites it;
// where the named fixture cannot be reproduced without building a whole engine
// (a collocation KKT taken "at the first major" of a solve, an interior-point
// KKT taken at iteration k of a barrier sequence), the recipe implements the
// STRUCTURE the authority describes and says so in its provenance string,
// which the report prints. Those are named, not buried -- see the task report's
// fixture-fidelity table.
//
// Every recipe returns a KKT in the convention the whole library uses: the
// UPPER TRIANGLE of a symmetric matrix in compressed row-major CSR, with every
// diagonal entry structurally present.

#include <string>
#include <vector>

#include "hven/core/types.h"

namespace hven::rig {

// One regenerated fixture: the matrix, a right-hand side sized to match, the
// block split, and the provenance that says where the recipe came from and
// what it does and does not reproduce.
struct Fixture {
    std::string name;
    std::string provenance;
    SpMatRM K;
    Vec rhs;
    Index n_primal = 0; // leading block: the Hessian/curvature rows
    Index n_dual = 0;   // trailing block: the constraint rows
};

// --- the shared assembly the small recipes are written in terms of ----------

// Assembles the upper triangle of
//
//     K = [ H + delta*I     A^T    ]
//         [                -mu*I   ]
//
// from a dense symmetric H (read as its upper triangle) and a dense constraint
// block A. This is the block form both engines' KKT assemblies produce -- the
// sqp side's is the same expression with its own primal/dual regularization
// names -- so a recipe written against it is written against the shape the
// seams actually see. The delta and -mu diagonals also guarantee the
// structural diagonal every backend requires, on both blocks.
SpMatRM assemble_kkt(const Mat &H, const Mat &A, double delta, double mu);

// --- the recipes -----------------------------------------------------------

// A trapezoidal collocation chain's KKT: `nodes` nodes of 3 states and 2
// controls, defect rows coupling consecutive nodes, an initial-condition
// block, and an empty path-constraint window (no inequality is in the working
// set, which is what the naming authority's "empty window" means). Banded and
// symmetric-indefinite, which is the structural class the trace exists to
// exercise.
//
// `value_seed` changes the numeric values WITHOUT touching the sparsity
// pattern -- the value-only refactorization trace's whole mechanism.
Fixture collocation_chain_kkt(Index nodes, unsigned value_seed);

// The node count every collocation-chain trace uses. A rig constant, not a
// tunable: an expected table derived at one node count is meaningless against
// a run at another, so this moves only by editing it here and re-deriving.
Index chain_nodes();

// The same collocation structure with an interior-point barrier diagonal added
// to the primal block, which is what makes an interior-point KKT a different
// matrix class from a bound-eliminated one. `iterate` selects the barrier
// parameter's rung; the pattern is identical across rungs.
Fixture barrier_chain_kkt(Index nodes, Index iterate);

// HS76's KKT at a working set. HS76 is a four-variable, three-inequality
// quadratic program; `active_rows` names which inequality rows are in the
// working set.
Fixture hs76_kkt(const std::vector<Index> &active_rows);

// The saddle KKT: a two-variable problem whose Hessian has one negative
// eigenvalue and no constraints, so the correct inertia for its dimension is
// not the one a minimizer would have. The case an inertia gate exists for.
Fixture saddle_kkt();

// A positive-SEMIdefinite Hessian on the face -- one exactly-zero eigenvalue
// before regularization. The boundary between the definite and indefinite
// cases.
Fixture semidefinite_boundary_kkt();

// A clean positive-definite-on-the-face equality-constrained KKT with
// `n_free` primal and `m_face` dual rows: the accepted case, whose inertia is
// exactly (n_free, m_face, 0).
Fixture pd_on_face_kkt(Index n_free, Index m_face);

// A rank-deficient constraint block: the same equality stated twice, with no
// dual regularization, so the KKT is exactly singular by construction. This is
// the shape of both the rank-deficient-face refusal and the singular-KKT
// routing fixtures.
//
// `dual_reg` > 0 turns it back into a regular system, which is what an
// inertia-correction ladder does rung by rung.
Fixture duplicated_equality_kkt(double primal_reg, double dual_reg);

// The active-bound curvature control: a bound-constrained problem whose
// condensed KKT carries a very large curvature term on one primal diagonal
// entry. Pinned to NOT read as singular, however large that term gets.
Fixture active_bound_curvature_kkt(double sigma);

// A brutally scaled feasible QP's KKT: a Hessian spanning five orders of
// magnitude and a right-hand side at 1e9, which is where iterative refinement
// and pivot perturbation have something to do.
Fixture brutally_scaled_kkt();

// An exactly singular 3x3 whose zero pivot the MKL backend perturbs rather
// than reporting a zero class. Small, and the shortest path to a factorization
// with a nonzero perturbed-pivot count.
Fixture perturbing_singular_kkt();

// A small dense symmetric-indefinite block, for the traces that interleave a
// dense border factor with the sparse oracle.
Mat dense_border_block(Index m);

} // namespace hven::rig
