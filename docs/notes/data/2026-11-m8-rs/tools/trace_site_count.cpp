// RS-0 instrument: the trace-off-path scanner's site count, computed with the SAME std::regex and the
// SAME stem exclusions as tests/drivers/test_trace_off_path.cpp (the regex is copied verbatim).
// usage: trace_site_count <src-root> <dir> [<dir>...]   (directories scanned recursively, *.cpp)
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>
int main(int argc, char **argv) {
    if (argc < 3) return 2;
    const std::filesystem::path root(argv[1]);
    static const std::regex re(
        R"(\b[A-Za-z]+TraceEvent\s*(\{|[a-z_]+\s*[;={(])|\bemit_(iter|message|trace_[a-z_]+)\s*\(|\bmake_iteration_record\s*\(|->\s*on_(iter|message|solve_begin|solve_end|qp_mode|ipqp_[a-z]+|fallback_verdict|ipm_[a-z_]+)\s*\()");
    std::vector<std::filesystem::path> files;
    for (int i = 2; i < argc; ++i)
        for (const auto &e : std::filesystem::recursive_directory_iterator(root / argv[i]))
            if (e.is_regular_file() && e.path().extension() == ".cpp") files.push_back(e.path());
    std::sort(files.begin(), files.end());
    std::size_t sites = 0, nfiles = 0;
    for (const auto &f : files) {
        const std::string stem = f.stem().string();
        if (stem == "trace_writer" || stem == "console_trace_sink" || stem == "trace") continue;
        std::ifstream in(f);
        std::stringstream ss;
        ss << in.rdbuf();
        const std::string text = ss.str();
        const std::size_t n = static_cast<std::size_t>(
            std::distance(std::sregex_iterator(text.begin(), text.end(), re), std::sregex_iterator()));
        sites += n;
        ++nfiles;
        std::cout << n << "\t" << std::filesystem::relative(f, root).generic_string() << "\n";
    }
    std::cout << "TOTAL sites " << sites << " files " << nfiles << "\n";
}
