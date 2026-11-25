"""Stage-2 selection (the harness's own i19_rule.stage2_arm_set on the stage-1 results), coverage table (H0 rows per touched site),
H0 default-path values of the six new counters, the CONFOUNDED block per arm (incl. whether P trips R1), and the vacuity/precheck lines.
Loader: the harness's own, except a row without a counters side row is read as not-ended (see info_stage2.py: the A10 stop on the
ten NLP-46 dnf_wall rows)."""
import sys, io
sys.path.insert(0, "/home/ghecht/Projects/hven-bench-i19/tu")
sys.path.insert(0, "/home/ghecht/Projects/hven/.claude/worktrees/i19-2a/docs/notes/data/2026-11-m8-i19-2a/gate-I19-2a/scripts")
import i19_run, i19_rule
store = sys.argv[1]
spec = i19_run.load_spec(); params = i19_rule.params_from_spec(spec)
arms = i19_run.arms_by_name(spec)
corp = spec["stages"]["stage1_corpora"]
load1 = i19_run.default_loader(store)
res, k, members, loo = i19_run.stage1_report(spec, store, params, out=io.StringIO(), write_members=False)
names, c1c = i19_rule.stage2_arm_set(list(arms.values()), res, k, None)
print("STAGE-2 SELECTION (i19_rule.stage2_arm_set): arms %s ; C1 candidate=%s ; K=%s ; passing stage-1 arms: %s" % (
    names, c1c, "none" if k is None else k["name"], [n for n, r in res.items() if r["pass"]] or "none"))
# informational loader for the NLP-46-inclusive reads
import importlib.util
spec_i = importlib.util.spec_from_file_location("info", sys.argv[0].replace("final_tables.py", "info_stage2.py"))
def load(arm, corpora):
    data = {}
    from i19_run import tu2rule
    import os
    for corpus in corpora:
        if corpus == i19_rule.SENS:
            path = os.path.join(store, corpus, arm, "sens.csv")
            if os.path.exists(path): data.update(i19_run.read_sens(path))
            continue
        for r, side, _ in i19_run.arm_rows(store, corpus, arm)[0]:
            sv = r.get("set_version", "")
            sid = "hs" if r["problem_set"] == "hs" else "sd" if sv.startswith("cutest-small-dense") else "nlp"
            data[(corpus, sid, r["problem"], tu2rule.base_label(r["options_label"]))] = i19_rule.rec_from_rows(r, side)
    return data
allc = corp + ["NLP-46"]
h0 = load("H0", allc); p = load("P", allc)
print("\nCOVERAGE (H0 rows per touched site; %d H0 cells incl. NLP-46):" % len(h0))
for site, col, hits, present, status in i19_run.coverage_table(spec, h0):
    print("  %-22s %-30s hit %4d of %4d rows  %s" % (site, col, hits, present, status))
print("\nH0 default-path values of the six new counters (sum over H0 rows, side column present):")
for c in ("trust_region_growths", "trust_region_growth_refusals", "trust_region_shrinks_to_step", "trust_region_grown_step_rejections",
          "trust_region_unbound_rejections", "step_limit_binding_majors"):
    vals = [float(i19_rule.col(r, c)) for r in h0.values() if i19_rule.col(r, c) not in (None, "", "null")]
    print("  %-38s rows %4d sum %10.0f max %8.0f" % (c, len(vals), sum(vals), max(vals) if vals else 0))
print("\nCONFOUNDED block (BRITGAS, DALLASM, YORKNET), per arm, over TRAIN/CAP/SENS/NLP-46:")
h0s = {c: v for c, v in h0.items() if c[0] != "NLP-46"}; ps = {c: v for c, v in p.items() if c[0] != "NLP-46"}
for name in ["P", "G1", "G2", "G3", "G4", "G5", "G6", "G7", "C1", "C2"]:
    full = name in ("P", "C1")   # only P and C1 have NLP-46 rows, where the confounded problems live (they are not in TRAIN-238)
    x = load(name, allc if full else corp)
    H, PP = (h0, p) if full else (h0s, ps)
    ids = i19_rule.common_ids(H, x)
    cf = i19_rule.confounded_report(H, x, PP if name != "P" else None, ids, params)
    n_diff = sum(1 for r in cf["cells"] if r["status_h0"] != r["status_x"])
    print("  %-3s cells=%d status-changed=%d restoration H0=%d X=%d  X_R1_violations=%d" % (
        name, len(cf["cells"]), n_diff, sum(r["restoration_h0"] for r in cf["cells"]), sum(r["restoration_x"] for r in cf["cells"]), len(cf["x_r1_violations"])))
px = load("P", allc)
cfp = i19_rule.confounded_report(h0, px, None, i19_rule.common_ids(h0, px), params)
print("  P itself: R1 violations on the confounded cells: %d (P trips R1 there: %s); status changes %d" % (
    len(cfp["x_r1_violations"]), "yes" if cfp["x_r1_violations"] else "no", sum(1 for r in cfp["cells"] if r["status_h0"] != r["status_x"])))
for v in cfp["x_r1_violations"]: print("    P TAIL", v)
print("\nCAP precheck (H0 cells with step_limit_binding_majors > 0) per CAP family:")
import collections
cnt = collections.Counter(); tot = collections.Counter()
for cid, r in h0.items():
    if cid[0] in ("CAP-238-1", "CAP-238-01"):
        fam = i19_rule.family_of(cid); tot[fam] += 1
        v = i19_rule.col(r, "step_limit_binding_majors")
        if v not in (None, "", "null") and float(v) > 0: cnt[fam] += 1
for f in sorted(tot): print("  %-9s %3d of %3d cells cap-binding" % (f, cnt[f], tot[f]))
print("\nSENS-P H0 precheck (growth and rejection both > 0) per family:")
cnt = collections.Counter(); tot = collections.Counter()
for cid, r in h0.items():
    if cid[0] == "SENS-P":
        fam = i19_rule.family_of(cid); tot[fam] += 1
        g = i19_rule.col(r, "trust_region_growths"); 
        if g not in (None, "", "null") and float(g) > 0 and r["rejected_steps"] > 0: cnt[fam] += 1
for f in sorted(tot): print("  %-10s %3d of %3d cells" % (f, cnt[f], tot[f]))
print("\nH0 solved counts per family (strict/acc):")
fam = collections.defaultdict(lambda: [0, 0, 0])
for cid, r in h0.items():
    f = i19_rule.family_of(cid); fam[f][0] += 1; fam[f][1] += int(i19_rule.is_strict(r)); fam[f][2] += int(i19_rule.is_acc(r))
for f in sorted(fam): print("  %-10s cells %3d strict %3d acc %3d" % ((f,) + tuple(fam[f])))
