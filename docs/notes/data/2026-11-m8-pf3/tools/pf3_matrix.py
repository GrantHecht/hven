#!/usr/bin/env python3
"""Compact attribution matrix (solver-only corpus shares).  usage: pf3_matrix.py <dirs comma> -> markdown on stdout"""
import glob, json, os, sys, collections
cells = []
for d in sys.argv[1].split(","):
    for f in sorted(glob.glob(d + "/*.json")):
        c = json.load(open(f))
        if "error" in c: continue
        comp = dict(c["components"]); un = comp.pop("unattributed.truncated_unwind", 0); moved = 0.0
        for fam, amt in c.get("blas_family_truncated", {}).items():
            dd = c.get("blas_family_attributed", {}).get(fam, {}); z = sum(dd.values())
            if z > 0:
                for k, v in dd.items(): comp[k] = comp.get(k, 0) + amt * v / z
                moved += amt
        if un - moved > 1e-9 * max(un, 1): comp["unattributed.unresolved"] = comp.get("unattributed.unresolved", 0) + un - moved
        c["comp"] = comp
        c["set"] = "sd" if os.path.basename(d.rstrip("/")) == "cells-sd" else ("hs" if c["problem"].startswith("hs") else "nlp35")
        c["eng"] = c["lane"][5:]
        cells.append(c)
groups = [("SUITE (full cells)", lambda c: not c.get("truncated"))]
for e in ("ipm", "sqp"):
    groups.append((f"{e} (full)", lambda c, e=e: c["eng"] == e and not c.get("truncated")))
    for s in ("hs", "nlp35", "sd"):
        groups.append((f"{e} / {s}", lambda c, e=e, s=s: c["eng"] == e and c["set"] == s and not c.get("truncated")))
    groups.append((f"{e} / deadline-class (shares only)", lambda c, e=e: c["eng"] == e and c.get("truncated")))
cols = [("FACTOR sparse factorize", "la.sparse_factorize"), ("FACTOR sparse solve", "la.sparse_solve"), ("FACTOR sparse analyze", "la.sparse_analyze"),
        ("FACTOR dense factorize", "la.dense_factorize"), ("FACTOR dense solve", "la.dense_solve"), ("FACTOR other/scaling", ("la.other", "la.matching_scaling")),
        ("QP schur/border", "qp.schur_border"), ("QP walk+dep+refine+other", ("qp.walk", "qp.eqp_dependency", "qp.refine_polish", "qp.ssn", "qp.ipqp", "qp.elastic")),
        ("EVAL callbacks", "eval.callback"), ("EVAL hven assembly", "eval.assembly"), ("KKT assembly+glue", ("kkt.assembly", "kkt.factor_glue")),
        ("OUTER loop", ("outer.ipm_nlp_core", "outer.acceptance_linesearch", "outer.scaling_basis", "outer.quasi_newton", "outer.restoration", "outer.sqp_loop")),
        ("SETUP library init", "setup.library_init"), ("SETUP init/teardown", "setup.init_teardown"), ("INSTR", ("instr.ledger_trace", "instr.counting_wrapper")), ("unattrib.", ("unattributed.unresolved", "unattributed.other"))]
print("| group | cells | total instr | " + " | ".join(c[0] for c in cols) + " |")
print("|---|---:|---:|" + "---:|" * len(cols))
for name, pred in groups:
    g = [c for c in cells if pred(c)]
    if not g: continue
    den = sum(sum(v for k, v in c["comp"].items() if not k.startswith("proc.")) for c in g)
    row = []
    for _, keys in cols:
        keys = (keys,) if isinstance(keys, str) else keys
        s = sum(c["comp"].get(k, 0) for c in g for k in keys)
        row.append(f"{100*s/den:.1f}%")
    print(f"| {name} | {len(g)} | {den:.3g} | " + " | ".join(row) + " |")
