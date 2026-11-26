#!/usr/bin/env python3
"""QPC leg M1R, TRAIN-238 member 0 (DECLARATION-QPC section 6): every cell of the harness's TRAIN-238 list under the H0
arm's label (member 0: no perturbation key), run on the M1R DIAGNOSTIC lane binary with HVEN_QPC_M1=1, one process per
cell, co-run one process per pinned physical core of the box lane ($BOX_LANE_PHYS), MKL_NUM_THREADS=1. The per-cell
argv is run_serial.py's (hven-bench `ens` 27c02687, runners/run_serial.py run_cell): --lane hven-sqp --problem P
--options LABEL --budget-iters 0 --options-dump D --assert-reference [--budget-facts N where the set carries one], the
300 s wall watchdog killing the process group. Writes <out>/<set>-<problem>-<label>.{out,err,result}: stdout (the cell
block), stderr (the [m1r] lines) and `rc=<rc> verdict=<ok|wall|reference|error>`. HOLDOUT is refused by the harness's
own guard (i19_run.refuse_holdout).
usage: run_m1r_train.py <hven-bench checkout at 27c02687> <M1R lane binary> <out dir>
Run under: ~/.local/bin/box-lane -- (inside legs/legM1R.sh)."""
import concurrent.futures
import os
import queue
import signal
import subprocess
import sys

WALL_S = 300.0  # TRAIN-238's wall_cap_s (tu/i19_spec.json)


def main():
    hb, lane_bin, out = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2]), os.path.abspath(sys.argv[3])
    sys.path.insert(0, os.path.join(hb, "tu"))
    import i19_cells  # noqa: E402  (the harness's own cell list and label rule)
    import i19_run  # noqa: E402
    spec = i19_cells.load_spec()
    config = i19_cells.arm_config(spec, "H0")
    cells = []
    for set_id, by_base in i19_run.problems_by_set(spec, "TRAIN-238").items():  # refuses HOLDOUT
        _, _, wall, facts = i19_run.set_terms(spec, "TRAIN-238", set_id)
        if float(wall) != WALL_S:
            sys.exit(f"STOP: TRAIN-238 wall cap {wall} != {WALL_S}")
        for base, problems in by_base.items():
            lab = i19_cells.label(spec, "TRAIN-238", base, config)
            cells += [(set_id, p, lab, facts) for p in problems]
    if len(cells) != spec["corpora"]["TRAIN-238"]["cells"]:
        sys.exit(f"STOP: {len(cells)} cells, the spec says {spec['corpora']['TRAIN-238']['cells']}")
    phys = os.environ.get("BOX_LANE_PHYS", "").split()
    if not phys:
        sys.exit("STOP: BOX_LANE_PHYS is unset (run under box-lane)")
    os.makedirs(out, exist_ok=True)
    cpus = queue.Queue()
    for c in phys:
        cpus.put(c)
    env = dict(os.environ, HVEN_QPC_M1="1", MKL_NUM_THREADS="1", OMP_NUM_THREADS="1")

    def run(cell):
        set_id, problem, lab, facts = cell
        stem = os.path.join(out, f"{set_id}-{problem}-{lab}")
        argv = [lane_bin, "--lane", "hven-sqp", "--problem", problem, "--options", lab, "--budget-iters", "0",
                "--options-dump", stem + ".dump", "--assert-reference"]
        if facts is not None:
            argv += ["--budget-facts", str(int(facts))]
        cpu = cpus.get()
        try:
            with open(stem + ".out", "w") as fo, open(stem + ".err", "w") as fe:
                child = subprocess.Popen(["taskset", "-c", cpu] + argv, stdout=fo, stderr=fe, env=env, cwd=hb,
                                         start_new_session=True)
                try:
                    rc = child.wait(timeout=WALL_S)
                    verdict = "ok" if rc == 0 else ("reference" if rc == 3 else "error")
                except subprocess.TimeoutExpired:
                    os.killpg(child.pid, signal.SIGKILL)
                    rc = child.wait()
                    verdict = "wall"
        finally:
            cpus.put(cpu)
        with open(stem + ".result", "w") as fr:
            fr.write(f"rc={rc} verdict={verdict} argv={' '.join(argv)}\n")
        return verdict

    with concurrent.futures.ThreadPoolExecutor(max_workers=len(phys)) as ex:
        verdicts = list(ex.map(run, cells))
    print(f"M1R TRAIN-238 m0 cells={len(cells)} " +
          " ".join(f"{v}={verdicts.count(v)}" for v in ("ok", "reference", "error", "wall")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
