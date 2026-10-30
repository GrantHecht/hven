// The T5 generator: prints the OFF fingerprint of the dependent-networks QP under
// each qp_mode, compiled against E1's pre-merge base 8fdeed3e.
#include <cstdio>

#define HVEN_E1B_GOLDEN_AT_BASE 1
#include "support/e1b_off_golden.h"

int main() {
    using hven::solvers::QpMode;
    const std::pair<QpMode, const char *> modes[] = {
        {QpMode::kWalk, "kWalk"}, {QpMode::kSsn, "kSsn"}, {QpMode::kIpm, "kIpm"}};
    for (const auto &[mode, name] : modes) {
        for (const std::string &line : hven::solvers::e1b_golden::off_fingerprint(mode)) {
            std::printf("%s %s\n", name, line.c_str());
        }
    }
    return 0;
}
