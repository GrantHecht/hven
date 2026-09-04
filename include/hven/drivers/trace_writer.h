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
/// neither opens, closes nor flushes it beyond what `operator<<` does. Nothing
/// here throws on a failed stream -- a trace sink that aborted a solve because a
/// disk filled would make instrumentation load-bearing, which CLAUDE.md section
/// 7 forbids; the caller reads the stream's own state to learn whether the
/// artifact is complete.
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

    /// Lines written so far, which is also the `seq` the LAST line carried
    /// (`seq` starts at 1, so a reader can prove it saw every line).
    Index lines_written() const { return seq_; }

    /// The value the NEXT line's `depth` will carry. 0 for the whole of W4 T1:
    /// the `sqp.solve` begin/end pair that moves it is T2's, and the two hooks
    /// below exist NOW so that task wires calls rather than fields.
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
};

} // namespace hven::solvers
