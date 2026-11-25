"""Addendum section 1: P (placebo) vs H0, moved rows split into bookkeeping-only and outcome-level, per corpus, with the
problem-name lists and the H0-counter association tables.  Read-only on the stores."""
from common import *
import collections, csv

CORP = ["TRAIN-238", "CAP-238-1", "CAP-238-01", "NLP-46"]
h0 = load(STORE_H, "H0", CORP); p = load(STORE_H, "P", CORP)
assert set(h0) == set(p)

def outcome_moved(a, b):
    return (a["status"] != b["status"] or a["majors"] != b["majors"] or a["la"] != b["la"]
            or a["eval_f"] != b["eval_f"] or a["eval_jac"] != b["eval_jac"])

def differing_cols(a, b):
    skip = set(params["nondeterministic"]); new = tuple(params["new_prefixes"]); out = []
    for k in set(a["cols"]) | set(b["cols"]):
        bare = k[len("side."):] if k.startswith("side.") else k
        if bare in skip or bare.startswith(new):
            continue
        if not i19_rule.columns_agree(bare, a["cols"].get(k), b["cols"].get(k)):
            out.append(bare)
    return out

cls = {}   # cid -> "stable" | "bookkeeping" | "outcome"
for cid in h0:
    mv = i19_rule.moved(h0[cid], p[cid], params)
    om = outcome_moved(h0[cid], p[cid])
    # outcome-level movement implies the harness moved() test fires (status and eval_f/eval_jac/factorizations/iterations are compared columns);
    # majors differences are compared through side.major_iters.  Assert the implication so nothing is silently lost.
    assert (not om) or mv, cid
    cls[cid] = "outcome" if om else ("bookkeeping" if mv else "stable")

print("## 1a. P vs H0 moved rows by corpus (harness moved() test = 'moved'; outcome-level = status or majors or factorizations or eval_f or eval_jac differs)")
print("| corpus | cells | moved | bookkeeping-only | outcome-level | outcome-level % of cells | status changes (all) | majors changed, both optimal |")
print("|---|---|---|---|---|---|---|---|")
for c in CORP:
    ids = [k for k in h0 if k[0] == c]
    n = len(ids); nb = sum(cls[k] == "bookkeeping" for k in ids); no = sum(cls[k] == "outcome" for k in ids)
    st = sum(h0[k]["status"] != p[k]["status"] for k in ids)
    both_opt = [k for k in ids if h0[k]["status"] == "optimal" and p[k]["status"] == "optimal"]
    mj = sum(h0[k]["majors"] != p[k]["majors"] for k in both_opt)
    print("| %s | %d | %d | %d | %d | %.1f%% | %d | %d of %d |" % (c, n, nb + no, nb, no, 100.0 * no / n, st, mj, len(both_opt)))
print("PILOT-62: see section 1c (rows carry only status/outcome/factorizations).")

print("\n## 1b. Status changes (H0 -> P) and what the bookkeeping-only rows differ in")
for c in CORP:
    ids = [k for k in h0 if k[0] == c]
    print("\n%s status changes:" % c)
    for k in sorted(ids):
        if h0[k]["status"] != p[k]["status"]:
            print("  %s/%s: %s -> %s  (majors %d->%d, LA %d->%d)" % (k[2], k[3], h0[k]["status"], p[k]["status"], h0[k]["majors"], p[k]["majors"], h0[k]["la"], p[k]["la"]))
    cnt = collections.Counter()
    for k in ids:
        if cls[k] == "bookkeeping":
            for b in differing_cols(h0[k], p[k]):
                cnt[b] += 1
    nb = sum(cls[k] == "bookkeeping" for k in ids)
    print("%s bookkeeping-only rows (%d): differing columns, count of rows (top 12): %s" % (c, nb, cnt.most_common(12)))
    # any bookkeeping-only row that differs in a column other than refinement / polish / residual class?
    other = collections.Counter()
    for k in ids:
        if cls[k] == "bookkeeping":
            for b in differing_cols(h0[k], p[k]):
                other[b] += 1
    # (the full per-column table is in the raw output file)
    print("  all differing columns on bookkeeping-only rows:", dict(sorted(other.items(), key=lambda kv: -kv[1])))

print("\n## 1c. PILOT-62 (rows: problem,label,rc,status,outcome,factorizations,certified only)")
def pilot(arm):
    # P's pilot labels are the pilot's own label with "+trust_region_initial=1.000000000000001" appended (merge_pilot_label); strip it to pair cells.
    rows = list(csv.DictReader(open("%s/PILOT-62/%s/pilot_rows.csv" % (STORE_H, arm))))
    return {(r["problem"], r["label"].replace("+trust_region_initial=1.000000000000001", "")): r for r in rows}
ph, pp = pilot("H0"), pilot("P")
assert set(ph) == set(pp), (len(ph), len(pp))
n = len(ph)
st = sum(ph[k]["status"] != pp[k]["status"] for k in ph)
oc = sum(ph[k]["outcome"] != pp[k]["outcome"] for k in ph)
fa = sum(ph[k]["factorizations"] != pp[k]["factorizations"] for k in ph)
mv = sum((ph[k]["status"], ph[k]["outcome"], ph[k]["factorizations"]) != (pp[k]["status"], pp[k]["outcome"], pp[k]["factorizations"]) for k in ph)
print("cells %d; moved on (status, outcome, factorizations): %d; status changed %d; outcome class changed %d; factorizations changed %d" % (n, mv, st, oc, fa))
print("H0 outcomes", dict(collections.Counter(r["outcome"] for r in ph.values())), "| P outcomes", dict(collections.Counter(r["outcome"] for r in pp.values())))
print("H0 status", dict(collections.Counter(r["status"] for r in ph.values())), "| P status", dict(collections.Counter(r["status"] for r in pp.values())))
print("Bookkeeping-only vs outcome-level cannot be separated here (no counters rows); every moved pilot cell changed status or the factorization count, i.e. all %d moved cells are outcome-level on the only columns recorded." % mv)
print("pilot cells with status change: %d of %d" % (st, n))


print("\n## 1b'. Bookkeeping-only rows that differ in a column OUTSIDE the refinement/polish/backend-ledger family")
fam_prefix = ("eqp_polish", "backend_solve", "border_refine", "terminal_polish", "dense_factorizations", "harness_factorizations_total", "harness_qpsum", "eqp_refine", "verdict_refine", "backend_symbolic", "symbolic_analys")
for c in CORP:
    ids = [k for k in h0 if k[0] == c and cls[k] == "bookkeeping"]
    odd = [k for k in ids if any(not b.startswith(fam_prefix) for b in differing_cols(h0[k], p[k]))]
    oc = collections.Counter(b for k in odd for b in differing_cols(h0[k], p[k]) if not b.startswith(fam_prefix))
    print("%s: %d of %d bookkeeping-only rows also differ in a non-refinement column: %s" % (c, len(odd), len(ids), dict(oc)))

print("\n## 1d. Problem-name lists (outcome-level movers)")
allnames = set()
for c in CORP:
    nm = sorted({k[2] for k in h0 if k[0] == c and cls[k] == "outcome"})
    print("\n%s (%d problems): %s" % (c, len(nm), ", ".join(nm)))
tr = sorted({k[2] for k in h0 if k[0] in LIVE and cls[k] == "outcome"})
tot = sorted({k[2] for k in h0 if k[0] in LIVE})
print("\nUnion over TRAIN-238, CAP-238-1, CAP-238-01: %d of %d problem names (%.1f%%): %s" % (len(tr), len(tot), 100.0 * len(tr) / len(tot), ", ".join(tr)))
# cells (TRAIN+CAP) share
tc = [k for k in h0 if k[0] in LIVE]
print("Cells TRAIN+CAP-1+CAP-01: %d of %d outcome-level (%.1f%%)" % (sum(cls[k] == "outcome" for k in tc), len(tc), 100.0 * sum(cls[k] == "outcome" for k in tc) / len(tc)))

print("\n## 1e. H0-counter association (H0 rows; outcome-level movers vs the rest = bookkeeping-only + unmoved)")
def ev(k, name):
    v = i19_rule.col(h0[k], name)
    f = fnum(v)
    return 0 if f is None else f
tests = [("restoration entered (restoration_entries>0)", lambda k: h0[k]["restoration_entries"] > 0),
         ("qp_numerical_exits>0", lambda k: ev(k, "qp_numerical_exits") > 0),
         ("eqp_residual_refusals>0", lambda k: ev(k, "eqp_residual_refusals") > 0),
         ("suspect_escalations>0", lambda k: ev(k, "suspect_escalations") > 0),
         ("reject_funnel_ceiling>0", lambda k: h0[k]["reject_funnel_ceiling"] > 0)]
def assoc(label, ids):
    mv = [k for k in ids if cls[k] == "outcome"]; st = [k for k in ids if cls[k] != "outcome"]
    print("\n%s: %d cells, %d outcome-level, %d other" % (label, len(ids), len(mv), len(st)))
    print("| H0 event | outcome-level cells with event | share of outcome-level | other cells with event | share of other | P(outcome-level given event) | P(outcome-level given no event) |")
    print("|---|---|---|---|---|---|---|")
    for nm, f in tests:
        a = sum(f(k) for k in mv); b = sum(f(k) for k in st)
        ev_cells = a + b; non = len(ids) - ev_cells
        print("| %s | %d | %.0f%% | %d | %.0f%% | %d/%d | %d/%d |" % (nm, a, 100.0 * a / max(len(mv), 1), b, 100.0 * b / max(len(st), 1), a, ev_cells, len(mv) - a, non))
    med = lambda xs: sorted(xs)[len(xs) // 2] if xs else None
    print("median H0 majors: outcome-level %s, other %s" % (med([h0[k]["majors"] for k in mv]), med([h0[k]["majors"] for k in st])))
for c in CORP:
    assoc(c, [k for k in h0 if k[0] == c])
assoc("TRAIN+CAP-1+CAP-01 pooled", tc)
