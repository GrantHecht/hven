#!/usr/bin/env python3
"""WA-1 r5: apply (or restore) one declared mutation of the r5 code. usage: mutate_r5.py <id> | restore.
Ids and dispositions are declared in ../DECLARATION-WA1-r5.md; each anchor must occur exactly once."""
import subprocess
import sys

E = "src/qp/walk/walk_engine.cpp"
H = "include/hven/detail/qp/walk/walk_engine.h"

MUT = {
    # id: (file, old, new)
    "N1": (E, "                } else if (eff_opts.dual_regularization > 0.0 &&\n                           verdict != detail::InertiaVerdict::kSuspect) {",
              "                } else if (false && eff_opts.dual_regularization > 0.0 &&\n                           verdict != detail::InertiaVerdict::kSuspect) {"),
    "N2": (E, "                        Vec d = -p;", "                        Vec d = p;"),
    "N3": (H, "    const double xmag = x.lpNorm<Eigen::Infinity>();\n    const auto within = [&](double r, double b, double norm1) {",
              "    return true;\n    const double xmag = x.lpNorm<Eigen::Infinity>();\n    const auto within = [&](double r, double b, double norm1) {"),
    "N4": (H, "        if (!within(qp.Ai.row(j).dot(x) - qp.bi(j), qp.bi(j), ai_row_norm1(j))) {",
              "        if (!within(std::abs(qp.Ai.row(j).dot(x) - qp.bi(j)), qp.bi(j), ai_row_norm1(j))) {"),
    "N5": (H, "        if (!within(qp.Ai.row(j).dot(x) - qp.bi(j), qp.bi(j), ai_row_norm1(j))) {",
              "        if (false && !within(qp.Ai.row(j).dot(x) - qp.bi(j), qp.bi(j), ai_row_norm1(j))) {"),
    "N6": (E, "} else if (!footprint_rungs_off && counters.dual_regularization_decreases <",
              "} else if (false && counters.dual_regularization_decreases <"),
    "N7": (E, "                        if (pu_descends) {", "                        if (false && pu_descends) {"),
    "N8": (E, "                               !(along_p.slope > along_p.tol)) {", "                               false) {"),
    "N9": (H, "    m.slope = gx.dot(d) + rows / mu;", "    m.slope = gx.dot(d) - rows / mu;"),
    "N10": (H, "    m.tol = kEngineDenomTol * (gx.norm() * d.norm() + rows_abs / mu);", "    m.tol = kEngineDenomTol * (gx.norm() * d.norm());"),
    "N11": (H, "constexpr double kFootprintRowBudget = 0.5;", "constexpr double kFootprintRowBudget = 5.0;"),
    "N12": (E, "                        } else if (!ride_moves &&\n", "                        } else if (false && !ride_moves &&\n"),
    "N13": (E, "                            landing = &eqp.x_regularized;\n", ""),
    "N14": (E, "                           verdict != detail::InertiaVerdict::kSuspect) {", "                           true) {"),
    "N15": (E, "            footprint_rung_from = 0.0;\n            if (verdict == detail::InertiaVerdict::kSuspect) {",
               "            footprint_rung_from = 0.0;\n            if (false && verdict == detail::InertiaVerdict::kSuspect) {"),
    "N16": (E, "} else if (!footprint_rungs_off && counters.dual_regularization_decreases <",
               "} else if (counters.dual_regularization_decreases <"),
}


def sub1(path, old, new):
    s = open(path).read()
    if s.count(old) != 1:
        sys.exit(f"MUTATION_DOES_NOT_APPLY {path} (count {s.count(old)}): {old[:70]!r}")
    open(path, "w").write(s.replace(old, new))


arg = sys.argv[1]
if arg == "restore":
    subprocess.check_call(["git", "checkout", "--", E, H])
    print("RESTORED")
    sys.exit(0)
if arg not in MUT:
    sys.exit("UNKNOWN")
sub1(*MUT[arg])
print(f"APPLIED {arg}")
