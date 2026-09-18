// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// bench/counting_trace_sink.h -- THE COUNTING TRACE SINK, SHARED BY THE TWO
// BENCH TUs THAT ATTACH ONE (M7 W0 T2).
//
// The class below is M6 W6 T6's, MOVED here VERBATIM from bench_corpus.cpp's
// anonymous namespace -- not rewritten and not re-derived. It was a local of
// that translation unit, which is why W6 T6 could not attach it on the
// top-level interior-point leg (bench/ipm_corpus_leg.cpp is a second TU): that
// leg's missing sink arm was T6's declared omission and this header is half of
// closing it. bench_corpus.cpp now uses it from here, so the SQP corpus's
// `--trace sink` arm and the interior leg's HVEN_LEG_TRACE_SINK arm attach the
// SAME sink and a reader comparing the two legs is comparing one observer.
//
// IT IS A BENCH INSTRUMENT AND NOTHING ELSE. Nothing in the library, the tests
// or the install surface includes it.

#include <hven/drivers/trace.h>

namespace hven::solvers::corpus {

/// @brief A real sink that COUNTS, per site, and keeps nothing else.
///
/// It must do real work at the call -- a sink whose overrides were empty could
/// be optimised into nothing and would not prove the emit path ran -- but it
/// must also not dominate the timing it is there to enable, so it counts and
/// returns. The per-site counts ARE the evidence that each mapper site fired.
class CountingTraceSink final : public TraceSink {
  public:
    void on_ipqp_iter(const hven::solvers::IpqpTraceIterEvent &) override { ++ipqp_iter_; }
    void on_ipqp_reg(const hven::solvers::IpqpTraceRegEvent &) override { ++other_; }
    void on_ipqp_restart(const hven::solvers::IpqpTraceRestartEvent &) override { ++other_; }
    void on_ipqp_route(const hven::solvers::IpqpTraceRouteEvent &) override { ++other_; }
    void on_ipqp_certify(const hven::solvers::IpqpTraceCertifyEvent &) override { ++other_; }
    void on_ipqp_escape(const hven::solvers::IpqpTraceEscapeEvent &) override { ++other_; }
    void on_fallback_verdict(const hven::solvers::SqpFallbackVerdictTraceEvent &) override {
        ++fallback_verdict_;
    }
    void on_sqp_major(const hven::solvers::SqpMajorTraceEvent &) override { ++other_; }
    void on_sqp_solve_begin(const hven::solvers::SqpSolveBeginTraceEvent &) override { ++other_; }
    void on_sqp_solve_end(const hven::solvers::SqpSolveEndTraceEvent &) override { ++other_; }
    void on_ipm_iter(const hven::solvers::IpmIterTraceEvent &) override { ++other_; }
    void on_ipm_solve_begin(const hven::solvers::IpmSolveBeginTraceEvent &) override { ++other_; }
    void on_ipm_solve_end(const hven::solvers::IpmSolveEndTraceEvent &) override { ++other_; }

    /// THE ONE THAT MATTERS. `qp.mode` is the event both driver-side
    /// `trace_outcome_of` calls construct and both kernels-side
    /// `emit_qp_mode_line` calls construct, and `site` is what tells them
    /// apart -- so this counter, split by site, is the whole proof that the
    /// mapper's call sites executed.
    void on_qp_mode(const QpModeTraceEvent &event) override {
        switch (event.site) {
        case QpModeSite::kDispatch:
            ++dispatch;
            break;
        case QpModeSite::kSsnWarmGrade:
            ++ssn_warm_grade;
            break;
        case QpModeSite::kFallbackRungB:
            ++fallback_rung_b;
            break;
        case QpModeSite::kElasticRung:
            ++elastic_rung;
            break;
        case QpModeSite::kSocResolve:
            ++soc_resolve;
            break;
        // M7 W2 M1 added this site with SqpOptions::qp_cap_retry, which ships
        // false, so nothing in this bench reaches it; the switch was left
        // unhandled and -Wswitch has warned on every build since. Folded into
        // the unnamed pool rather than given a counter of its own: unnamed_events()
        // is what proves "attached and reached" for a site no named column
        // reports, and promoting it to a column is the job of whichever task
        // first measures the retry lever.
        case QpModeSite::kCapRetry:
            ++other_;
            break;
        }
    }

    /// Zeroed explicitly rather than by assigning a fresh instance: this type
    /// is polymorphic, and copy-assigning a polymorphic object to reset it is
    /// the kind of clever that stops being true when someone adds a member.
    void reset() {
        dispatch = 0;
        soc_resolve = 0;
        elastic_rung = 0;
        fallback_rung_b = 0;
        ssn_warm_grade = 0;
        ipqp_iter_ = 0;
        fallback_verdict_ = 0;
        other_ = 0;
    }

    /// M6 W6 T6. The events that reach NO named counter above -- the SQP
    /// major/solve-begin/solve-end sites, the IPQP iteration site and the
    /// fallback verdict. The corpus attached-observer arm needs them because a
    /// walk-mode cell fires no `qp.mode` site at all, so every counter above
    /// stays zero and the five of them together could not tell "the sink was
    /// attached and never reached" from "the sink was attached and reached".
    /// The HS leg's own report is unchanged: it reads the named counters.
    long long unnamed_events() const { return ipqp_iter_ + fallback_verdict_ + other_; }

    long long dispatch = 0;
    long long soc_resolve = 0;
    long long elastic_rung = 0;
    long long fallback_rung_b = 0;
    long long ssn_warm_grade = 0;

  private:
    long long ipqp_iter_ = 0;
    long long fallback_verdict_ = 0;
    long long other_ = 0;
};

} // namespace hven::solvers::corpus
