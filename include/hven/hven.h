// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief The umbrella header: the whole public surface of hven in one include.
///
/// Everything here is in namespace `hven`, except the factorization layer, which is
/// `hven::linear`. Nothing under `hven/detail/` is part of the public surface.

// core
#include <hven/core/compiler.h>
#include <hven/core/errors.h>
#include <hven/core/ledger.h>
#include <hven/core/pattern_hash.h>
#include <hven/core/solver_counters.h>
#include <hven/core/solver_status.h>
#include <hven/core/start_level.h>
#include <hven/core/stop.h>
#include <hven/core/type_storage.h>
#include <hven/core/types.h>
#include <hven/core/version.h>

// the factorization layer (hven::linear)
#include <hven/linear/dense_policy.h>
#include <hven/linear/dense_symmetric_factor.h>
#include <hven/linear/symmetric_factor.h>

// problems: the callback NLP, the problem both engines solve, and pieces
#include <hven/model/assembly_declaration.h>
#include <hven/model/candidate_point.h>
#include <hven/model/claim_space.h>
#include <hven/model/claim_stream_source.h>
#include <hven/model/constraint_function.h>
#include <hven/model/local_blocks.h>
#include <hven/model/nlp.h>
#include <hven/model/nlp_assembly.h>
#include <hven/model/objective_function.h>
#include <hven/model/parsed_io_flags.h>
#include <hven/model/piece_interfaces.h>
#include <hven/model/problem.h>
#include <hven/model/sizable_spec.h>
#include <hven/model/solver_function_base.h>
#include <hven/model/solver_indexing_data.h>
#include <hven/model/solver_interface_adapter.h>
#include <hven/model/structure_identity.h>
#include <hven/model/threading_flags.h>

// the QP tier's public types
#include <hven/qp/ipqp_evidence.h>
#include <hven/qp/qp_types.h>

// warm starts
#include <hven/warmstart/ipm_polish_extension.h>
#include <hven/warmstart/seeding.h>
#include <hven/warmstart/sqp_warm_start.h>
#include <hven/warmstart/warm_start.h>

// the engines, their options, results and traces
#include <hven/drivers/common_options.h>
#include <hven/drivers/console_trace_sink.h>
#include <hven/drivers/ipm_enums.h>
#include <hven/drivers/ipm_iterate.h>
#include <hven/drivers/ipm_options.h>
#include <hven/drivers/ipm_phase_report.h>
#include <hven/drivers/ipm_solver.h>
#include <hven/drivers/ipm_solver_types.h>
#include <hven/drivers/json_lines_trace_sink.h>
#include <hven/drivers/solve_result.h>
#include <hven/drivers/sqp_options.h>
#include <hven/drivers/sqp_solver.h>
#include <hven/drivers/sqp_solver_types.h>
#include <hven/drivers/step_verdict.h>
#include <hven/drivers/trace.h>
