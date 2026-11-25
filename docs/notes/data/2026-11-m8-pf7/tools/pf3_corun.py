#!/usr/bin/env python3
"""PF-3 co-run driver: runs the cells of a cell list, one process per pinned PHYSICAL core, W wide.

usage: pf3_corun.py <cells.csv> <outdir> <width> <cpus comma list>
cells.csv columns: lane,problem,mode,prior_wall_s   (ordered longest-first by prior_wall_s before the run starts;
the order is fixed by the file and is not tuned by anything the run produces).
cwd = hven-bench pf3 checkout.  The CPU list must name ONE logical cpu of distinct physical cores (SMT siblings idle).
"""
import csv, os, queue, subprocess, sys, threading, time

HERE = os.path.dirname(os.path.abspath(__file__))


def main():
    cells_csv, outdir, width, cpus = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4].split(",")
    assert width <= len(cpus)
    cells = list(csv.DictReader(open(cells_csv)))
    q = queue.Queue(); cq = queue.Queue()
    for c in cells: q.put(c)
    for c in cpus[:width]: cq.put(c)
    lock = threading.Lock(); done = [0]
    t0 = time.time()

    def worker():
        while True:
            try: c = q.get_nowait()
            except queue.Empty: return
            cpu = cq.get()
            s = time.time()
            r = subprocess.run([sys.executable, os.path.join(HERE, "pf3_cell.py"), cpu, c["lane"], c["problem"], c["mode"], outdir],
                               capture_output=True, text=True)
            cq.put(cpu)
            with lock:
                done[0] += 1
                print(f"[{done[0]}/{len(cells)}] {c['lane']} {c['problem']} mode={c['mode']} cpu={cpu} rc={r.returncode} {time.time()-s:.0f}s", flush=True)
                if r.returncode: print(r.stderr[-500:], flush=True)

    ts = [threading.Thread(target=worker) for _ in range(width)]
    [t.start() for t in ts]; [t.join() for t in ts]
    print(f"CORUN_DONE {time.time()-t0:.0f}s", flush=True)


if __name__ == "__main__":
    main()
