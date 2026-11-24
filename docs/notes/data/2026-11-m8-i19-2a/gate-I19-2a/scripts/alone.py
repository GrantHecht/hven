"""Re-run ALONE (serial, one process, no co-run width) every TRAIN/CAP cell that decides a stage-1 bar (lost cells and R1 tail cells,
read from the stage-1 report text), into a separate store laid out like the arm store, then compare with the co-run rows
(counters/statuses exact, residual-class columns by the near-ulp gate).  SENS-P cells come from the grid binary, which already
runs its cells serially in one process; they are not re-run.
usage: alone.py plan <stage report> <alone store> | run <stage report> <alone store> | compare <stage report> <store-H> <alone store>"""
import ast, collections, os, re, subprocess, sys
sys.path.insert(0, "/home/ghecht/Projects/hven-bench-i19/tu")
import i19_run, i19_cells
HB = "/home/ghecht/Projects/hven-bench-i19"
BIN = HB + "/build-i19-H/lanes/hven/hven_bench_cell"
SHA = "91787d329a06ae99770879762804a1babf316cbe"; PFX = HB + "/.hven/install-91787d329a06"

def cells_from_report(path):
    out = collections.OrderedDict()   # (arm, corpus, set, problem, base)
    arm = None
    for ln in open(path):
        m = re.match(r"STAGE1B? (\S+) ", ln)
        if m:
            arm = m.group(1); continue
        m = re.match(r"\s+(LOST|TAIL) (\(.*?\))", ln)
        if m and arm:
            corpus, s, p, b = ast.literal_eval(m.group(2))
            if corpus == "SENS-P":
                continue
            out[(arm, corpus, s, p, b)] = m.group(1)
    return out

def groups(cells):
    g = collections.OrderedDict()
    for (arm, corpus, s, p, b) in cells:
        g.setdefault((arm, corpus, s, b), []).append(p)
    return g

def main():
    cmd, rep = sys.argv[1], sys.argv[2]
    spec = i19_run.load_spec()
    cells = cells_from_report(rep)
    if cmd == "plan":
        print(len(cells), "cells,", len(groups(cells)), "groups")
        for k, v in groups(cells).items():
            print(k, v)
        return 0
    if cmd == "run":
        store = sys.argv[3]
        env = dict(os.environ, **i19_run.arm_env(SHA, PFX))
        for (arm, corpus, s, b), probs in groups(cells).items():
            probs = sorted(set(probs))
            cfg = i19_cells.arm_config(spec, arm)
            label = i19_cells.label(spec, corpus, b, cfg)
            ps, sv, wall, _ = i19_run.set_terms(spec, corpus, s)
            i19_run.refuse_holdout(s, probs)
            out = i19_run.leg_dir(store, corpus, arm, b, s, "alone", probs)
            argv = [sys.executable, HB + "/runners/run_serial.py", "--out", out, "--problems", ",".join(probs), "--split", "TRAIN",
                    "--problem-set", ps] + (["--set-version", sv] if sv else []) + ["--lanes", i19_run.LANE, "--options", label,
                    "--budget-iters", "0", "--budget-wall-s", repr(float(wall)), "--binary", BIN]
            wrapped = ["box-lane", "--"] + argv
            print("ALONE", arm, corpus, s, b, probs, flush=True)
            os.makedirs(os.path.dirname(out), exist_ok=True)
            rc = subprocess.call(wrapped, cwd=HB, env=env)
            if rc not in (0, 1):
                print("ALONE leg rc", rc); return 3
        return 0
    if cmd == "compare":
        sh, sa = sys.argv[3], sys.argv[4]
        allowed = set(spec["neutrality"]["N2"]["differ_columns_allowed"])
        nd = 0; n = 0
        for (arm, corpus, s, p, b), why in cells.items():
            ta, tb = i19_run.cell_table(sh, corpus, arm), i19_run.cell_table(sa, corpus, arm)
            hit = [k for k in ta if k in tb]
        # compare per arm/corpus on the cells re-run
        by = collections.OrderedDict()
        for (arm, corpus, s, p, b) in cells:
            by.setdefault((arm, corpus), set()).add((s, p, b))
        for (arm, corpus), keys in by.items():
            ta, tb = i19_run.cell_table(sh, corpus, arm), i19_run.cell_table(sa, corpus, arm)
            sel = [k for k in tb]
            a2 = {k: ta[k] for k in sel if k in ta}; b2 = {k: tb[k] for k in sel}
            d, x, y, u = i19_run.compare_arm_rows(a2, b2, allowed, spec["rule"]["new_counter_prefixes"], "corun", "alone")
            n += len(b2); nd += len(d)
            for t in d:
                print("DIFF", arm, corpus, t)
            if len(b2) != len(keys):
                print("NOTE", arm, corpus, "alone cells", len(b2), "wanted", len(keys))
        print("ALONE compare: %d cells, %d difference(s)" % (n, nd))
        return 0 if nd == 0 else 3
sys.exit(main())
