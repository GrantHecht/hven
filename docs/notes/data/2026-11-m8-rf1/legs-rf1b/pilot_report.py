#!/usr/bin/env python3
"""RF-1 gate A+B2 leg P: before (C) / after (AB2) per pilot cell. usage: pilot_report.py C_OUT AB2_OUT"""
import os, sys, glob
def load(f):
    r = {}
    for line in open(f, errors="replace"):
        if "=" in line:
            k, v = line.rstrip("\n").split("=", 1); r.setdefault(k, v)
    return r
c_dir, n_dir = sys.argv[1:3]
K = ("qp_retry_rung_shrink", "qp_retry_rung_cold", "qp_retry_rung_algebra", "qp_retry_rung_regularization",
     "qp_retry_replays", "qp_failure_box_projections", "trust_region_floor_exits", "restoration_entries",
     "restoration_reduction_resumes", "elastic_activations", "soc_qp_iteration_limits")
for f in sorted(glob.glob(os.path.join(n_dir, "cells", "*.out"))):
    b = os.path.basename(f); c = os.path.join(c_dir, "cells", b)
    n = load(f); o = load(c) if os.path.exists(c) else {}
    ctr = " ".join(f"{k.replace('qp_retry_rung_','R:')}={n.get('side.'+k)}" for k in K if n.get('side.' + k) not in (None, "0"))
    print(f"{b[:-4]} | {n.get('label')} | C {o.get('status')} it={o.get('iterations')} fact={o.get('factorizations')} f={o.get('objective')} "
          f"rc={o.get('rc')} | AB2 {n.get('status')} it={n.get('iterations')} fact={n.get('factorizations')} f={n.get('objective')} "
          f"stat={n.get('kkt_stationarity')} rc={n.get('rc')} | {ctr}")
