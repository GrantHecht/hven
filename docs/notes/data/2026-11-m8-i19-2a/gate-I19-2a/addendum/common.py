"""Shared loader for the I19-2a addendum scripts.  Reads the stores read-only through the harness checkout's own
loaders (hven-bench-i19, branch i19: tu/i19_run.py, tu/i19_rule.py); nothing in that checkout is modified.
The only deviation from i19_run.default_loader: a results row with no counters side row (the ten NLP-46 dnf_wall cells,
which have none by construction, RESULTS Deviation 4 / A10) is read as a not-ended-with-side-columns-absent record via
i19_rule.rec_from_rows(row, None), the same bypass info_stage2.py and the TU-3 reader use."""
import sys, os, csv
HB = "/home/ghecht/Projects/hven-bench-i19"
sys.path.insert(0, HB + "/tu")
import i19_run, i19_rule
from i19_run import tu2rule

STORE_H = HB + "/.scratch/i19-2a/store-H"
STORE_R = HB + "/.scratch/i19-2a/store-R"
STORE_ALONE = HB + "/.scratch/i19-2a/store-alone"
spec = i19_run.load_spec()
params = i19_rule.params_from_spec(spec)
ARMS = ["P", "G1", "G2", "G3", "G4", "G5", "G6", "G7", "C1", "C2"]
STAGE1 = spec["stages"]["stage1_corpora"]          # TRAIN-238, CAP-238-1, CAP-238-01, SENS-P
LIVE = ["TRAIN-238", "CAP-238-1", "CAP-238-01"]

def load(store, arm, corpora):
    data = {}
    for corpus in corpora:
        if corpus == i19_rule.SENS:
            p = os.path.join(store, corpus, arm, "sens.csv")
            if os.path.exists(p):
                data.update(i19_run.read_sens(p))
            continue
        base = os.path.join(store, corpus, arm)
        if not os.path.isdir(base):
            continue
        for r, side, _ in i19_run.arm_rows(store, corpus, arm)[0]:
            if r.get("split", "TRAIN") != "TRAIN":
                raise SystemExit("HOLDOUT row")
            sv = r.get("set_version", "")
            sid = "hs" if r["problem_set"] == "hs" else "sd" if sv.startswith("cutest-small-dense") else "nlp"
            cid = (corpus, sid, r["problem"], tu2rule.base_label(r["options_label"]))
            if cid in data:
                raise SystemExit("cell twice %s %s" % (arm, cid))
            data[cid] = i19_rule.rec_from_rows(r, side)
    return data

def fnum(x):
    try:
        return float(x)
    except (TypeError, ValueError):
        return None

def name(cid):
    return "%s %s/%s" % (cid[0], cid[2], cid[3])
