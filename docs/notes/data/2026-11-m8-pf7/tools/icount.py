#!/usr/bin/env python3
"""PF-7 instruction counter: runs a cell list against one lane binary, one process per pinned physical CPU (co-run, section 7 terms:
counters only, MKL_NUM_THREADS=1), `perf stat -e instructions:u` per cell, and writes one CSV row per cell with the instruction count and the
cell's own CELL block (status, iterations, factorizations, objective, every counter) for the identity check.
usage: icount.py <bin> <cells.csv (lane,problem[,mode])> <out.csv> <cpus comma list> [timeout_s]   cwd = the bench clone.
Cells that exceed timeout_s are recorded as DNF (not counted)."""
import csv, os, queue, subprocess, sys, threading, json
binp, cells_csv, out, cpus = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4].split(",")
tmo = int(sys.argv[5]) if len(sys.argv) > 5 else 300
cells = [c for c in csv.DictReader(open(cells_csv)) if c.get("mode", "full") == "full"]
cells.sort(key=lambda c: -float(c.get("prior_wall_s", 0) or 0))
q = queue.Queue()
for c in cells: q.put(c)
res = {}; lock = threading.Lock()
env = dict(os.environ, MKL_NUM_THREADS="1", OMP_NUM_THREADS="1")
def parse(o):
    f, inside = {}, False
    for l in o.splitlines():
        if l == "CELL_BEGIN": inside = True
        elif l == "CELL_END": inside = False
        elif inside and "=" in l:
            k, _, v = l.partition("="); f[k] = v
    return f
def work(cpu):
    while True:
        try: c = q.get_nowait()
        except queue.Empty: return
        argv = ["taskset", "-c", cpu, "timeout", str(tmo), "perf", "stat", "-x,", "-e", "instructions:u", "--", binp, "--lane", c["lane"], "--problem", c["problem"],
                "--options", "default", "--budget-iters", "0", "--assert-reference"]
        r = subprocess.run(argv, capture_output=True, text=True, env=env)
        n = None
        for l in r.stderr.splitlines():
            p = l.split(",")
            if len(p) > 2 and p[2].startswith("instructions"):
                try: n = int(p[0])
                except ValueError: pass
        f = parse(r.stdout)
        with lock: res[(c["lane"], c["problem"])] = (n, r.returncode, f)
ts = [threading.Thread(target=work, args=(c,)) for c in cpus]
[t.start() for t in ts]; [t.join() for t in ts]
keys = sorted({k for _, _, f in res.values() for k in f})
with open(out, "w", newline="") as fh:
    w = csv.writer(fh); w.writerow(["lane", "problem", "instructions", "rc"] + keys)
    for (lane, prob), (n, rc, f) in sorted(res.items()):
        w.writerow([lane, prob, n if n is not None else "DNF", rc] + [f.get(k, "") for k in keys])
print("rows", len(res))
