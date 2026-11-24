#!/usr/bin/env python3
"""p1a-diag dev co-run (not a leg): runs cells `lane prob label` (one per line of CELLS) with the lane binary,
one process per physical core of the held box lane (BOX_LANE_PHYS), MKL_NUM_THREADS=1, cap CAP s per cell
(SIGKILL of the cell process only). Extra env for the arm: ARMENV="K=V K=V". Output OUT/<lane>-<prob>-<label>.{out,err,result}.
usage (under box-lane): corun.py CELLS OUT BIN [CAP] """
import os, sys, subprocess, time, shlex
cells_f, out, binp = sys.argv[1], sys.argv[2], sys.argv[3]
cap = float(sys.argv[4]) if len(sys.argv) > 4 else 300.0
HB = "/home/ghecht/Projects/hven-bench-p1a"
phys = os.environ.get("BOX_LANE_PHYS", "0 1 2 3").split()
os.makedirs(out, exist_ok=True)
env = dict(os.environ, MKL_NUM_THREADS="1", OMP_NUM_THREADS="1")
for kv in shlex.split(os.environ.get("ARMENV", "")):
    k, v = kv.split("=", 1); env[k] = v
cells = [l.split() for l in open(cells_f) if l.strip() and not l.startswith("#")]
todo = [c for c in cells if not os.path.exists(f"{out}/{c[0]}-{c[1]}-{c[2].replace('/','_')}.result")]
running = {}
free = list(phys)
t0 = time.time()
while todo or running:
    while todo and free:
        c = todo.pop(0); core = free.pop(0)
        tag = f"{out}/{c[0]}-{c[1]}-{c[2].replace('/','_')}"
        fo, fe = open(tag + ".out", "w"), open(tag + ".err", "w")
        p = subprocess.Popen(["taskset", "-c", core, binp, "--lane", c[0], "--problem", c[1], "--options", c[2],
                              "--budget-iters", "0"], cwd=HB, stdout=fo, stderr=fe, env=env)
        running[p.pid] = (p, core, tag, time.time(), fo, fe)
    time.sleep(0.05)
    for pid, (p, core, tag, st, fo, fe) in list(running.items()):
        rc = p.poll(); capped = False
        if rc is None and time.time() - st >= cap:
            p.kill(); rc = p.wait(); capped = True
        if rc is not None:
            fo.close(); fe.close()
            open(tag + ".result", "w").write(f"rc={rc} wall={time.time()-st:.2f}{' CAPPED' if capped else ''}\n")
            del running[pid]; free.append(core)
print(f"corun done {len(cells)} cells {time.time()-t0:.0f}s")
