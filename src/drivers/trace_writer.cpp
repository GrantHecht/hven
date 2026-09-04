// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// trace_writer.cpp -- JsonLinesTraceSink, schema v0's serializer. Printing and
// orchestration code, so CLAUDE.md section 5 puts it in a TU rather than the
// header.
//
// The contract is docs/notes/2026-09-m6-w4-plan.md section 2, quoted where each
// rule is executed below. Two decisions this file makes that the plan leaves to
// the implementer, both argued at their site:
//
//   - the five `Vec` members of `IpqpInfeasibilityEvidence` are NOT written
//     (v0 carries no vectors -- plan section 7 amendment G);
//
//   - a write to a failed stream is not an error here (instrumentation must not
//     be able to end a solve -- CLAUDE.md section 7).

#include <cmath>
#include <iterator>
#include <ostream>
#include <string>
#include <string_view>

#include <fmt/format.h>

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

/// Opens the next key of an object body built without braces: the comma is
/// emitted iff something precedes it, which is what keeps key order the
/// schema's and the line free of padding (rule 1).
void key(std::string &s, const char *k) {
    if (!s.empty()) {
        s += ',';
    }
    s += '"';
    s += k;
    s += "\":";
}

void key_double(std::string &s, const char *k, double v) {
    key(s, k);
    append_double(s, v);
}

void key_index(std::string &s, const char *k, Index v) {
    key(s, k);
    append_index(s, v);
}

void key_bool(std::string &s, const char *k, bool v) {
    key(s, k);
    s += v ? "true" : "false";
}

void key_enum(std::string &s, const char *k, const char *v) {
    key(s, k);
    append_string(s, v);
}

// --- rule 8: the enum strings, verbatim from spec section 7 ---------------
//
// No `default` label anywhere below, so `-Wswitch` fails the build if an
// enumerator is added without a spelling. The trailing return is the house
// pattern (src/core/enum_names.cpp) and is unreachable.
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
// ipqp_engine.h reads the schema. The infeasibility block's five `Vec` members
// are absent by the no-vectors rule (plan section 7 amendment G).
std::string evidence_object(const IpqpTraceEscapeEvidence &e) {
    std::string stall;
    key_bool(stall, "fired", e.stall.fired);
    key_index(stall, "window", e.stall.window);
    key_double(stall, "mu_ratio", e.stall.mu_ratio);
    key_double(stall, "residual_improvement", e.stall.residual_improvement);
    key_double(stall, "min_alpha", e.stall.min_alpha);
    key_double(stall, "max_step_alpha", e.stall.max_step_alpha);

    const IpqpInfeasibilityEvidence &i = e.infeasibility;
    std::string infeas;
    key_bool(infeas, "fired", i.fired);
    key_bool(infeas, "exhaustion_route", i.exhaustion_route);
    key_index(infeas, "window", i.window);
    key_double(infeas, "primal_start", i.primal_start);
    key_double(infeas, "primal_end", i.primal_end);
    key_double(infeas, "primal_improvement", i.primal_improvement);
    key_double(infeas, "dual_norm_start", i.dual_norm_start);
    key_double(infeas, "dual_norm_end", i.dual_norm_end);
    key_double(infeas, "dual_growth", i.dual_growth);
    key_double(infeas, "dual_step_growth", i.dual_step_growth);
    key_bool(infeas, "farkas_corroborated", i.farkas_corroborated);
    key_double(infeas, "farkas_residual", i.farkas_residual);
    key_double(infeas, "farkas_gap", i.farkas_gap);
    key_double(infeas, "least_infeasible_primal", i.least_infeasible_primal);
    key_double(infeas, "least_infeasible_mu", i.least_infeasible_mu);

    return "{\"stall\":{" + stall + "},\"infeasibility\":{" + infeas + "}}";
}

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
}

void JsonLinesTraceSink::on_ipqp_iter(const IpqpTraceIterEvent &event) {
    std::string b;
    key_index(b, "solve", event.solve);
    key_index(b, "major", event.major);
    key_index(b, "it", event.it);
    key_double(b, "mu", event.mu);
    key_double(b, "rho", event.rho);
    key_double(b, "delta", event.delta);
    key_double(b, "res_p", event.res_p);
    key_double(b, "res_d", event.res_d);
    key_double(b, "res_c", event.res_c);
    key_double(b, "sigma", event.sigma);
    key_double(b, "alpha_p", event.alpha_p);
    key_double(b, "alpha_d", event.alpha_d);
    // Rule 5: an unavailable or unreadable inertia is `null`, NEVER [0,0,0] --
    // a zero-filled read would be indistinguishable from a real one.
    key(b, "inertia");
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
    key_bool(b, "zero_derived", event.zero_derived);
    key(b, "perturbed");
    if (event.perturbed.has_value()) {
        append_index(b, *event.perturbed);
    } else {
        b += "null";
    }
    key(b, "facts");
    append_string(b, event.facts);
    write_line("ipqp.iter", b);
}

void JsonLinesTraceSink::on_ipqp_reg(const IpqpTraceRegEvent &event) {
    std::string b;
    key_enum(b, "dir", to_json(event.dir));
    key_double(b, "rho", event.rho);
    key_double(b, "delta", event.delta);
    key_enum(b, "reason", to_json(event.reason));
    write_line("ipqp.reg", b);
}

void JsonLinesTraceSink::on_ipqp_restart(const IpqpTraceRestartEvent &event) {
    std::string b;
    key_enum(b, "grade", to_json(event.grade));
    key_bool(b, "repaired", event.repaired);
    key_double(b, "shift_p", event.shift_p);
    key_double(b, "shift_d", event.shift_d);
    key_double(b, "mu0", event.mu0);
    key_double(b, "mu_payload", event.mu_payload);
    key_bool(b, "adopted", event.adopted);
    key_bool(b, "abandoned", event.abandoned);
    write_line("ipqp.restart", b);
}

void JsonLinesTraceSink::on_ipqp_route(const IpqpTraceRouteEvent &event) {
    std::string b;
    key_enum(b, "to", to_json(event.to));
    key_index(b, "uncertain", event.uncertain);
    key_index(b, "face_rows", event.face_rows);
    key_index(b, "face_bounds", event.face_bounds);
    write_line("ipqp.route", b);
}

void JsonLinesTraceSink::on_ipqp_certify(const IpqpTraceCertifyEvent &event) {
    std::string b;
    key_enum(b, "final_inertia", to_json(event.final_inertia));
    key_bool(b, "downgraded", event.downgraded);
    write_line("ipqp.certify", b);
}

void JsonLinesTraceSink::on_ipqp_escape(const IpqpTraceEscapeEvent &event) {
    std::string b;
    key_enum(b, "reason", to_json(event.reason));
    key(b, "evidence");
    b += evidence_object(event.evidence);
    write_line("ipqp.escape", b);
}

void JsonLinesTraceSink::on_qp_mode(const QpModeTraceEvent &event) {
    std::string b;
    key_enum(b, "mode", to_json(event.mode));
    key_enum(b, "outcome", to_json(event.outcome));
    key(b, "facts");
    append_string(b, event.facts);
    key_index(b, "iters", event.iters);
    write_line("qp.mode", b);
}

void JsonLinesTraceSink::on_fallback_verdict(const SqpFallbackVerdictTraceEvent &event) {
    std::string b;
    key_bool(b, "entered_rung_a", event.entered_rung_a);
    key_enum(b, "verdict", to_json(event.verdict));
    key(b, "rho_0");
    if (event.rho_0.has_value()) {
        append_double(b, *event.rho_0);
    } else {
        b += "null";
    }
    key_bool(b, "rho0_ceiling_hit", event.rho0_ceiling_hit);
    key_bool(b, "floor_retry", event.floor_retry);
    key_index(b, "qp_minor_iters", event.qp_minor_iters);
    key_index(b, "qp_factorizations", event.qp_factorizations);
    write_line("fallback.verdict", b);
}

} // namespace hven::solvers
