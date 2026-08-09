#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <utility>

#include <gtest/gtest.h>

#include <fmt/format.h>

#if !defined(__APPLE__)
#include <mkl_service.h>
#endif

#if defined(__unix__) || defined(__APPLE__)
#include <sys/utsname.h>
#endif

#include "trace_support.h"

#ifndef HVEN_RIG_COMMIT
#define HVEN_RIG_COMMIT "unknown"
#endif

#ifndef HVEN_RIG_BUILD_CONFIG
#define HVEN_RIG_BUILD_CONFIG "unknown"
#endif

// Defined only when tests/golden_rig/CMakeLists.txt configured the
// corresponding old-seam arm in (HVEN_RIG_HAVE_PSIOPT_SEAM /
// HVEN_RIG_HAVE_SQP_SEAM); the #ifndef fallbacks below only matter for a
// build that somehow defines HAVE without COMMIT/VERIFIED, which should not
// happen but should not read garbage if it does.
#ifndef HVEN_RIG_PSIOPT_SEAM_COMMIT
#define HVEN_RIG_PSIOPT_SEAM_COMMIT "unknown"
#endif
#ifndef HVEN_RIG_PSIOPT_SEAM_VERIFIED
#define HVEN_RIG_PSIOPT_SEAM_VERIFIED "unverified"
#endif
#ifndef HVEN_RIG_SQP_SEAM_TAG
#define HVEN_RIG_SQP_SEAM_TAG "unknown"
#endif
#ifndef HVEN_RIG_SQP_SEAM_COMMIT
#define HVEN_RIG_SQP_SEAM_COMMIT "unknown"
#endif
#ifndef HVEN_RIG_SQP_SEAM_VERIFIED
#define HVEN_RIG_SQP_SEAM_VERIFIED "unverified"
#endif

namespace hven::rig {
namespace {

std::string env_or(const char *name, const std::string &fallback) {
    const char *v = std::getenv(name);
    if (v == nullptr || *v == '\0') {
        return fallback;
    }
    return v;
}

std::string detect_machine() {
    std::string from_env = env_or("HVEN_RIG_MACHINE", "");
    if (!from_env.empty()) {
        return from_env;
    }
#if defined(__unix__) || defined(__APPLE__)
    utsname u{};
    if (uname(&u) == 0) {
        return fmt::format("{} {} {}", u.nodename, u.sysname, u.machine);
    }
#endif
    return "unknown-machine";
}

std::string detect_backend() {
#if defined(__APPLE__)
    // No version string is read here because none has been observed on
    // hardware; a fabricated one would be exactly the kind of value this
    // project records as UNOBSERVED instead.
    return "accelerate/UNOBSERVED";
#else
    char buf[256] = {};
    ::mkl_get_version_string(buf, static_cast<int>(sizeof(buf)));
    std::string s(buf);
    // The version string is padded and multi-line; collapse it so it fits one
    // CSV field, and strip the comma that would otherwise split the row.
    for (char &c : s) {
        if (c == '\n' || c == '\r' || c == ',') {
            c = ' ';
        }
    }
    while (!s.empty() && s.back() == ' ') {
        s.pop_back();
    }
    return s.empty() ? "mkl/unknown" : s;
#endif
}

std::string today_utc() {
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &now);
#else
    gmtime_r(&now, &tm);
#endif
    char buf[32] = {};
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
    return buf;
}

} // namespace

RunMode run_mode() {
    static const RunMode mode = [] {
        const std::string m = env_or("HVEN_RIG_MODE", "assert");
        return (m == "report") ? RunMode::kReport : RunMode::kAssert;
    }();
    return mode;
}

const RunProvenance &run_provenance() {
    static const RunProvenance p = [] {
        RunProvenance r;
        r.machine = detect_machine();
        r.backend = detect_backend();
        r.commit = HVEN_RIG_COMMIT;
        r.date = today_utc();
        r.build_config = HVEN_RIG_BUILD_CONFIG;
#if defined(HVEN_RIG_HAVE_PSIOPT_SEAM)
        r.psiopt_seam_provenance = fmt::format("commit {} ({})", HVEN_RIG_PSIOPT_SEAM_COMMIT,
                                               HVEN_RIG_PSIOPT_SEAM_VERIFIED);
#else
        r.psiopt_seam_provenance = "not configured (HVEN_RIG_PSIOPT_SEAM not set)";
#endif
#if defined(HVEN_RIG_HAVE_SQP_SEAM)
        r.sqp_seam_provenance = fmt::format("tag {} (commit {}, {})", HVEN_RIG_SQP_SEAM_TAG,
                                            HVEN_RIG_SQP_SEAM_COMMIT, HVEN_RIG_SQP_SEAM_VERIFIED);
#else
        r.sqp_seam_provenance = "not configured (HVEN_RIG_SQP_SEAM not set)";
#endif
        return r;
    }();
    return p;
}

ObservationSink &ObservationSink::instance() {
    static ObservationSink sink;
    return sink;
}

void ObservationSink::add(RecordedObservation r) { observations_.push_back(std::move(r)); }

void ObservationSink::add_skip(std::string trace, std::string arm, std::string reason) {
    skips_.push_back(Skip{std::move(trace), std::move(arm), std::move(reason)});
}

void ObservationSink::add_note(std::string trace, std::string arm, std::string note) {
    notes_.push_back(Note{std::move(trace), std::move(arm), std::move(note)});
}

TraceRun::TraceRun(std::string trace, const ArmSpec &arm)
    : trace_(std::move(trace)), arm_(arm), label_(arm.label()) {
    if (ExpectedTable::exists(trace_)) {
        table_ = std::make_unique<ExpectedTable>(ExpectedTable::load(trace_));
        return;
    }
    // A MISSING TABLE FAILS AS LOUDLY AS A MALFORMED ONE. Every trace has a
    // committed table, so absence is only ever an accident -- a deleted file,
    // a renamed trace -- and the accident's consequence is the worst kind:
    // every observation degrades to "no expectation", nothing contradicts
    // anything, and the trace goes green while gating exactly nothing. That is
    // indistinguishable from a healthy run unless someone reads the report.
    //
    // Report mode is the exception, and deliberately: that mode exists to be
    // run when expectations do not exist yet, and its whole output is the
    // visible no-expectation rows.
    if (run_mode() == RunMode::kAssert) {
        ADD_FAILURE() << "golden rig: no expected table for trace " << trace_
                      << " (tests/golden_rig/expected/" << trace_
                      << ".csv). Every trace has a committed table, so this is a deleted or "
                         "renamed file, not a state to run in -- without it this trace asserts "
                         "nothing and would pass for that reason alone. Run the report target if "
                         "you meant to observe rather than gate.";
    }
}

SeamUnderTest &TraceRun::seam() {
    if (!seam_) {
        seam_ = make_seam(arm_.seam, arm_.options);
        if (!configuration_noted_) {
            configuration_noted_ = true;
            note(fmt::format("parity: {}", arm_.parity_note));
            note(fmt::format("configuration in force: {}", seam_->configuration_note()));
            note(fmt::format("thread pin: {}={}",
                             thread_pin_mechanism_name(seam_->thread_pin_mechanism()),
                             seam_->thread_pin_value()));
        }
        remember_pin(*seam_);
    }
    return *seam_;
}

void TraceRun::remember_pin(const SeamUnderTest &s) {
    pin_mechanism_ = thread_pin_mechanism_name(s.thread_pin_mechanism());
    pin_value_ = s.thread_pin_value();
}

std::unique_ptr<SeamUnderTest> TraceRun::another_seam() {
    auto s = make_seam(arm_.seam, arm_.options);
    remember_pin(*s);
    return s;
}

std::unique_ptr<SeamUnderTest> TraceRun::seam_with(const SeamOptions &opts,
                                                   const std::string &why) {
    auto s = make_seam(arm_.seam, opts);
    note(fmt::format("configuration OVERRIDE for this trace ({}): {}", why,
                     s->configuration_note()));
    note(fmt::format("thread pin under that override: {}={}",
                     thread_pin_mechanism_name(s->thread_pin_mechanism()), s->thread_pin_value()));
    remember_pin(*s);
    return s;
}

void TraceRun::record(Observation obs) {
    obs.trace = trace_;
    obs.arm = label_;

    const RunProvenance &p = run_provenance();
    // An observation measured under a configuration other than the run's own
    // carries its own pin, so the row says what it was actually taken under
    // rather than what the last engine built happened to be pinned to. Only
    // record-only observations do this -- and the kind, not the stamp, is what
    // keeps such a row from ever becoming an expectation.
    const std::string mechanism = obs.pin_mechanism_override.value_or(pin_mechanism_);
    const int pin = obs.pin_value_override.value_or(pin_value_);

    RecordedObservation rec;
    const ObservedContext ctx{p.machine, p.build_config, mechanism, std::to_string(pin)};
    rec.comparison = compare(obs, table_.get(), ctx);
    rec.csv_row = to_csv_row(obs, p.machine, p.backend, mechanism, pin, p.commit, p.date);
    rec.obs = std::move(obs);

    if (run_mode() == RunMode::kAssert &&
        rec.comparison.verdict == Comparison::Verdict::kMismatch) {
        ADD_FAILURE() << trace_ << " / " << label_ << ": " << rec.comparison.detail;
    }
    // A context mismatch is never a failure -- see kContextMismatch's own
    // doc comment -- but it must never be silent either: it is the visible
    // difference between "this machine genuinely disagrees with the pinned
    // expectation" and "this machine cannot honestly compare against it at
    // all". Printed unconditionally, in both assert and report mode, so it
    // shows up in a direct run of either binary rather than only in the
    // report tool's own summary and comparisons block.
    if (rec.comparison.verdict == Comparison::Verdict::kContextMismatch) {
        std::cout << "CONTEXT-MISMATCH " << trace_ << " / " << label_ << ": "
                  << rec.comparison.detail << "\n";
    }
    ObservationSink::instance().add(std::move(rec));
}

void TraceRun::record_counters(const Counters &c) {
    record(Observation::counter(trace_, label_, "analyze_count", c.analyze_count));
    record(Observation::counter(trace_, label_, "factorize_count", c.factorize_count));
    record(Observation::counter(trace_, label_, "solve_count", c.solve_count));
    record(Observation::counter(trace_, label_, "partial_solve_count", c.partial_solve_count));
}

void TraceRun::record_inertia(const hven::linear::InertiaEvidence &e, const std::string &prefix) {
    record(
        Observation::state(trace_, label_, prefix + "inertia_state", inertia_state_name(e.state)));
    record(Observation::counter(trace_, label_, prefix + "n_pos", e.n_pos));
    record(Observation::counter(trace_, label_, prefix + "n_neg", e.n_neg));
    record(Observation::counter(trace_, label_, prefix + "n_zero", e.n_zero));
    record(Observation::boolean(trace_, label_, prefix + "zero_is_derived", e.zero_is_derived));
    // Presence FIRST, and separately from the value: an absent optional is a
    // different reading from a zero, and a table that recorded only the number
    // could not tell them apart.
    record(Observation::presence(trace_, label_, prefix + "perturbed_pivots_presence",
                                 e.perturbed_pivots.has_value()));
    if (e.perturbed_pivots.has_value()) {
        record(
            Observation::counter(trace_, label_, prefix + "perturbed_pivots", *e.perturbed_pivots));
    }
}

void TraceRun::record_solve_info(const hven::linear::SolveInfo &s, const std::string &prefix) {
    record(Observation::presence(trace_, label_, prefix + "refinement_iters_presence",
                                 s.refinement_iters.has_value()));
    if (s.refinement_iters.has_value()) {
        record(
            Observation::counter(trace_, label_, prefix + "refinement_iters", *s.refinement_iters));
    }
}

void TraceRun::record_vector_head(const std::string &quantity, const Vec &v, Index count,
                                  double tolerance) {
    const Index n = std::min<Index>(count, v.size());
    for (Index i = 0; i < n; ++i) {
        record(
            Observation::real(trace_, label_, fmt::format("{}[{}]", quantity, i), v(i), tolerance));
    }
}

void TraceRun::record_only(const std::string &quantity, const std::string &value,
                           const std::string &why, const SeamUnderTest &measured_under) {
    Observation obs = Observation::record_only(trace_, label_, quantity, value, why);
    obs.pin_mechanism_override = thread_pin_mechanism_name(measured_under.thread_pin_mechanism());
    obs.pin_value_override = measured_under.thread_pin_value();
    record(std::move(obs));
}

void TraceRun::note(std::string text) {
    ObservationSink::instance().add_note(trace_, label_, std::move(text));
}

std::string TraceRun::skip(const std::string &what) {
    const std::string reason =
        fmt::format("{} does not have {} -- trace {} needs it", label_, what, trace_);
    ObservationSink::instance().add_skip(trace_, label_, reason);
    return reason;
}

double relative_residual(const SpMatRM &K, const Vec &x, const Vec &b) {
    const Vec r = K.selfadjointView<Eigen::Upper>() * x - b;
    const double scale = std::max(1.0, b.lpNorm<Eigen::Infinity>());
    return r.lpNorm<Eigen::Infinity>() / scale;
}

bool bitwise_equal(const Vec &a, const Vec &b) {
    if (a.size() != b.size()) {
        return false;
    }
    return std::memcmp(a.data(), b.data(), static_cast<std::size_t>(a.size()) * sizeof(double)) ==
           0;
}

const char *inertia_state_name(hven::linear::InertiaEvidence::State s) {
    switch (s) {
    case hven::linear::InertiaEvidence::State::kObserved:
        return "kObserved";
    case hven::linear::InertiaEvidence::State::kQueryFailed:
        return "kQueryFailed";
    case hven::linear::InertiaEvidence::State::kUnavailable:
        return "kUnavailable";
    }
    return "unknown";
}

} // namespace hven::rig
