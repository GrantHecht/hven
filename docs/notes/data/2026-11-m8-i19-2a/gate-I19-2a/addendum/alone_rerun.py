"""A3: re-run the three NLP-46 cells where the placebo P loses an H0-optimal cell, ALONE (one process, serial, one cell at a time, each under
`box-lane --exclusive`), for BOTH H0 and P, with the frozen H lane binary (BINARIES.txt, sha checked), and compare with the store rows.
usage: alone_rerun.py run <out dir> | compare <out dir>"""
import os, sys, subprocess, hashlib, csv
from common import *
import i19_cells
BIN = HB + "/build-i19-H/lanes/hven/hven_bench_cell"
SHA = "91787d329a06ae99770879762804a1babf316cbe"; PFX = HB + "/.hven/install-91787d329a06"
BINSHA = "617241550fc016e235e95a46667f3273aee66fba6f1af8841cb7af8f633bc72b"
CELLS = [("ACOPR118", "default"), ("BRITGAS", "default"), ("SAWPATH", "sqp-qn")]
def label_for(arm, base):
    cfg = i19_cells.arm_config(spec, arm)
    return i19_cells.label(spec, "NLP-46", base, cfg)
def main():
    cmd, out = sys.argv[1], os.path.abspath(sys.argv[2])
    if cmd == "run":
        h = hashlib.sha256(open(BIN, "rb").read()).hexdigest()
        assert h == BINSHA, h
        print("binary sha ok", h, flush=True)
        ps, sv, wall, _ = i19_run.set_terms(spec, "NLP-46", "nlp")
        env = dict(os.environ, **i19_run.arm_env(SHA, PFX))
        for prob, base in CELLS:
            for arm in ("H0", "P"):
                lab = label_for(arm, base)
                d = os.path.join(out, arm, "%s-%s" % (prob, base))
                os.makedirs(os.path.dirname(d), exist_ok=True)
                argv = ["box-lane", "--exclusive", "--", sys.executable, HB + "/runners/run_serial.py", "--out", d, "--problems", prob, "--split", "TRAIN",
                        "--problem-set", ps, "--set-version", sv, "--lanes", i19_run.LANE, "--options", lab, "--budget-iters", "0",
                        "--budget-wall-s", repr(float(wall)), "--binary", BIN]
                print("ALONE-RERUN", arm, prob, base, lab, flush=True)
                rc = subprocess.call(argv, cwd=HB, env=env)
                print("rc", rc, flush=True)
        return 0
    if cmd == "compare":
        ps_allowed = None
        for prob, base in CELLS:
            for arm in ("H0", "P"):
                d = os.path.join(out, arm, "%s-%s" % (prob, base))
                rows, _ = i19_run.read_leg_rows(d)
                assert len(rows) == 1, (arm, prob, len(rows))
                r, side = rows[0]
                rec = i19_rule.rec_from_rows(r, side)
                key = ("NLP-46", "nlp", prob, base)
                store = load(STORE_H, arm, ["NLP-46"])[key]
                moved = i19_rule.moved(store, rec, params)
                print("%-9s %-8s %-8s | store: %-16s majors %3d LA %6d EV %6d f %s | alone: %-16s majors %3d LA %6d EV %6d f %s | differs(moved test): %s" % (
                    prob, base, arm, store["status"], store["majors"], store["la"], store["ev"], store["objective"],
                    rec["status"], rec["majors"], rec["la"], rec["ev"], rec["objective"], moved))
        return 0
sys.exit(main())
