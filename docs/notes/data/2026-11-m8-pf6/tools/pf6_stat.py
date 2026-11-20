#!/usr/bin/env python3
"""PF-6 instruction leg: perf stat -e instructions:u for each (layer, rep, cell), co-run W wide, one process per pinned PHYSICAL core.

usage: pf6_stat.py <cells.csv> <outdir> <width> <cpus comma list> <reps> <layer:harness_dir> [<layer:harness_dir> ...]
cells.csv: PF-3's cells.csv (lane,problem,mode,prior_wall_s); only mode=full rows run (deadline-class rows are not asserted).
Each job's cwd is the layer's private hven-bench worktree (CUTEst paths are cwd-relative); argv is PF-3's serial argv.
Job order is fixed before the run (layer-interleaved per cell, longest-first), results are one JSON per job.  MKL_NUM_THREADS=1, OMP_NUM_THREADS=1.
"""
import csv, json, os, queue, subprocess, sys, threading, time

BIN = "build/lanes/hven/hven_bench_cell"


def parse_cell(out):
    f, inside = {}, False
    for line in out.splitlines():
        if line == "CELL_BEGIN": inside = True
        elif line == "CELL_END": inside = False
        elif inside and "=" in line:
            k, _, v = line.partition("="); f[k] = v
    return f


def job(cpu, layer, hb, lane, problem, rep, outdir, scratch):
    tag = f"{layer}__{lane}__{problem}__r{rep}"
    dump = os.path.join(scratch, tag + ".opts")
    argv = ["taskset", "-c", cpu, "perf", "stat", "-x,", "-e", "instructions:u", "--", BIN, "--lane", lane, "--problem", problem,
            "--options", "default", "--budget-iters", "0", "--options-dump", dump, "--assert-reference"]
    env = dict(os.environ, MKL_NUM_THREADS="1", OMP_NUM_THREADS="1")
    t0 = time.monotonic()
    r = subprocess.run(["timeout", "600"] + argv, capture_output=True, text=True, env=env, cwd=hb)
    wall = time.monotonic() - t0
    total = None
    for line in r.stderr.splitlines():
        p = line.split(",")
        if len(p) > 2 and p[2].startswith("instructions"):
            try: total = int(p[0])
            except ValueError: pass
    f = parse_cell(r.stdout)
    json.dump({"tag": tag, "layer": layer, "lane": lane, "problem": problem, "rep": rep, "cpu": cpu, "rc": r.returncode,
               "instructions": total, "wall_s_informational": wall, "status": f.get("status"), "outcome": f.get("outcome"),
               "iterations": f.get("iterations"), "factorizations": f.get("factorizations"), "fields": f,
               "stderr_tail": r.stderr[-600:] if total is None else ""}, open(f"{outdir}/{tag}.json", "w"))
    try: os.remove(dump)
    except OSError: pass
    return r.returncode


def main():
    cells_csv, outdir, width, cpus, reps = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4].split(","), int(sys.argv[5])
    layers = [a.split(":", 1) for a in sys.argv[6:]]
    os.makedirs(outdir, exist_ok=True)
    scratch = os.environ.get("TMPDIR", "/tmp")
    cells = [c for c in csv.DictReader(open(cells_csv)) if c["mode"] == "full"]
    cells.sort(key=lambda c: -float(c["prior_wall_s"]))
    q = queue.Queue(); cq = queue.Queue()
    for rep in range(reps):
        for c in cells:
            for layer, hb in layers:
                q.put((layer, hb, c["lane"], c["problem"], rep))
    for c in cpus[:width]: cq.put(c)
    lock = threading.Lock(); done = [0]; n = q.qsize(); t0 = time.time()

    def worker():
        while True:
            try: j = q.get_nowait()
            except queue.Empty: return
            cpu = cq.get()
            rc = job(cpu, j[0], j[1], j[2], j[3], j[4], outdir, scratch)
            cq.put(cpu)
            with lock:
                done[0] += 1
                print(f"[{done[0]}/{n}] {j[0]} {j[2]} {j[3]} r{j[4]} cpu={cpu} rc={rc}", flush=True)

    ts = [threading.Thread(target=worker) for _ in range(width)]
    [t.start() for t in ts]; [t.join() for t in ts]
    print(f"STAT_DONE {time.time()-t0:.0f}s", flush=True)


if __name__ == "__main__":
    main()
