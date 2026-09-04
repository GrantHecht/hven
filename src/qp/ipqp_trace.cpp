// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// R6 (Codex 5, CLAUDE.md section 5): the sink's non-hot virtual destructor
// lives here, not inline in the header.

#include <hven/detail/qp/ipqp_trace.h>

namespace hven::solvers {

IpqpTraceSink::~IpqpTraceSink() = default;

// The W4 events' defaults are EMPTY, not pure (plan section 6 Q-S3): a sink
// written for the eight W1/W2 events keeps compiling and simply ignores them.
void IpqpTraceSink::on_sqp_major(const SqpMajorTraceEvent &) {}

} // namespace hven::solvers
