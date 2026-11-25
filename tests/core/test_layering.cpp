// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_layering.cpp -- the regression net for CLAUDE.md section 2's layer order, checked where it
// is cheapest to check: the include directives of the headers and translation units themselves.
//
// TWO PARTS. The CoreLayering tests (first half of this file) pin the bottom layer: nothing in
// core/ reaches up. The Layering tests (second half) enforce the whole order over every file under
// include/hven (public and detail/) and src/, with an explicit allow-list of known upward pairs,
// each carrying a debt note. The first half is the special case that was written first and is kept
// whole; the second subsumes it.
//
// ---- the core-only half ----
//
// WHAT IT ASSERTS. `core/` is the BOTTOM tier of the repository map. Every other
// tier -- linear/, model/, kkt/, qp/, warmstart/, solver/,
// ipm/, sqp/, trace/, and the detail/ bodies of all of them -- is allowed to depend on
// core/, and core/ is allowed to depend on none of them. So a header in
// include/hven/core/ may include: standard-library headers, third-party headers
// hven vendors (Eigen, fmt), and OTHER core/ headers. Nothing else under hven/.
//
// WHY IT EXISTS, stated because a rule with no failure story is decoration. Before
// M3 phase-C S2, core/ledger.h included sqp/sqp_solver_types.h and, through it,
// detail/globalization/ and detail/warmstart/ -- core/ depending upward on
// drivers/. S2 eliminated that by moving the counters, the two status enums and
// StartLevel into core/ homes. Nothing then stopped it coming back: the property
// was asserted once, by hand, with `clang++ -H` in a report, and a single
// `#include <hven/sqp/sqp_solver_types.h>` added to a core/ header would compile
// cleanly, pass every other test, and silently restore the inversion while the
// plan record still claimed it was gone. This test is the thing that fails.
//
// TWO CONCRETE ROUTES BACK IN, both live:
//   * core/ledger.h's private section used to carry a note asserting the very
//     dependency S2 deleted. It has been corrected and now says the opposite,
//     but a reader who wants a status-rendering helper is exactly the reader who
//     reaches for sqp/sqp_solver_types.h.
//   * core/start_level.h once re-declared `using Index = Eigen::Index;` rather
//     than including qp/qp_types.h for it (S2); S2c deleted that alias and the
//     header now takes `Index` from core/types.h. A later "simplification" that
//     reaches for qp/qp_types.h from any core/ header is still the inversion.
//
// DIRECT INCLUDES ARE ENOUGH, and that is a proof rather than a shortcut: if
// EVERY core/ header includes only core/ headers (plus non-hven ones), then the
// transitive closure of any core/ header contains only core/ headers, by
// induction on the closure. Checking one hop at every node therefore checks the
// whole closure, and does it without a compiler, a build tree or a parse of
// anything outside this directory.
//
// WHAT IT DELIBERATELY DOES NOT DO. It does not parse C++ -- it reads include
// DIRECTIVES, which is all the invariant is about. A line whose first non-space
// character is not '#' is not a directive and is skipped, so a commented-out or
// prose mention of an include (there is one in core/ledger.h, pointing at this
// file) cannot trip it.

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#ifndef HVEN_INCLUDE_ROOT
#error "HVEN_INCLUDE_ROOT (the include/hven directory) must be defined by tests/CMakeLists.txt"
#endif
#ifndef HVEN_SRC_ROOT
#error "HVEN_SRC_ROOT (the src directory) must be defined by tests/CMakeLists.txt"
#endif

namespace {

// One include directive, with enough context to name it in a failure message.
struct IncludeSite {
    std::string file; // file name only -- the directory is fixed and known
    int line = 0;     // 1-based
    std::string path; // exactly what was between the <> or ""
};

// Reads the include directives out of one file. Deliberately minimal: a line is
// a directive iff its first non-whitespace character is '#', the token that
// follows is `include`, and the next non-whitespace character opens a `<...>` or
// `"..."`. Anything else is not a directive and is ignored.
std::vector<IncludeSite> scan_includes(const std::filesystem::path &file) {
    std::vector<IncludeSite> out;
    std::ifstream in(file);
    EXPECT_TRUE(in.good()) << "could not open " << file.string();

    std::string line;
    int lineno = 0;
    while (std::getline(in, line)) {
        ++lineno;

        std::string_view s(line);
        const auto first = s.find_first_not_of(" \t");
        if (first == std::string_view::npos || s[first] != '#') {
            continue;
        }
        s.remove_prefix(first + 1);

        const auto kw = s.find_first_not_of(" \t");
        if (kw == std::string_view::npos) {
            continue;
        }
        s.remove_prefix(kw);
        constexpr std::string_view kInclude = "include";
        if (s.substr(0, kInclude.size()) != kInclude) {
            continue;
        }
        s.remove_prefix(kInclude.size());

        const auto open = s.find_first_not_of(" \t");
        if (open == std::string_view::npos) {
            continue;
        }
        s.remove_prefix(open);
        char closer = '\0';
        if (s.front() == '<') {
            closer = '>';
        } else if (s.front() == '"') {
            closer = '"';
        } else {
            continue; // a macro-expanded include; not something this rule polices
        }
        s.remove_prefix(1);
        const auto end = s.find(closer);
        if (end == std::string_view::npos) {
            continue;
        }

        out.push_back(IncludeSite{file.filename().string(), lineno, std::string(s.substr(0, end))});
    }
    return out;
}

// The verdict on one include path. Empty means "allowed"; otherwise the returned
// string is the reason, phrased for a failure message.
//
// Both spellings the repository uses in practice are handled -- `<hven/x/y.h>`
// and `"hven/x/y.h"` reach this function identically, since only the text
// between the delimiters is passed in.
std::string violation_reason(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');

    constexpr std::string_view kHven = "hven/";
    if (path.compare(0, kHven.size(), kHven) == 0) {
        const std::string rest = path.substr(kHven.size());
        const auto slash = rest.find('/');
        // `hven/foo.h` -- no tier directory at all. Nothing like this exists
        // today (include/hven's only loose header is solver_interface_adapter.h,
        // which no core/ header has any reason to want), and a core/ header
        // reaching for one would be reaching outside core/ just the same.
        const std::string tier =
            (slash == std::string::npos) ? std::string() : rest.substr(0, slash);
        // include/hven/detail/core/ is core's own internal tier (RS-1a): the same layer, one
        // directory over, and scanned by this test as well.
        if (tier == "detail" && rest.compare(0, 12, "detail/core/") == 0) {
            return {};
        }
        if (tier != "core") {
            return tier.empty()
                       ? "reaches outside core/ (a loose header directly under include/hven/)"
                       : "reaches UP into hven/" + tier +
                             "/ -- core/ is the bottom tier and may "
                             "depend on no tier above it";
        }
        return {};
    }

    // A relative include that climbs out of this directory can only land in a
    // sibling tier or outside include/hven entirely. `#include "types.h"` and
    // friends stay inside core/ and are fine.
    if (path.find("../") != std::string::npos) {
        return "climbs out of core/ with a relative path";
    }

    // Everything else is a standard-library or vendored third-party header
    // (Eigen, fmt), which core/ is free to use.
    return {};
}

std::vector<std::filesystem::path> headers_under(const std::filesystem::path &dir) {
    std::vector<std::filesystem::path> headers;
    if (!std::filesystem::is_directory(dir)) {
        return headers;
    }
    // Recursive so that a future subdirectory is covered the day it appears rather than the day
    // someone remembers this file.
    for (const auto &entry : std::filesystem::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".h") {
            headers.push_back(entry.path());
        }
    }
    std::sort(headers.begin(), headers.end());
    return headers;
}

// include/hven/core and include/hven/detail/core, both reached from the one directory definition
// tests/CMakeLists.txt passes (HVEN_INCLUDE_ROOT), so no second definition has to be kept.
std::filesystem::path core_dir() { return std::filesystem::path(HVEN_INCLUDE_ROOT) / "core"; }
std::filesystem::path core_detail_dir() {
    return std::filesystem::path(HVEN_INCLUDE_ROOT) / "detail" / "core";
}

// Every header of core's two directories: include/hven/core and include/hven/detail/core.
std::vector<std::filesystem::path> core_headers() {
    std::vector<std::filesystem::path> headers = headers_under(core_dir());
    const std::vector<std::filesystem::path> detail = headers_under(core_detail_dir());
    headers.insert(headers.end(), detail.begin(), detail.end());
    return headers;
}

// The directory a scanned header lives in, spelled for a failure message.
std::string label_of(const std::filesystem::path &h) {
    return h.parent_path().parent_path().filename() == "detail" ? "include/hven/detail/core/"
                                                                : "include/hven/core/";
}

} // namespace

// THE SCAN MUST BE SHOWN TO HAVE HAPPENED, and this test exists before the rule
// itself because the rule's failure mode is a VACUOUS PASS: a wrong
// HVEN_INCLUDE_ROOT, a renamed directory or a build that stopped passing the
// definition would make the scan find zero files and the invariant test below
// would report success while checking nothing. So the directory is asserted to
// exist, to be non-empty, and to contain the specific headers this rule is about.
TEST(CoreLayering, TheScanActuallyReadsTheCoreHeaders) {
    const std::filesystem::path dir = core_dir();
    ASSERT_TRUE(std::filesystem::is_directory(dir))
        << "include/hven/core does not exist under HVEN_INCLUDE_ROOT: " << dir.string()
        << " -- the layering check below would pass vacuously.";

    const std::vector<std::filesystem::path> headers = core_headers();
    ASSERT_FALSE(headers.empty()) << "no .h files found under " << dir.string();

    // Anchors: the header the inversion ran through, and the one carrying the
    // Index re-declaration a future simplification is most likely to "fix" by
    // including qp/qp_types.h. If either is ever renamed, this test is the place
    // that says so out loud instead of quietly scanning less.
    // THE COUNT (RS-1a): 11 public headers and 7 internal ones (the seventh: M8 AP-4 G1's
    // detail/core/options_registry.h, the options registry's vocabulary -- ranges, field metadata,
    // rendering and the table-check macros -- which every layer's options table includes, so it
    // sits at the bottom), so a new core file has to be acknowledged here, in the same commit,
    // with the reason for it. It is asserted inside this
    // test, not as a test of its own, so the gtest list stays what it was.
    EXPECT_EQ(headers_under(core_dir()).size(), 11U)
        << "include/hven/core/ gained or lost a header: acknowledge it here";
    EXPECT_EQ(headers_under(core_detail_dir()).size(), 7U)
        << "include/hven/detail/core/ gained or lost a header: acknowledge it here";
    EXPECT_EQ(headers.size(), 18U);

    std::vector<std::string> names;
    names.reserve(headers.size());
    for (const auto &h : headers) {
        names.push_back(h.filename().string());
    }
    EXPECT_NE(std::find(names.begin(), names.end(), "ledger.h"), names.end())
        << "core/ledger.h was not scanned";
    EXPECT_NE(std::find(names.begin(), names.end(), "start_level.h"), names.end())
        << "core/start_level.h was not scanned";

    // Every header must also actually have been READ. A file that opens but
    // yields no directives at all is legal (core/version.h has none today), so
    // this asserts the weaker, sufficient thing: the whole set together carries
    // at least the includes we know are there.
    std::size_t total_directives = 0;
    for (const auto &h : headers) {
        total_directives += scan_includes(h).size();
    }
    EXPECT_GT(total_directives, 0u) << "scanned " << headers.size()
                                    << " core/ headers and found no #include directives at all, "
                                       "which means the reader is broken, not that the headers are";
}

// THE RULE. See this file's header note for why one hop at every node is
// equivalent to checking the whole transitive closure.
TEST(CoreLayering, NoCoreHeaderDependsUpwardOnAnotherTier) {
    std::vector<std::string> violations;

    for (const auto &header : core_headers()) {
        for (const IncludeSite &site : scan_includes(header)) {
            const std::string reason = violation_reason(site.path);
            if (!reason.empty()) {
                violations.push_back(label_of(header) + site.file + ":" +
                                     std::to_string(site.line) + ": #include of '" + site.path +
                                     "' " + reason);
            }
        }
    }

    std::string report;
    for (const std::string &v : violations) {
        report += "\n  " + v;
    }
    EXPECT_TRUE(violations.empty())
        << "core/ is the bottom tier of CLAUDE.md section 2's map and must depend on no tier "
           "above it. A core/ header including a linear/, model/, qp/, "
           "warmstart/, solver/, ipm/, sqp/, trace/ or detail/ header reopens the include-graph "
           "inversion M3 phase-C S2 eliminated.\n"
           "Offending include directive(s):"
        << report
        << "\n\nIf the type you need lives above core/, MOVE THE TYPE DOWN rather than the "
           "include up -- that is what S2 did for the counters, the status enums and "
           "StartLevel. If a deliberate ruling changes the tier order itself, change this "
           "test with it, in the same commit, and say so.";
}

// ======================================================================================
// The whole-order half. CLAUDE.md section 2, "Layering, for every header and TU":
//
//   core < util < barrier < linear < model < qp-data < kkt < qp-engines < warmstart
//        < solver < {ipm, sqp} public types < trace < engine implementations < hven.h
//
// A file includes only files of its own layer or a lower one, and ipm and sqp never include each
// other. The layer of a file follows from its path alone (classify() below):
//   - qp-data is include/hven/qp/*.h plus detail/qp/*.h (qp_problem, working_set);
//   - qp-engines is detail/qp/{walk,ssn,ipqp}/ plus src/qp/**;
//   - engine implementations are detail/{ipm,sqp}/** and src/{ipm,sqp}/**;
//   - every other include/hven/X, detail/X and src/X sits at X's layer.
// Trace is structural: its events carry the engines' public types, and the engine
// implementations emit into it.
//
// As with the core half, direct includes are enough: the rule is monotone, so checking one hop at
// every node checks the transitive closure. A file whose path has no layer is itself a failure,
// so a new directory has to be placed in the order the day it appears.
// ======================================================================================

namespace {

enum class Layer {
    kCore,
    kUtil,
    kBarrier,
    kLinear,
    kModel,
    kQpData,
    kKkt,
    kQpEngines,
    kWarmstart,
    kSolver,
    kEnginePublic, // include/hven/ipm and include/hven/sqp
    kTrace,
    kEngineImpl, // detail/{ipm,sqp} and src/{ipm,sqp}
    kUmbrella    // hven.h and the PCH header: may include anything
};

const char *layer_name(Layer l) {
    switch (l) {
    case Layer::kCore:
        return "core";
    case Layer::kUtil:
        return "util";
    case Layer::kBarrier:
        return "barrier";
    case Layer::kLinear:
        return "linear";
    case Layer::kModel:
        return "model";
    case Layer::kQpData:
        return "qp-data";
    case Layer::kKkt:
        return "kkt";
    case Layer::kQpEngines:
        return "qp-engines";
    case Layer::kWarmstart:
        return "warmstart";
    case Layer::kSolver:
        return "solver";
    case Layer::kEnginePublic:
        return "ipm/sqp public";
    case Layer::kTrace:
        return "trace";
    case Layer::kEngineImpl:
        return "engine implementation";
    case Layer::kUmbrella:
        return "umbrella";
    }
    return "?";
}

struct Placed {
    Layer layer = Layer::kCore;
    std::string family; // "ipm" or "sqp" for the engine layers, otherwise empty
};

std::vector<std::string> split_path(const std::string &p) {
    std::vector<std::string> parts;
    std::size_t i = 0;
    while (i <= p.size()) {
        const auto j = p.find('/', i);
        parts.push_back(p.substr(i, j == std::string::npos ? std::string::npos : j - i));
        if (j == std::string::npos) {
            break;
        }
        i = j + 1;
    }
    return parts;
}

// `rel` is a repo-relative generic path: include/hven/... or src/.... Empty optional means the
// path names no component of the order.
std::optional<Placed> classify(const std::string &rel) {
    if (rel == "include/hven/hven.h" || rel == "src/hven_pch.h") {
        return Placed{Layer::kUmbrella, {}};
    }
    const std::vector<std::string> parts = split_path(rel);
    std::string comp;
    std::string sub; // the directory below the component, when the file is not directly in it
    enum { kPublic, kDetail, kSrc } where;
    if (rel.rfind("include/hven/detail/", 0) == 0 && parts.size() >= 5) {
        where = kDetail;
        comp = parts[3];
        sub = parts.size() > 5 ? parts[4] : std::string();
    } else if (rel.rfind("include/hven/", 0) == 0 && parts.size() >= 4) {
        where = kPublic;
        comp = parts[2];
    } else if (rel.rfind("src/", 0) == 0 && parts.size() >= 3) {
        where = kSrc;
        comp = parts[1];
        sub = parts.size() > 3 ? parts[2] : std::string();
    } else {
        return std::nullopt;
    }
    (void)sub;
    if (comp == "core") {
        return Placed{Layer::kCore, {}};
    }
    if (comp == "util") {
        return Placed{Layer::kUtil, {}};
    }
    if (comp == "barrier") {
        return Placed{Layer::kBarrier, {}};
    }
    if (comp == "linear") {
        return Placed{Layer::kLinear, {}};
    }
    if (comp == "model") {
        return Placed{Layer::kModel, {}};
    }
    if (comp == "qp") {
        // qp data: the public qp/*.h and the files directly in detail/qp/. Everything deeper in
        // detail/qp (walk, ssn, ipqp) and all of src/qp is an engine.
        if (where == kPublic || (where == kDetail && sub.empty())) {
            return Placed{Layer::kQpData, {}};
        }
        return Placed{Layer::kQpEngines, {}};
    }
    if (comp == "kkt") {
        return Placed{Layer::kKkt, {}};
    }
    if (comp == "warmstart") {
        return Placed{Layer::kWarmstart, {}};
    }
    if (comp == "solver") {
        return Placed{Layer::kSolver, {}};
    }
    if (comp == "trace") {
        return Placed{Layer::kTrace, {}};
    }
    if (comp == "ipm" || comp == "sqp") {
        return Placed{where == kPublic ? Layer::kEnginePublic : Layer::kEngineImpl, comp};
    }
    return std::nullopt;
}

// Why a pair is in the order's way: the verdict on one include of `from` that names `to`.
// Empty means "fine". Layers compare by order; the two engine families never include each other.
std::string order_violation(const Placed &from, const Placed &to) {
    if (!from.family.empty() && !to.family.empty() && from.family != to.family) {
        return "an " + from.family + " file includes an " + to.family +
               " file -- ipm and sqp are siblings";
    }
    if (static_cast<int>(to.layer) > static_cast<int>(from.layer)) {
        return std::string("a ") + layer_name(from.layer) + " file includes a " +
               layer_name(to.layer) + " file -- upward";
    }
    return {};
}

struct FileScan {
    std::string rel; // repo-relative, generic separators
    Placed placed;
    std::vector<IncludeSite> includes; // only the hven/ ones, path as written
};

std::string normalised(std::string p) {
    std::replace(p.begin(), p.end(), '\\', '/');
    return p;
}

// Every .h/.cpp under include/hven and src, in a fixed order. `unplaced` receives the paths no
// layer was found for.
std::vector<FileScan> scan_tree(std::vector<std::string> &unplaced) {
    std::vector<FileScan> out;
    const std::filesystem::path include_root(HVEN_INCLUDE_ROOT);
    const std::filesystem::path src_root(HVEN_SRC_ROOT);
    struct Root {
        std::filesystem::path dir;
        std::string prefix;
    };
    for (const Root &r : {Root{include_root, "include/hven/"}, Root{src_root, "src/"}}) {
        if (!std::filesystem::is_directory(r.dir)) {
            continue;
        }
        for (const auto &e : std::filesystem::recursive_directory_iterator(r.dir)) {
            const auto ext = e.path().extension();
            if (!e.is_regular_file() || (ext != ".h" && ext != ".cpp")) {
                continue;
            }
            FileScan f;
            f.rel = r.prefix + normalised(std::filesystem::relative(e.path(), r.dir).string());
            const auto placed = classify(f.rel);
            if (!placed) {
                unplaced.push_back(f.rel);
                continue;
            }
            f.placed = *placed;
            for (IncludeSite &s : scan_includes(e.path())) {
                s.path = normalised(s.path);
                if (s.path.rfind("hven/", 0) == 0) {
                    f.includes.push_back(std::move(s));
                }
            }
            out.push_back(std::move(f));
        }
    }
    std::sort(out.begin(), out.end(),
              [](const FileScan &a, const FileScan &b) { return a.rel < b.rel; });
    std::sort(unplaced.begin(), unplaced.end());
    return out;
}

// One allow-listed upward pair, file to file, with the debt that explains it. Every entry is
// re-derived from the tree, and an entry that no longer occurs fails
// EveryAllowListedPairStillOccurs, so the list can only shrink by paying the debt. The numbers
// are stable names, not a count: a paid debt leaves a gap.
struct AllowedPair {
    const char *file;     // repo-relative
    const char *included; // as written in the #include, hven/...
    const char *debt;     // why it exists and what removes it
};

constexpr AllowedPair kAllowed[] = {
    // 1 -- the fixed-variable enum is an IpmOptions field, so the model layer names the ipm header.
    {"include/hven/detail/model/fixed_variable_treatment.h", "hven/ipm/ipm_enums.h",
     "DEBT 1: the fixed-variable treatment enum is declared with IpmOptions. Removed by the "
     "model-contract unification (T8 section 6 group), which moves the enum below the model "
     "layer."},
    // (2 and 3, the IPQP/SSN engines' include of the sqp public types, were paid by moving
    // IpqpOptions, QpMode and the Ssn*Rule enums to qp/qp_options.h; the numbers are not reused.)
    // 4-5 -- Problem's warm hand-off from a result.
    {"src/model/problem.cpp", "hven/solver/solve_result.h",
     "DEBT 4: Problem's warm hand-off reads a SolveResult. Removed when the hand-off moves to the "
     "solver layer (model-contract unification)."},
    {"src/model/problem.cpp", "hven/detail/solver/result_access.h", "DEBT 5: as debt 4."},
    // 6 -- the IPQP engine emits its own trace events.
    {"src/qp/ipqp/ipqp_engine.cpp", "hven/trace/trace.h",
     "DEBT 6: the IPQP engine emits its own trace events. Removed when the engine's events go "
     "through a sink interface owned below trace/ (trace upgrade)."},
    // 7-8 -- one TU prints both engines' counters.
    {"src/solver/counters_dump.cpp", "hven/ipm/ipm_solver_types.h",
     "DEBT 7: one TU prints both engines' counters. Removed by the per-engine split of "
     "counters_dump (RS-7), after which each half sits at its engine's layer."},
    {"src/solver/counters_dump.cpp", "hven/sqp/sqp_solver_types.h", "DEBT 8: as debt 7."},
    // 9-11 -- one TU renders both engines' result messages.
    {"src/solver/result_message.cpp", "hven/ipm/ipm_options.h",
     "DEBT 9: one TU renders both engines' result messages. Removed by the per-engine split of "
     "result_message (RS-7)."},
    {"src/solver/result_message.cpp", "hven/sqp/sqp_options.h", "DEBT 10: as debt 9."},
    {"src/solver/result_message.cpp", "hven/sqp/sqp_solver_types.h", "DEBT 11: as debt 9."},
    // (12, the JSON-lines serializer's include of the funnel header, was unused and deleted.)
};

bool allowed(const std::string &file, const std::string &included) {
    for (const AllowedPair &a : kAllowed) {
        if (file == a.file && included == a.included) {
            return true;
        }
    }
    return false;
}

// "hven/x/y.h" -> "include/hven/x/y.h"
std::string repo_path_of(const std::string &included) { return "include/" + included; }

} // namespace

// THE SCAN MUST BE SHOWN TO HAVE HAPPENED -- the rule's failure mode is a vacuous pass, a wrong
// root or a renamed directory scanning nothing.
TEST(Layering, TheScanReadsTheWholeTree) {
    ASSERT_TRUE(std::filesystem::is_directory(HVEN_INCLUDE_ROOT)) << HVEN_INCLUDE_ROOT;
    ASSERT_TRUE(std::filesystem::is_directory(HVEN_SRC_ROOT)) << HVEN_SRC_ROOT;

    std::vector<std::string> unplaced;
    const std::vector<FileScan> files = scan_tree(unplaced);

    std::string report;
    for (const std::string &u : unplaced) {
        report += "\n  " + u;
    }
    EXPECT_TRUE(unplaced.empty())
        << "file(s) under include/hven or src/ whose path names no layer of the order. A new "
           "directory has to be placed in CLAUDE.md section 2's order AND in classify() above, in "
           "the same commit:"
        << report;

    // Floors, not exact counts (the core half pins its own exact counts): a scan that finds a
    // fraction of the tree has stopped looking.
    std::size_t headers = 0, sources = 0, directives = 0;
    for (const FileScan &f : files) {
        (f.rel.rfind("include/", 0) == 0 ? headers : sources) += 1;
        directives += f.includes.size();
    }
    EXPECT_GE(headers, 150U);
    EXPECT_GE(sources, 50U);
    EXPECT_GE(directives, 600U);

    // Anchors: one file per layer, and the shape of the two special rules. If one is renamed,
    // this says so out loud instead of quietly scanning less.
    struct Anchor {
        const char *rel;
        Layer layer;
        const char *family;
    };
    const Anchor anchors[] = {
        {"include/hven/core/ledger.h", Layer::kCore, ""},
        {"include/hven/detail/util/thread_pool.h", Layer::kUtil, ""},
        {"include/hven/detail/barrier/barrier_math.h", Layer::kBarrier, ""},
        {"include/hven/linear/symmetric_factor.h", Layer::kLinear, ""},
        {"include/hven/model/problem.h", Layer::kModel, ""},
        {"include/hven/qp/qp_types.h", Layer::kQpData, ""},
        {"include/hven/detail/qp/qp_problem.h", Layer::kQpData, ""},
        {"include/hven/detail/kkt/kkt_assembly.h", Layer::kKkt, ""},
        {"include/hven/detail/qp/walk/walk_engine.h", Layer::kQpEngines, ""},
        {"src/qp/ipqp/ipqp_engine.cpp", Layer::kQpEngines, ""},
        {"include/hven/warmstart/warm_start.h", Layer::kWarmstart, ""},
        {"src/solver/result_message.cpp", Layer::kSolver, ""},
        {"include/hven/ipm/ipm_solver.h", Layer::kEnginePublic, "ipm"},
        {"include/hven/sqp/sqp_options.h", Layer::kEnginePublic, "sqp"},
        {"src/trace/json_lines_trace_sink.cpp", Layer::kTrace, ""},
        {"include/hven/detail/sqp/sqp_solver_impl.h", Layer::kEngineImpl, "sqp"},
        {"src/ipm/ipm_solver.cpp", Layer::kEngineImpl, "ipm"},
        {"include/hven/hven.h", Layer::kUmbrella, ""},
    };
    for (const Anchor &a : anchors) {
        const auto it = std::find_if(files.begin(), files.end(),
                                     [&](const FileScan &f) { return f.rel == a.rel; });
        ASSERT_NE(it, files.end()) << a.rel << " was not scanned (renamed or moved?)";
        EXPECT_EQ(it->placed.layer, a.layer)
            << a.rel << " is placed at " << layer_name(it->placed.layer);
        EXPECT_EQ(it->placed.family, a.family) << a.rel;
    }

    // Every hven/ include must name a file that exists: a dangling one inside a platform #if
    // would otherwise be invisible to this scan and to every build that does not take the branch.
    // The one generated public header is written into the build tree by configure_file
    // (include/hven/core/build_config.h.in), so it is not in the scanned source tree.
    std::string dangling;
    for (const FileScan &f : files) {
        for (const IncludeSite &s : f.includes) {
            if (s.path == "hven/core/build_config.h") {
                continue;
            }
            const std::filesystem::path target =
                std::filesystem::path(HVEN_INCLUDE_ROOT).parent_path() / s.path;
            if (!std::filesystem::exists(target)) {
                dangling +=
                    "\n  " + f.rel + ":" + std::to_string(s.line) + " includes '" + s.path + "'";
            }
        }
    }
    EXPECT_TRUE(dangling.empty()) << "include(s) of a hven header that does not exist:" << dangling;
}

// THE RULE.
TEST(Layering, NoIncludeGoesUpTheLayerOrder) {
    std::vector<std::string> unplaced;
    const std::vector<FileScan> files = scan_tree(unplaced);
    ASSERT_FALSE(files.empty());

    std::string report;
    for (const FileScan &f : files) {
        for (const IncludeSite &s : f.includes) {
            const auto to = classify(repo_path_of(s.path));
            if (!to) {
                continue; // reported as a dangling or unplaced file by the scan test
            }
            const std::string why = order_violation(f.placed, *to);
            if (!why.empty() && !allowed(f.rel, s.path)) {
                report += "\n  " + f.rel + ":" + std::to_string(s.line) + ": #include <" + s.path +
                          "> -- " + why;
            }
        }
    }
    EXPECT_TRUE(report.empty())
        << "CLAUDE.md section 2's layer order is core < util < barrier < linear < model < "
           "qp-data < kkt < qp-engines < warmstart < solver < {ipm, sqp} public < trace < "
           "engine implementations. A file may include its own layer or a lower one; ipm and "
           "sqp never include each other. MOVE THE TYPE DOWN rather than the include up. Do not "
           "add to kAllowed to make this pass: an entry is a debt with a named owner, and a new "
           "one is a design decision.\nOffending include(s):"
        << report;
}

// A stale allow-list entry is itself a failure: the debt is paid, so the entry must go.
TEST(Layering, EveryAllowListedPairStillOccurs) {
    std::vector<std::string> unplaced;
    const std::vector<FileScan> files = scan_tree(unplaced);

    for (const AllowedPair &a : kAllowed) {
        const auto file = std::find_if(files.begin(), files.end(),
                                       [&](const FileScan &f) { return f.rel == a.file; });
        ASSERT_NE(file, files.end()) << "allow-listed file " << a.file << " no longer exists: "
                                     << "remove its kAllowed entry (" << a.debt << ")";
        const auto inc = std::find_if(file->includes.begin(), file->includes.end(),
                                      [&](const IncludeSite &s) { return s.path == a.included; });
        ASSERT_NE(inc, file->includes.end())
            << a.file << " no longer includes " << a.included << ": the debt is paid, remove its "
            << "kAllowed entry (" << a.debt << ")";
        // The pair must still BE a violation; one that became legal (the order was re-cut) is
        // equally stale.
        const auto to = classify(repo_path_of(a.included));
        ASSERT_TRUE(to.has_value()) << a.included;
        EXPECT_FALSE(order_violation(file->placed, *to).empty())
            << a.file << " -> " << a.included
            << " is no longer an upward pair: remove its kAllowed entry";
        EXPECT_NE(std::string(a.debt).find("DEBT"), std::string::npos)
            << a.file << " has no debt note";
    }
}

// Public headers never include detail/ (CLAUDE.md section 2). check_export_contract.sh asserts the
// same on the installed tree; this is the source-tree twin, so the failure shows before an install.
TEST(Layering, PublicHeadersDoNotIncludeDetail) {
    std::vector<std::string> unplaced;
    const std::vector<FileScan> files = scan_tree(unplaced);

    std::string report;
    std::size_t public_headers = 0;
    for (const FileScan &f : files) {
        if (f.rel.rfind("include/hven/", 0) != 0 || f.rel.rfind("include/hven/detail/", 0) == 0) {
            continue;
        }
        ++public_headers;
        for (const IncludeSite &s : f.includes) {
            if (s.path.rfind("hven/detail/", 0) == 0) {
                report +=
                    "\n  " + f.rel + ":" + std::to_string(s.line) + " includes <" + s.path + ">";
            }
        }
    }
    EXPECT_GE(public_headers, 40U) << "the public headers were not scanned";
    EXPECT_TRUE(report.empty()) << "public header(s) include a detail/ header:" << report;
}

// ---------------------------------------------------------------------------------------------
// FORWARD DECLARATIONS ARE INCLUDES IN DISGUISE. A `struct X;` (or `friend struct X;`) of a type
// whose definition sits ABOVE the declaring file in the layer order is an upward dependency that
// the include scan above cannot see. The known ones are listed here with the reason each is
// tolerated; a new unlisted one fails NoForwardDeclarationGoesUpTheLayerOrder, and a listed one
// that no longer occurs fails KnownForwardDeclarationsStillOccur. The scan reads a declaration that
// stands on a line of its own (`struct X;`, `friend class X;`), which is how clang-format lays them
// out in every file of this tree; it does not parse C++.
// ---------------------------------------------------------------------------------------------
namespace {

struct KnownForward {
    const char *file; // repo-relative
    const char *name; // the forward-declared (or befriended) type, unqualified
    const char *why;
};

constexpr KnownForward kKnownForward[] = {
    {"include/hven/warmstart/sqp_warm_start.h", "SqpResult",
     "SqpWarmStart befriends the SQP result that builds it; goes with the model-contract "
     "unification of the warm hand-off."},
    {"include/hven/solver/solve_result.h", "IpmSolverImpl",
     "SolveResult befriends the engines that fill it (friend declarations, no definition needed)."},
    {"include/hven/solver/solve_result.h", "SqpSolverImpl", "as IpmSolverImpl."},
    {"include/hven/detail/solver/result_message.h", "IpmOptions",
     "the shared message renderer takes both engines' options by reference (debt 9-11's header "
     "half; removed with the per-engine split)."},
    {"include/hven/detail/solver/result_message.h", "SqpOptions", "as IpmOptions."},
    {"include/hven/detail/solver/result_message.h", "SqpResult", "as IpmOptions."},
    {"include/hven/detail/qp/ipqp/ipqp_engine.h", "TraceSink",
     "the IPQP engine emits its own trace events (debt 6's header half; removed with it)."},
    {"include/hven/detail/qp/ipqp/ipqp_engine.h", "IpqpIterTraceEvent", "as TraceSink."},
    {"include/hven/detail/qp/ipqp/ipqp_engine.h", "IpqpRegTraceEvent", "as TraceSink."},
    {"include/hven/detail/qp/ipqp/ipqp_engine.h", "IpqpRestartTraceEvent", "as TraceSink."},
    {"include/hven/detail/qp/ipqp/ipqp_engine.h", "IpqpCertifyTraceEvent", "as TraceSink."},
    {"include/hven/detail/qp/ipqp/ipqp_engine.h", "IpqpEscapeTraceEvent", "as TraceSink."},
    // The public solver handles are opaque (pimpl): the header names the implementation class and
    // its accessor, which are defined in the engine implementation layer, and the trace sink the
    // handle attaches. By design; they go only with the handle design itself.
    {"include/hven/ipm/ipm_solver.h", "TraceSink", "the handle attaches a sink (opaque handle)."},
    {"include/hven/ipm/ipm_solver.h", "IpmSolverImpl", "the handle's pimpl (opaque handle)."},
    {"include/hven/ipm/ipm_solver.h", "IpmSolverAccess", "the handle's befriended accessor."},
    {"include/hven/sqp/sqp_solver.h", "TraceSink", "the handle attaches a sink (opaque handle)."},
    {"include/hven/sqp/sqp_solver.h", "SqpSolverImpl", "the handle's pimpl (opaque handle)."},
    {"include/hven/sqp/sqp_solver.h", "SqpSolverAccess", "the handle's befriended accessor."},
    {"include/hven/sqp/sqp_options.h", "GlobalizationStrategy",
     "SqpAdvancedOptions::make_strategy returns the strategy interface, whose definition is "
     "internal (the header says so); a caller that implements one includes the internal header."},
    {"include/hven/detail/model/problem_access.h", "SolveResult",
     "the engines' door into Problem names the result type it hands a warm start from (debt 4-5's "
     "header half)."},
};

struct ForwardDecl {
    std::string file;
    int line;
    std::string name;
};

// type name -> the headers that DEFINE it (a class/struct head with no ';' on its line).
using Definitions = std::vector<std::pair<std::string, std::string>>;

void scan_forward_decls(std::vector<ForwardDecl> &decls, Definitions &defs) {
    static const std::regex fwd(
        R"(^\s*(?:friend\s+)?(?:class|struct)\s+(?:\w+::)*(\w+)\s*;\s*(?://.*)?$)");
    static const std::regex def(
        R"(^\s*(?:template\s*<[^>]*>\s*)?(?:class|struct)\s+(\w+)\b[^;]*$)");
    const std::filesystem::path include_root(HVEN_INCLUDE_ROOT);
    const std::filesystem::path src_root(HVEN_SRC_ROOT);
    struct Root {
        std::filesystem::path dir;
        std::string prefix;
    };
    for (const Root &r : {Root{include_root, "include/hven/"}, Root{src_root, "src/"}}) {
        for (const auto &e : std::filesystem::recursive_directory_iterator(r.dir)) {
            const auto ext = e.path().extension();
            if (!e.is_regular_file() || (ext != ".h" && ext != ".cpp")) {
                continue;
            }
            const std::string rel =
                r.prefix + normalised(std::filesystem::relative(e.path(), r.dir).string());
            std::ifstream in(e.path());
            std::string line;
            int lineno = 0;
            std::smatch m;
            while (std::getline(in, line)) {
                ++lineno;
                if (std::regex_match(line, m, fwd)) {
                    decls.push_back({rel, lineno, m[1].str()});
                } else if (ext == ".h" && std::regex_match(line, m, def)) {
                    defs.emplace_back(m[1].str(), rel);
                }
            }
        }
    }
}

// The forward declarations whose every definition sits above the declaring file.
std::vector<ForwardDecl> upward_forward_decls() {
    std::vector<ForwardDecl> decls;
    Definitions defs;
    scan_forward_decls(decls, defs);
    std::vector<ForwardDecl> up;
    for (const ForwardDecl &d : decls) {
        const auto from = classify(d.file);
        if (!from) {
            continue;
        }
        std::size_t n_defs = 0, n_up = 0;
        for (const auto &[name, file] : defs) {
            if (name != d.name || file == d.file) {
                continue;
            }
            const auto to = classify(file);
            if (!to) {
                continue;
            }
            ++n_defs;
            n_up += order_violation(*from, *to).empty() ? 0U : 1U;
        }
        if (n_defs > 0 && n_up == n_defs) {
            up.push_back(d);
        }
    }
    return up;
}

bool known_forward(const std::string &file, const std::string &name) {
    for (const KnownForward &k : kKnownForward) {
        if (file == k.file && name == k.name) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST(Layering, NoForwardDeclarationGoesUpTheLayerOrder) {
    const std::vector<ForwardDecl> up = upward_forward_decls();
    std::string report;
    for (const ForwardDecl &d : up) {
        if (!known_forward(d.file, d.name)) {
            report += "\n  " + d.file + ":" + std::to_string(d.line) + ": forward-declares '" +
                      d.name + "', which is defined in a layer above this file";
        }
    }
    EXPECT_TRUE(report.empty())
        << "A forward declaration of a higher layer's type is an upward dependency. Move the "
           "type down, or (a design decision with a named owner) list it in kKnownForward:"
        << report;
}

TEST(Layering, KnownForwardDeclarationsStillOccur) {
    const std::vector<ForwardDecl> up = upward_forward_decls();
    // The check must have looked: a vacuous pass is its failure mode.
    EXPECT_GE(up.size(), std::size(kKnownForward) / 2);
    for (const KnownForward &k : kKnownForward) {
        const bool found = std::any_of(up.begin(), up.end(), [&](const ForwardDecl &d) {
            return d.file == k.file && d.name == k.name;
        });
        EXPECT_TRUE(found) << k.file << " no longer forward-declares '" << k.name
                           << "' upward: remove its kKnownForward entry (" << k.why << ")";
    }
}
