#!/usr/bin/env python3
"""PF-3: attribute one arbitrary command (the routes-census probe) with the same instrument and table as pf3_cell.py.
usage: pf3_probe.py <cpu> <out.json> <cmd...>      (pass A perf stat total; pass B perf record ~12000 samples; stdout of pass A kept in <out>.stdout)
"""
import collections, json, os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import components as C

cpu, out = sys.argv[1], sys.argv[2]
cmd = sys.argv[3:]
env = dict(os.environ, MKL_NUM_THREADS="1", OMP_NUM_THREADS="1")
base = ["taskset", "-c", cpu]
a = subprocess.run(base + ["perf", "stat", "-x,", "-e", "instructions:u", "--"] + cmd, capture_output=True, text=True, env=env)
open(out + ".stdout", "w").write(a.stdout)
total = next(int(l.split(",")[0]) for l in a.stderr.splitlines() if len(l.split(",")) > 2 and l.split(",")[2].startswith("instructions"))
period = max(500_000, total // 12000)
data = out + ".perf.data"
subprocess.run(base + ["perf", "record", "-q", "--per-thread", "-m", "4M", "-e", "instructions:u", "-c", str(period), "--user-callchains", "--call-graph", "dwarf,16384", "-o", data, "--"] + cmd,
               capture_output=True, text=True, env=env)
sp = subprocess.Popen(["perf", "script", "-i", data, "-F", "ip,sym,dso", "--no-inline"], stdout=subprocess.PIPE, text=True, errors="replace")
comp = collections.Counter(); hfr = collections.Counter(); n = 0
rx = re.compile(r"^\s*[0-9a-f]+ (.*) \((.*)\)$"); buf = []


def flush():
    global n
    fr = [(m.group(1), os.path.basename(m.group(2))) for m in (rx.match(l) for l in buf) if m]
    buf.clear()
    if not fr:
        return
    n += 1; comp[C.classify(fr)] += 1
    seen = set()
    for s, d in fr:
        if s.startswith("hven::"):
            k = re.sub(r"\(.*", "", s)[:110]
            if k not in seen:
                seen.add(k); hfr[k] += 1


for line in sp.stdout:
    if line.strip() == "":
        flush()
    else:
        buf.append(line)
flush(); sp.wait()
json.dump({"cmd": cmd, "instructions_total": total, "period": period, "samples": n, "est_over_total": n * period / total,
           "components": {k: v * period for k, v in comp.items()}, "hven_frames_inclusive": [[k, v * period] for k, v in hfr.most_common(60)],
           "components_sha256": __import__("hashlib").sha256(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "components.py"), "rb").read()).hexdigest()},
          open(out, "w"), indent=1)
os.remove(data)
