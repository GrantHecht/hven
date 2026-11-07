// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// M8 AP-2 B7 TRAIN COUNT: for every corpus cell (bench/corpus_cells.h all_cells(), the 57
// census cells, of which the 27 replay cells are a subset), the declared variable-bound
// values the default absent-bound threshold (CommonOptions{}.infinite_bound) reads as
// ABSENT, and the declared true infinities. Every QP mode (walk, SSN, IPQP, the degeneracy
// arm) and the interior-point engine read the same declared box, so one count per cell
// serves every mode. The model is built exactly as the corpus builds it (make_model).
//
// usage: count_absent <out.txt>

#include <cmath>
#include <cstdio>
#include <limits>

#include <hven/drivers/common_options.h>

#include "corpus_cells.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: count_absent <out.txt>\n");
        return 2;
    }
    std::FILE *out = std::fopen(argv[1], "w");
    if (out == nullptr) {
        return 2;
    }
    const double thr = hven::solvers::CommonOptions{}.infinite_bound;
    std::fprintf(out,
                 "# B7 TRAIN count: declared variable bounds at or beyond infinite_bound = %.17g\n",
                 thr);
    std::fprintf(out, "# columns: cell_id,n,absent_lower,absent_upper,absent_total,inf_lower,"
                      "inf_upper,finite_real_lower,finite_real_upper\n");
    long long cells = 0, cells_with_absent = 0, absent_sum = 0;
    for (const hven::solvers::corpus::CorpusCell &cell : hven::solvers::corpus::all_cells()) {
        const hven::solvers::corpus::F7CollocationChain model =
            hven::solvers::corpus::detail::make_model(cell);
        const hven::Vec &lo = model.lower();
        const hven::Vec &up = model.upper();
        long long al = 0, au = 0, il = 0, iu = 0, rl = 0, ru = 0;
        for (hven::Index i = 0; i < lo.size(); ++i) {
            if (std::isinf(lo[i])) {
                ++il;
            } else if (lo[i] <= -thr) {
                ++al;
            } else {
                ++rl;
            }
            if (std::isinf(up[i])) {
                ++iu;
            } else if (up[i] >= thr) {
                ++au;
            } else {
                ++ru;
            }
        }
        std::fprintf(out, "%s,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld\n", cell.id,
                     static_cast<long long>(lo.size()), al, au, al + au, il, iu, rl, ru);
        ++cells;
        cells_with_absent += (al + au) > 0 ? 1 : 0;
        absent_sum += al + au;
    }
    std::fprintf(out, "# cells %lld, with an absent bound %lld, absent values in all %lld\n", cells,
                 cells_with_absent, absent_sum);
    std::fclose(out);
    std::printf("cells %lld, with an absent bound %lld, absent values in all %lld\n", cells,
                cells_with_absent, absent_sum);
    return 0;
}
