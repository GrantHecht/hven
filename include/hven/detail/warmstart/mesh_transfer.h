// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// mesh_transfer.h — MeshTransfer: mapping a WarmStart (warm_start.h) from one
// DISCRETIZATION MESH onto another, which is the operation a mesh-refinement
// loop needs between its solves.
//
// This is the SYNTHETIC, TRANSCRIPTION-AGNOSTIC implementation: it knows
// nothing about collocation, defect constraints, phases, or how a real
// transcription lays a trajectory out in a flat vector. It assumes ONE VALUE
// PER MESH NODE (the LAYOUT CONTRACT below) and nothing else. A future
// transcription-aware version replaces EXACTLY THIS LAYER -- the layout
// mapping and nothing beneath it; the mathematics below (what to unscale,
// what to interpolate, what to rescale) and the ACTIVITY INHERITANCE POLICY
// are meant to survive that replacement unchanged.
//
// =====================================================================
// 1. WHY RAW MULTIPLIER COPYING IS THE KNOWN WRONG ANSWER
// =====================================================================
//
// A mesh-discretized optimal-control problem is a QUADRATURE of a continuous
// one. Take the shape every such transcription has in common:
//
//     min_y  INTEGRAL_a^b g(y(t), t) dt      s.t.  c(y(t), t) <= 0 for all t,
//
// discretized on nodes t_0 < ... < t_{M-1} with quadrature weights w_i > 0 as
//
//     min_x  sum_i w_i g(x_i, t_i)           s.t.  c(x_i, t_i) <= 0, each i.
//
// THE OBJECTIVE CARRIES THE WEIGHT AND THE CONSTRAINT DOES NOT, and that
// asymmetry is the whole story. Stationarity of the DISCRETE problem (this
// project's convention, nlp_model.h) at node i reads
//
//     w_i * dg/dy(x_i, t_i) + (dc/dy)^T lambda_i = 0,
//
// while stationarity of the CONTINUOUS problem, whose multiplier nu(t) is a
// DENSITY -- a price per unit t, because the constraint is imposed on a
// continuum -- reads
//
//     dg/dy(y(t), t) + (dc/dy)^T nu(t) = 0.
//
// Comparing the two at t = t_i gives the mapping this header is built on:
//
//     lambda_i = w_i * nu(t_i).                                    (COSTATE)
//
// THE DISCRETE MULTIPLIER IS NOT A SAMPLE OF A FUNCTION. It is a sample TIMES
// THE LOCAL QUADRATURE WEIGHT -- an INTEGRATED quantity attached to a node,
// whose magnitude therefore shrinks as the mesh refines even though the
// underlying physics is unchanged. Two meshes' multipliers for the SAME
// continuous solution differ by the ratio of their weights: refine 2x and
// every multiplier HALVES.
//
// So COPYING lambda ACROSS MESHES -- or, equivalently, interpolating lambda
// itself as if it were a function sample -- IS WRONG BY THE WEIGHT RATIO: a
// uniform factor on a uniform refinement that DOES NOT DIMINISH as either
// mesh refines, and a spatially varying factor (wrong SHAPE as well as wrong
// size) on a non-uniform destination.
//
// THE FIX is the standard costate/transformed-adjoint conversion of the
// transcription literature (Garg-Rao's costate mapping for pseudospectral
// collocation; Betts's transformed adjoint variables):
//
//     UNSCALE      lambda_hat_i := lambda_i / w_i        ~ nu(t_i)
//     INTERPOLATE  nu_hat(s_k)  := interpolate lambda_hat at destination s_k
//     RESCALE      lambda'_k    := w'_k * nu_hat(s_k)
//
// The middle step is the only one that can lose accuracy, and it operates on
// nu, which IS a mesh-free function of t and so is exactly what interpolation
// is valid on. Everything else is exact arithmetic.
//
// THE RULE IS ABOUT A ROW'S SCALING, NOT ABOUT WHICH VECTOR IT LANDS IN, and
// lambda_e, lambda_i and z are treated identically here for that reason. A
// PATH CONSTRAINT is weight-scaled whichever vector a transcription parks it
// in -- written as a row of cI it gives lambda_i = w_i * nu(t_i); the same
// bound written as a box on the node variable gives z_i = w_i * nu(t_i). Both
// are instances of (COSTATE): THE OBJECTIVE TERM CARRIES THE QUADRATURE
// WEIGHT AND THE CONSTRAINT ROW DOES NOT.
//
// A ROW THAT CARRIES A WEIGHT OF ITS OWN BREAKS THAT HYPOTHESIS AND IS NOT
// WEIGHT-SCALED, and the DEFECT BLOCK of an integration transcription is
// exactly where that happens -- so it is AN EXAMPLE OF TRANSCRIPTION-
// DEPENDENT SCALING, NOT AN EXAMPLE OF THE RULE. Run (COSTATE)'s own
// derivation on a row pre-multiplied by the interval length,
// h_i * c(x_i) = 0: stationarity becomes w_i * dg/dy + h_i * J^T lambda_i = 0,
// and with w_i proportional to h_i the two weights CANCEL, leaving
// lambda_i = nu(t_i) -- UNWEIGHTED. Concretely:
//   - Betts's h-MULTIPLIED defect form has the interval length inside the
//     row, so its multiplier IS the costate already, O(1) and
//     mesh-independent; sending it through the density path below would
//     introduce precisely the weight-ratio error this header exists to
//     prevent.
//   - The RESIDUAL form of the same defect, and Radau pseudospectral
//     differentiation-matrix defects, carry no weight in the row, so their
//     multipliers ARE weight-scaled and lambda/w is the standard costate
//     estimate.
// Two transcriptions of the same continuous problem therefore disagree on
// whether lambda_e needs the density treatment. THIS SYNTHETIC LAYER CANNOT
// TELL THEM APART, so it applies the density rule uniformly, which is correct
// for every block under its own layout contract (section 2) and is why a
// transcription-aware replacement must classify the DEFECT BLOCK FIRST when
// it builds its per-block map: weight-scaled blocks keep this arithmetic,
// h-multiplied ones are copied through unchanged.
//
// x IS NOT WEIGHT-SCALED and is interpolated plainly. x_i approximates y(t_i),
// a POINTWISE state value in the state's own physical units, with no quadrature
// weight anywhere in its definition. Applying the multiplier rule to it would
// be exactly the mirror-image error this header exists to prevent.
//
// =====================================================================
// 2. THE LAYOUT CONTRACT
// =====================================================================
//
// FOR THE SYNTHETIC MODEL, EVERY FLAT VECTOR OF A WarmStart IS ONE VALUE PER
// MESH NODE, IN NODE ORDER:
//
//     ws.x[i], ws.lambda_e[i], ws.lambda_i[i], ws.z[i],
//     ws.ineq_active[i], ws.bound_active[i]      <-> mesh node i
//
// so a source WarmStart's every non-empty vector has EXACTLY from.nodes.size()
// entries and every vector of the result has to.nodes.size(). AN EMPTY VECTOR
// IS ABSENT, NOT MALFORMED (a model with me == 0 emits an empty lambda_e; a
// solve that could attribute no activity emits empty activity vectors --
// warm_start.h) and transfers to an empty vector of the same kind. Any other
// size is a caller error and throws.
//
// THIS IS THE ONLY PART OF THE MATHEMATICS A TRANSCRIPTION-AWARE REPLACEMENT
// CHANGES -- but not the only work it has to do; see section 4's ingest note,
// which is a DRIVER concern and not a transfer change. A real transcription
// interleaves states, controls, static parameters and per-phase blocks, so
// "entry i belongs to node i" becomes a per-block index map (which is also
// where section 1's defect-block classification lives); the unscale/
// interpolate/rescale arithmetic and the inheritance rule below are applied
// blockwise instead of vectorwise, and nothing else in the mathematics
// changes.
//
// =====================================================================
// 3. ACTIVITY INHERITANCE
// =====================================================================
//
// THE RULE, in one sentence: A DESTINATION NODE INHERITS A LABEL IFF EVERY
// SOURCE NODE BOUNDING THE SOURCE INTERVAL(S) WHOSE CLOSURE CONTAINS IT
// CARRIES THAT SAME LABEL; OTHERWISE IT IS LEFT FREE AND THE DESTINATION QP
// DECIDES.
//
// Concretely, the STENCIL of a destination node s is
//   - {j, j+1}          when s lies strictly inside the source cell (t_j, t_j+1);
//   - {k-1, k, k+1}     when s coincides with source node k (both cells whose
//                       closure contains it, clipped at the mesh ends).
// Unanimity over the stencil gives the label; disagreement gives FREE (0),
// which for ineq_active is "not active" and for bound_active is "not at a
// bound" -- the two encodings agree on 0 meaning exactly that.
//
// WHY UNANIMITY AND NOT NEAREST-NEIGHBOUR. An active set on a mesh is the
// discrete shadow of an active SUB-INTERVAL of the continuum, and the
// JUNCTIONS -- where the constraint enters and leaves the active set -- are
// located only to within one source cell. Anywhere strictly inside the active
// region the source's evidence is unambiguous and transferring it saves the
// destination QP the work of rediscovering it. Within a cell of a junction the
// source has NO information at the destination's resolution, and a guess there
// is worse than no guess: a wrongly-activated row constrains the first
// destination subproblem to a face the solution is not on, which the QP must
// then spend minor iterations backing off. Refusing to guess costs at most the
// same iterations and can never mislead. This is also why the WHOLE stencil
// must agree rather than the containing cell alone: a destination node sitting
// exactly ON a junction source node is bounded by one active cell and one
// inactive cell, and is precisely the case a containing-cell rule would decide
// arbitrarily (by which side the floating-point comparison fell on).
//
// THE THREE EDGE CASES, DECIDED:
//   (i)   JUNCTION AT A SHARED NODE. A destination node coinciding with a
//         source node whose two neighbours disagree with it (or with each
//         other) is FREE. Under a nested refinement this means the two END
//         nodes of every active run are surrendered and only the run's strict
//         interior is inherited -- deliberately, per the paragraph above.
//   (ii)  SINGLE-NODE ACTIVE ISLANDS VANISH ENTIRELY. A run of length one is
//         all junction and no interior: nothing is inherited anywhere near
//         it. This is the conservative reading and intentional -- a single
//         active node is as likely to be a mesh artifact as a real feature,
//         and the destination mesh (usually finer exactly there) is better
//         placed to decide.
//   (iii) DESTINATION COARSER THAN SOURCE. Inheritance is POINT SAMPLING, so
//         an active run survives only where a destination node lands strictly
//         inside it. A run narrower than the destination spacing may be missed
//         entirely; that is a lost hint, never a wrong answer, and the
//         destination solve recovers it.
//
// NODE COINCIDENCE IS A GEOMETRIC TEST, NOT A BITWISE ONE. Two meshes built
// independently can name "the same" node with coordinates differing in the
// last bits, and the stencil rule must not turn that into a different label.
// A destination node is treated as coincident with source node k when it is
// within kNodeMatchRelTol times the local source cell width of it -- far
// below any mesh feature and far above the rounding of ordinary coordinate
// arithmetic.
//
// =====================================================================
// 4. WHAT THE TRANSFERRED WarmStart CLAIMS, AND WHAT IT REFUSES TO
// =====================================================================
//
// THE OUTPUT DESCRIBES A DIFFERENT PROBLEM SIZE THAN THE INPUT, and every
// field below is decided from that single fact.
//
// structure_hash == 0, ALWAYS. That is warm_start.h's own "no model was seen"
// sentinel, and sqp_driver.h's warm-start ingest treats it EXACTLY like a
// mismatch -- so a transferred object can never reach kWarm or kHot. That is
// correct and intended: the destination is a different model with a different
// sparsity pattern, so a hash carried over from the source could only ever be
// wrong, and a hash that MATCHED would be a collision authorizing the reuse
// of a factorization of the wrong matrix. Setting the sentinel makes the
// refusal EXPLICIT rather than leaving it to the accident that two different
// models happen to hash differently.
//
// IT DOES NOT MEAN "RESOLVES THE SOLVE TO kCold AND TRUSTS NOTHING": a
// transferred object that is dimensionally consistent with the DESTINATION
// model -- which is exactly what a transfer produces -- and finite resolves
// `StartLevel::kSeeded`: its x, its unscaled costates, its inequality prices
// and its activity hint are all ingested; only the factorization, the funnel
// width, the trust-region radius and the Kungurtsev-Diehl window are refused.
// The sentinel's job is undiminished, because the ONE thing it was ever
// protecting -- factorization reuse -- is precisely the one thing kSeeded does
// not offer. (See warm_start.h's StartLevel note for the full take/refuse
// list.)
//
// hot == nullptr, ALWAYS, for the same reason one level down: a HotState is a
// frozen factorization of a specific K0, and no K0 of the source mesh
// factorizes anything on the destination mesh. Dropping it also releases the
// source solve's Pardiso handle at the transfer, which is the right ownership
// story for a refinement loop that keeps the transferred object and discards
// the source one.
//
// funnel_width IS NOT TRANSFERRED (left at warm_start.h's -1 "unset"), and
// this is the one judgement call in the list. The funnel width tau is measured
// in the units of the violation measure h(x) = ||cE||_1 + sum_j max(0, cI_j)
// -- an l1 SUM OVER A MESH-DEPENDENT SET OF ROWS. Refining a mesh changes both
// the number of terms in that sum and the size of each, by a factor this layer
// cannot know without knowing the transcription. The destination ingest's
// re-base CLAMPS the carried width from BELOW by kappa_bar * h0 at the
// destination's own first iterate, but has no ceiling: a stale width that is
// too LARGE passes through and leaves the funnel LOOSER than any evidence on
// the destination problem justifies, which is exactly the failure that matters
// (an over-tight funnel merely rejects steps; an over-loose one accepts bad
// ones). Since a cross-mesh width can be wrong in that direction by an unknown
// factor, the honest answer is to decline: the sentinel makes the destination
// solve seed its funnel from its own first measured h0, which is a real
// measurement on the right problem and costs nothing a cold solve does not
// already pay. A transcription-aware transfer that KNOWS how h scales with
// the mesh may carry a rescaled width; this layer does not guess.
//
// tr_radius IS TRANSFERRED UNCHANGED, and the contrast with funnel_width is
// the point. The trust region is an l-infinity bound ON THE STEP IN x, and x
// is a pointwise state value in physical units -- the SAME units on both
// meshes, with no quadrature weight and no row count in them. (primal_delta/
// dual_mu are carried for the same reason: they are algorithmic regularization
// constants, not mesh-scaled quantities. Neither is READ by today's ingest --
// kSeeded refuses trust-region state outright -- but carrying them keeps the
// record of what the source solve ran at, and a future level that trusts a
// transfer's provenance could read them.)
//
// valid IS TRUE, and a cold input THROWS rather than producing a cold output: a
// WarmStart with valid == false carries nothing that may be trusted or fed
// forward (warm_start.h), so asking to transfer one is a caller error, not a
// request for an empty answer.
//
// qp_working_set IS RE-DERIVED FROM THE TRANSFERRED ACTIVITY VECTORS rather
// than transferred separately: warm_start.h states that it carries exactly
// the same information as ineq_active/bound_active in the form the hot-start
// seeding consumes, so re-deriving it is free and makes it impossible for the
// two representations to disagree on the destination mesh.
//
// THE INPUT IS NEVER MUTATED (transfer() is const and takes its WarmStart by
// const reference; every output vector is freshly allocated).

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <fmt/format.h>

#include <hven/detail/qp/working_set.h>
#include <hven/detail/warmstart/warm_start.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers {

// A discretization mesh: STRICTLY INCREASING nodes and STRICTLY POSITIVE
// quadrature weights, one weight per node. Deliberately a plain aggregate with
// no constructor and no factory -- where a mesh came from (uniform, Gauss,
// adaptively refined) is the caller's business, and MeshTransfer validates both
// preconditions on every call rather than trusting a construction-time check
// that a caller could have bypassed by assigning to the members.
struct Mesh {
    Vec nodes;
    Vec weights;
};

namespace mesh_detail {

// See NODE COINCIDENCE in this header's section 3: a destination node counts
// as sitting ON a source node when it is within this fraction of the local
// source cell width of it.
inline constexpr double kNodeMatchRelTol = 1e-12;

inline void validate_mesh(const Mesh &mesh, const char *which) {
    if (mesh.nodes.size() != mesh.weights.size()) {
        throw std::invalid_argument(
            fmt::format("MeshTransfer::transfer: `{}` mesh has {} nodes but {} weights; expected {}"
                        " (one weight per node)",
                        which, mesh.nodes.size(), mesh.weights.size(), mesh.nodes.size()));
    }
    if (mesh.nodes.size() < 2) {
        throw std::invalid_argument(
            fmt::format("MeshTransfer::transfer: `{}` mesh has {} nodes, needs >= 2 (interpolation "
                        "needs at least one interval)",
                        which, mesh.nodes.size()));
    }
    for (Index i = 0; i < mesh.nodes.size(); ++i) {
        if (!std::isfinite(mesh.nodes(i))) {
            throw std::invalid_argument(
                fmt::format("MeshTransfer::transfer: `{}` mesh nodes[{}] = {} must be finite",
                            which, i, mesh.nodes(i)));
        }
        if (!(mesh.weights(i) > 0.0) || !std::isfinite(mesh.weights(i))) { // also catches NaN
            throw std::invalid_argument(fmt::format(
                "MeshTransfer::transfer: `{}` mesh weights[{}] = {} must be finite and > 0 (a "
                "quadrature weight scales this node's multipliers -- see mesh_transfer.h's "
                "(COSTATE) note; a zero weight would make the unscaling a division by zero)",
                which, i, mesh.weights(i)));
        }
    }
    for (Index i = 0; i + 1 < mesh.nodes.size(); ++i) {
        if (!(mesh.nodes(i + 1) > mesh.nodes(i))) {
            throw std::invalid_argument(fmt::format(
                "MeshTransfer::transfer: `{}` mesh nodes must be strictly increasing, but "
                "nodes[{}] = {} is not less than nodes[{}] = {}",
                which, i, mesh.nodes(i), i + 1, mesh.nodes(i + 1)));
        }
    }
}

// Where a destination coordinate `s` sits on `nodes`. `cell` is the index of
// the containing source cell [nodes[cell], nodes[cell+1]] and `theta` in
// [0, 1] the position within it; `at_node` is the index of the source node `s`
// coincides with, or -1 when it lies strictly inside the cell.
//
// OUT OF RANGE IS CLAMPED, NOT EXTRAPOLATED. A destination node below
// nodes[0] (or above the last) is treated as sitting AT that endpoint, so
// every interpolated quantity is extended by its endpoint VALUE rather than by
// its endpoint SLOPE. Linear extrapolation of a costate is the more dangerous
// choice by far -- it can drive a multiplier that must be >= 0 negative, and a
// state past a bound -- and the situation it arises in (a destination mesh
// spanning slightly more than the source's, from independently computed
// endpoints) is precisely the one where the extrapolated distance is
// meaningless anyway.
struct Locus {
    Index cell = 0;
    double theta = 0.0;
    Index at_node = -1;
};

inline Locus locate(const Vec &nodes, double s) {
    const Index last = nodes.size() - 1;
    Locus loc;
    if (!(s > nodes(0))) { // the !(>) form also sends NaN to the left endpoint
        loc.cell = 0;
        loc.theta = 0.0;
        loc.at_node = 0;
        return loc;
    }
    if (s >= nodes(last)) {
        loc.cell = last - 1;
        loc.theta = 1.0;
        loc.at_node = last;
        return loc;
    }
    // First index with nodes[j] > s, minus one: the cell containing s.
    const double *begin = nodes.data();
    const double *found = std::upper_bound(begin, begin + nodes.size(), s);
    loc.cell = static_cast<Index>(found - begin) - 1;
    loc.cell = std::clamp<Index>(loc.cell, 0, last - 1);
    const double width = nodes(loc.cell + 1) - nodes(loc.cell);
    loc.theta = (s - nodes(loc.cell)) / width;
    const double tol = kNodeMatchRelTol * width;
    if (s - nodes(loc.cell) <= tol) {
        loc.at_node = loc.cell;
        loc.theta = 0.0;
    } else if (nodes(loc.cell + 1) - s <= tol) {
        loc.at_node = loc.cell + 1;
        loc.theta = 1.0;
    }
    return loc;
}

// Piecewise-linear interpolation of `values` (one per source node) at every
// destination node, with the clamped extrapolation `locate` documents.
inline Vec interpolate(const Vec &from_nodes, const Vec &values, const Vec &to_nodes) {
    Vec out(to_nodes.size());
    for (Index k = 0; k < to_nodes.size(); ++k) {
        const Locus loc = locate(from_nodes, to_nodes(k));
        out(k) = (1.0 - loc.theta) * values(loc.cell) + loc.theta * values(loc.cell + 1);
    }
    return out;
}

// UNSCALE -> INTERPOLATE -> RESCALE, this header's section 1. An empty input
// is ABSENT and stays absent (section 2's layout contract).
inline Vec transfer_density(const Vec &lambda, const Mesh &from, const Mesh &to) {
    if (lambda.size() == 0) {
        return Vec(0);
    }
    const Vec hat = lambda.array() / from.weights.array();      // UNSCALE
    const Vec sampled = interpolate(from.nodes, hat, to.nodes); // INTERPOLATE
    return (sampled.array() * to.weights.array()).matrix();     // RESCALE
}

// The unanimity rule of this header's section 3, for either activity encoding
// (std::uint8_t for rows, std::int8_t for bounds -- both use 0 for "free").
template <typename Label>
std::vector<Label> inherit_activity(const std::vector<Label> &src, const Mesh &from,
                                    const Mesh &to) {
    if (src.empty()) {
        return {};
    }
    const Index last = from.nodes.size() - 1;
    std::vector<Label> out(static_cast<std::size_t>(to.nodes.size()), Label{0});
    for (Index k = 0; k < to.nodes.size(); ++k) {
        const Locus loc = locate(from.nodes, to.nodes(k));
        Index lo = loc.cell;
        Index hi = loc.cell + 1;
        if (loc.at_node >= 0) {
            // Both cells whose closure contains this node, clipped at the ends.
            lo = std::max<Index>(0, loc.at_node - 1);
            hi = std::min<Index>(last, loc.at_node + 1);
        }
        const Label candidate = src[static_cast<std::size_t>(lo)];
        bool unanimous = true;
        for (Index i = lo + 1; i <= hi; ++i) {
            if (src[static_cast<std::size_t>(i)] != candidate) {
                unanimous = false;
                break;
            }
        }
        out[static_cast<std::size_t>(k)] = unanimous ? candidate : Label{0};
    }
    return out;
}

// Section 2's layout contract, as a check: a vector is either ABSENT (empty)
// or has exactly one entry per source node.
inline void check_layout(Index size, Index expected, const char *field) {
    if (size != 0 && size != expected) {
        throw std::invalid_argument(fmt::format(
            "MeshTransfer::transfer: WarmStart::{} has size {}, expected {} (one value per `from` "
            "mesh node) or 0 (absent) -- see mesh_transfer.h's LAYOUT CONTRACT",
            field, size, expected));
    }
}

} // namespace mesh_detail

// Maps a WarmStart between discretization meshes. Stateless and const; one
// instance may serve any number of transfers.
class MeshTransfer {
  public:
    // Transfers `from_ws` -- whose vectors live on `from` under this header's
    // LAYOUT CONTRACT -- onto `to`. See sections 1-4 above for the complete
    // contract; in brief: x is linearly interpolated, every multiplier vector
    // is unscaled by the source weights, interpolated and rescaled by the
    // destination's, activity is inherited by the unanimity rule, and the
    // result carries structure_hash == 0 with a null `hot`.
    //
    // Throws std::invalid_argument (the offending value in
    // the message) on: a mesh with fewer than two nodes, non-finite or
    // non-increasing nodes, a non-finite or non-positive weight, a
    // nodes/weights length mismatch, a WarmStart vector that is neither empty
    // nor one-per-node, a qp_working_set whose shape contradicts the source
    // mesh, or a `from_ws` that is not `valid`.
    WarmStart transfer(const WarmStart &from_ws, const Mesh &from, const Mesh &to) const {
        mesh_detail::validate_mesh(from, "from");
        mesh_detail::validate_mesh(to, "to");

        if (!from_ws.valid) {
            throw std::invalid_argument(
                "MeshTransfer::transfer: `from_ws` is not valid (warm_start.h's cold state); a "
                "cold WarmStart carries nothing that may be trusted or fed forward, so there is "
                "nothing to transfer");
        }

        const Index m = from.nodes.size();
        const Index n = to.nodes.size();

        // x is MANDATORY -- it is the primal point the whole object is about
        // -- while every other vector may be absent (layout contract).
        if (from_ws.x.size() != m) {
            throw std::invalid_argument(fmt::format(
                "MeshTransfer::transfer: WarmStart::x has size {}, expected {} (one value per "
                "`from` mesh node); the primal point is mandatory and may not be absent",
                from_ws.x.size(), m));
        }
        mesh_detail::check_layout(from_ws.lambda_e.size(), m, "lambda_e");
        mesh_detail::check_layout(from_ws.lambda_i.size(), m, "lambda_i");
        mesh_detail::check_layout(from_ws.z.size(), m, "z");
        mesh_detail::check_layout(static_cast<Index>(from_ws.ineq_active.size()), m, "ineq_active");
        mesh_detail::check_layout(static_cast<Index>(from_ws.bound_active.size()), m,
                                  "bound_active");
        // The working set is not READ (it is re-derived below), but a shape
        // that contradicts the source mesh means the caller's layout
        // assumption and this header's disagree, which is worth reporting
        // rather than silently overwriting.
        if (from_ws.qp_working_set.n() != m) {
            throw std::invalid_argument(fmt::format(
                "MeshTransfer::transfer: WarmStart::qp_working_set has n() = {}, expected {} (one "
                "variable per `from` mesh node)",
                from_ws.qp_working_set.n(), m));
        }
        mesh_detail::check_layout(from_ws.qp_working_set.mi(), m, "qp_working_set.mi()");

        WarmStart out;

        // PRIMAL: a pointwise state value, interpolated plainly (section 1's
        // last paragraph).
        out.x = mesh_detail::interpolate(from.nodes, from_ws.x, to.nodes);

        // DUALS: unscale / interpolate / rescale (section 1).
        out.lambda_e = mesh_detail::transfer_density(from_ws.lambda_e, from, to);
        out.lambda_i = mesh_detail::transfer_density(from_ws.lambda_i, from, to);
        out.z = mesh_detail::transfer_density(from_ws.z, from, to);

        // ACTIVITY: the unanimity rule (section 3).
        out.ineq_active = mesh_detail::inherit_activity(from_ws.ineq_active, from, to);
        out.bound_active = mesh_detail::inherit_activity(from_ws.bound_active, from, to);

        // The working set, RE-DERIVED from the two vectors just produced so
        // the three representations cannot disagree (section 4). Its mi() is n
        // whenever the source carried ANY inequality information AT ALL, and 0
        // otherwise, so it mirrors the source model's own mi() as the layout
        // contract records it. ALL THREE CARRIERS ARE CONSULTED -- the
        // multiplier vector, the activity vector AND the source working set's
        // own mi() -- because any one of them being non-empty means the source
        // model had inequality rows, and a source that carried only the third
        // (mi() == m with an empty lambda_i and an empty ineq_active) would
        // otherwise be silently reported here as a model with no rows. Today's
        // driver never emits that shape (a solve with mi > 0 always populates
        // lambda_i), so this is a consistency guarantee rather than a live
        // path -- which is exactly why it is spelled out instead of left to the
        // reader to notice it cannot happen.
        const bool has_ineq = from_ws.lambda_i.size() != 0 || !from_ws.ineq_active.empty() ||
                              from_ws.qp_working_set.mi() != 0;
        out.qp_working_set = WorkingSet(n, has_ineq ? n : 0);
        if (!out.ineq_active.empty()) {
            for (Index k = 0; k < n; ++k) {
                if (out.ineq_active[static_cast<std::size_t>(k)] != 0) {
                    out.qp_working_set.add_ineq(k);
                }
            }
        }
        if (!out.bound_active.empty()) {
            std::vector<BoundState> &states = out.qp_working_set.bound_state();
            for (Index k = 0; k < n; ++k) {
                const std::int8_t label = out.bound_active[static_cast<std::size_t>(k)];
                states[static_cast<std::size_t>(k)] = label < 0   ? BoundState::kAtLower
                                                      : label > 0 ? BoundState::kAtUpper
                                                                  : BoundState::kFree;
            }
        }

        // GLOBALIZATION / REGULARIZATION STATE, per section 4: the radius and
        // the regularization constants are in mesh-independent units and carry
        // over; the funnel width is in h's mesh-dependent units and is left at
        // warm_start.h's "unset" sentinel so the destination solve seeds its
        // own funnel from its own first measurement.
        out.funnel_width = -1.0;
        out.tr_radius = from_ws.tr_radius;
        out.primal_delta = from_ws.primal_delta;
        out.dual_mu = from_ws.dual_mu;

        // A TRANSFERRED WARM CAN NEVER CLAIM A STRUCTURAL MATCH (section 4).
        out.structure_hash = 0;
        out.hot = nullptr;
        out.valid = true;
        return out;
    }
};

} // namespace hven::solvers
