"""Stage-1 table (machine-read from the stores through tu/i19_rule.py), for RESULTS-I19-2a.md."""
import sys, json
sys.path.insert(0, "/home/ghecht/Projects/hven-bench-i19/tu")
import i19_run, i19_rule
store = sys.argv[1]
spec = i19_run.load_spec(); params = i19_rule.params_from_spec(spec)
corpora = spec["stages"]["stage1_corpora"]
load = i19_run.default_loader(store)
arms = i19_run.arms_by_name(spec)
h0 = load("H0", corpora); p = load("P", corpora)
print("H0 cells", len(h0), "P cells", len(p))
rows = []
for name in spec["stages"]["stage1_arms"]:
    if name == "H0": continue
    x = load(name, corpora)
    r = i19_rule.stage1_eval(h0, x, p, arms[name], params)
    rows.append((name, r))
def f(b): return "yes" if b else "NO"
print("| arm | pass | (a) V1 | (b) no -1 | (c) V3' | (d) tail | (e) moves | lost | tail cells | live moved | pooled net strict | LA net/sign | EV net/sign | plus |")
print("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|")
for name, r in rows:
    s = r["summary"]; t = s["tally"]
    print("| %s | %s | %s | %s | %s | %s | %s | %d | %d | %d | %+d | %+d/%+d | %+d/%+d | %s |" % (
        name, f(r["pass"]), f(r["a_v1"]), f(r["b_no_minus_one"]), r["c_sens"], f(r["d_tail_guard"]), f(r["e_moves_a_row"]),
        len(r["lost_cells"]), len(r["tail_violations"]), r["moved_cells"], s["net_strict"],
        t["LA"]["net"], t["LA"]["sign"], t["EV"]["net"], t["EV"]["sign"], ",".join(r["plus"]) or "-"))
print()
print("Placebo floor (P vs H0 on the live set of the arm; TRAIN/CAP):")
seen = set()
for name, r in rows:
    k = json.dumps(r["placebo"], sort_keys=True, default=str)
    if k in seen: continue
    seen.add(k); print(" ", name, r["placebo"])
print()
print("Per-arm minus-one families and failing V1 families:")
for name, r in rows:
    print(" ", name, "minus_one:", r["minus_one"], "| V1:", {k: v for k, v in r["v1"].items() if not (v.get("ok", True) if isinstance(v, dict) else True)} if isinstance(r["v1"], dict) else r["v1"])
print()
print("Family signs (LA/EV) per arm:")
for name, r in rows:
    print(" ", name, {fam: (d["LA"]["sign"], d["EV"]["sign"]) for fam, d in sorted(r["family_signs"].items())})
