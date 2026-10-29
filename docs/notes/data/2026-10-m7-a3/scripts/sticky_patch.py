#!/usr/bin/env python3
"""Probe P (design page s3.3, DESCRIPTIVE): the sticky trial, patched into a SCRATCH copy of the
measured head's src/qp/ipqp_engine.cpp. Never committed as code; this file is the patch.
Usage: sticky_patch.py SRC_TREE pu|pc
  pu -- T9's UNCONDITIONAL form: the licensed first trial is max(floor, rho_dem_last), not /3.
  pc -- T4b's CONDITIONAL form: as pu only on an iteration following one whose memory trial was
        refused (a reclimb was charged); otherwise the head's /3."""
import sys

tree, form = sys.argv[1], sys.argv[2]
p = f"{tree}/src/qp/ipqp_engine.cpp"
s = open(p).read()


def rep(old, new):
    global s
    assert s.count(old) == 1, old
    s = s.replace(old, new)


TRIAL = "            trial = std::max(iopts.ipqp_reg_floor, rho_dem_last / detail::kIpqpLadderDown);"
if form == "pu":
    rep(TRIAL, "            trial = std::max(iopts.ipqp_reg_floor, rho_dem_last); // PROBE P-U")
elif form == "pc":
    rep(TRIAL, "            trial = std::max(iopts.ipqp_reg_floor,\n"
               "                             probe_last_reclimbed ? rho_dem_last\n"
               "                                                  : rho_dem_last / detail::kIpqpLadderDown); // PROBE P-C")
    rep("    int consec_modified = 0;\n",
        "    int consec_modified = 0;\n    bool probe_last_reclimbed = false; // PROBE P-C\n")
    rep("        const InertiaRead read = factorize_with_ladder(rho_sched, rho_dem, delta);\n\n"
        "        // I4 + N1:",
        "        const Index probe_reclimbs_before = out.counters.ipqp_ladder_reclimbs; // PROBE P-C\n"
        "        const InertiaRead read = factorize_with_ladder(rho_sched, rho_dem, delta);\n"
        "        probe_last_reclimbed = out.counters.ipqp_ladder_reclimbs > probe_reclimbs_before;\n\n"
        "        // I4 + N1:")
else:
    raise SystemExit("form must be pu or pc")
open(p, "w").write(s)
print(f"patched {p} with {form}")
