#!/usr/bin/env python3
"""I18 mutations: apply one mutation by exact anchor (each anchor must occur exactly once), or check every anchor.
usage: mutate_i18.py <worktree> check | <worktree> apply <ID>     (the tree is restored by `git checkout` of the touched file)"""
import os
import sys

EQP = "include/hven/detail/qp/walk/eqp_solve.h"
BEQP = "include/hven/detail/qp/walk/bordered_eqp.h"
LOADER = "tests/qp/support/qp_dump.h"
M = {
    # the eliminated path's accuracy stop never runs
    "A1": (EQP, "            if (opts.independent_working_set) {\n                accuracy_steps += refine_to_accuracy(y_model, y);",
                "            if (false) {\n                accuracy_steps += refine_to_accuracy(y_model, y);"),
    # the border path's trigger never fires
    "A2": (BEQP, "    bool exceeds_radius = false; // THE ACCURACY STOP's trigger\n    if (resolution != nullptr) {",
                 "    bool exceeds_radius = false; // THE ACCURACY STOP's trigger\n    if (false) {"),
    # the verdict always reads resolved (both paths stop at once)
    "A3": (EQP, "    return e <= kAccuracyStopRatio * std::max(p, step_tol);", "    return true || e <= kAccuracyStopRatio * std::max(p, step_tol);"),
    # the keep rule loses its contraction (strict decrease alone)
    "A4": (EQP, "    return res_new < res_old && res_new <= kAccuracyContraction * res_old &&",
                "    return res_new < res_old && (true || res_new <= kAccuracyContraction * res_old) &&"),
    # the keep rule loses its correction half
    "A5": (EQP, "           corr_new <= kAccuracyContraction * corr_old;", "           (true || corr_new <= kAccuracyContraction * corr_old);"),
    # the border path keeps further steps without the accuracy keep rule
    "A6": (BEQP, "        if (exceeds_radius &&\n            !accuracy_step_contracts(", "        if (false &&\n            !accuracy_step_contracts("),
    # the eliminated trigger fires on every solve (no radius condition)
    "A7": (EQP, "                exceeds_radius || std::abs(yy(k) - before(k)) > opts.trust_region_radius;",
                "                true || std::abs(yy(k) - before(k)) > opts.trust_region_radius;"),
    # the loader ignores hkc= again
    # the border trigger fires on every bordered solve (no radius condition)
    "A8": (BEQP, "                    exceeds_radius || std::abs(best(i) - sol(i)) > opts.trust_region_radius;",
                 "                    true || std::abs(best(i) - sol(i)) > opts.trust_region_radius;"),
    "L1": (LOADER, '                    d.opts.hessian_known_convex = tok == "hkc=1";', '                    (void)tok;'),
}


def main():
    wt = sys.argv[1]
    if sys.argv[2] == "check":
        bad = 0
        for k, (f, old, _new) in sorted(M.items()):
            n = open(os.path.join(wt, f)).read().count(old)
            print(f"{k} {f} count={n}")
            bad += n != 1
        print("ANCHORS_OK" if not bad else "ANCHORS_BAD")
        return 1 if bad else 0
    k = sys.argv[3]
    f, old, new = M[k]
    p = os.path.join(wt, f)
    s = open(p).read()
    if s.count(old) != 1:
        print(f"ANCHOR_COUNT {k} {s.count(old)}")
        return 1
    open(p, "w").write(s.replace(old, new))
    print(f"APPLIED {k} {f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
