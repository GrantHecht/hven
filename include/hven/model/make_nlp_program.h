// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// make_nlp_program.h -- THE PUBLIC ENTRY from a declared problem to the
// NonLinearProgram the interior-point engine consumes.
//
// M7 W0 T5(a) MOVED these two declarations here from
// detail/model/nlp_adapter.h. Nothing about them changed -- same namespace,
// same signatures, same defaults, same doc comments -- and the detail header
// now includes this one, so every existing include site compiles unchanged.
// The move is purely about reachability: transcription is the first call an
// external consumer makes, and a consumer was being told to include a detail
// header to make it (nlp_triplet_model.h's own class comment said so, and the
// install-smoke consumer did exactly that).
//
// DECLARATIONS ONLY: the bodies stay in src/model/nlp_adapter.cpp, and the
// adapter machinery -- NLPAdapterCore, the pieces, the claim pass -- stays in
// detail/. A caller that only transcribes never needs that header.

#include <memory>

namespace hven::solvers {

struct NonLinearProgram;
struct NLPAdapterCore;
class NlpTripletModel;

/// @brief Builds the NonLinearProgram for an adapter core.
/// @param core The host whose patterns and bounds the program is laid over.
/// @param num_partitions Requested evaluation-partition count; see the
///        LAYOUT-ONLY note below. Default 1, which is what every caller before
///        M6 W5 T8.9 got.
/// @return The program: the objective piece, the constraint pieces the row
///         counts call for, the staged variable bounds, and the layout.
/// @throws std::invalid_argument if @p num_partitions is below 1.
///
/// The one production path, and the one the tests assemble through.
///
/// PARTITIONS THROUGH THIS ADAPTER ARE LAYOUT ONLY. All three pieces are
/// ThreadingFlags::MainThread, because NLPAdapterCore is one shared stateful
/// object, and analyze_partitioning forces every MainThread function into the
/// LAST partition to be run inline on the calling thread. So N here means N
/// laid partitions with the whole problem in partition N-1, evaluated serially
/// -- nothing reorders and nothing runs in parallel. Only treatment-added rows
/// (the MakeConstraint fixing rows, which are RoundRobin) ever populate the
/// others. Genuine partitioned evaluation over an NlpTripletModel needs a
/// thread-safe adapter and is registered for the M7 ClaimStreamSource widening.
///
/// THE COUNT IS CLAMPED, NOT REFUSED: make_nlp caps it at
/// num_user_kkt_elems_ / kMinKktElementsPerPartition (1000), so a small problem
/// silently ADOPTS fewer. Read the adopted count off the returned program's
/// public `num_partitions_` member (equivalently `declaration().partition_count_`),
/// never off the request.
std::shared_ptr<NonLinearProgram> make_nlp_program(const std::shared_ptr<NLPAdapterCore> &core,
                                                   int num_partitions = 1);

/// @brief THE ONE-CALL TRANSCRIPTION: an NlpTripletModel to the program the
///        interior-point engine consumes.
///
/// The named replacement for NLPSolver::transcribe() (M6 W5 T8.9), which is
/// what every former consumer of that wrapper re-derived by hand:
/// NlpProblemModel(problem) -> NLPAdapterCore(model, problem->name()) -> the
/// program. A caller that needs the intermediate model as well (to compose or
/// split user multipliers, say) builds the NlpProblemModel itself and calls the
/// core-taking overload above.
///
/// Transcription evaluates eval_jac and eval_hess once, at the model's start
/// point (the origin projected onto the declared variable bounds), and keeps
/// only the sparsity patterns; those two callbacks must be defined there. A
/// transcription that faults commits nothing.
///
/// @param problem The declared problem; must not be null.
/// @param num_partitions Requested partition count, LAYOUT ONLY and CLAMPED --
///        see the overload above.
/// @return The program.
/// @throws std::invalid_argument if @p problem is null or @p num_partitions is
///         below 1, and whatever the conversion and the layout themselves throw.
std::shared_ptr<NonLinearProgram> make_nlp_program(std::shared_ptr<NlpTripletModel> problem,
                                                   int num_partitions = 1);

} // namespace hven::solvers
