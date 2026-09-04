// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// trace_writer.cpp -- JsonLinesTraceSink, schema v0's serializer. Printing and
// orchestration code, so CLAUDE.md section 5 puts it in a TU rather than the
// header.
//
// The contract is docs/notes/2026-09-m6-w4-plan.md section 2, quoted where each
// rule is executed below. Two of its rules are worth naming here:
//
//   - the five `Vec` members of `IpqpInfeasibilityEvidence` are NOT written.
//     That is the SETTLER'S RULE -- schema v0 carries no vector-valued field
//     anywhere, plan section 2 rule 4 as amended at the T1 ledger.
//
//   - the stream is the CALLER'S, and exceptions have TWO cases: under the
//     default mask the sink never throws and never ends a solve, under an armed
//     mask the caller's choice propagates by design. Stated in trace_writer.h.

#include <cmath>
#include <iterator>
#include <ostream>
#include <string>
#include <string_view>

#include <fmt/format.h>

#include <hven/core/detail/aggregate_arity.h>
#include <hven/detail/globalization/sqp/globalization.h>
#include <hven/drivers/sqp_types.h>
#include <hven/drivers/trace_writer.h>

namespace hven::solvers {
namespace {

// --- rule 3: numbers ------------------------------------------------------
//
// `{:.17g}` is 17 significant digits, which is what makes the round trip exact
// for binary64; the three non-finite values become JSON STRINGS because JSON has
// no spelling for them and a sentinel number would be a fabricated value.
void append_double(std::string &s, double x) {
    if (std::isnan(x)) {
        s += "\"nan\"";
        return;
    }
    if (std::isinf(x)) {
        s += (x > 0.0) ? "\"inf\"" : "\"-inf\"";
        return;
    }
    fmt::format_to(std::back_inserter(s), "{:.17g}", x);
}

/// `Index` is signed 64-bit (core/types.h); readers parse these as int64.
void append_index(std::string &s, Index v) {
    fmt::format_to(std::back_inserter(s), "{}", static_cast<long long>(v));
}

// --- rule 9: strings, RFC 8259 section 7 ----------------------------------
//
// The two mandatory escapes plus the C0 range; the five short forms are
// preferred over `\u00xx` because they are what a reader expects to see. UTF-8
// above 0x1f passes through unchanged -- the schema does not re-encode text.
void append_string(std::string &s, std::string_view v) {
    s += '"';
    for (char c : v) {
        const unsigned char u = static_cast<unsigned char>(c);
        switch (c) {
        case '"':
            s += "\\\"";
            continue;
        case '\\':
            s += "\\\\";
            continue;
        case '\b':
            s += "\\b";
            continue;
        case '\f':
            s += "\\f";
            continue;
        case '\n':
            s += "\\n";
            continue;
        case '\r':
            s += "\\r";
            continue;
        case '\t':
            s += "\\t";
            continue;
        default:
            break;
        }
        if (u < 0x20) {
            fmt::format_to(std::back_inserter(s), "\\u{:04x}", static_cast<unsigned>(u));
        } else {
            s += c;
        }
    }
    s += '"';
}

/// Opens the next key of an object body built without braces (rule 1: key order
/// is the schema's, the line carries no padding).
///
/// `first` is EXPLICIT, not inferred from `s.empty()`: that inference is right
/// only while an object's first key is unconditional, and T2 adds events whose
/// first key may be an absent optional -- which would then leave the SECOND key
/// comma-less.
void key(std::string &s, bool &first, const char *k) {
    if (!first) {
        s += ',';
    }
    first = false;
    s += '"';
    s += k;
    s += "\":";
}

void key_double(std::string &s, bool &first, const char *k, double v) {
    key(s, first, k);
    append_double(s, v);
}

void key_index(std::string &s, bool &first, const char *k, Index v) {
    key(s, first, k);
    append_index(s, v);
}

void key_bool(std::string &s, bool &first, const char *k, bool v) {
    key(s, first, k);
    s += v ? "true" : "false";
}

void key_enum(std::string &s, bool &first, const char *k, const char *v) {
    key(s, first, k);
    append_string(s, v);
}

// --- rule 8: the enum strings, verbatim from spec section 7 ---------------
//
// No `default` label anywhere below, so `-Wswitch` WARNS when an enumerator is
// added without a spelling. It does NOT fail the build -- this tree carries no
// `-Werror` -- so the trailing `"unknown"` is REACHABLE.
//
// The net is in the tests: `-Wswitch` is an ERROR there, over one exhaustive
// switch per enum, and no emitted line may contain `"unknown"`
// (tests/sqp/test_trace_writer.cpp).
const char *to_json(IpqpTraceRegDir v) {
    switch (v) {
    case IpqpTraceRegDir::kDown:
        return "down";
    case IpqpTraceRegDir::kUp:
        return "up";
    }
    return "unknown";
}

const char *to_json(IpqpTraceRegReason v) {
    switch (v) {
    case IpqpTraceRegReason::kAccept:
        return "accept";
    case IpqpTraceRegReason::kInertia:
        return "inertia";
    case IpqpTraceRegReason::kStall:
        return "stall";
    case IpqpTraceRegReason::kFloor:
        return "floor";
    }
    return "unknown";
}

const char *to_json(IpqpTraceRestartGrade v) {
    switch (v) {
    case IpqpTraceRestartGrade::kCold:
        return "cold";
    case IpqpTraceRestartGrade::kBase:
        return "base";
    case IpqpTraceRestartGrade::kFull:
        return "full";
    }
    return "unknown";
}

const char *to_json(IpqpTraceRouteTo v) {
    switch (v) {
    case IpqpTraceRouteTo::kRefine:
        return "refine";
    case IpqpTraceRouteTo::kSsn:
        return "ssn";
    case IpqpTraceRouteTo::kWalk:
        return "walk";
    }
    return "unknown";
}

const char *to_json(IpqpTraceFinalInertia v) {
    switch (v) {
    case IpqpTraceFinalInertia::kOk:
        return "ok";
    case IpqpTraceFinalInertia::kWrong:
        return "wrong";
    case IpqpTraceFinalInertia::kUnreadable:
        return "unreadable";
    }
    return "unknown";
}

const char *to_json(IpqpTraceEscapeReason v) {
    switch (v) {
    case IpqpTraceEscapeReason::kBudget:
        return "budget";
    case IpqpTraceEscapeReason::kStall:
        return "stall";
    case IpqpTraceEscapeReason::kIndefinite:
        return "indefinite";
    case IpqpTraceEscapeReason::kNumerical:
        return "numerical";
    case IpqpTraceEscapeReason::kInfeasibleSuspect:
        return "infeasible_suspect";
    }
    return "unknown";
}

const char *to_json(IpqpTraceQpMode v) {
    switch (v) {
    case IpqpTraceQpMode::kIpqp:
        return "ipqp";
    case IpqpTraceQpMode::kWalk:
        return "walk";
    case IpqpTraceQpMode::kSsn:
        return "ssn";
    }
    return "unknown";
}

// Two DRIVER enums the `sqp.major` row carries. Lower-case snake per plan
// section 2 rule 8, not the PascalCase display strings core/ has -- those name
// a column in a printed table, and this is machine text.
const char *to_json(QpStatus v) {
    switch (v) {
    case QpStatus::kOptimal:
        return "optimal";
    case QpStatus::kMaxIter:
        return "max_iter";
    case QpStatus::kInfeasible:
        return "infeasible";
    case QpStatus::kNumericalError:
        return "numerical_error";
    }
    return "unknown";
}

const char *to_json(StepVerdict v) {
    switch (v) {
    case StepVerdict::kAcceptF:
        return "accept_f";
    case StepVerdict::kAcceptH:
        return "accept_h";
    case StepVerdict::kReject:
        return "reject";
    case StepVerdict::kRestore:
        return "restore";
    }
    return "unknown";
}

const char *to_json(IpqpTraceOutcome v) {
    switch (v) {
    case IpqpTraceOutcome::kOptimal:
        return "optimal";
    case IpqpTraceOutcome::kRouted:
        return "routed";
    case IpqpTraceOutcome::kEscaped:
        return "escaped";
    }
    return "unknown";
}

const char *to_json(SqpFallbackVerdict v) {
    switch (v) {
    case SqpFallbackVerdict::kDisproved:
        return "disproved";
    case SqpFallbackVerdict::kRelaxed:
        return "relaxed";
    case SqpFallbackVerdict::kExhausted:
        return "exhausted";
    case SqpFallbackVerdict::kRungB:
        return "rung_b";
    case SqpFallbackVerdict::kUnfired:
        return "unfired";
    }
    return "unknown";
}

// --- rule 4/5: the escape event's one nested object ------------------------
//
// Field names are the C++ members', in DECLARATION order, so a reader of
// ipqp_engine.h reads the schema.
//
// v0 CARRIES NO VECTOR-VALUED FIELD (settler ruling, fix round 1), so the
// infeasibility block's five `Vec` members are absent and `ipqp.escape` carries
// spec section 6.3's least-infeasible point's TWO SCALARS, not the point.
std::string evidence_object(const IpqpTraceEscapeEvidence &e) {
    std::string stall;
    bool stall_first = true;
    key_bool(stall, stall_first, "fired", e.stall.fired);
    key_index(stall, stall_first, "window", e.stall.window);
    key_double(stall, stall_first, "mu_ratio", e.stall.mu_ratio);
    key_double(stall, stall_first, "residual_improvement", e.stall.residual_improvement);
    key_double(stall, stall_first, "min_alpha", e.stall.min_alpha);
    key_double(stall, stall_first, "max_step_alpha", e.stall.max_step_alpha);

    const IpqpInfeasibilityEvidence &i = e.infeasibility;
    std::string infeas;
    bool infeas_first = true;
    key_bool(infeas, infeas_first, "fired", i.fired);
    key_bool(infeas, infeas_first, "exhaustion_route", i.exhaustion_route);
    key_index(infeas, infeas_first, "window", i.window);
    key_double(infeas, infeas_first, "primal_start", i.primal_start);
    key_double(infeas, infeas_first, "primal_end", i.primal_end);
    key_double(infeas, infeas_first, "primal_improvement", i.primal_improvement);
    key_double(infeas, infeas_first, "dual_norm_start", i.dual_norm_start);
    key_double(infeas, infeas_first, "dual_norm_end", i.dual_norm_end);
    key_double(infeas, infeas_first, "dual_growth", i.dual_growth);
    key_double(infeas, infeas_first, "dual_step_growth", i.dual_step_growth);
    key_bool(infeas, infeas_first, "farkas_corroborated", i.farkas_corroborated);
    key_double(infeas, infeas_first, "farkas_residual", i.farkas_residual);
    key_double(infeas, infeas_first, "farkas_gap", i.farkas_gap);
    key_double(infeas, infeas_first, "least_infeasible_primal", i.least_infeasible_primal);
    key_double(infeas, infeas_first, "least_infeasible_mu", i.least_infeasible_mu);

    return "{\"stall\":{" + stall + "},\"infeasibility\":{" + infeas + "}}";
}

// --- the `sqp.major` row -------------------------------------------------
//
// THE ROW'S OWN DECLARATION ORDER IS THE KEY ORDER, and this assertion is what
// keeps the two in step: `SqpIterate` gaining a field stops the build here
// until the field gets a key below.
//
// 23 is a COUNT OF INITIALIZERS, not a sizeof (plan amendment E). W4 T3 ADDS
// row fields, and this number and the golden line move with it as a declared
// additive re-derivation (plan section 2 rule 6).
constexpr std::size_t kSqpIterateFieldCount = 23;
static_assert(::hven::detail::kAggregateArity<SqpIterate> == kSqpIterateFieldCount,
              "SqpIterate's field count moved: give the new field a key in "
              "JsonLinesTraceSink::on_sqp_major (in DECLARATION order, ahead of `major`), "
              "re-derive the sqp.major golden line, and then update this count.");

} // namespace

JsonLinesTraceSink::JsonLinesTraceSink(std::ostream &out) : out_(out) {}

JsonLinesTraceSink::~JsonLinesTraceSink() = default;

void JsonLinesTraceSink::push_depth() { ++depth_; }

void JsonLinesTraceSink::pop_depth() {
    if (depth_ > 0) {
        --depth_;
    }
}

// Rule 2: the envelope leads every line, and `seq` counts from 1 so a reader can
// prove it saw every line. `depth` is 0 for the whole of W4 T1.
void JsonLinesTraceSink::write_line(const char *ev, const std::string &body) {
    ++seq_;
    std::string line = "{\"v\":0,\"ev\":\"";
    line += ev;
    line += "\",\"seq\":";
    append_index(line, seq_);
    line += ",\"depth\":";
    append_index(line, depth_);
    if (!body.empty()) {
        line += ',';
        line += body;
    }
    line += "}\n";
    out_ << line;
    // R3(b): the stream's own state, read after every write and sticky. It says
    // "failed at or before one of this sink's writes" and cannot say whose write
    // it was; `seq_` advances regardless, so the gap counts the lines lost.
    if (!out_) {
        failed_ = true;
    }
}

void JsonLinesTraceSink::on_ipqp_iter(const IpqpTraceIterEvent &event) {
    std::string b;
    bool first = true;
    key_index(b, first, "solve", event.solve);
    key_index(b, first, "major", event.major);
    key_index(b, first, "it", event.it);
    key_double(b, first, "mu", event.mu);
    key_double(b, first, "rho", event.rho);
    key_double(b, first, "delta", event.delta);
    key_double(b, first, "res_p", event.res_p);
    key_double(b, first, "res_d", event.res_d);
    key_double(b, first, "res_c", event.res_c);
    key_double(b, first, "sigma", event.sigma);
    key_double(b, first, "alpha_p", event.alpha_p);
    key_double(b, first, "alpha_d", event.alpha_d);
    // Rule 5: an unavailable or unreadable inertia is `null`, NEVER [0,0,0] --
    // a zero-filled read would be indistinguishable from a real one.
    key(b, first, "inertia");
    if (event.inertia.has_value()) {
        b += '[';
        append_index(b, (*event.inertia)[0]);
        b += ',';
        append_index(b, (*event.inertia)[1]);
        b += ',';
        append_index(b, (*event.inertia)[2]);
        b += ']';
    } else {
        b += "null";
    }
    key_bool(b, first, "zero_derived", event.zero_derived);
    key(b, first, "perturbed");
    if (event.perturbed.has_value()) {
        append_index(b, *event.perturbed);
    } else {
        b += "null";
    }
    key(b, first, "facts");
    append_string(b, event.facts);
    write_line("ipqp.iter", b);
}

void JsonLinesTraceSink::on_ipqp_reg(const IpqpTraceRegEvent &event) {
    std::string b;
    bool first = true;
    key_enum(b, first, "dir", to_json(event.dir));
    key_double(b, first, "rho", event.rho);
    key_double(b, first, "delta", event.delta);
    key_enum(b, first, "reason", to_json(event.reason));
    write_line("ipqp.reg", b);
}

void JsonLinesTraceSink::on_ipqp_restart(const IpqpTraceRestartEvent &event) {
    std::string b;
    bool first = true;
    key_enum(b, first, "grade", to_json(event.grade));
    key_bool(b, first, "repaired", event.repaired);
    key_double(b, first, "shift_p", event.shift_p);
    key_double(b, first, "shift_d", event.shift_d);
    key_double(b, first, "mu0", event.mu0);
    key_double(b, first, "mu_payload", event.mu_payload);
    key_bool(b, first, "adopted", event.adopted);
    key_bool(b, first, "abandoned", event.abandoned);
    write_line("ipqp.restart", b);
}

void JsonLinesTraceSink::on_ipqp_route(const IpqpTraceRouteEvent &event) {
    std::string b;
    bool first = true;
    key_enum(b, first, "to", to_json(event.to));
    key_index(b, first, "uncertain", event.uncertain);
    key_index(b, first, "face_rows", event.face_rows);
    key_index(b, first, "face_bounds", event.face_bounds);
    write_line("ipqp.route", b);
}

void JsonLinesTraceSink::on_ipqp_certify(const IpqpTraceCertifyEvent &event) {
    std::string b;
    bool first = true;
    key_enum(b, first, "final_inertia", to_json(event.final_inertia));
    key_bool(b, first, "downgraded", event.downgraded);
    write_line("ipqp.certify", b);
}

void JsonLinesTraceSink::on_ipqp_escape(const IpqpTraceEscapeEvent &event) {
    std::string b;
    bool first = true;
    key_enum(b, first, "reason", to_json(event.reason));
    key(b, first, "evidence");
    b += evidence_object(event.evidence);
    write_line("ipqp.escape", b);
}

void JsonLinesTraceSink::on_qp_mode(const QpModeTraceEvent &event) {
    std::string b;
    bool first = true;
    key_enum(b, first, "mode", to_json(event.mode));
    key_enum(b, first, "outcome", to_json(event.outcome));
    key(b, first, "facts");
    append_string(b, event.facts);
    key_index(b, first, "iters", event.iters);
    write_line("qp.mode", b);
}

void JsonLinesTraceSink::on_fallback_verdict(const SqpFallbackVerdictTraceEvent &event) {
    std::string b;
    bool first = true;
    key_bool(b, first, "entered_rung_a", event.entered_rung_a);
    key_enum(b, first, "verdict", to_json(event.verdict));
    key(b, first, "rho_0");
    if (event.rho_0.has_value()) {
        append_double(b, *event.rho_0);
    } else {
        b += "null";
    }
    key_bool(b, first, "rho0_ceiling_hit", event.rho0_ceiling_hit);
    key_bool(b, first, "floor_retry", event.floor_retry);
    key_index(b, first, "qp_minor_iters", event.qp_minor_iters);
    key_index(b, first, "qp_factorizations", event.qp_factorizations);
    write_line("fallback.verdict", b);
}

void JsonLinesTraceSink::on_sqp_major(const SqpMajorTraceEvent &event) {
    const SqpIterate &r = event.row;
    std::string b;
    bool first = true;
    key_index(b, first, "trial", r.trial);
    key_double(b, first, "f", r.f);
    key_double(b, first, "stationarity", r.stationarity);
    key_double(b, first, "feasibility", r.feasibility);
    key_double(b, first, "complementarity", r.complementarity);
    key_double(b, first, "kkt_residual", r.kkt_residual);
    key_double(b, first, "violation_l1", r.violation_l1);
    key_double(b, first, "tr_radius", r.tr_radius);
    key_double(b, first, "mu", r.mu);
    key_double(b, first, "step_norm", r.step_norm);
    key_bool(b, first, "qp_solved", r.qp_solved);
    key_double(b, first, "ipqp_least_infeasible_primal", r.ipqp_least_infeasible_primal);
    key_bool(b, first, "ipqp_farkas_corroborated", r.ipqp_farkas_corroborated);
    key_enum(b, first, "qp_status", to_json(r.qp_status));
    key_index(b, first, "qp_minor_iters", r.qp_minor_iters);
    key_index(b, first, "qp_factorizations", r.qp_factorizations);
    key_bool(b, first, "tr_binding", r.tr_binding);
    key_enum(b, first, "verdict", to_json(r.verdict));
    key_bool(b, first, "soc_applied", r.soc_applied);
    key_bool(b, first, "elastic_applied", r.elastic_applied);
    key_bool(b, first, "elastic_rho0_ceiling_hit", r.elastic_rho0_ceiling_hit);
    key_bool(b, first, "restoration_seed_used", r.restoration_seed_used);
    key_bool(b, first, "watchdog_restored", r.watchdog_restored);
    key_index(b, first, "major", event.major);
    key_enum(b, first, "mode", to_json(event.mode));
    write_line("sqp.major", b);
}

} // namespace hven::solvers
