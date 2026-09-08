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
#include <utility>

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

const char *to_json(QpModeSite v) {
    switch (v) {
    case QpModeSite::kDispatch:
        return "dispatch";
    case QpModeSite::kSsnWarmGrade:
        return "ssn_warm_grade";
    case QpModeSite::kFallbackRungB:
        return "fallback_rung_b";
    case QpModeSite::kElasticRung:
        return "elastic_rung";
    case QpModeSite::kSocResolve:
        return "soc_resolve";
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

// The SQP end event's own to_json(SqpStatus) stood here until M6 W5 T8.4. Its
// five spellings were BYTE-IDENTICAL to to_string(SolveStatus)'s five --
// optimal / max_iter / infeasible / numerical_error / budget_exhausted -- so
// folding it into the one overload below moves no trace byte and the golden
// rig's traces do not move.

// LOWER SNAKE, NOT core's display `to_string` (fix round 1, R2): plan section 2
// rule 8 governs the schema's alphabet, and `to_string` names a printed column.
const char *to_json(StartLevel v) {
    switch (v) {
    case StartLevel::kCold:
        return "cold";
    case StartLevel::kSeeded:
        return "seeded";
    case StartLevel::kWarm:
        return "warm";
    case StartLevel::kHot:
        return "hot";
    }
    return "unknown";
}

const char *to_json(WorkingSetLinearAlgebra v) {
    switch (v) {
    case WorkingSetLinearAlgebra::kRefactorize:
        return "refactorize";
    case WorkingSetLinearAlgebra::kSchurBorder:
        return "schur_border";
    }
    return "unknown";
}

// THE INTERIOR-POINT DRIVER'S OWN THREE (W4 T4). The exit status is
// `SolveStatus` since M6 W5 T8.4 -- the engine's own `ConvergenceFlags` is gone
// -- and the two mode selectors shape the run `ipm.solve.begin` announces.
//
// THE SPELLINGS MOVED WITH THE ENUM, and that is a DECLARED change (T8.4):
// `converged` -> `optimal`, `not_converged` -> `max_iter` (or `stalled` at the
// stall and locally-infeasible-restoration exits, which the old vocabulary
// could not tell apart at all), `singular_kkt` -> `numerical_error`;
// `acceptable` and `diverging` are unchanged. Nothing pinned compares these
// bytes -- there is no committed interior-point trace baseline and no control
// -- so unlike the corpus CSV's status column (which keeps its capitalised
// spellings through a bench-local table for exactly that reason) this one takes
// the new vocabulary now. The SQP's `to_json(SolveStatus)` above already spelled
// its five the same way `to_string(SolveStatus)` does, so the golden rig's
// traces do not move.
const char *to_json(SolveStatus v) { return to_string(v); }

const char *to_json(InertiaModes v) {
    switch (v) {
    case InertiaModes::classic:
        return "classic";
    case InertiaModes::proximal_regularization:
        return "proximal_regularization";
    }
    return "unknown";
}

const char *to_json(RestorationModes v) {
    switch (v) {
    case RestorationModes::off:
        return "off";
    case RestorationModes::proximal_switch:
        return "proximal_switch";
    case RestorationModes::l1_nested:
        return "l1_nested";
    }
    return "unknown";
}

// --- the counters object, GENERATED from solver_counters.h's tables -------
//
// One overload per field type, so a table entry is just a name and a predicate:
// `Index` is an integer, `double` goes through rule 3, `start_level_used` is an
// enum with its own lower-snake spelling.
//
// THE PREDICATE IS THE TABLE'S (fix round 1, R7): a field whose doc names a
// sentinel meaning "never measured" is written `null` per rule 5, and which
// fields those are is a property of the counter, not of the serializer.
void key_value(std::string &s, bool &first, const char *k, Index v, bool absent) {
    key(s, first, k);
    if (absent) {
        s += "null";
    } else {
        append_index(s, v);
    }
}

void key_value(std::string &s, bool &first, const char *k, double v, bool absent) {
    key(s, first, k);
    if (absent) {
        s += "null";
    } else {
        append_double(s, v);
    }
}

void key_value(std::string &s, bool &first, const char *k, StartLevel v, bool absent) {
    // HONOURED, not ignored (fix round 2, F4(ii)): no enum field carries an
    // absence predicate today, and a silently dropped argument is how the first
    // one would go unnoticed.
    key(s, first, k);
    if (absent) {
        s += "null";
    } else {
        append_string(s, to_json(v));
    }
}

std::string counters_object(const SqpCounters &c) {
    std::string b;
    bool first = true;
#define HVEN_TRACE_WRITE_FIELD(f, absent) key_value(b, first, #f, c.f, absent(c.f));
    HVEN_SQP_COUNTERS_FIELDS(HVEN_TRACE_WRITE_FIELD)
#undef HVEN_TRACE_WRITE_FIELD

    std::string ssn;
    bool ssn_first = true;
#define HVEN_TRACE_WRITE_FIELD(f, absent) key_value(ssn, ssn_first, #f, c.ssn.f, absent(c.ssn.f));
    HVEN_SSN_COUNTERS_FIELDS(HVEN_TRACE_WRITE_FIELD)
#undef HVEN_TRACE_WRITE_FIELD

    std::string ipqp;
    bool ipqp_first = true;
#define HVEN_TRACE_WRITE_FIELD(f, absent)                                                          \
    key_value(ipqp, ipqp_first, #f, c.ipqp.f, absent(c.ipqp.f));
    HVEN_IPQP_COUNTERS_FIELDS(HVEN_TRACE_WRITE_FIELD)
#undef HVEN_TRACE_WRITE_FIELD

    return "{" + b + ",\"ssn\":{" + ssn + "},\"ipqp\":{" + ipqp + "}}";
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
// 29 is a COUNT OF INITIALIZERS, not a sizeof (plan amendment E). W4 T3 added
// the six telemetry fields, and this number and the four golden lines moved
// with them as a declared additive re-derivation (plan section 2 rule 6).
constexpr std::size_t kSqpIterateFieldCount = 29;
static_assert(::hven::detail::kAggregateArity<SqpIterate> == kSqpIterateFieldCount,
              "SqpIterate's field count moved: give the new field a key in "
              "JsonLinesTraceSink::on_sqp_major (in DECLARATION order, ahead of `major`), "
              "re-derive the sqp.major golden line, and then update this count.");

// AND ON `qp.mode`, WHICH W4 T5 GREW A TRAILING KEY (`site`). 5 is a count of
// initializers, not a sizeof.
//
// The event is written key by key below, so a sixth field would be silently
// dropped from a line the golden pins compare byte for byte.
static_assert(::hven::detail::kAggregateArity<QpModeTraceEvent> == 5,
              "QpModeTraceEvent gained or lost a field: give it a key in "
              "JsonLinesTraceSink::on_qp_mode, re-derive the qp.mode golden lines as a DECLARED "
              "additive trailing key (this event is frozen by plan section 2 rule 6), and "
              "update this count.");

// THE SAME NET ON THE PAIR'S OWN STRUCTS (fix round 2, F3). Both are written key
// by key below, so a field added to either would be silently dropped from a line
// the golden pins compare byte for byte -- and the pin would still pass.
static_assert(::hven::detail::kAggregateArity<SqpSolveBeginTraceEvent> == 10,
              "SqpSolveBeginTraceEvent gained or lost a field: give it a key in "
              "JsonLinesTraceSink::on_sqp_solve_begin, re-derive the golden line, and update "
              "this count. The variable-bound census is the part most likely to grow.");
// THE END EVENT HOLDS A REFERENCE (`counters`), and `kAggregateArity` reads 0
// for such a struct -- it refuses N = 0 and the climb stops there (the helper's
// own note). The two-sided form below is the exact net for that shape.
static_assert(::hven::detail::aggregate_initializable_with<SqpSolveEndTraceEvent>(
                  std::make_index_sequence<3>{}) &&
                  !::hven::detail::aggregate_initializable_with<SqpSolveEndTraceEvent>(
                      std::make_index_sequence<4>{}),
              "SqpSolveEndTraceEvent gained or lost a field: give it a key in "
              "JsonLinesTraceSink::on_sqp_solve_end, re-derive the golden line, and update "
              "these two counts.");

// --- the `ipm.iter` record and the `ipm.solve` pair (W4 T4) ---------------
//
// 28 is `IterateInfo`'s own declared field count. The record is serialized in
// DECLARATION ORDER with the trailing underscores dropped, so a field added
// there stops the build here until it gets a key.
//
// 27 OF THE 28 ARE KEYS. `p_pivots_observed_` is the ABSENCE PREDICATE for
// `p_pivots` and carries no key of its own -- the same shape the counters
// tables' `absent` column has (fix round 1, R2).
constexpr std::size_t kIterateInfoFieldCount = 28;
static_assert(::hven::detail::kAggregateArity<IterateInfo> == kIterateInfoFieldCount,
              "IterateInfo's field count moved: give the new field a key in "
              "JsonLinesTraceSink::on_ipm_iter (in DECLARATION order, ahead of `phase`) -- or, "
              "if it is an absence PREDICATE like p_pivots_observed_, wire it to the field it "
              "gates -- re-derive the ipm.iter golden lines, and then update this count.");

// `IpmIterTraceEvent` HOLDS A REFERENCE, so `kAggregateArity` reads 0 for it --
// the two-sided form is the exact net for that shape, exactly as for
// `SqpSolveEndTraceEvent` above.
static_assert(::hven::detail::aggregate_initializable_with<IpmIterTraceEvent>(
                  std::make_index_sequence<2>{}) &&
                  !::hven::detail::aggregate_initializable_with<IpmIterTraceEvent>(
                      std::make_index_sequence<3>{}),
              "IpmIterTraceEvent gained or lost a field: give it a key in "
              "JsonLinesTraceSink::on_ipm_iter, re-derive the golden line, and update these "
              "two counts.");

// 20 = 4 dimensions + the flat five-key census + `phases` + the 10 run-shaping
// settings. The census is FLAT on the struct so this count can see it: the
// arity helper reads a NESTED aggregate's members through brace elision.
static_assert(::hven::detail::kAggregateArity<IpmSolveBeginTraceEvent> == 20,
              "IpmSolveBeginTraceEvent gained or lost a field: give it a key in "
              "JsonLinesTraceSink::on_ipm_solve_begin, re-derive the golden line, and update "
              "this count.");
static_assert(::hven::detail::kAggregateArity<VariableBoundCensus> == 5,
              "VariableBoundCensus gained or lost a field: give it a key in BOTH "
              "JsonLinesTraceSink::on_sqp_solve_begin and ::on_ipm_solve_begin, re-derive both "
              "golden lines, and update this count.");
static_assert(::hven::detail::kAggregateArity<IpmSolveEndTraceEvent> == 9,
              "IpmSolveEndTraceEvent gained or lost a field: give it a key in "
              "JsonLinesTraceSink::on_ipm_solve_end, re-derive the golden line, and update "
              "this count.");

} // namespace

JsonLinesTraceSink::JsonLinesTraceSink(std::ostream &out) : out_(out) {}

JsonLinesTraceSink::~JsonLinesTraceSink() = default;

void JsonLinesTraceSink::push_depth() { ++depth_; }

void JsonLinesTraceSink::reset_nesting() {
    depth_ = 0;
    open_solves_ = 0;
}

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
    // THE TRAILING KEY, and it is trailing because rule 6 freezes this event:
    // a frozen event's golden line moves only by a declared additive TRAILING
    // key. W4 T5 declared this one (docs/trace-schema-v0.md, the freeze table).
    key_enum(b, first, "site", to_json(event.site));
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
    key_index(b, first, "active_set_delta", r.active_set_delta);
    key_index(b, first, "weak_active_rows", r.weak_active_rows);
    key_index(b, first, "near_active_rows", r.near_active_rows);
    key_index(b, first, "active_rows", r.active_rows);
    key_index(b, first, "active_lower_sides", r.active_lower_sides);
    key_index(b, first, "active_upper_sides", r.active_upper_sides);
    key_index(b, first, "major", event.major);
    key_enum(b, first, "mode", to_json(event.mode));
    write_line("sqp.major", b);
}

void JsonLinesTraceSink::on_sqp_solve_begin(const SqpSolveBeginTraceEvent &event) {
    // THE DEPTH MOVES BEFORE THE LINE IS WRITTEN, so a nested solve's own
    // `begin` already reads 1; the OUTERMOST solve pushes nothing, which is what
    // keeps a top-level stream entirely at depth 0.
    if (open_solves_ > 0) {
        push_depth();
    }
    ++open_solves_;
    std::string b;
    bool first = true;
    key_index(b, first, "n", event.n);
    key_index(b, first, "me", event.me);
    key_index(b, first, "mi", event.mi);
    key_index(b, first, "vars_free", event.vars_free);
    key_index(b, first, "vars_lower_only", event.vars_lower_only);
    key_index(b, first, "vars_upper_only", event.vars_upper_only);
    key_index(b, first, "vars_ranged", event.vars_ranged);
    key_index(b, first, "vars_fixed", event.vars_fixed);
    key_enum(b, first, "qp_mode", to_json(event.qp_mode));
    key_enum(b, first, "ws_algebra", to_json(event.ws_algebra));
    write_line("sqp.solve.begin", b);
}

void JsonLinesTraceSink::on_sqp_solve_end(const SqpSolveEndTraceEvent &event) {
    std::string b;
    bool first = true;
    key_enum(b, first, "status", to_json(event.status));
    key_index(b, first, "majors", event.majors);
    key(b, first, "counters");
    b += counters_object(event.counters);
    write_line("sqp.solve.end", b);
    // Written FIRST, then unwound: this line belongs to the solve it closes.
    if (open_solves_ > 0) {
        --open_solves_;
    }
    if (open_solves_ > 0) {
        pop_depth();
    }
}

void JsonLinesTraceSink::on_ipm_iter(const IpmIterTraceEvent &event) {
    const IterateInfo &r = event.iterate;
    std::string b;
    bool first = true;
    key_index(b, first, "iter", r.iter_);
    key_double(b, first, "mu", r.mu_);
    key_double(b, first, "prim_obj", r.prim_obj_);
    key_double(b, first, "barr_obj", r.barr_obj_);
    key_double(b, first, "kkt_inf", r.kkt_inf_);
    key_double(b, first, "barr_inf", r.barr_inf_);
    key_double(b, first, "econ_inf", r.econ_inf_);
    key_double(b, first, "icon_inf", r.icon_inf_);
    key_double(b, first, "pen_par1", r.pen_par1_);
    key_double(b, first, "pen_par2", r.pen_par2_);
    key_index(b, first, "ls_iters", r.ls_iters_);
    key_double(b, first, "alpha_p", r.alpha_p_);
    key_double(b, first, "alpha_d", r.alpha_d_);
    key_double(b, first, "alpha_t", r.alpha_t_);
    key_double(b, first, "h_pert", r.h_pert_);
    // Rule 5, SENTINEL: -1 is "the Newton direction was non-finite, so no
    // inertia ladder ran", not a step count of -1 (fix round 1, R1).
    key_value(b, first, "h_facs", static_cast<Index>(r.h_facs_), r.h_facs_ < 0);
    key_double(b, first, "h_pert_cum", r.h_pert_cum_);
    // Rule 5, SENTINEL ONE OF TWO: negative is "proximal mode off", not a shift
    // of -1. The classic path writes it on every iteration.
    key_value(b, first, "prox_reg_primal", r.prox_reg_primal_, r.prox_reg_primal_ < 0.0);
    key_value(b, first, "prox_reg_dual", r.prox_reg_dual_, r.prox_reg_dual_ < 0.0);
    // Rule 5, ABSENCE: the projection substitutes 0 for a backend that reports
    // no perturbed-pivot count, and an unfactorized record never had one.
    // Repeating that 0 fabricates an Apple value -- CLAUDE.md section 6.
    key_value(b, first, "p_pivots", static_cast<Index>(r.p_pivots_), !r.p_pivots_observed_);
    key_double(b, first, "max_e_mult", r.max_e_mult_);
    key_double(b, first, "max_i_mult", r.max_i_mult_);
    key_double(b, first, "merit_val", r.merit_val_);
    key_bool(b, first, "accepted", r.accepted_);
    // Rule 5, SENTINEL TWO OF TWO -- a DIFFERENT absence: "no rejection was
    // recorded this line search", which is why the doc names the meaning per
    // field rather than saying "-1 is null" once.
    key_value(b, first, "first_rejection_iter", static_cast<Index>(r.first_rejection_iter_),
              r.first_rejection_iter_ < 0);
    key_value(b, first, "theta_at_first_rejection", r.theta_at_first_rejection_,
              r.theta_at_first_rejection_ < 0.0);
    key_index(b, first, "eval_exceptions", r.eval_exceptions_);
    key_index(b, first, "phase", event.phase);
    write_line("ipm.iter", b);
}

void JsonLinesTraceSink::on_ipm_solve_begin(const IpmSolveBeginTraceEvent &event) {
    std::string b;
    bool first = true;
    key_index(b, first, "n", event.n);
    key_index(b, first, "n_reduced", event.n_reduced);
    key_index(b, first, "me", event.me);
    key_index(b, first, "mi", event.mi);
    // THE SAME FIVE KEYS, IN THE SAME ORDER, AS `sqp.solve.begin` -- one census,
    // one spelling, so a reader comparing the two engines' openers compares like
    // with like.
    key_index(b, first, "vars_free", event.vars_free);
    key_index(b, first, "vars_lower_only", event.vars_lower_only);
    key_index(b, first, "vars_upper_only", event.vars_upper_only);
    key_index(b, first, "vars_ranged", event.vars_ranged);
    key_index(b, first, "vars_fixed", event.vars_fixed);
    key_index(b, first, "phases", event.phases);
    key_index(b, first, "max_iters", event.max_iters);
    key_index(b, first, "max_acc_iters", event.max_acc_iters);
    key_double(b, first, "kkt_tol", event.kkt_tol);
    key_double(b, first, "econ_tol", event.econ_tol);
    key_double(b, first, "icon_tol", event.icon_tol);
    key_double(b, first, "bar_tol", event.bar_tol);
    key_double(b, first, "init_mu", event.init_mu);
    key_double(b, first, "obj_scale", event.obj_scale);
    key_enum(b, first, "inertia_mode", to_json(event.inertia_mode));
    key_enum(b, first, "restoration_mode", to_json(event.restoration_mode));
    write_line("ipm.solve.begin", b);
}

void JsonLinesTraceSink::on_ipm_solve_end(const IpmSolveEndTraceEvent &event) {
    std::string b;
    bool first = true;
    key_enum(b, first, "status", to_json(event.status));
    key_index(b, first, "iters", event.iters);
    // Rule 7: every `_s` below is WALL-CLOCK and informational. No pin reads one.
    key_double(b, first, "total_time_s", event.total_time_s);
    key_double(b, first, "pre_time_s", event.pre_time_s);
    key_double(b, first, "func_time_s", event.func_time_s);
    key_double(b, first, "kkt_time_s", event.kkt_time_s);
    key_double(b, first, "print_time_s", event.print_time_s);
    key_double(b, first, "solver_init_time_s", event.solver_init_time_s);
    key_double(b, first, "misc_time_s", event.misc_time_s);
    write_line("ipm.solve.end", b);
}

} // namespace hven::solvers
