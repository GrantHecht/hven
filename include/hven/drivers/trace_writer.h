// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// trace_writer.h -- schema v0's ONE serializer: `IpqpTraceSink`'s eight events
// written as JSON lines to a caller-owned `std::ostream`.
//
// The contract is FIXED by docs/notes/2026-09-m6-w4-plan.md section 2 and is not
// this file's to re-decide. One object per line, `\n`-terminated, no padding,
// schema key order, envelope first.
//
// Doubles at `{:.17g}`, with the three non-finite JSON STRINGS "nan"/"inf"/
// "-inf"; absence is `null`, never zero-filled (CLAUDE.md section 6); strings
// escaped per RFC 8259 section 7.
//
// SCHEMA v0 CARRIES NO VECTOR-VALUED FIELD ANYWHERE (settler ruling, W4 T1 fix
// round 1), so `IpqpInfeasibilityEvidence`'s five `Vec` members are out and
// `ipqp.escape` carries the least-infeasible point's two scalars, not the point.
//
// `seq` and `depth` are SINK-owned -- no event struct carries either. Off by
// default: nothing in the library constructs one, so an unattached solve pays
// exactly what it paid before (W4 T1 pin (iv)).

#include <iosfwd>
#include <string>

#include <hven/core/types.h>
#include <hven/detail/qp/ipqp_trace.h>

namespace hven::solvers {

/// @brief Writes one JSON object per line to a caller-owned `std::ostream`.
///
/// THE STREAM IS BORROWED, NOT OWNED: it must outlive the sink, and the sink
/// neither opens, closes nor flushes it -- not even at destruction, so a caller
/// reading the file before the `ofstream` is closed sees a short artifact.
///
/// EXCEPTIONS -- TWO CASES, and the instrumentation invariant holds only in the
/// first. Under the DEFAULT exception mask the sink never throws and can never
/// end a solve: it constructs none of its own, and `operator<<` on a failed
/// stream sets state bits rather than throwing. Under a mask the caller ARMED
/// (`exceptions(std::ios::badbit)`) `std::ios_base::failure` propagates out of
/// the emitting `on_*` call and therefore out of the solve -- BY DESIGN, since
/// swallowing it would silently defeat the caller's own request. A caller who
/// wants a solve that its trace can never end must not arm one. (`bad_alloc` on
/// the per-line buffer is the process's, not the sink's.)
///
/// `facts` IS CALLER TEXT AND MUST BE VALID UTF-8: the sink escapes `"`, `\` and
/// the C0 range and passes every other byte through, so a caller that hands it
/// ill-formed UTF-8 gets a line that is not JSON text (RFC 8259 section 8.1).
/// Nothing in the library sets the field.
///
/// THREADING: one sink serves one solve at a time on one thread. `seq_`,
/// `depth_` and the write are unsynchronized; T2's nested restoration driver
/// shares the sink SEQUENTIALLY, which is the only sharing v0 supports.
class JsonLinesTraceSink final : public IpqpTraceSink {
  public:
    explicit JsonLinesTraceSink(std::ostream &out);
    ~JsonLinesTraceSink() override;

    JsonLinesTraceSink(const JsonLinesTraceSink &) = delete;
    JsonLinesTraceSink &operator=(const JsonLinesTraceSink &) = delete;

    void on_ipqp_iter(const IpqpTraceIterEvent &event) override;
    void on_ipqp_reg(const IpqpTraceRegEvent &event) override;
    void on_ipqp_restart(const IpqpTraceRestartEvent &event) override;
    void on_ipqp_route(const IpqpTraceRouteEvent &event) override;
    void on_ipqp_certify(const IpqpTraceCertifyEvent &event) override;
    void on_ipqp_escape(const IpqpTraceEscapeEvent &event) override;
    void on_qp_mode(const QpModeTraceEvent &event) override;
    void on_fallback_verdict(const SqpFallbackVerdictTraceEvent &event) override;
    void on_sqp_major(const SqpMajorTraceEvent &event) override;
    void on_sqp_solve_begin(const SqpSolveBeginTraceEvent &event) override;
    void on_sqp_solve_end(const SqpSolveEndTraceEvent &event) override;

    /// Lines ATTEMPTED, which is also the `seq` the last line carried (`seq`
    /// starts at 1). Compared against the artifact's own line count it gives the
    /// number of lines lost, process-side; the `seq` gap gives the same number
    /// artifact-side.
    Index lines_written() const { return seq_; }

    /// @brief THE PROCESS-SIDE FAILURE PREDICATE: the stream reported failure at
    /// or before one of this sink's writes.
    ///
    /// Read from the stream's state after EVERY write and sticky from the first
    /// failure. It CANNOT attribute the failure to this sink -- another writer
    /// on the same stream produces the same reading -- and it never resets, even
    /// if the caller clears the stream. `seq_` keeps advancing either way, so a
    /// failed run still yields a countable gap rather than a renumbered stream.
    bool failed() const { return failed_; }

    /// @brief The value the NEXT line's `depth` will carry.
    ///
    /// THE TOP-LEVEL SOLVE IS DEPTH 0, and the nested restoration sub-solve's
    /// lines -- its own `sqp.solve` pair included -- are depth 1. So the counter
    /// moves on the SECOND solve to open, not the first: `depth` is the number
    /// of ENCLOSING solves, which is what lets a reader separate the two streams
    /// with no stack of its own. Balanced by construction over a normal solve;
    /// it reads 0 again once the outermost `sqp.solve.end` is written.
    Index depth() const { return depth_; }

    /// @brief Nested-solve entry: the restoration sub-solve's own events then
    /// read one level deeper, with no stack in the reader and no field on any
    /// event.
    void push_depth();

    /// @brief Nested-solve exit. Saturates at 0 rather than going negative: an
    /// unbalanced pop is a caller bug, and a negative depth in the artifact
    /// would be a fabricated reading of one.
    void pop_depth();

  private:
    /// Wraps `body` -- the event's own keys, comma-separated, no braces -- in
    /// this line's envelope, writes it, and advances `seq_`.
    void write_line(const char *ev, const std::string &body);

    std::ostream &out_;
    Index seq_ = 0;
    Index depth_ = 0;
    /// Solves currently OPEN on this sink. `depth_` is this minus one, floored
    /// at 0; keeping the two separate is what makes the outermost solve depth 0
    /// while `push_depth`/`pop_depth` stay usable on their own.
    Index open_solves_ = 0;
    bool failed_ = false;
};

} // namespace hven::solvers
