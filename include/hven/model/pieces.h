// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// pieces.h -- everything a problem built from pieces needs, in one include.
//
// - model/local_blocks.h: the local-block piece layer, the default way to write
//   a piece (dense local blocks; the library writes the claims and the scatter).
// - model/assembly_declaration.h: the declaration and its builder.
// - model/problem.h: Problem(AssemblyDeclaration), what IpmSolver solves.
// - The raw piece protocol, the expert route: ConstraintInterface /
//   ObjectiveInterface (model/solver_interface_specs.h), ConstraintFunction /
//   ObjectiveFunction, SolverIndexingData and kkt_canonical_lock_col
//   (model/indexing_data.h). Specified in docs/assembly-protocol.md.
//
// The SQP engine does not solve a problem built from pieces; SqpSolver refuses it
// by name.

#include "hven/model/assembly_declaration.h"
#include "hven/model/constraint_function.h"
#include "hven/model/indexing_data.h"
#include "hven/model/local_blocks.h"
#include "hven/model/objective_function.h"
#include "hven/model/problem.h"
#include "hven/model/solver_interface_specs.h"
#include "hven/model/threading_flags.h"
