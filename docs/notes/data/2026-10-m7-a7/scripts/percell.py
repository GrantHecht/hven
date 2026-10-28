# Per-cell table, base vs arm, run A: outcome, majors, factorizations, facts/major, objective, wall.
import csv
import sys

merged, base, arm, s = sys.argv[1:5]


def load(label, f):
    p = "{}/{}/{}-run-A/{}".format(merged, label, s, f)
    try:
        return {r["problem"] if "problem" in r else r["cell_id"].split("/")[1]: r
                for r in csv.DictReader(l for l in open(p) if not l.startswith("#"))}
    except FileNotFoundError:
        return {}


def side(label):
    out = {}
    for r in csv.DictReader(l for l in open("{}/{}/{}-run-A/counters-hven-sqp.csv".format(merged, label, s))
                            if not l.startswith("#")):
        out[r["cell_id"].split("/")[1]] = r
    return out


rb, ra, sb, sa = load(base, "results.csv"), load(arm, "results.csv"), side(base), side(arm)
print("| cell | {b} outcome | {a} outcome | {b} maj | {a} maj | {b} facts | {a} facts | {b} f/maj | {a} f/maj | {b} f | {a} f | {b} wall | {a} wall |".format(b=base, a=arm))
print("|" + "---|" * 13)
for p in sorted(rb):
    b, a = rb[p], ra[p]
    def fm(r, sd):
        try:
            return "{:.1f}".format(int(r["factorizations"]) / max(1, int(sd[p]["major_iters"])))
        except (KeyError, ValueError):
            return "-"
    def mj(sd):
        return sd[p]["major_iters"] if p in sd else "-"
    print("| {} | {} | {} | {} | {} | {} | {} | {} | {} | {} | {} | {} | {} |".format(
        p, b["outcome"], a["outcome"], mj(sb), mj(sa), b["factorizations"], a["factorizations"],
        fm(b, sb), fm(a, sa), b["objective"][:14], a["objective"][:14], b["wall_s"][:6], a["wall_s"][:6]))
