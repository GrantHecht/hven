// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// Two trace-surface gaps the sink suites left open:
//
//   * FanOutTraceSink forwards EVERY event kind (all seventeen virtuals) to both borrowed sinks,
//     first then second, tolerates a null on either side, and the base TraceSink's defaults are
//     inert, so a sink that overrides nothing can be attached to any solve.
//   * The console's elastic_exhausted line words each ElasticEntry and each QpStatus, and the
//     older cases only pinned one spelling of each. Every enumerator is rendered here, so a
//     reworded or mislabelled entry cannot slip through.

#include <cstdio>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <hven/trace/console_trace_sink.h>
#include <hven/trace/trace.h>

namespace hven {
namespace {

// Records which virtual was entered, in order, tagged with this sink's own name.
class Recorder final : public TraceSink {
  public:
    Recorder(std::string name, std::vector<std::string> *log) : name_(std::move(name)), log_(log) {}

    void on_ipqp_iter(const IpqpIterTraceEvent &) override { note("ipqp_iter"); }
    void on_ipqp_reg(const IpqpRegTraceEvent &) override { note("ipqp_reg"); }
    void on_ipqp_restart(const IpqpRestartTraceEvent &) override { note("ipqp_restart"); }
    void on_ipqp_route(const IpqpRouteTraceEvent &) override { note("ipqp_route"); }
    void on_ipqp_certify(const IpqpCertifyTraceEvent &) override { note("ipqp_certify"); }
    void on_ipqp_escape(const IpqpEscapeTraceEvent &) override { note("ipqp_escape"); }
    void on_qp_mode(const QpModeTraceEvent &) override { note("qp_mode"); }
    void on_fallback_verdict(const SqpFallbackVerdictTraceEvent &) override {
        note("fallback_verdict");
    }
    void on_solve_begin(const SolveBeginTraceEvent &) override { note("solve_begin"); }
    void on_iter(const IterTraceEvent &) override { note("iter"); }
    void on_message(const MessageTraceEvent &) override { note("message"); }
    void on_solve_end(const SolveEndTraceEvent &) override { note("solve_end"); }
    void on_solve_abort() noexcept override { note("solve_abort"); }
    void on_ipm_phase_begin(const IpmPhaseTraceEvent &) override { note("ipm_phase_begin"); }
    void on_ipm_phase_end(const IpmPhaseTraceEvent &) override { note("ipm_phase_end"); }
    void on_ipm_kkt_analysis(const IpmKktAnalysisTraceEvent &) override {
        note("ipm_kkt_analysis");
    }
    void on_ipm_phase_exit(const IpmPhaseExitTraceEvent &) override { note("ipm_phase_exit"); }

  private:
    void note(const char *what) { log_->push_back(name_ + ":" + what); }
    std::string name_;
    std::vector<std::string> *log_;
};

// Drives every virtual once on `sink`, in the order of the expected list below.
void drive_all(TraceSink &sink) {
    const IterationRecord record{};
    const IpmPhaseReport report{};
    const IpmIterate iterate{};
    sink.on_ipqp_iter({});
    sink.on_ipqp_reg({});
    sink.on_ipqp_restart({});
    sink.on_ipqp_route({});
    sink.on_ipqp_certify({});
    sink.on_ipqp_escape({});
    sink.on_qp_mode({});
    sink.on_fallback_verdict({});
    sink.on_solve_begin({});
    sink.on_iter(IterTraceEvent{record});
    sink.on_message({});
    sink.on_solve_end({});
    sink.on_solve_abort();
    sink.on_ipm_phase_begin({});
    sink.on_ipm_phase_end({});
    sink.on_ipm_kkt_analysis({});
    sink.on_ipm_phase_exit(IpmPhaseExitTraceEvent{report, iterate});
}

const char *const kEvents[] = {"ipqp_iter",   "ipqp_reg",     "ipqp_restart",    "ipqp_route",
                               "ipqp_certify", "ipqp_escape",  "qp_mode",         "fallback_verdict",
                               "solve_begin",  "iter",         "message",         "solve_end",
                               "solve_abort",  "ipm_phase_begin", "ipm_phase_end", "ipm_kkt_analysis",
                               "ipm_phase_exit"};

TEST(FanOutTraceSink, ForwardsEveryEventKindToBothSinksFirstThenSecond) {
    std::vector<std::string> log;
    Recorder a("a", &log);
    Recorder b("b", &log);
    FanOutTraceSink fan(&a, &b);
    EXPECT_EQ(fan.first(), &a);
    EXPECT_EQ(fan.second(), &b);
    drive_all(fan);
    ASSERT_EQ(log.size(), 2u * std::size(kEvents));
    for (std::size_t i = 0; i < std::size(kEvents); ++i) {
        EXPECT_EQ(log[2 * i], std::string("a:") + kEvents[i]);
        EXPECT_EQ(log[2 * i + 1], std::string("b:") + kEvents[i]);
    }
}

TEST(FanOutTraceSink, ANullSideIsSkippedNotDereferenced) {
    std::vector<std::string> log;
    Recorder only("only", &log);
    {
        FanOutTraceSink fan(&only, nullptr);
        drive_all(fan);
    }
    EXPECT_EQ(log.size(), std::size(kEvents));
    log.clear();
    {
        FanOutTraceSink fan(nullptr, &only);
        drive_all(fan);
    }
    EXPECT_EQ(log.size(), std::size(kEvents));
    FanOutTraceSink neither(nullptr, nullptr);
    EXPECT_NO_THROW(drive_all(neither));
}

TEST(TraceSinkDefaults, ASinkThatOverridesNothingIgnoresEveryEvent) {
    struct Inert final : TraceSink {};
    Inert inert;
    EXPECT_NO_THROW(drive_all(inert));
}

// ---------------------------------------------------------------------------
// the elastic ladder's console words
// ---------------------------------------------------------------------------

std::string render(const MessageTraceEvent &m) {
    std::FILE *f = std::tmpfile();
    std::string out;
    if (f != nullptr) {
        {
            ConsoleTraceSink::Format fmt;
            fmt.verbosity = Verbosity::kDetail;
            fmt.color = ConsoleColor::kNever;
            ConsoleTraceSink sink(fmt, f);
            sink.on_message(m);
        }
        std::fflush(f);
        std::fseek(f, 0, SEEK_END);
        const long end = std::ftell(f);
        out.assign(static_cast<std::size_t>(end), '\0');
        std::fseek(f, 0, SEEK_SET);
        out.resize(std::fread(out.data(), 1, out.size(), f));
        std::fclose(f);
    }
    return out;
}

MessageTraceEvent ladder_event(ElasticEntry entry, QpStatus main, QpStatus rung) {
    MessageTraceEvent m;
    m.engine = TraceEngine::kSqp;
    m.kind = MessageKind::kElasticExhausted;
    m.severity = default_message_severity(m.kind);
    m.k = 4;
    ElasticLadderRow l;
    l.entry = entry;
    l.main_status = main;
    l.rung_status = rung;
    l.rho = 1e3;
    l.closed = true;
    l.reduced = true;
    l.promises_f = true;
    l.slack_l1 = 2.5e-3;
    m.ladder = l;
    return m;
}

TEST(ConsoleLadderWords, EveryEntryHasItsOwnPhrase) {
    const std::vector<std::pair<ElasticEntry, const char *>> entries = {
        {ElasticEntry::kVerifiedInfeasible, "entered: verified infeasible in trust region;"},
        {ElasticEntry::kWorkingSetTermination, "entered: working-set numerical termination;"},
        {ElasticEntry::kRefusedCertificate, "entered: refused certificate;"},
        {ElasticEntry::kIpqpFallback, "entered: IPQP fallback;"},
    };
    for (const auto &[entry, phrase] : entries) {
        const std::string line = render(ladder_event(entry, QpStatus::kOptimal, QpStatus::kOptimal));
        EXPECT_NE(line.find(phrase), std::string::npos) << line;
        EXPECT_NE(line.find("elastic ladder exhausted after 4 rungs"), std::string::npos) << line;
        EXPECT_NE(line.find("closed yes, reduced yes, promises_f yes; slack_l1 2.5e-03"),
                  std::string::npos)
            << line;
        EXPECT_NE(line.find("at rho 1e+03"), std::string::npos) << line;
    }
}

TEST(ConsoleLadderWords, EveryQpStatusHasItsOwnPhraseInBothSlots) {
    const std::vector<std::pair<QpStatus, const char *>> statuses = {
        {QpStatus::kOptimal, "optimal"},
        {QpStatus::kIterationLimit, "iteration limit"},
        {QpStatus::kInfeasible, "infeasible"},
        {QpStatus::kNumericalError, "numerical failure"},
    };
    for (const auto &[status, word] : statuses) {
        const std::string line =
            render(ladder_event(ElasticEntry::kVerifiedInfeasible, status, QpStatus::kOptimal));
        EXPECT_NE(line.find(std::string("main QP: ") + word + ";"), std::string::npos) << line;
        const std::string line2 =
            render(ladder_event(ElasticEntry::kVerifiedInfeasible, QpStatus::kOptimal, status));
        EXPECT_NE(line2.find(std::string("last rung: ") + word + " at rho"), std::string::npos)
            << line2;
    }
}

TEST(ConsoleLadderWords, AnEnumeratorOutsideTheSetRendersAsUnknownNotACrash) {
    const std::string line = render(ladder_event(static_cast<ElasticEntry>(77),
                                                 static_cast<QpStatus>(77),
                                                 static_cast<QpStatus>(78)));
    EXPECT_NE(line.find("entered: unknown;"), std::string::npos) << line;
    EXPECT_NE(line.find("main QP: unknown;"), std::string::npos) << line;
    EXPECT_NE(line.find("last rung: unknown at rho"), std::string::npos) << line;
}

} // namespace
} // namespace hven
