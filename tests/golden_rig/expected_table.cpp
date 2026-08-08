#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <fmt/format.h>

#include "expected_table.h"

#ifndef HVEN_RIG_EXPECTED_DIR
#error "HVEN_RIG_EXPECTED_DIR must be defined -- it is where the committed expected tables live"
#endif

namespace hven::rig {
namespace {

const char *const kHeader = "arm,quantity,kind,value,tolerance,machine,backend,"
                            "thread_pin_mechanism,thread_pin_value,commit,date";
constexpr std::size_t kColumnCount = 11;

std::string trim(const std::string &s) {
    const auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

std::vector<std::string> split_csv(const std::string &line) {
    std::vector<std::string> out;
    std::string field;
    std::istringstream in(line);
    while (std::getline(in, field, ',')) {
        out.push_back(trim(field));
    }
    // A trailing comma means one more (empty) field, which getline drops.
    if (!line.empty() && line.back() == ',') {
        out.emplace_back();
    }
    return out;
}

std::string table_path(const std::string &trace) {
    return std::string(HVEN_RIG_EXPECTED_DIR) + "/" + trace + ".csv";
}

// Formats a double so that reading it back gives the same value. The rig's
// float rows are compared at a tolerance, but a report row is also the raw
// material for a derived table, so it must not lose digits on the way out.
std::string format_exact(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    return buf;
}

bool parse_double(const std::string &s, double &out) {
    try {
        std::size_t consumed = 0;
        out = std::stod(s, &consumed);
        return consumed == s.size();
    } catch (const std::exception &) {
        return false;
    }
}

bool parse_int(const std::string &s, long long &out) {
    const char *first = s.data();
    const char *last = s.data() + s.size();
    const auto result = std::from_chars(first, last, out);
    return result.ec == std::errc{} && result.ptr == last;
}

} // namespace

const char *expected_table_header() { return kHeader; }

const char *value_kind_name(ValueKind k) {
    switch (k) {
    case ValueKind::kCounter:
        return "counter";
    case ValueKind::kFloat:
        return "float";
    case ValueKind::kState:
        return "state";
    case ValueKind::kPresence:
        return "presence";
    case ValueKind::kBool:
        return "bool";
    }
    return "unknown";
}

ValueKind value_kind_from_name(const std::string &name) {
    if (name == "counter") {
        return ValueKind::kCounter;
    }
    if (name == "float") {
        return ValueKind::kFloat;
    }
    if (name == "state") {
        return ValueKind::kState;
    }
    if (name == "presence") {
        return ValueKind::kPresence;
    }
    if (name == "bool") {
        return ValueKind::kBool;
    }
    if (name == "float0ulp" || name == "bitwise") {
        throw std::invalid_argument(fmt::format(
            "expected table: kind '{}' is not permitted in a committed table. Bitwise equality "
            "is valid only between two observations of ONE pinned-thread process run, never "
            "between a file written on one machine and a run on another -- a trace that needs a "
            "bitwise property asserts it against its own second observation instead.",
            name));
    }
    throw std::invalid_argument(
        fmt::format("expected table: unknown kind '{}' (want counter, float, state, presence or "
                    "bool)",
                    name));
}

bool ExpectedTable::exists(const std::string &trace) {
    std::ifstream in(table_path(trace));
    return in.good();
}

ExpectedTable ExpectedTable::load(const std::string &trace) {
    const std::string path = table_path(trace);
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error(fmt::format("expected table: cannot open {}", path));
    }

    ExpectedTable table;
    table.trace_ = trace;

    std::string line;
    bool header_seen = false;
    int line_no = 0;
    while (std::getline(in, line)) {
        ++line_no;
        const std::string t = trim(line);
        if (t.empty()) {
            continue;
        }
        if (t[0] == '#') {
            const std::string body = trim(t.substr(1));
            const auto colon = body.find(':');
            if (colon != std::string::npos) {
                table.metadata_.emplace(trim(body.substr(0, colon)), trim(body.substr(colon + 1)));
            }
            continue;
        }
        if (!header_seen) {
            if (t != kHeader) {
                throw std::runtime_error(
                    fmt::format("expected table {}: header row must be exactly\n  {}\ngot\n  {}",
                                path, kHeader, t));
            }
            header_seen = true;
            continue;
        }

        const std::vector<std::string> f = split_csv(t);
        if (f.size() != kColumnCount) {
            throw std::runtime_error(fmt::format("expected table {}:{}: {} columns, want {}", path,
                                                 line_no, f.size(), kColumnCount));
        }

        ExpectedRow row;
        row.arm = f[0];
        row.quantity = f[1];
        try {
            row.kind = value_kind_from_name(f[2]);
        } catch (const std::invalid_argument &e) {
            throw std::invalid_argument(
                fmt::format("expected table {}:{}: {}", path, line_no, e.what()));
        }
        row.value = f[3];
        row.machine = f[5];
        row.backend = f[6];
        row.thread_pin_mechanism = f[7];
        row.thread_pin_value = f[8];
        row.commit = f[9];
        row.date = f[10];

        if (!row.unobserved()) {
            // Comparison policy: a row without a thread pin is invalid.
            if (row.thread_pin_mechanism.empty() || row.thread_pin_value.empty()) {
                throw std::invalid_argument(fmt::format(
                    "expected table {}:{}: row ({}, {}) carries an observed value but no thread "
                    "pin. Every asserted row must record the mechanism the seam pinned with and "
                    "the value it pinned to; without them the row is not an expectation.",
                    path, line_no, row.arm, row.quantity));
            }
            long long pin = 0;
            if (!parse_int(row.thread_pin_value, pin)) {
                throw std::invalid_argument(
                    fmt::format("expected table {}:{}: thread_pin_value '{}' is not an integer",
                                path, line_no, row.thread_pin_value));
            }
            for (const auto &[label, field] :
                 {std::pair<const char *, const std::string *>{"machine", &row.machine},
                  {"backend", &row.backend},
                  {"commit", &row.commit},
                  {"date", &row.date}}) {
                if (field->empty()) {
                    throw std::invalid_argument(fmt::format(
                        "expected table {}:{}: row ({}, {}) carries an observed value but its "
                        "'{}' provenance column is empty",
                        path, line_no, row.arm, row.quantity, label));
                }
            }
            if (row.kind == ValueKind::kFloat) {
                if (f[4].empty() || !parse_double(f[4], row.tolerance) || !(row.tolerance > 0.0)) {
                    throw std::invalid_argument(fmt::format(
                        "expected table {}:{}: row ({}, {}) is a float row and must state a "
                        "positive tolerance",
                        path, line_no, row.arm, row.quantity));
                }
            }
        } else if (!f[4].empty()) {
            parse_double(f[4], row.tolerance);
        }

        table.rows_.push_back(std::move(row));
    }

    if (!header_seen) {
        throw std::runtime_error(fmt::format("expected table {}: no header row found", path));
    }
    return table;
}

const ExpectedRow *ExpectedTable::find(const std::string &arm, const std::string &quantity) const {
    for (const ExpectedRow &r : rows_) {
        if (r.arm == arm && r.quantity == quantity) {
            return &r;
        }
    }
    return nullptr;
}

Observation Observation::counter(std::string trace, std::string arm, std::string quantity,
                                 Index v) {
    Observation o;
    o.trace = std::move(trace);
    o.arm = std::move(arm);
    o.quantity = std::move(quantity);
    o.kind = ValueKind::kCounter;
    o.value = std::to_string(v);
    return o;
}

Observation Observation::real(std::string trace, std::string arm, std::string quantity, double v,
                              double tolerance) {
    Observation o;
    o.trace = std::move(trace);
    o.arm = std::move(arm);
    o.quantity = std::move(quantity);
    o.kind = ValueKind::kFloat;
    o.value = format_exact(v);
    o.tolerance = tolerance;
    return o;
}

Observation Observation::state(std::string trace, std::string arm, std::string quantity,
                               std::string s) {
    Observation o;
    o.trace = std::move(trace);
    o.arm = std::move(arm);
    o.quantity = std::move(quantity);
    o.kind = ValueKind::kState;
    o.value = std::move(s);
    return o;
}

Observation Observation::presence(std::string trace, std::string arm, std::string quantity,
                                  bool present) {
    Observation o;
    o.trace = std::move(trace);
    o.arm = std::move(arm);
    o.quantity = std::move(quantity);
    o.kind = ValueKind::kPresence;
    o.value = present ? "present" : "absent";
    return o;
}

Observation Observation::boolean(std::string trace, std::string arm, std::string quantity, bool v) {
    Observation o;
    o.trace = std::move(trace);
    o.arm = std::move(arm);
    o.quantity = std::move(quantity);
    o.kind = ValueKind::kBool;
    o.value = v ? "true" : "false";
    return o;
}

Comparison compare(const Observation &obs, const ExpectedTable *table) {
    Comparison c;
    if (table == nullptr) {
        c.verdict = Comparison::Verdict::kNoExpectation;
        c.detail = fmt::format("no expected table for trace {} -- observed {} = {}", obs.trace,
                               obs.quantity, obs.value);
        return c;
    }
    const ExpectedRow *row = table->find(obs.arm, obs.quantity);
    if (row == nullptr) {
        c.verdict = Comparison::Verdict::kNoExpectation;
        c.detail = fmt::format("no expected row for ({}, {}) -- observed {}", obs.arm, obs.quantity,
                               obs.value);
        return c;
    }
    if (row->unobserved()) {
        c.verdict = Comparison::Verdict::kUnobserved;
        c.detail = fmt::format("({}, {}) is an unfilled slot -- observed {}", obs.arm, obs.quantity,
                               obs.value);
        return c;
    }
    if (row->kind != obs.kind) {
        c.verdict = Comparison::Verdict::kMismatch;
        c.detail = fmt::format("({}, {}): expected a {} row, observed a {}", obs.arm, obs.quantity,
                               value_kind_name(row->kind), value_kind_name(obs.kind));
        return c;
    }

    switch (obs.kind) {
    case ValueKind::kFloat: {
        double expected = 0.0;
        double observed = 0.0;
        if (!parse_double(row->value, expected) || !parse_double(obs.value, observed)) {
            c.verdict = Comparison::Verdict::kMismatch;
            c.detail = fmt::format("({}, {}): could not read '{}' or '{}' as a number", obs.arm,
                                   obs.quantity, row->value, obs.value);
            return c;
        }
        const double scale = std::max(1.0, std::abs(expected));
        const double err = std::abs(observed - expected);
        if (err <= row->tolerance * scale) {
            c.verdict = Comparison::Verdict::kMatch;
            c.detail = fmt::format("({}, {}): {} within {} of {}", obs.arm, obs.quantity, obs.value,
                                   row->tolerance, row->value);
        } else {
            c.verdict = Comparison::Verdict::kMismatch;
            c.detail = fmt::format(
                "({}, {}): observed {}, expected {} (relative error {:.3e} exceeds "
                "tolerance {:.3e}); expectation pinned by {}={} on {}",
                obs.arm, obs.quantity, obs.value, row->value, err / scale, row->tolerance,
                row->thread_pin_mechanism, row->thread_pin_value, row->machine);
        }
        return c;
    }
    case ValueKind::kCounter:
    case ValueKind::kState:
    case ValueKind::kPresence:
    case ValueKind::kBool:
        if (row->value == obs.value) {
            c.verdict = Comparison::Verdict::kMatch;
            c.detail = fmt::format("({}, {}): {}", obs.arm, obs.quantity, obs.value);
        } else {
            c.verdict = Comparison::Verdict::kMismatch;
            c.detail = fmt::format("({}, {}): observed {}, expected {} (pinned by {}={} on {})",
                                   obs.arm, obs.quantity, obs.value, row->value,
                                   row->thread_pin_mechanism, row->thread_pin_value, row->machine);
        }
        return c;
    }
    c.verdict = Comparison::Verdict::kMismatch;
    c.detail = "unreachable value kind";
    return c;
}

std::string to_csv_row(const Observation &obs, const std::string &machine,
                       const std::string &backend, const std::string &thread_pin_mechanism,
                       int thread_pin_value, const std::string &commit, const std::string &date) {
    std::string tolerance;
    if (obs.kind == ValueKind::kFloat) {
        tolerance = format_exact(obs.tolerance);
    }
    return fmt::format("{},{},{},{},{},{},{},{},{},{},{}", obs.arm, obs.quantity,
                       value_kind_name(obs.kind), obs.value, tolerance, machine, backend,
                       thread_pin_mechanism, thread_pin_value, commit, date);
}

} // namespace hven::rig
