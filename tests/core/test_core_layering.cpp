// test_core_layering.cpp -- the regression net for CLAUDE.md section 2's tier
// order, checked where it is cheapest to check: the include directives of the
// public headers in include/hven/core/.
//
// WHAT IT ASSERTS. `core/` is the BOTTOM tier of the repository map. Every other
// tier -- linear/, model/, kkt/, interior/, qp/, globalization/, warmstart/,
// drivers/, and the detail/ bodies of all of them -- is allowed to depend on
// core/, and core/ is allowed to depend on none of them. So a header in
// include/hven/core/ may include: standard-library headers, third-party headers
// hven vendors (Eigen, fmt), and OTHER core/ headers. Nothing else under hven/.
//
// WHY IT EXISTS, stated because a rule with no failure story is decoration. Before
// M3 phase-C S2, core/ledger.h included drivers/sqp_types.h and, through it,
// detail/globalization/ and detail/warmstart/ -- core/ depending upward on
// drivers/. S2 eliminated that by moving the counters, the two status enums and
// StartLevel into core/ homes. Nothing then stopped it coming back: the property
// was asserted once, by hand, with `clang++ -H` in a report, and a single
// `#include <hven/drivers/sqp_types.h>` added to a core/ header would compile
// cleanly, pass every other test, and silently restore the inversion while the
// plan record still claimed it was gone. This test is the thing that fails.
//
// TWO CONCRETE ROUTES BACK IN, both live:
//   * core/ledger.h's private section used to carry a note asserting the very
//     dependency S2 deleted. It has been corrected and now says the opposite,
//     but a reader who wants a status-rendering helper is exactly the reader who
//     reaches for drivers/sqp_types.h.
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
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#ifndef HVEN_CORE_INCLUDE_DIR
#error "HVEN_CORE_INCLUDE_DIR must be defined by tests/CMakeLists.txt"
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

std::vector<std::filesystem::path> core_headers() {
    std::vector<std::filesystem::path> headers;
    const std::filesystem::path dir(HVEN_CORE_INCLUDE_DIR);
    if (!std::filesystem::is_directory(dir)) {
        return headers;
    }
    // Recursive so that a future include/hven/core/<subdir>/ is covered the day
    // it appears rather than the day someone remembers this file.
    for (const auto &entry : std::filesystem::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".h") {
            headers.push_back(entry.path());
        }
    }
    std::sort(headers.begin(), headers.end());
    return headers;
}

} // namespace

// THE SCAN MUST BE SHOWN TO HAVE HAPPENED, and this test exists before the rule
// itself because the rule's failure mode is a VACUOUS PASS: a wrong
// HVEN_CORE_INCLUDE_DIR, a renamed directory or a build that stopped passing the
// definition would make the scan find zero files and the invariant test below
// would report success while checking nothing. So the directory is asserted to
// exist, to be non-empty, and to contain the specific headers this rule is about.
TEST(CoreLayering, TheScanActuallyReadsTheCoreHeaders) {
    const std::filesystem::path dir(HVEN_CORE_INCLUDE_DIR);
    ASSERT_TRUE(std::filesystem::is_directory(dir))
        << "HVEN_CORE_INCLUDE_DIR does not name a directory: " << dir.string()
        << " -- the layering check below would pass vacuously.";

    const std::vector<std::filesystem::path> headers = core_headers();
    ASSERT_FALSE(headers.empty()) << "no .h files found under " << dir.string();

    // Anchors: the header the inversion ran through, and the one carrying the
    // Index re-declaration a future simplification is most likely to "fix" by
    // including qp/qp_types.h. If either is ever renamed, this test is the place
    // that says so out loud instead of quietly scanning less.
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
                violations.push_back("include/hven/core/" + site.file + ":" +
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
           "above it. A core/ header including a drivers/, qp/, model/, kkt/, interior/, "
           "linear/, globalization/, warmstart/ or detail/ header reopens the include-graph "
           "inversion M3 phase-C S2 eliminated.\n"
           "Offending include directive(s):"
        << report
        << "\n\nIf the type you need lives above core/, MOVE THE TYPE DOWN rather than the "
           "include up -- that is what S2 did for the counters, the status enums and "
           "StartLevel. If a deliberate ruling changes the tier order itself, change this "
           "test with it, in the same commit, and say so.";
}
