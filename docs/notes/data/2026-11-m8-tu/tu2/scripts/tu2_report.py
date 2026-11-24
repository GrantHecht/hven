#!/usr/bin/env python3
"""TU-2 reported quantities (DECLARATION-TU-2.md section 10) and the section 5.4 inputs, from rows on disk.

    .scratch/venv/bin/python tu/tu2_report.py STORE [--tu1-store DIR] > report.txt

Informational except the arm signs, V1/V2 and ranking, which are the ruling inputs (tu2_run.selection).
"""
import collections
import csv
import os
import sys

ROOT_ = "/home/ghecht/Projects/hven-bench-tu2"
HERE = ROOT_ + "/tu"
sys.path.insert(0, HERE)
import cells as cellmod  # noqa: E402
import folds  # noqa: E402
import rule  # noqa: E402
import spec_check  # noqa: E402
import tu2_run  # noqa: E402

TU1_STORE = os.path.join(ROOT_, "runs", "2026-11-m8-tu1-pilot", "store")


def family_name(engine, key):
    return "%s-%s" % ("SQP" if engine == "sqp" else "IPM", "HS" if key[0] == "hs" else "CUTEst")


def classify_row(r):
    st, oc = r.get("status"), r.get("outcome")
    if oc == "dnf_wall":
        return "dnf_wall"
    if oc == "infeasible":
        return "infeasible"
    return st if st not in (None, "", "null") else oc


def outcome_counts(spec, root, name):
    engine, lane, config = tu2_run.arm_def(spec, name)
    if not _has_rows(root, lane, config):
        return None
    rows, _ = tu2_run.arm_rows(root, lane, config)
    obs = rule.load_dir_r1(tu2_run.arm_dir(root, lane, config), lane)
    counts = collections.defaultdict(collections.Counter)
    for r, _ in rows:
        key = (r["problem_set"], r["problem"], rule.base_label(r["options_label"]))
        fam = family_name(engine, key)
        counts[fam][classify_row(r)] += 1
        o = obs[key]
        if o.outcome == "infeasible" and o.certified == 1:
            counts[fam]["certified"] += 1
    return counts


def movers(spec, root, tu1_store):
    """TU-1's default (0ee37c2ca) rows against A0 (the PIN), per cell: outcome/status/counters; informational."""
    lane = spec["engines"]["sqp"]["lane"]
    a0 = {}
    rows, _ = tu2_run.arm_rows(root, lane, {})
    for r, _ in rows:
        a0[(r["problem_set"], r["problem"], rule.base_label(r["options_label"]))] = r
    old = {}
    base = os.path.join(tu1_store, lane, "default")
    for dirpath, _, files in os.walk(base):
        if "results.csv" in files:
            skip = set()
            if "superseded.txt" in files:
                skip = {ln.strip() for ln in open(os.path.join(dirpath, "superseded.txt")) if ln.strip()}
            for r in rule.read_results(os.path.join(dirpath, "results.csv")):
                if r["cell_id"] not in skip and r["options_label"] in ("default", "sqp-qn"):
                    old[(r["problem_set"], r["problem"], r["options_label"])] = r
    out = []
    for key in sorted(a0):
        o, n = old.get(key), a0[key]
        if o is None:
            out.append((key, "no TU-1 row", None))
            continue
        f = lambda r: (classify_row(r), r.get("iterations"), r.get("factorizations"))
        if f(o) != f(n):
            out.append((key, f(o), f(n)))
    return out, len(a0)


def foldwise(spec, arms):
    """Section 10.1: per TU-1 fold k, section 5.4 on TRAIN(k) (no census), the selected configuration vs A0 on HELD(k);
    the three comparisons pooled into family signs."""
    fold_of = folds.fold_of(spec)
    a0 = arms["A0"]
    pooled_b, pooled_c, per = {}, {}, {}
    for k in sorted(spec["folds"]):
        train = lambda key: fold_of["%s/%s" % (key[0], key[1])] != k
        held = lambda key: fold_of["%s/%s" % (key[0], key[1])] == k
        sub = {a: {c: o for c, o in rows.items() if train(c)} for a, rows in arms.items()}
        evals, _ = rule.evaluate_all(sub)
        ranked = rule.rank_passing(evals)
        pick = ranked[0]["config"] if ranked else None
        per[k] = rule.config_label(pick) if pick else "A0 (nothing passes)"
        held_base = {c: o for c, o in a0.items() if held(c)}
        if pick is None:
            held_cand = held_base
        else:
            full = rule.build_config(pick, arms)
            held_cand = {c: o for c, o in full.items() if held(c)}
        pooled_b.update(held_base)
        pooled_c.update(held_cand)
    _, tallies, signs = rule.compare(pooled_b, pooled_c, family_of=rule.family2)
    return per, tallies, signs


def _has_rows(root, lane, config):
    d = tu2_run.arm_dir(root, lane, config)
    return any("results.csv" in f for _, _, f in os.walk(d))


def selection(spec, root):
    lane = spec["engines"]["sqp"]["lane"]
    certs = tu2_run.read_certs(root)
    arms = {}
    for a in spec["sqp"]["arms"]:
        config = dict(a["settings"])
        if _has_rows(root, lane, config):
            arms[a["name"]] = tu2_run.attach_verdicts(tu2_run.load_arm(root, lane, config), config, certs)
    evals, missing = rule.evaluate_all(arms)
    ranked = rule.rank_passing(evals)
    return {"arms": arms, "evals": evals, "unbuildable": missing, "ranked": ranked,
            "best_uniform": rule.best_uniform(ranked)}


def main(argv):
    spec = spec_check.load_spec()
    root = argv[1]
    tu1 = argv[argv.index("--tu1-store") + 1] if "--tu1-store" in argv else TU1_STORE
    out = []
    p = out.append
    p("== TU-2 report (informational except arm signs / V1 / V2 / ranking) ==")
    p("\n-- outcome counts per arm and family (section 10.3)")
    for name in ("A0", "A1", "A2", "A3", "A4", "A5", "B0", "B1"):
        c = outcome_counts(spec, root, name)
        if c is None:
            p("%s: not run" % name)
            continue
        for fam in sorted(c):
            p("%s %s: %s" % (name, fam, dict(sorted(c[fam].items()))))
    sel = selection(spec, root)
    arms = sel["arms"]
    p("\n-- family and mode-row signs of each arm against A0 (A0 = the PIN default)")
    for name, rows in sorted(arms.items()):
        if name == "A0":
            continue
        s, m = tu2_run.arm_signs(arms["A0"], rows)
        _, t, _ = rule.compare(arms["A0"], rows, family_of=rule.family2)
        _, mt, _ = rule.compare(arms["A0"], rows, family_of=rule.mode_family)
        p("%s: families %s tallies %s" % (name, s, {f: (v[rule.WIN], v[rule.LOSS], v[rule.TIE]) for f, v in t.items()}))
        p("%s: mode rows %s tallies %s" % (name, m, {f: (v[rule.WIN], v[rule.LOSS], v[rule.TIE]) for f, v in mt.items()}))
        ok, d = rule.v1_solved_count_guard(arms["A0"], rows, rule.family2)
        okm, dm = rule.v1_solved_count_guard(arms["A0"], rows, rule.mode_family)
        p("%s: V1 pooled %s %s; mode rows %s %s" % (name, ok, d, okm, dm))
    p("\n-- configurations (win/loss WL = wins,losses per pooled family; mode signs; V1; V2; cond a-d; passes)")
    for e in sorted(sel["evals"], key=lambda e: e["label"]):
        p("%s | signs %s | W/L %s | mode %s | V1 %s/%s | false certs %d approx %d | cond %s | passes %s | won %d net %d gain %.4f changed %d" % (
            e["label"], e["signs"], {f: (v[rule.WIN], v[rule.LOSS]) for f, v in e["tallies"].items()},
            e["mode_signs"], e["cond"]["c"], "", len(e["false_certificates"]), len(e["approx_certificates"]),
            "".join(k for k, v in e["cond"].items() if v), e["passes"], e["won"], e["net"], e["gain"], e["changed"]))
    p("unbuildable (arm not run): %s" % sel["unbuildable"])
    p("\n-- RANKED PASSING (section 5.4 item 3)")
    for i, e in enumerate(sel["ranked"], 1):
        p("%d. %s signs %s won %d net %d gain %.4f changed %d" % (i, e["label"], e["signs"], e["won"], e["net"], e["gain"], e["changed"]))
    bu = sel["best_uniform"]
    p("best uniform (t_x == t_q): %s" % (bu["label"] if bu else None))
    if "B0" in {n for n in ("B0",) if outcome_counts(spec, root, n)} and outcome_counts(spec, root, "B1"):
        passes, signs, detail = tu2_run.ipm_verdict(spec, root)
        p("\n-- IPM B1 vs B0: passes %s signs %s V1 %s" % (passes, signs, detail))
    p("\n-- fold-wise re-read (section 10.1)")
    if "A0" in arms:
        per, tallies, signs = foldwise(spec, arms)
        p("selected per fold: %s" % per)
        p("pooled held-out signs: %s tallies %s" % (signs, {f: (v[rule.WIN], v[rule.LOSS], v[rule.TIE]) for f, v in tallies.items()}))
    p("\n-- movers TU-1 default (0ee37c2ca) vs A0 (section 10.2)")
    mv, n = movers(spec, root, tu1)
    p("%d of %d A0 cells differ in (class, iterations, factorizations)" % (len(mv), n))
    for key, o, nw in mv:
        p("%s: %s -> %s" % ("/".join(key), o, nw))
    print("\n".join(out))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
