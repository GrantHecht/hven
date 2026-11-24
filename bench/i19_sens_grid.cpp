// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// hven_sqp_i19_grid -- the I19-2a SENS-P grid runner (bench/i19_sens_grid.h has the cell file,
// the declared --set keys and the CSV columns).
//
//     hven_sqp_i19_grid --cells <tsv> [--set key=value ...] --out <csv>
//
// One cold SQP solve per cell, in file order, one CSV row each. Wall-clock is not written. A cell
// whose solve throws is a row with status `error` and the message on stderr; the exit code is 1
// when any cell errored, 2 on a usage or cell-file error.

#include "i19_sens_grid.h"

#include <cstdio>
#include <exception>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int usage(const char *why) {
    std::fprintf(stderr,
                 "hven_sqp_i19_grid: %s\nusage: hven_sqp_i19_grid --cells <tsv> [--set k=v ...] "
                 "--out <csv>\n",
                 why);
    return 2;
}

} // namespace

int main(int argc, char **argv) {
    using namespace hven::bench::i19;
    std::string cells_path;
    std::string out_path;
    std::vector<std::string> sets;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if ((a == "--cells" || a == "--out" || a == "--set") && i + 1 >= argc) {
            return usage("an option is missing its value");
        }
        if (a == "--cells") {
            cells_path = argv[++i];
        } else if (a == "--out") {
            out_path = argv[++i];
        } else if (a == "--set") {
            sets.emplace_back(argv[++i]);
        } else {
            return usage(("unknown argument " + a).c_str());
        }
    }
    if (cells_path.empty() || out_path.empty()) {
        return usage("--cells and --out are required");
    }
    std::vector<SensCell> cells;
    try {
        std::ifstream in(cells_path);
        if (!in) {
            return usage(("cannot open " + cells_path).c_str());
        }
        cells = parse_cells(in);
        for (const SensCell &c : cells) {
            (void)cell_options(c, sets); // refuse a bad --set before any solve
            (void)make_model(c);
        }
    } catch (const std::exception &e) {
        return usage(e.what());
    }
    std::ofstream out(out_path);
    if (!out) {
        return usage(("cannot write " + out_path).c_str());
    }
    bool header = false;
    bool any_error = false;
    std::vector<std::string> pending; // rows of cells that threw before the header was known
    for (const SensCell &c : cells) {
        try {
            const hven::SqpResult r = run_cell(c, sets);
            if (!header) {
                out << csv_header(r) << "\n";
                header = true;
                for (const std::string &p : pending) {
                    out << p << "\n";
                }
                pending.clear();
            }
            out << csv_row(c, sets, r) << "\n";
        } catch (const std::exception &e) {
            any_error = true;
            std::fprintf(stderr, "hven_sqp_i19_grid: cell %s threw: %s\n", c.cell_id.c_str(),
                         e.what());
            const std::string row = c.cell_id + "," + c.set + "," + c.problem + "," + c.base + "," +
                                    join_sets(sets) + ",error";
            if (header) {
                out << row << "\n";
            } else {
                pending.push_back(row);
            }
        }
    }
    if (!header) {
        for (const std::string &p : pending) {
            out << p << "\n";
        }
    }
    out.flush();
    return any_error ? 1 : 0;
}
