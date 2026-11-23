// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// json_lines_trace_sink.cpp -- JsonLinesTraceSink, schema v0's serializer. Printing and
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
//     mask the caller's choice propagates by design. Stated in json_lines_trace_sink.h.

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <fmt/format.h>

#include <hven/core/version.h>
#include <hven/detail/core/aggregate_arity.h>
#include <hven/sqp/sqp_solver_types.h>
#include <hven/trace/json_lines_trace_sink.h>

#include "trace_names.h"

namespace hven {
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

// An absent optional is `null`: the emitting site did not compute the field.
void key_null(std::string &s, bool &first, const char *k) {
    key(s, first, k);
    s += "null";
}

void key_opt_double(std::string &s, bool &first, const char *k, const std::optional<double> &v) {
    if (v) {
        key_double(s, first, k, *v);
    } else {
        key_null(s, first, k);
    }
}

void key_opt_index(std::string &s, bool &first, const char *k, const std::optional<Index> &v) {
    if (v) {
        key_index(s, first, k, *v);
    } else {
        key_null(s, first, k);
    }
}

void key_opt_bool(std::string &s, bool &first, const char *k, const std::optional<bool> &v) {
    if (v) {
        key_bool(s, first, k, *v);
    } else {
        key_null(s, first, k);
    }
}

void key_string(std::string &s, bool &first, const char *k, std::string_view v) {
    key(s, first, k);
    append_string(s, v);
}

using detail::to_json;

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

// THE TABLE'S SECOND ENUM (M7 W2 M3a, `restoration_sub_status`). Its nine
// status spellings are `to_json(SolveStatus)`'s, byte for byte -- a consumer
// compares this key against the end line's `status` without a translation
// table -- and its tenth is the `not_run` the counter's own enum carries for
// the solves that never restored. It honours the predicate for the StartLevel
// overload's reason.
void key_value(std::string &s, bool &first, const char *k, RestorationSubStatus v, bool absent) {
    key(s, first, k);
    if (absent) {
        s += "null";
    } else {
        append_string(s, to_json(v));
    }
}

// A fixed-size integer array is a JSON array of int64.
template <std::size_t N>
void key_value(std::string &s, bool &first, const char *k, const std::array<Index, N> &v,
               bool absent) {
    key(s, first, k);
    if (absent) {
        s += "null";
        return;
    }
    s += '[';
    for (std::size_t i = 0; i < N; ++i) {
        if (i != 0) {
            s += ',';
        }
        append_index(s, v[i]);
    }
    s += ']';
}

std::string counters_object(const IpmCounters &c) {
    std::string b;
    bool first = true;
#define HVEN_TRACE_WRITE_FIELD(f, absent) key_value(b, first, #f, c.f, absent(c.f));
    HVEN_IPM_COUNTERS_FIELDS(HVEN_TRACE_WRITE_FIELD)
#undef HVEN_TRACE_WRITE_FIELD
    return "{" + b + "}";
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

// --- the counter tables' arity pins ---------------------------------------
//
// Each counters table in core/solver_counters.h is pinned to its struct by the
// largest N for which the struct is brace-initializable with N arguments (not
// sizeof, which is padding-dependent). The pins are here, beside the writer
// that serializes the tables, so a struct-table mismatch fails the library
// build without the public header including its arity helper.
//
// TO ADD A COUNTER: declare it in the struct, add its `X(name)` to the table IN
// THE SAME POSITION, and the assert passes again.
static_assert(::hven::detail::kAggregateArity<SsnCounters> == kSsnCountersFieldCount,
              "SsnCounters and HVEN_SSN_COUNTERS_FIELDS disagree: give the new field an X() entry "
              "in its declaration position (and the trace's golden line moves with it).");

static_assert(::hven::detail::kAggregateArity<IpqpCounters> == kIpqpCountersFieldCount,
              "IpqpCounters and HVEN_IPQP_COUNTERS_FIELDS disagree: give the new field an X() "
              "entry in its declaration position (and the trace's golden line moves with it).");

// PLUS TWO: `ssn` and `ipqp` are nested aggregates, each counted as ONE
// initializer by the arity trick and serialized through its own table above, so
// the table holds the 77 direct fields and the struct takes 79 initializers.
// (37 -> 38 at M6 W5 T8.5, which appended `polish_ignored` after both nested
// aggregates -- last in declaration order, so the table order below is still
// the struct's and every pre-existing offset is unmoved. 38 -> 39 at M7 W2 M4,
// which appended `start_point_clamps` the same way and for the same reason.
// 39 -> 44 at M7 W2 M1, which appended the five-field subproblem-exit group the
// same way and for the same reason. 44 -> 49 at M7 W2 M2, which appended the
// five-field restoration excursion record the same way and for the same reason
// -- and which is where the table first carries `double` entries outside the
// two nested aggregates; they are 8-byte members like every `Index` above them,
// so the offsets stay monotone and the check below is unchanged. 49 -> 50 at
// M7 W2 M3a, which appended `restoration_sub_status` to that record the same
// way and for the same reason. It is the SECOND enum in the table, after
// `start_level_used`, and the first field NARROWER than 8 bytes: a 4-byte
// `RestorationSubStatus` last in the struct, which the struct's own 8-byte
// alignment then pads, so the offsets stay monotone and `sizeof` moves
// 848 -> 856. Its type was a bare `SolveStatus` until M7 W2 M3a fix1 replaced
// it with the ten-valued enum that has an explicit `kNotRun` -- the SAME
// underlying type and therefore the SAME width, so that re-derivation moved
// `sizeof` no further and no pre-existing offset at all. 50 -> 56 at M7 W2 E4
// fix round 1, which appended the QP engine's six-field E4 group -- the EQP-site
// residual check's two, the unregularized polish's one and the terminal polish's
// three -- the same way and for the same reason. They are `Index` again, so they
// land 8-byte-aligned after `restoration_sub_status`'s 4 bytes of padding and the
// offsets stay monotone. 56 -> 57 at M7 W2 E4 fix round 2, which made that group
// SEVEN with `eqp_row_drift_refusals` -- appended LAST in the struct and last in
// the table on the declared additive-trailing-key route, so no pre-existing offset
// moves and `sizeof` goes 904 -> 912. 57 -> 60 at M7 W2 R2, which appended the
// failed-subproblem step group and `nonfinite_subproblems` the same way and for the
// same reason: three `Index` fields last, `sizeof` 912 -> 936. 60 -> 61 at M7 W2 W1,
// `nonfinite_box_projections`, the same way: `sizeof` 936 -> 944. 61 -> 63 at M7 W2
// P1E, `elastic_last_rung_fallbacks` and `restoration_reduction_resumes`, the same way:
// `sizeof` 944 -> 960. 63 -> 77 at M7 QN stage 3, `eval_hess_calls` and the thirteen-field
// quasi-Newton group, the same way: `sizeof` 960 -> 1072. 77 -> 79 at the QN review and
// riders, `qn_nonpd_seen` and `qn_dimension`; 79 -> 80 at M7 W2 R2-1,
// `nonfinite_step_rollbacks`, the same way: `sizeof` 1088 -> 1096. 80 -> 81 at M7 W2 SNR,
// `restoration_feas_out`, the same way: `sizeof` 1096 -> 1104. 81 -> 82 at M7 W2 SCL,
// `scaled_convergence_refusals`, the same way: `sizeof` 1104 -> 1112. 82 -> 83 at M7 W2 RST,
// `restoration_curvature_switches`, the same way: `sizeof` 1112 -> 1120. 83 -> 85 at M8 DN-1,
// `inertia_suspect_verdicts` and `inertia_wrong_verdicts`, the same way: `sizeof` +16.)
static_assert(::hven::detail::kAggregateArity<SqpCounters> == kSqpCountersFieldCount + 2,
              "SqpCounters and HVEN_SQP_COUNTERS_FIELDS disagree: give the new field an X() entry "
              "in its declaration position, or -- if it is a new NESTED aggregate -- give it a "
              "table of its own and raise the + 2 here.");

// --- the `sqp.major` row -------------------------------------------------
//
// THE ROW'S OWN DECLARATION ORDER IS THE KEY ORDER, and this assertion is what
// keeps the two in step: `SqpMajorRow` gaining a field stops the build here
// until the field gets a key below.
//
// 30 is a COUNT OF INITIALIZERS, not a sizeof (plan amendment E). W4 T3 added
// the six telemetry fields, and this number and the four golden lines moved
// with them as a declared additive re-derivation (plan section 2 rule 6).
static_assert(::hven::detail::kAggregateArity<QpModeTraceEvent> == 5,
              "QpModeTraceEvent gained or lost a field: give it a key in "
              "JsonLinesTraceSink::on_qp_mode, re-derive the qp.mode golden lines as a DECLARED "
              "additive trailing key (this event is frozen by plan section 2 rule 6), and "
              "update this count.");

constexpr std::size_t kIpmIterateFieldCount = 35;
static_assert(::hven::detail::kAggregateArity<IpmIterate> == kIpmIterateFieldCount,
              "IpmIterate's field count moved: `iter` rows are built from it by "
              "IpmSolverImpl::make_iteration_record and `ipm.phase.exit` reads it here; give the "
              "new field its place there, re-derive the goldens, and update this count.");

static_assert(::hven::detail::kAggregateArity<IterationRecord> == 19,
              "IterationRecord gained or lost a member: give it a key in "
              "JsonLinesTraceSink::on_iter, add it to the freeze table of "
              "docs/trace-schema-v1.md, re-derive the iter golden lines, and update this count.");
static_assert(::hven::detail::kAggregateArity<IpmIterationRow> == 23,
              "IpmIterationRow gained or lost a member: give it a key in the `ipm` block of "
              "JsonLinesTraceSink::on_iter, add it to the freeze table of "
              "docs/trace-schema-v1.md, re-derive the golden line, and update this count.");
static_assert(::hven::detail::kAggregateArity<SqpIterationRow> == 36,
              "SqpIterationRow gained or lost a member: give it a key in the `sqp` block of "
              "JsonLinesTraceSink::on_iter, add it to the freeze table of "
              "docs/trace-schema-v1.md, re-derive the golden line, and update this count.");
static_assert(::hven::detail::kAggregateArity<SolveBeginTraceEvent> == 7,
              "SolveBeginTraceEvent gained or lost a member: give it a key in "
              "JsonLinesTraceSink::on_solve_begin, re-derive the golden lines, and update this "
              "count.");
static_assert(::hven::detail::kAggregateArity<IpmSolveBeginInfo> == 16,
              "IpmSolveBeginInfo gained or lost a member: give it a key in the `ipm` block of "
              "JsonLinesTraceSink::on_solve_begin, re-derive the golden line, and update this "
              "count.");
static_assert(::hven::detail::kAggregateArity<SqpSolveBeginInfo> == 7,
              "SqpSolveBeginInfo gained or lost a member: give it a key in the `sqp` block of "
              "JsonLinesTraceSink::on_solve_begin, re-derive the golden line, and update this "
              "count.");
static_assert(
    ::hven::detail::aggregate_initializable_with<IterTraceEvent>(std::make_index_sequence<1>{}) &&
        !::hven::detail::aggregate_initializable_with<IterTraceEvent>(
            std::make_index_sequence<2>{}),
    "IterTraceEvent gained or lost a member: update JsonLinesTraceSink::on_iter and "
    "these two counts.");
static_assert(::hven::detail::kAggregateArity<MessageTraceEvent> == 15,
              "MessageTraceEvent gained or lost a member: give it a key in "
              "JsonLinesTraceSink::on_message, add it to the per-kind table in "
              "docs/trace-schema-v1.md, re-derive the golden line, and update this count.");
static_assert(::hven::detail::kAggregateArity<DeclaredDiagnosticsTrace> == 3,
              "DeclaredDiagnosticsTrace gained or lost a member: give it a key in "
              "`solve.end.diagnostics`, re-derive the golden line, and update this count.");
static_assert(::hven::detail::kAggregateArity<SolveEndTraceEvent> == 13,
              "SolveEndTraceEvent gained or lost a member: give it a key in "
              "JsonLinesTraceSink::on_solve_end, re-derive the golden lines, and update this "
              "count.");
static_assert(::hven::detail::kAggregateArity<IpmSolveEndInfo> == 13,
              "IpmSolveEndInfo gained or lost a member: give it a key in the `ipm` block of "
              "JsonLinesTraceSink::on_solve_end, re-derive the golden line, and update this "
              "count.");
static_assert(::hven::detail::kAggregateArity<SqpSolveEndInfo> == 10,
              "SqpSolveEndInfo gained or lost a member: give it a key in the `sqp` block of "
              "JsonLinesTraceSink::on_solve_end, re-derive the golden line, and update this "
              "count.");
static_assert(::hven::detail::kAggregateArity<VariableBoundCensus> == 5,
              "VariableBoundCensus gained or lost a field: update census_variable_bounds and "
              "this count.");
static_assert(::hven::detail::kAggregateArity<ProblemStatistics> == 13,
              "ProblemStatistics gained or lost a field: give it a key in "
              "JsonLinesTraceSink::on_solve_begin (declaration order), re-derive the golden "
              "lines, and update this count.");

static_assert(::hven::detail::kAggregateArity<IpmPhaseTraceEvent> == 3,
              "IpmPhaseTraceEvent gained or lost a field: give it a key in BOTH "
              "JsonLinesTraceSink::on_ipm_phase_begin and ::on_ipm_phase_end, re-derive both "
              "golden lines, and update this count.");

static_assert(::hven::detail::kAggregateArity<IpmKktAnalysisTraceEvent> == 6,
              "IpmKktAnalysisTraceEvent gained or lost a field: give it a key in "
              "JsonLinesTraceSink::on_ipm_kkt_analysis, re-derive the golden line, and update "
              "this count.");

static_assert(::hven::detail::aggregate_initializable_with<IpmPhaseExitTraceEvent>(
                  std::make_index_sequence<9>{}) &&
                  !::hven::detail::aggregate_initializable_with<IpmPhaseExitTraceEvent>(
                      std::make_index_sequence<10>{}),
              "IpmPhaseExitTraceEvent gained or lost a field: give it a key in "
              "JsonLinesTraceSink::on_ipm_phase_exit, re-derive the golden line, and update "
              "these two counts.");

// AND THE EMBEDDED REPORT'S OWN ARITY. `ipm.phase.exit` flattens
// `IpmPhaseReport`'s first six fields as trailing keys, so a field added THERE --
// in a header this file does not otherwise track -- must get a key here too. The
// ten diagnostics after `ran` (G6) are filled after this line is written and are
// not keys; `kkt_factor_status` is the line's own `kkt_factor_status` key.
static_assert(::hven::detail::kAggregateArity<IpmPhaseReport> == 16,
              "IpmPhaseReport gained or lost a field: `ipm.phase.exit` flattens this struct, so "
              "give the new field a trailing key in JsonLinesTraceSink::on_ipm_phase_exit, "
              "re-derive the golden line, and update this count.");

} // namespace

JsonLinesTraceSink::JsonLinesTraceSink(std::ostream &out) : out_(out) {}

// M7 W0 T5(c): the destructor flushes, under the only guard a destructor can
// carry. `flush()` propagates under an armed mask (the class contract), and a
// destructor that let that escape would terminate the process during unwinding,
// so the failure is swallowed HERE and nowhere else -- a caller who needs to see
// it calls flush() while it can still be reported.
JsonLinesTraceSink::~JsonLinesTraceSink() {
    try {
        flush();
    } catch (...) { // NOLINT(bugprone-empty-catch) -- see above; nothing to report to.
    }
}

// A STREAM OPERATION ONLY: no line is written and `seq_`/`depth_` do not move.
// The failure reading mirrors write_line() exactly -- the stream's own state,
// read after the operation and sticky -- so `failed()` means the same thing
// whether the failure happened on a write or on a flush.
void JsonLinesTraceSink::flush() {
    out_.flush();
    if (!out_) {
        failed_ = true;
    }
}

void JsonLinesTraceSink::push_depth() { ++depth_; }

// A solve left by an exception (`on_solve_abort`): the non-emitting twin of the unwind at
// the end of on_solve_end. Saturating, so an abort for a begin this sink never saw (an
// earlier sink in a fan-out threw first) cannot drive the counts negative.
void JsonLinesTraceSink::on_solve_abort() noexcept {
    if (open_solves_ > 0) {
        --open_solves_;
    }
    if (open_solves_ > 0) {
        pop_depth();
    } else {
        depth_ = 0;
    }
}

void JsonLinesTraceSink::pop_depth() {
    if (depth_ > 0) {
        --depth_;
    }
}

// Rule 2: the envelope leads every line, and `seq` counts from 1 so a reader can
// prove it saw every line. `depth` is 0 for the whole of W4 T1.
void JsonLinesTraceSink::write_line(const char *ev, const std::string &body, const char *eng) {
    ++seq_;
    std::string line = "{\"v\":1,\"ev\":\"";
    line += ev;
    line += "\",\"seq\":";
    append_index(line, seq_);
    line += ",\"sid\":";
    append_index(line, sid_);
    line += ",\"depth\":";
    append_index(line, depth_);
    line += ",\"eng\":\"";
    line += eng;
    line += "\"";
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

void JsonLinesTraceSink::on_ipqp_iter(const IpqpIterTraceEvent &event) {
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

void JsonLinesTraceSink::on_ipqp_reg(const IpqpRegTraceEvent &event) {
    std::string b;
    bool first = true;
    key_enum(b, first, "dir", to_json(event.dir));
    key_double(b, first, "rho", event.rho);
    key_double(b, first, "delta", event.delta);
    key_enum(b, first, "reason", to_json(event.reason));
    write_line("ipqp.reg", b);
}

void JsonLinesTraceSink::on_ipqp_restart(const IpqpRestartTraceEvent &event) {
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

void JsonLinesTraceSink::on_ipqp_route(const IpqpRouteTraceEvent &event) {
    std::string b;
    bool first = true;
    key_enum(b, first, "to", to_json(event.to));
    key_index(b, first, "uncertain", event.uncertain);
    key_index(b, first, "face_rows", event.face_rows);
    key_index(b, first, "face_bounds", event.face_bounds);
    write_line("ipqp.route", b);
}

void JsonLinesTraceSink::on_ipqp_certify(const IpqpCertifyTraceEvent &event) {
    std::string b;
    bool first = true;
    key_enum(b, first, "final_inertia", to_json(event.final_inertia));
    key_bool(b, first, "downgraded", event.downgraded);
    write_line("ipqp.certify", b);
}

void JsonLinesTraceSink::on_ipqp_escape(const IpqpEscapeTraceEvent &event) {
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

// --- M6 W5 T8.7b: the interior-point engine's last direct prints ----------
//
// FIVE ADDITIVE RECORDS, and `v` STAYS 0. Section 8 of docs/trace-schema-v0.md
// says so in its own words -- "ADDITIVE changes -- new events, new enum
// strings, new trailing fields -- stay v0 until the schema has an external
// consumer" -- and nothing here renames, removes or retypes a key on a frozen
// event. A bump would be wrong under the document's own rule.

void JsonLinesTraceSink::on_ipm_phase_begin(const IpmPhaseTraceEvent &event) {
    std::string b;
    bool first = true;
    key_index(b, first, "phase", event.phase);
    key_enum(b, first, "entry", to_json(event.entry));
    write_line("ipm.phase.begin", b, "ipm");
}

void JsonLinesTraceSink::on_ipm_phase_end(const IpmPhaseTraceEvent &event) {
    std::string b;
    bool first = true;
    key_index(b, first, "phase", event.phase);
    key_enum(b, first, "entry", to_json(event.entry));
    write_line("ipm.phase.end", b, "ipm");
}

void JsonLinesTraceSink::on_ipm_kkt_analysis(const IpmKktAnalysisTraceEvent &event) {
    std::string b;
    bool first = true;
    key_index(b, first, "kkt_dim", event.kkt_dim);
    key_index(b, first, "nnz", event.nnz);
    key_bool(b, first, "docompute", event.docompute);
    // Rule 5, ABSENCE: on a REFACTORIZATION these two are the LAST analysis's
    // figures, re-read from the result -- not this event's. Repeating them
    // would report one analysis's size for another's.
    key_value(b, first, "factor_mem", event.factor_mem, !event.docompute);
    key_value(b, first, "factor_flops", event.factor_flops, !event.docompute);
    // Rule 7: wall-clock, informational.
    key_double(b, first, "analysis_time_s", event.analysis_time_s);
    write_line("ipm.kkt_analysis", b, "ipm");
}

// THE ROW IS NOT REPEATED, exactly as on `ipm.restoration_exit_row`: the five
// values the console block prints are written here and the adjacent `ipm.iter`
// line carries the record's other 22 keys, joined by (`phase`, `iter`).
//
// THE REPORT IS FLATTENED as the trailing keys -- `entry` is its `IpmPhase`,
// `phase` above is the index -- so the event's own fields and the returned
// `IpmPhaseReport`'s appear on one line without a nested object.
void JsonLinesTraceSink::on_ipm_phase_exit(const IpmPhaseExitTraceEvent &event) {
    std::string b;
    bool first = true;
    key_index(b, first, "phase", event.phase);
    key_index(b, first, "iter", event.iterate.iter);
    key_bool(b, first, "best_substituted", event.best_substituted);
    key_double(b, first, "prim_obj", event.iterate.prim_obj);
    key_double(b, first, "stationarity", event.iterate.kkt_inf);
    key_double(b, first, "complementarity", event.iterate.barr_inf);
    key_double(b, first, "econ_inf", event.iterate.econ_inf);
    key_double(b, first, "icon_inf", event.iterate.icon_inf);
    key_enum(b, first, "kkt_factor_status", to_json(event.kkt_factor_status));
    // Rule 7: every `_s` below is wall-clock and informational.
    key_double(b, first, "total_s", event.total_s);
    key_double(b, first, "func_s", event.func_s);
    key_double(b, first, "kkt_s", event.kkt_s);
    key_double(b, first, "print_s", event.print_s);
    // The embedded report, flattened.
    key_enum(b, first, "entry", to_json(event.report.phase));
    key_enum(b, first, "status", to_json(event.report.status));
    key_index(b, first, "iterations", event.report.iterations);
    key_double(b, first, "phase_seconds", event.report.phase_seconds);
    key_enum(b, first, "stop_reason", to_json(event.report.stop_reason));
    key_bool(b, first, "ran", event.report.ran);
    write_line("ipm.phase.exit", b, "ipm");
}

// ---- the four records both engines share -------------------------------

namespace {

const char *engine_name(TraceEngine e) { return e == TraceEngine::kIpm ? "ipm" : "sqp"; }

void write_ipm_row(std::string &b, const IpmIterationRow &r) {
    bool f = true;
    key_index(b, f, "phase", r.phase);
    key_double(b, f, "mu", r.mu);
    key_opt_double(b, f, "alpha_primal", r.alpha_primal);
    key_opt_double(b, f, "alpha_dual", r.alpha_dual);
    key_opt_double(b, f, "alpha_scale", r.alpha_scale);
    key_opt_index(b, f, "line_search_trials", r.line_search_trials);
    key_opt_double(b, f, "regularization_primal", r.regularization_primal);
    key_opt_double(b, f, "regularization_primal_total", r.regularization_primal_total);
    key_opt_double(b, f, "regularization_dual", r.regularization_dual);
    key_opt_double(b, f, "proximal_primal", r.proximal_primal);
    key_opt_index(b, f, "inertia_corrections", r.inertia_corrections);
    key_opt_index(b, f, "perturbed_pivots", r.perturbed_pivots);
    key_opt_double(b, f, "barrier_objective", r.barrier_objective);
    key_double(b, f, "feasibility_eq", r.feasibility_eq);
    key_double(b, f, "feasibility_ineq", r.feasibility_ineq);
    key_opt_double(b, f, "merit", r.merit);
    key_double(b, f, "merit_penalty_1", r.merit_penalty_1);
    key_double(b, f, "merit_penalty_2", r.merit_penalty_2);
    key_double(b, f, "max_multiplier_eq", r.max_multiplier_eq);
    key_double(b, f, "max_multiplier_ineq", r.max_multiplier_ineq);
    key_opt_index(b, f, "first_rejected_trial", r.first_rejected_trial);
    key_opt_double(b, f, "theta_first_rejected", r.theta_first_rejected);
    key_index(b, f, "eval_exceptions", r.eval_exceptions);
}

// The `ladder` block of an `sqp` row and of an `elastic_exhausted` message: an object, or null.
void key_opt_ladder(std::string &b, bool &f, const std::optional<ElasticLadderRow> &ladder) {
    if (!ladder) {
        key_null(b, f, "ladder");
        return;
    }
    const ElasticLadderRow &l = *ladder;
    std::string o;
    bool lf = true;
    key_enum(o, lf, "entry", to_json(l.entry));
    key_enum(o, lf, "main_status", to_json(l.main_status));
    key_enum(o, lf, "rung_status", to_json(l.rung_status));
    key_double(o, lf, "rho", l.rho);
    key_index(o, lf, "rungs", l.rungs);
    key_bool(o, lf, "closed", l.closed);
    key_bool(o, lf, "reduced", l.reduced);
    key_bool(o, lf, "promises_f", l.promises_f);
    key_double(o, lf, "slack_l1", l.slack_l1);
    key_bool(o, lf, "exhausted", l.exhausted);
    key_double(o, lf, "rho_0", l.rho_0);
    key_index(o, lf, "minor_iterations", l.minor_iterations);
    key_index(o, lf, "factorizations", l.factorizations);
    key(b, f, "ladder");
    b += "{" + o + "}";
}

void write_sqp_row(std::string &b, const SqpIterationRow &r) {
    bool f = true;
    key_index(b, f, "depth", r.depth);
    key_enum(b, f, "mode", to_json(r.mode));
    if (r.verdict) {
        key_enum(b, f, "verdict", to_json(*r.verdict));
    } else {
        key_null(b, f, "verdict");
    }
    key_double(b, f, "trust_radius", r.trust_radius);
    key_double(b, f, "violation_l1", r.violation_l1);
    key_opt_double(b, f, "regularization_dual", r.regularization_dual);
    key_bool(b, f, "qp_solved", r.qp_solved);
    if (r.qp_status) {
        key_enum(b, f, "qp_status", to_json(*r.qp_status));
    } else {
        key_null(b, f, "qp_status");
    }
    key_opt_index(b, f, "qp_minor_iterations", r.qp_minor_iterations);
    key_opt_index(b, f, "qp_factorizations", r.qp_factorizations);
    key_bool(b, f, "tr_binding", r.tr_binding);
    key_bool(b, f, "elastic", r.elastic);
    key_bool(b, f, "elastic_clamped", r.elastic_clamped);
    key_bool(b, f, "restoration_seed_used", r.restoration_seed_used);
    key_opt_double(b, f, "ipqp_least_infeasible_primal", r.ipqp_least_infeasible_primal);
    key_opt_bool(b, f, "ipqp_farkas_corroborated", r.ipqp_farkas_corroborated);
    key_index(b, f, "active_set_size", r.active_set_size);
    key_index(b, f, "active_set_delta", r.active_set_delta);
    key_index(b, f, "weak_active_rows", r.weak_active_rows);
    key_index(b, f, "near_active_rows", r.near_active_rows);
    key_index(b, f, "active_rows", r.active_rows);
    key_index(b, f, "active_lower_sides", r.active_lower_sides);
    key_index(b, f, "active_upper_sides", r.active_upper_sides);
    key_opt_double(b, f, "switching_ratio", r.switching_ratio);
    key_opt_ladder(b, f, r.ladder);
    key_bool(b, f, "cap_binding", r.cap_binding);
    key_opt_double(b, f, "pred_df", r.pred_df);
    key_opt_double(b, f, "f_trial", r.f_trial);
    key_opt_double(b, f, "h_trial", r.h_trial);
    key_opt_double(b, f, "soc_f", r.soc_f);
    key_opt_double(b, f, "soc_h", r.soc_h);
    key_opt_double(b, f, "h_lin", r.h_lin);
    key_index(b, f, "qp_minor_iterations_total", r.qp_minor_iterations_total);
    key_index(b, f, "qp_factorizations_total", r.qp_factorizations_total);
    key_index(b, f, "factorizations_delta", r.factorizations_delta);
    key_double(b, f, "t_s", r.t_s);
}

} // namespace

void JsonLinesTraceSink::on_solve_begin(const SolveBeginTraceEvent &event) {
    // THE DEPTH MOVES BEFORE THE LINE IS WRITTEN, so a nested solve's own `begin`
    // already reads 1; the OUTERMOST solve pushes nothing and takes the next `sid`.
    if (open_solves_ > 0) {
        push_depth();
    } else {
        ++sid_;
    }
    ++open_solves_;
    std::string b;
    bool first = true;
    const ProblemStatistics &st = event.statistics;
    key_index(b, first, "variables", st.variables);
    key_index(b, first, "free_variables", st.free_variables);
    key_index(b, first, "lower_bounded_variables", st.lower_bounded_variables);
    key_index(b, first, "upper_bounded_variables", st.upper_bounded_variables);
    key_index(b, first, "boxed_variables", st.boxed_variables);
    key_index(b, first, "fixed_variables", st.fixed_variables);
    key_index(b, first, "constraints", st.constraints);
    key_index(b, first, "equality_rows", st.equality_rows);
    key_index(b, first, "lower_bounded_rows", st.lower_bounded_rows);
    key_index(b, first, "upper_bounded_rows", st.upper_bounded_rows);
    key_index(b, first, "range_rows", st.range_rows);
    key_index(b, first, "free_rows", st.free_rows);
    key_index(b, first, "absent_bounds", st.absent_bounds);
    key_double(b, first, "optimality_tol", event.optimality_tol);
    key_double(b, first, "feasibility_tol", event.feasibility_tol);
    key_index(b, first, "max_iterations", event.max_iterations);
    key_string(b, first, "hven_version", version());
    if (event.ipm) {
        const IpmSolveBeginInfo &i = *event.ipm;
        std::string o;
        bool f = true;
        key_index(o, f, "n_reduced", i.n_reduced);
        key_index(o, f, "me", i.me);
        key_index(o, f, "mi", i.mi);
        key_index(o, f, "internal_fixed_rows", i.internal_fixed_rows);
        key_index(o, f, "phases", i.phases);
        key_index(o, f, "max_acceptable_iterations", i.max_acceptable_iterations);
        key_double(o, f, "initial_mu", i.initial_mu);
        key_double(o, f, "obj_scale", i.obj_scale);
        key_double(o, f, "acceptable_stationarity_tol", i.acceptable_stationarity_tol);
        key_double(o, f, "acceptable_complementarity_tol", i.acceptable_complementarity_tol);
        key_double(o, f, "acceptable_equality_tol", i.acceptable_equality_tol);
        key_double(o, f, "acceptable_inequality_tol", i.acceptable_inequality_tol);
        key_enum(o, f, "inertia_mode", to_json(i.inertia_mode));
        key_enum(o, f, "restoration_mode", to_json(i.restoration_mode));
        key_index(o, f, "kkt_dim", i.kkt_dim);
        key_index(o, f, "kkt_nnz", i.kkt_nnz);
        key(b, first, "ipm");
        b += "{" + o + "}";
    }
    if (event.sqp) {
        const SqpSolveBeginInfo &i = *event.sqp;
        std::string o;
        bool f = true;
        key_index(o, f, "me", i.me);
        key_index(o, f, "mi", i.mi);
        key_enum(o, f, "qp_mode", to_json(i.qp_mode));
        key_enum(o, f, "ws_algebra", to_json(i.ws_algebra));
        key_enum(o, f, "hessian_mode", to_json(i.hessian_mode));
        key_bool(o, f, "row_scaling", i.row_scaling);
        key_enum(o, f, "column_scaling", to_json(i.column_scaling));
        key(b, first, "sqp");
        b += "{" + o + "}";
    }
    write_line("solve.begin", b, engine_name(event.engine));
}

void JsonLinesTraceSink::on_iter(const IterTraceEvent &event) {
    const IterationRecord &r = event.record;
    std::string b;
    bool first = true;
    key_index(b, first, "iter", r.iteration);
    key_bool(b, first, "recovering", r.recovering);
    key_enum(b, first, "tier", to_json(r.tier));
    key_opt_double(b, first, "f", r.f);
    key_opt_double(b, first, "f_recovery", r.f_recovery);
    key_double(b, first, "stationarity", r.tested_stationarity);
    key_double(b, first, "feasibility", r.tested_feasibility);
    key_double(b, first, "complementarity", r.tested_complementarity);
    key_opt_double(b, first, "step", r.step_norm);
    key_opt_bool(b, first, "accepted", r.accepted);
    key_bool(b, first, "soc", r.soc);
    key_bool(b, first, "watchdog", r.watchdog);
    key_bool(b, first, "rolled_back", r.rolled_back);
    key_index(b, first, "factorizations", r.factorizations);
    key_double(b, first, "stationarity_caller", r.stationarity);
    key_double(b, first, "feasibility_caller", r.feasibility);
    key_double(b, first, "complementarity_caller", r.complementarity);
    if (r.ipm) {
        std::string o;
        write_ipm_row(o, *r.ipm);
        key(b, first, "ipm");
        b += "{" + o + "}";
    }
    if (r.sqp) {
        std::string o;
        write_sqp_row(o, *r.sqp);
        key(b, first, "sqp");
        b += "{" + o + "}";
    }
    write_line("iter", b, r.ipm ? "ipm" : "sqp");
}

void JsonLinesTraceSink::on_message(const MessageTraceEvent &event) {
    std::string b;
    bool first = true;
    key_enum(b, first, "kind", to_json(event.kind));
    key_enum(b, first, "severity", to_json(event.severity));
    key_opt_index(b, first, "phase", event.phase);
    key_opt_index(b, first, "iter", event.iter);
    // `solver_initialized.a` is wall-clock: null on the wire, so a deterministic
    // solve's stream is byte-reproducible; the console prints it from the event.
    if (event.kind == MessageKind::kSolverInitialized) {
        key_null(b, first, "a");
    } else {
        key_opt_double(b, first, "a", event.a);
    }
    key_opt_double(b, first, "b", event.b);
    key_opt_index(b, first, "k", event.k);
    if (event.s) {
        key_string(b, first, "s", *event.s);
    } else {
        key_null(b, first, "s");
    }
    key_opt_index(b, first, "p", event.p);
    key_opt_index(b, first, "n", event.n);
    key_opt_index(b, first, "z", event.z);
    key_opt_index(b, first, "expected_p", event.expected_p);
    key_opt_index(b, first, "expected_n", event.expected_n);
    key_opt_ladder(b, first, event.ladder);
    write_line("message", b, engine_name(event.engine));
}

void JsonLinesTraceSink::on_solve_end(const SolveEndTraceEvent &event) {
    std::string b;
    bool first = true;
    key_enum(b, first, "status", to_json(event.status));
    if (event.stop_reason) {
        key_enum(b, first, "stop_reason", to_json(*event.stop_reason));
    } else {
        key_null(b, first, "stop_reason");
    }
    if (event.message) {
        key_string(b, first, "message", *event.message);
    } else {
        key_null(b, first, "message");
    }
    key_index(b, first, "iterations", event.iterations);
    key_bool(b, first, "returned_best_iterate", event.returned_best_iterate);
    key_bool(b, first, "infeasibility_certified", event.infeasibility_certified);
    key_double(b, first, "f", event.f);
    if (event.diagnostics) {
        std::string o;
        bool f = true;
        key_double(o, f, "stationarity", event.diagnostics->stationarity);
        key_double(o, f, "feasibility", event.diagnostics->feasibility);
        key_double(o, f, "complementarity", event.diagnostics->complementarity);
        key(b, first, "diagnostics");
        b += "{" + o + "}";
    } else {
        key_null(b, first, "diagnostics");
    }
    {
        std::string o;
        bool f = true;
        if (event.ipm) {
            const IpmConvergence &c = event.ipm->convergence;
            key_double(o, f, "stationarity", c.stationarity);
            key_double(o, f, "complementarity", c.complementarity);
            key_double(o, f, "equality", c.equality);
            key_double(o, f, "inequality", c.inequality);
            key_double(o, f, "feasibility", c.feasibility);
        } else if (event.sqp) {
            const SqpConvergence &c = event.sqp->convergence;
            key_double(o, f, "stationarity", c.stationarity);
            key_double(o, f, "feasibility", c.feasibility);
            key_double(o, f, "complementarity", c.complementarity);
            key_double(o, f, "kkt_residual", c.kkt_residual);
        }
        key(b, first, "convergence");
        b += "{" + o + "}";
    }
    if (event.work) {
        std::string o;
        bool f = true;
        key_index(o, f, "objective_evals", event.work->objective_evals);
        key_index(o, f, "gradient_evals", event.work->gradient_evals);
        key_index(o, f, "constraint_evals", event.work->constraint_evals);
        key_index(o, f, "jacobian_evals", event.work->jacobian_evals);
        key_index(o, f, "hessian_evals", event.work->hessian_evals);
        key_index(o, f, "factorizations", event.work->factorizations);
        key(b, first, "work");
        b += "{" + o + "}";
    } else {
        key_null(b, first, "work");
    }
    key(b, first, "counters");
    if (event.ipm && event.ipm->counters != nullptr) {
        b += counters_object(*event.ipm->counters);
    } else if (event.sqp && event.sqp->counters != nullptr) {
        b += counters_object(*event.sqp->counters);
    } else {
        b += "null";
    }
    key_double(b, first, "wall_s", event.wall_s);
    if (event.ipm) {
        const IpmSolveEndInfo &i = *event.ipm;
        std::string o;
        bool f = true;
        key_double(o, f, "total_time_s", i.total_time_s);
        key_double(o, f, "pre_time_s", i.pre_time_s);
        key_double(o, f, "func_time_s", i.func_time_s);
        key_double(o, f, "kkt_time_s", i.kkt_time_s);
        key_double(o, f, "print_time_s", i.print_time_s);
        key_double(o, f, "solver_init_time_s", i.solver_init_time_s);
        key_double(o, f, "misc_time_s", i.misc_time_s);
        key_index(o, f, "phases_ran", i.phases_ran);
        key_index(o, f, "kkt_analyses_total", i.kkt_analyses_total);
        key_opt_index(o, f, "factor_memory", i.factor_memory);
        key_opt_index(o, f, "factor_flops", i.factor_flops);
        key(b, first, "ipm");
        b += "{" + o + "}";
    }
    if (event.sqp) {
        const SqpSolveEndInfo &i = *event.sqp;
        std::string o;
        bool f = true;
        key_bool(o, f, "scaling_active", i.scaling_active);
        key_double(o, f, "obj_scale", i.obj_scale);
        key_double(o, f, "row_scale_min", i.row_scale_min);
        key_double(o, f, "row_scale_max", i.row_scale_max);
        key_double(o, f, "scaled_kkt_residual", i.scaled_kkt_residual);
        key_bool(o, f, "columns_active", i.columns_active);
        key_double(o, f, "col_spread", i.col_spread);
        key_enum(o, f, "start_level_used", to_json(i.start_level_used));
        key(b, first, "sqp");
        b += "{" + o + "}";
    }
    write_line("solve.end", b, engine_name(event.engine));
    // Written FIRST, then unwound: this line belongs to the solve it closes.
    if (open_solves_ > 0) {
        --open_solves_;
    }
    if (open_solves_ > 0) {
        pop_depth();
    }
}
} // namespace hven
