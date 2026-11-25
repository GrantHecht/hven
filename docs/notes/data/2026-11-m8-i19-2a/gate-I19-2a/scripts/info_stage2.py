"""INFORMATIONAL stage-2 read for the arms stage 2 actually runs (P, C1), with H0 as baseline.
WHY NOT `i19_run.py stage2`: the harness's load_arm_data stops on any results row without a counters side row (A10), and the
ten NLP-46 cells that end at the 300 s wall cap (dnf_wall, the same ten on R and on H0) have none by construction.  This script
re-implements ONLY the loader (a row with no side row is read as a not-ended cell, via i19_rule.rec_from_rows, which already
accepts side=None) and then calls the harness's own i19_rule functions unchanged.  Not a declared instrument; informational,
never a bar.   usage: info_stage2.py <store>"""
import sys, json
sys.path.insert(0, "/home/ghecht/Projects/hven-bench-i19/tu")
import i19_run, i19_rule, i19_cells
from i19_run import tu2rule
store = sys.argv[1]
spec = i19_run.load_spec(); params = i19_rule.params_from_spec(spec)
arms = i19_run.arms_by_name(spec)

def load(arm, corpora):
    data = {}
    for corpus in corpora:
        if corpus == i19_rule.SENS:
            import os
            path = os.path.join(store, corpus, arm, "sens.csv")
            if os.path.exists(path):
                data.update(i19_run.read_sens(path))
            continue
        for r, side, _ in i19_run.arm_rows(store, corpus, arm)[0]:
            if r.get("split", "TRAIN") != "TRAIN":
                raise SystemExit("HOLDOUT row")
            sv = r.get("set_version", "")
            set_id = "hs" if r["problem_set"] == "hs" else "sd" if sv.startswith("cutest-small-dense") else "nlp"
            cid = (corpus, set_id, r["problem"], tu2rule.base_label(r["options_label"]))
            data[cid] = i19_rule.rec_from_rows(r, side)
    return data

allc = spec["stages"]["stage1_corpora"] + ["NLP-46"]
h0, p = load("H0", allc), load("P", allc)
nl = [c for c in h0 if c[0] == "NLP-46"]
print("H0 cells", len(h0), "(NLP-46 %d; not-ended NLP cells %d)" % (len(nl), sum(1 for c in nl if not h0[c]["ended"])))
for name in ("P", "C1"):
    x = load(name, allc)
    ids = i19_rule.common_ids(h0, x)
    nids = i19_rule.in_corpora(ids, ("NLP-46",))
    print("== %s: common cells %d (NLP-46 %d)" % (name, len(ids), len(nids)))
    ok_n, detail = i19_rule.nlp_stand_alone(h0, x, nids)
    print("NLP-46 stand-alone (R3): ok=%s %s" % (ok_n, json.dumps(detail, default=str)[:1500]))
    summ = i19_rule.pooled_summary(h0, x, nids)
    print("NLP-46 pooled: solved %s | tally %s | geo %s" % (summ["solved"], {c: (summ["tally"][c]["sign"], summ["tally"][c]["net"]) for c in i19_rule.CURRENCIES}, summ["geo"]))
    tg = i19_rule.tail_guard(h0, x, nids, params["tail_add"], params["tail_mult"])
    print("NLP-46 tail guard (V4) violations: %d" % len(tg))
    for t in tg[:25]:
        print("  TAIL", t)
    ok_v1, v1d, lost = i19_rule.v1(h0, x, nids)
    print("NLP-46 V1 ok=%s lost=%s" % (ok_v1, lost))
    fs = i19_rule.family_signs(h0, x, nids)
    print("NLP-46 family signs", {f: (d["LA"]["sign"], d["EV"]["sign"]) for f, d in sorted(fs.items())})
    print("restoration/floor", json.dumps(i19_rule.arm_reports(h0, x, nids), default=str))
