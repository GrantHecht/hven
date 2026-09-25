#!/usr/bin/env python3
"""Fold runs/<TAG>.out + .err into the arms x cells table (pure file folding; starts no solve).
usage: table.py RUNSDIR [TAG...]  (default: every *.out, sorted)"""
import glob, os, re, sys
QS = {0: "optimal", 1: "max_iter", 2: "infeasible", 3: "numerical_error"}
SS = {0: "optimal", 1: "acceptable", 2: "max_iter", 3: "infeasible", 4: "unbounded", 5: "?5", 6: "numerical_error", 7: "?7", 8: "interrupted"}
def kv(line):
    return dict(m.groups() for m in re.finditer(r"(\S+?)=(\S+)", line))
d = sys.argv[1]
WALL = {}
import os
SUM = next(p for p in (d + "/../logs/summary.txt", d + "/../summary.txt") if os.path.exists(p))
for l in open(SUM):
    m = re.match(r"RUN (\S+) .* wall_s=(\S+)", l)
    if m: WALL[m.group(1)] = m.group(2)
tags = sys.argv[2:] or sorted(os.path.basename(p)[:-4] for p in glob.glob(d + "/*.out"))
print("| run | status | majors | accepted | resto iters | resto sub status | h_out | f_out | abs dx inf | first post-resto QP (status, minors) | post-resto QPs ok/total | final f | stationarity | wall s |")
print("|---|---|---:|---:|---:|---|---:|---:|---:|---|---:|---:|---:|---:|")
for t in tags:
    out = {}
    for line in open(f"{d}/{t}.out"):
        if "=" in line and not line.startswith("side.harness_qpsum"):
            k, _, v = line.rstrip("\n").partition("="); out.setdefault(k, v)
    err = open(f"{d}/{t}.err").read().splitlines()
    rend = [kv(l) for l in err if l.startswith("R1RESTO_END")]
    post = [kv(l) for l in err if l.startswith("R1QP depth=0") and "post_resume_idx=-1" not in l]
    wall = [l for l in err if l.startswith("R1WALLSTOP")]
    r = rend[0] if rend else {}
    first = f"{QS.get(int(post[0]['status']), post[0]['status'])}, {post[0]['minors']}" if post else "—"
    ok = sum(1 for p in post if p["status"] == "0")
    subst = SS.get(int(r["sub_status"]), r["sub_status"]) if r else "—"
    def g(x, fmt="{:.4g}"):
        try: return fmt.format(float(x))
        except Exception: return x or "—"
    w = ""
    if wall: w = " (in-process stop at 300 s)"
    print(f"| {t} | {out.get('status','?')}{w} | {out.get('iterations','?')} | {out.get('side.steps_accepted','?')} | {out.get('side.restoration_iters','?')} | {subst} | {g(r.get('h_r'))} | {g(r.get('f_r'))} | {g(r.get('dx_inf'))} | {first} | {ok}/{len(post)} | {g(out.get('objective'),'{:.10g}')} | {g(out.get('kkt_stationarity'))} | {g(WALL.get(t, ''), '{:.0f}')} |")
