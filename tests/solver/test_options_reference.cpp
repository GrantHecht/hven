// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// M8 AP-4 G2: the options registry's public metadata and the generated options reference.
//
//   * OptionsReference.CheckedInFileIsCurrent -- docs/options-reference.md is byte-equal to
//     write_options_reference()'s output; a mismatch names the first differing line and the
//     command that regenerates the file.
//   * The rows agree with the options dump (names, order, defaults), the tiers follow the
//     `advanced.` prefix, and every enum's name table is contiguous, unique and round-trips.
//   * RANGE PROBES: for every row whose range is an interval, a value just outside it makes
//     validate() throw (and NaN, and +inf where the range is finite); every enum field takes a
//     value outside its enumerators and validate() throws; the defaults validate. The ranges the
//     tables print are the ranges validate() enforces, or this fails by field name.
//   * THE HEADER-DEFAULT RIDER: where a field's header doc comment says `Default <v>`, <v> equals
//     the rendered default.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include <hven/detail/ipm/ipm_options_fields.h>
#include <hven/detail/sqp/sqp_options_fields.h>
#include <hven/ipm/ipm_solver_types.h>
#include <hven/solver/options_reference.h>
#include <hven/sqp/sqp_solver_types.h>

namespace {

using hven::IpmOptions;
using hven::OptionInfo;
using hven::OptionTier;
using hven::SqpOptions;
using hven::detail::OptionMeta;
using hven::detail::OptionRange;

std::string read_file(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    EXPECT_TRUE(in.good()) << "cannot read " << path;
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

std::string reference_text() {
    std::ostringstream os;
    hven::write_options_reference(os);
    return os.str();
}

template <class Options> std::string dump_text(const Options &o) {
    std::ostringstream os;
    hven::dump_options(o, os);
    return os.str();
}

// ---------------------------------------------------------------------------
// The checked-in file.
// ---------------------------------------------------------------------------
TEST(OptionsReference, CheckedInFileIsCurrent) {
    const std::string path = std::string(HVEN_SOURCE_DIR) + "/docs/options-reference.md";
    const std::string expected = reference_text();
    const std::string actual = read_file(path);
    if (actual == expected) {
        SUCCEED();
        return;
    }
    std::size_t line = 1;
    std::size_t i = 0;
    while (i < expected.size() && i < actual.size() && expected[i] == actual[i]) {
        if (expected[i] == '\n') {
            ++line;
        }
        ++i;
    }
    const auto line_at = [](const std::string &text, std::size_t from) {
        std::size_t start = text.rfind('\n', from == 0 ? 0 : from - 1);
        start = (start == std::string::npos || from == 0) ? 0 : start + 1;
        const std::size_t end = text.find('\n', from);
        return text.substr(start, end == std::string::npos ? std::string::npos : end - start);
    };
    ADD_FAILURE() << "docs/options-reference.md is stale: it first differs from "
                     "write_options_reference()'s output at line "
                  << line << ".\n  checked in : " << line_at(actual, i)
                  << "\n  generated  : " << line_at(expected, i)
                  << "\nRegenerate it with: cmake --build <build dir> --target options-reference";
}

TEST(OptionsReference, TheFileIsStreamStateIndependent) {
    std::ostringstream os;
    os << std::hex << std::showbase << std::scientific << std::setprecision(3);
    os.width(40);
    hven::write_options_reference(os);
    EXPECT_EQ(os.str(), reference_text());
}

// ---------------------------------------------------------------------------
// The rows.
// ---------------------------------------------------------------------------
std::vector<std::pair<std::string, std::string>> dump_pairs(const std::string &text) {
    std::vector<std::pair<std::string, std::string>> out;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t end = text.find('\n', start);
        const std::string line = text.substr(start, end - start);
        const std::size_t eq = line.find(" = ");
        out.emplace_back(line.substr(0, eq), line.substr(eq + 3));
        start = end + 1;
    }
    return out;
}

void expect_rows_match_the_dump(const std::vector<OptionInfo> &rows, const std::string &dump) {
    const auto pairs = dump_pairs(dump);
    ASSERT_EQ(rows.size(), pairs.size());
    for (std::size_t i = 0; i < rows.size(); ++i) {
        EXPECT_EQ(rows[i].name, pairs[i].first) << "row " << i;
        EXPECT_EQ(rows[i].default_value, pairs[i].second) << rows[i].name;
        EXPECT_FALSE(rows[i].type.empty()) << rows[i].name;
        EXPECT_FALSE(rows[i].range.empty()) << rows[i].name;
        EXPECT_FALSE(rows[i].doc.empty()) << rows[i].name;
        EXPECT_EQ(rows[i].tier == OptionTier::kAdvanced, rows[i].name.rfind("advanced.", 0) == 0)
            << rows[i].name;
    }
}

TEST(OptionsReference, IpmRowsAreTheDumpsLines) {
    expect_rows_match_the_dump(hven::ipm_option_info(), dump_text(IpmOptions{}));
}

TEST(OptionsReference, SqpRowsAreTheDumpsLines) {
    expect_rows_match_the_dump(hven::sqp_option_info(), dump_text(SqpOptions{}));
}

TEST(OptionsReference, EveryRowAppearsInTheReference) {
    const std::string text = reference_text();
    for (const auto *rows : {&hven::ipm_option_info(), &hven::sqp_option_info()}) {
        for (const OptionInfo &o : *rows) {
            EXPECT_NE(text.find("| `" + o.name + "` |"), std::string::npos) << o.name;
        }
    }
}

TEST(OptionsReference, TheCommonRangesDifferPerEngineWhereValidateDoes) {
    const auto find = [](const std::vector<OptionInfo> &rows, const std::string &name) {
        for (const OptionInfo &o : rows) {
            if (o.name == name) {
                return o;
            }
        }
        ADD_FAILURE() << name;
        return OptionInfo{};
    };
    EXPECT_EQ(find(hven::ipm_option_info(), "common.threads").range, ">= 1");
    EXPECT_EQ(find(hven::sqp_option_info(), "common.threads").range, ">= 0");
    EXPECT_EQ(find(hven::ipm_option_info(), "common.max_iterations").range, "[1, 2147483647]");
    EXPECT_EQ(find(hven::sqp_option_info(), "common.max_iterations").range, "[0, 2147483647]");
}

// ---------------------------------------------------------------------------
// Enum name tables.
// ---------------------------------------------------------------------------
template <class Options> std::set<std::string> check_enums(Options &o) {
    std::set<std::string> types;
    hven::detail::for_each_option(o, [&types](const OptionMeta &m, auto &field) {
        using V = std::remove_cvref_t<decltype(field)>;
        if constexpr (std::is_enum_v<V>) {
            types.insert(hven::detail::option_type_name<V>());
            const auto &table = hven::detail::enumerators<V>();
            EXPECT_GE(table.size(), 2U) << m.member;
            std::set<std::string> names;
            for (std::size_t i = 0; i < table.size(); ++i) {
                EXPECT_EQ(static_cast<long long>(table[i].value), static_cast<long long>(i));
                EXPECT_EQ(std::string(hven::detail::EnumTraits<V>::name(table[i].value)),
                          table[i].name);
                EXPECT_EQ(std::string(table[i].name).rfind('k', 0), 0U) << table[i].name;
                names.insert(table[i].name);
            }
            EXPECT_EQ(names.size(), table.size()) << "duplicate enumerator names";
            EXPECT_STREQ(hven::detail::EnumTraits<V>::name(static_cast<V>(table.size())),
                         "unknown");
            EXPECT_STREQ(hven::detail::EnumTraits<V>::name(static_cast<V>(-1)), "unknown");
        }
    });
    return types;
}

TEST(OptionsReference, EveryEnumsNameTableIsContiguousUniqueAndRoundTrips) {
    IpmOptions i;
    SqpOptions s;
    std::set<std::string> types = check_enums(i);
    for (const std::string &t : check_enums(s)) {
        types.insert(t);
    }
    EXPECT_GE(types.size(), 24U);
}

// ---------------------------------------------------------------------------
// Range probes.
// ---------------------------------------------------------------------------
std::vector<double> probe_values(const OptionRange &r, bool integral) {
    std::vector<double> out;
    if (r.kind != OptionRange::Kind::kInterval) {
        return out;
    }
    constexpr double kInf = std::numeric_limits<double>::infinity();
    if (r.has_lo) {
        out.push_back(r.lo_open ? r.lo : (integral ? r.lo - 1.0 : std::nextafter(r.lo, -kInf)));
    }
    if (r.has_hi) {
        out.push_back(r.hi_open ? r.hi : (integral ? r.hi + 1.0 : std::nextafter(r.hi, kInf)));
    }
    if (!integral) {
        out.push_back(std::numeric_limits<double>::quiet_NaN());
        if (r.finite) {
            out.push_back(kInf);
        }
    }
    return out;
}

template <class T>
inline constexpr bool kProbeArithmetic = std::is_arithmetic_v<T> && !std::is_same_v<T, bool>;

// Sets probe number j of the field, or returns false when there are no more.
template <class V> bool apply_probe(const OptionMeta &m, V &field, std::size_t j) {
    if constexpr (std::is_enum_v<V>) {
        if (j != 0) {
            return false;
        }
        field = static_cast<V>(hven::detail::enumerators<V>().size());
        return true;
    } else if constexpr (kProbeArithmetic<V>) {
        const std::vector<double> values = probe_values(m.range, std::is_integral_v<V>);
        if (j >= values.size()) {
            return false;
        }
        field = static_cast<V>(values[j]);
        return true;
    } else if constexpr (hven::detail::IsOptional<V>::value) {
        using U = typename V::value_type;
        if constexpr (kProbeArithmetic<U>) {
            const std::vector<double> values = probe_values(m.range, std::is_integral_v<U>);
            if (j >= values.size()) {
                return false;
            }
            field = static_cast<U>(values[j]);
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

template <class Options> std::size_t run_probes(const std::vector<OptionInfo> &rows) {
    Options defaults;
    EXPECT_NO_THROW(hven::validate(defaults));
    std::size_t probes = 0;
    for (std::size_t k = 0; k < rows.size(); ++k) {
        for (std::size_t j = 0; j < 8; ++j) {
            Options o;
            bool applied = false;
            std::size_t index = 0;
            hven::detail::for_each_option(o, [&](const OptionMeta &m, auto &field) {
                if (index++ == k) {
                    applied = apply_probe(m, field, j);
                }
            });
            if (!applied) {
                break;
            }
            ++probes;
            EXPECT_THROW(hven::validate(o), std::invalid_argument)
                << rows[k].name << " (" << rows[k].range << "), probe " << j;
        }
    }
    return probes;
}

TEST(OptionsReference, IpmRangesAreWhatValidateEnforces) {
    EXPECT_GE(run_probes<IpmOptions>(hven::ipm_option_info()), 100U);
}

TEST(OptionsReference, SqpRangesAreWhatValidateEnforces) {
    EXPECT_GE(run_probes<SqpOptions>(hven::sqp_option_info()), 120U);
}

TEST(OptionsReference, AnEmptyOrOutOfEnumPhaseListIsRefused) {
    IpmOptions o;
    o.phases.clear();
    EXPECT_THROW(hven::validate(o), std::invalid_argument);
    o.phases = {static_cast<hven::IpmPhase>(2)};
    EXPECT_THROW(hven::validate(o), std::invalid_argument);
}

// ---------------------------------------------------------------------------
// THE HEADER-DEFAULT RIDER.
//
// A field's header doc comment that says `Default <v>` states its default. The rendered default
// (the one the dump prints and the reference shows) must be that value. Numbers compare as numbers
// (1e20 is 1e+20), booleans and enumerators as words. Phrases the rider cannot read as a value
// (`Default 2^26 = ...`) are not compared and are counted so the rider cannot go vacuous.
// ---------------------------------------------------------------------------
const char *header_of(const std::string &owner) {
    static const std::map<std::string, const char *> kHeaders = {
        {"CommonOptions", "include/hven/solver/common_options.h"},
        {"IpmLinearOptions", "include/hven/ipm/ipm_options.h"},
        {"IpmTerminationOptions", "include/hven/ipm/ipm_options.h"},
        {"IpmAdvancedOptions", "include/hven/ipm/ipm_options.h"},
        {"IpmOptions", "include/hven/ipm/ipm_options.h"},
        {"QpOptions", "include/hven/qp/qp_types.h"},
        {"IpqpOptions", "include/hven/qp/qp_options.h"},
        {"SqpLinearOptions", "include/hven/sqp/sqp_options.h"},
        {"SqpAdvancedOptions", "include/hven/sqp/sqp_options.h"},
        {"SqpOptions", "include/hven/sqp/sqp_options.h"},
    };
    const auto it = kHeaders.find(owner);
    return it == kHeaders.end() ? nullptr : it->second;
}

// The doc comment (`///` lines) directly above the declaration of `member` inside `struct owner`.
std::optional<std::string> doc_comment_of(const std::vector<std::string> &lines,
                                          const std::string &owner, const std::string &member) {
    std::size_t at = 0;
    const std::string open = "struct " + owner + " {";
    while (at < lines.size() && lines[at].find(open) == std::string::npos) {
        ++at;
    }
    if (at == lines.size()) {
        return std::nullopt;
    }
    const std::regex decl("^\\s{4}[A-Za-z_][A-Za-z0-9_:<>(), ]*\\s+" + member + "\\s*(=|\\{|;)");
    for (std::size_t i = at + 1; i < lines.size(); ++i) {
        if (lines[i].rfind("};", 0) == 0) {
            break;
        }
        if (!std::regex_search(lines[i], decl)) {
            continue;
        }
        std::string doc;
        for (std::size_t j = i; j-- > at + 1;) {
            const std::size_t slashes = lines[j].find("///");
            if (slashes == std::string::npos) {
                break;
            }
            doc = lines[j].substr(slashes + 3) + " " + doc;
        }
        return doc;
    }
    return std::nullopt;
}

// The value after the first `Default`, or empty if the doc states none the rider can read.
std::string stated_default(const std::string &doc) {
    static const std::regex phrase("\\b[Dd]efaults?\\b\\s+(?:is\\s+)?(\\S+)");
    std::smatch m;
    if (!std::regex_search(doc, m, phrase)) {
        return {};
    }
    std::string token = m[1].str();
    token.erase(std::remove(token.begin(), token.end(), '`'), token.end());
    while (!token.empty() && std::string(".,;:)").find(token.back()) != std::string::npos) {
        token.pop_back();
    }
    return token;
}

bool is_number(const std::string &s) {
    static const std::regex number("[+-]?([0-9]+\\.?[0-9]*|\\.[0-9]+)([eE][+-]?[0-9]+)?");
    return std::regex_match(s, number);
}

// Fields whose header phrase is a different statement than "the default is <v>".
const std::map<std::string, std::string> &rider_exceptions() {
    static const std::map<std::string, std::string> kExceptions = {
        {"CommonOptions.max_iterations",
         "documented per engine (IPM 1000 per phase, SQP 100); the registry's SQP row is the "
         "struct's"},
        {"CommonOptions.threads", "documented per engine (IPM HVEN_DEFAULT_IPM_THREADS, SQP 0)"},
        {"IpmLinearOptions.accelerate_zero_tol", "stated as a multiple of epsilon"},
    };
    return kExceptions;
}

template <class Options> void run_rider(std::size_t &compared, std::size_t &unread) {
    std::map<std::string, std::vector<std::string>> cache;
    const Options defaults;
    hven::detail::for_each_option(defaults, [&](const OptionMeta &m, const auto &field) {
        const std::string owner(m.owner);
        const std::string member(m.member);
        // CommonOptions is shared: read it once, from the SQP engine, whose defaults are the
        // struct's own.
        if (owner == "CommonOptions" && std::is_same_v<Options, IpmOptions>) {
            return;
        }
        const char *header = header_of(owner);
        ASSERT_NE(header, nullptr) << owner;
        auto it = cache.find(header);
        if (it == cache.end()) {
            std::istringstream in(read_file(std::string(HVEN_SOURCE_DIR) + "/" + header));
            std::vector<std::string> lines;
            for (std::string l; std::getline(in, l);) {
                lines.push_back(l);
            }
            it = cache.emplace(header, std::move(lines)).first;
        }
        const std::optional<std::string> doc = doc_comment_of(it->second, owner, member);
        ASSERT_TRUE(doc.has_value()) << owner << "::" << member << " in " << header;
        const std::string stated = stated_default(*doc);
        if (stated.empty() || rider_exceptions().count(owner + "." + member) != 0) {
            return;
        }
        const std::string rendered = hven::detail::render_option_value(field);
        const bool readable = is_number(stated) || stated == "true" || stated == "false" ||
                              (stated.size() > 1 && stated[0] == 'k' && std::isupper(stated[1]));
        if (!readable) {
            ++unread;
            return;
        }
        ++compared;
        if (is_number(stated) && is_number(rendered)) {
            EXPECT_EQ(std::stod(stated), std::stod(rendered))
                << owner << "::" << member << ": header says Default " << stated
                << ", the rendered default is " << rendered;
        } else {
            EXPECT_EQ(stated, rendered) << owner << "::" << member << ": header says Default "
                                        << stated << ", the rendered default is " << rendered;
        }
    });
}

TEST(OptionsReference, HeaderDefaultsAgreeWithTheRenderedDefaults) {
    std::size_t compared = 0;
    std::size_t unread = 0;
    run_rider<SqpOptions>(compared, unread);
    run_rider<IpmOptions>(compared, unread);
    // The rider must have read most of the headers' statements, or it proves nothing.
    EXPECT_GE(compared, 100U);
    std::cout << "[rider] compared " << compared << " stated defaults, " << unread
              << " phrases not comparable\n";
}

} // namespace
