#!/usr/bin/env python3
"""WA-1 r7: apply (or restore) one declared mutation of the r7 code. usage: mutate_r7.py <id> | restore.
Ids and dispositions are declared in ../DECLARATION-WA1-r7.md; each anchor must occur exactly once.
N1-N15 are r5's (mutate_r5.py) re-anchored to the r7 code (N6 on the footprint rungs' own cap); N7 and N13 are retired
with the kOk regularized step, N16 with footprint_rungs_off (at kMaxFootprintRungs = 1 the cap ends rungs after a revert)."""
import subprocess
import sys

E = "src/qp/walk/walk_engine.cpp"
H = "include/hven/detail/qp/walk/walk_engine.h"

MUT = {
    # id: (file, old, new)
    "N1": (E, "                } else if (eff_opts.dual_regularization > 0.0 &&\n                           verdict != detail::InertiaVerdict::kSuspect) {",
              "                } else if (false && eff_opts.dual_regularization > 0.0 &&\n                           verdict != detail::InertiaVerdict::kSuspect) {"),
    "N2": (E, "                        Vec d = -p;", "                        Vec d = p;"),
    "N3": (H, "    const double xmag = a.x.lpNorm<Eigen::Infinity>();\n    const auto within = [&](double r, double b, double norm1) {",
              "    return true;\n    const double xmag = a.x.lpNorm<Eigen::Infinity>();\n    const auto within = [&](double r, double b, double norm1) {"),
    "N4": (H, "        if (!within(qp.Ai.row(j).dot(a.x) - qp.bi(j), qp.bi(j), a.ai_row_norm1(j))) {",
              "        if (!within(std::abs(qp.Ai.row(j).dot(a.x) - qp.bi(j)), qp.bi(j), a.ai_row_norm1(j))) {"),
    "N5": (H, "        if (!within(qp.Ai.row(j).dot(a.x) - qp.bi(j), qp.bi(j), a.ai_row_norm1(j))) {",
              "        if (false && !within(qp.Ai.row(j).dot(a.x) - qp.bi(j), qp.bi(j), a.ai_row_norm1(j))) {"),
    "N6": (E, "} else if (counters.ascent_footprint_rungs < detail::kMaxFootprintRungs) {",
              "} else if (false && counters.ascent_footprint_rungs < detail::kMaxFootprintRungs) {"),
    "N8": (E, "                               !(along_p.slope > along_p.tol)) {", "                               false) {"),
    "N9": (H, "    m.slope = a.gx.dot(a.d) + rows / a.mu;", "    m.slope = a.gx.dot(a.d) - rows / a.mu;"),
    "N10": (H, "    m.tol = kEngineDenomTol * (a.gx.norm() * a.d.norm() + rows_abs / a.mu);", "    m.tol = kEngineDenomTol * (a.gx.norm() * a.d.norm());"),
    "N11": (H, "constexpr double kFootprintRowBudget = 0.5;", "constexpr double kFootprintRowBudget = 5.0;"),
    "N12": (E, "                        } else if (!ride_moves &&\n", "                        } else if (false && !ride_moves &&\n"),
    "N14": (E, "                           verdict != detail::InertiaVerdict::kSuspect) {", "                           true) {"),
    "N15": (E, "            footprint_rung_from = 0.0;\n            if (verdict == detail::InertiaVerdict::kSuspect) {",
               "            footprint_rung_from = 0.0;\n            if (false && verdict == detail::InertiaVerdict::kSuspect) {"),
    # r7 (rider 2)
    "N17": (E, "            if (footprint_ride_dir.size() > 0 && ride_blocked && !passed.empty() &&",
               "            if (false && footprint_ride_dir.size() > 0 && ride_blocked && !passed.empty() &&"),
    "N18": (E, "                        if (blocked && ride_moves && !footprint_ride_refused_in &&",
               "                        if (blocked && ride_moves &&"),
    "N19": (E, "                        // kOk, the merit rising along p: nothing to take (section 4c).\n                        ++counters.ascent_footprint_declines;\n",
               "                        // kOk, the merit rising along p: nothing to take (section 4c).\n"),
    "N20": (E, "                            if (!last_drop.active) {\n                                last_drop = dropped; // the pass is solved again\n                            }\n", ""),
    "N21": (E, "            standstill_steps = standstill_steps_in;\n            stagnation_idle = stagnation_idle_in;\n            stagnation_read = stagnation_read_in;\n            gray_hold_next = gray_hold_prev;\n            gray_return_pending = gray_return_pending_in;\n", ""),
    "N23": (E, "counters.ascent_footprint_rungs < detail::kMaxFootprintRungs) {",
               "counters.ascent_footprint_rungs < 2 * detail::kMaxFootprintRungs) {"),
    "N22": (E, "                --counters.ascent_footprint_rides;\n                --counters.ascent_rides;\n", ""),
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
