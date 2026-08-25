// Identity + partition-count discriminator probe for the M4 attribution session.
// Per problem and requested partition count: objective to full precision,
// iteration count, convergence flag (from the first solve), and the median and
// minimum wall time of R whole solves (fresh phase per solve, like the bench).
#include "../../bench/cpp/bench_phases.h"
#include <tycho/solvers.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

template <class Make> static void run(const char *name, Make make, int partitions, int reps) {
    std::vector<double> t;
    double obj = 0; int iter = 0, flag = 0, status = 0;
    for (int r = 0; r < reps; r++) {
        auto phase = make();
        if (partitions > 0) phase->set_num_partitions(partitions);
        auto t0 = std::chrono::steady_clock::now();
        auto st = phase->solve_optimize();
        auto t1 = std::chrono::steady_clock::now();
        t.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
        if (r == 0) {
            const auto &res = phase->optimizer_->result();
            obj = res.obj_val_; iter = res.iter_num_;
            flag = static_cast<int>(res.converge_flag_); status = static_cast<int>(st);
        }
    }
    std::sort(t.begin(), t.end());
    std::printf("%s partitions=%d obj=%.17g iter=%d flag=%d status=%d median_ms=%.4f min_ms=%.4f reps=%d\n",
                name, partitions, obj, iter, flag, status, t[t.size() / 2], t.front(), reps);
}

int main(int argc, char **argv) {
    const int partitions = argc > 1 ? std::atoi(argv[1]) : 1;
    const int reps = argc > 2 ? std::atoi(argv[2]) : 5;
    run("Brach_32seg", [] { return make_brach_phase(32, -1); }, partitions, reps);
    run("PolarLT_128seg", [] { return make_polar_lt_phase(128, -1); }, partitions, reps);
    return 0;
}
