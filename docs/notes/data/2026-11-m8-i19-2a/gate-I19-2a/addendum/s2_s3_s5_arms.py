"""Addendum sections 2, 3 and 5 (arms vs H0): gained cells beside lost cells, tail-line reclassification, objective differences,
and the exploratory placebo-stable-problem re-read.  Read-only on the stores."""
from common import *
import collections, math

CORP = STAGE1 + ["NLP-46"]
h0 = load(STORE_H, "H0", CORP)
data = {a: load(STORE_H, a, CORP) for a in ARMS}
H0s = {c: v for c, v in h0.items() if c[0] != "NLP-46"}
arms_spec = i19_run.arms_by_name(spec)

def view(arm):
    """(h0 view, arm view) over the stage-1 corpora; NLP-46 separately (only P and C1 have rows there)."""
    x = {c: v for c, v in data[arm].items() if c[0] != "NLP-46"}
    xn = {c: v for c, v in data[arm].items() if c[0] == "NLP-46"}
    return x, xn

def st(r):
    return r["status"] if r["ended"] else "no-status(dnf_wall)"
def cell(c):
    return "%s %s/%s" % (c[0], c[2], c[3])

# ------------------------------------------------------------------ placebo-unstable problem names (P vs H0, outcome-level, any cell on TRAIN/CAP)
def outcome_moved(a, b):
    return (a["status"] != b["status"] or a["majors"] != b["majors"] or a["la"] != b["la"]
            or a["eval_f"] != b["eval_f"] or a["eval_jac"] != b["eval_jac"])
unstable = {c[2] for c in H0s if c[0] in LIVE and outcome_moved(h0[c], data["P"][c])}
print("Placebo-unstable problem names (P vs H0, outcome-level on any TRAIN/CAP cell): %d of %d" % (len(unstable), len({c[2] for c in H0s if c[0] in LIVE})))

print("\n## 2a. Lost and GAINED cells per arm (stage-1 corpora incl. SENS-P guard cells; strict = status optimal; acceptable level = optimal or acceptable)")
print("| arm | lost strict | lost (acc level only) | gained strict | gained (acc level only) | RESULTS lost count |")
print("|---|---|---|---|---|---|")
RES_LOST = {"P": 0, "G1": 2, "G2": 1, "G3": 0, "G4": 2, "G5": 3, "G6": 0, "G7": 2, "C1": 20, "C2": 10}
gained_detail = {}
lost_detail = {}
for a in ARMS:
    x, xn = view(a)
    ids = sorted(H0s)
    lost = [c for c in ids if i19_rule.is_strict(H0s[c]) and not i19_rule.is_strict(x[c])]
    lost_acc = [c for c in ids if i19_rule.is_acc(H0s[c]) and not i19_rule.is_acc(x[c]) and c not in lost]
    # harness lost_cells() lists strict loss, else acceptable loss; a cell can be both -> mirror it
    h_lost = i19_rule.lost_cells(H0s, x, ids)
    gain = [c for c in ids if not i19_rule.is_strict(H0s[c]) and i19_rule.is_strict(x[c])]
    gain_acc = [c for c in ids if not i19_rule.is_acc(H0s[c]) and i19_rule.is_acc(x[c]) and c not in gain]
    gained_detail[a] = gain; lost_detail[a] = lost
    n_h_lost = len(h_lost)
    print("| %s | %d | %d | %d | %d | %d (harness lost_cells() recount: %d) |" % (a, len(lost), len(lost_acc), len(gain), len(gain_acc), RES_LOST[a], n_h_lost))
print("\nPer arm lists (stage-1 corpora):")
for a in ARMS:
    x, xn = view(a)
    ids = sorted(H0s)
    print("\n### %s" % a)
    for c in lost_detail[a]:
        print("- LOST %s: %s -> %s%s" % (cell(c), st(H0s[c]), st(x[c]), "  [placebo-unstable problem]" if c[2] in unstable else ""))
    for c in gained_detail[a]:
        print("- GAINED %s: %s -> %s  (majors %d -> %d, LA %d -> %d, f %s -> %s)%s" % (cell(c), st(H0s[c]), st(x[c]), H0s[c]["majors"], x[c]["majors"], H0s[c]["la"], x[c]["la"], H0s[c]["objective"], x[c]["objective"], "  [placebo-unstable problem]" if c[2] in unstable else ""))
    for c in ids:
        if not i19_rule.is_strict(H0s[c]) and i19_rule.is_acc(x[c]) and not i19_rule.is_strict(x[c]) and not i19_rule.is_acc(H0s[c]):
            print("- GAINED (acceptable only) %s: %s -> %s" % (cell(c), st(H0s[c]), st(x[c])))
for a in ("P", "C1"):
    x, xn = view(a)
    ids = sorted(c for c in h0 if c[0] == "NLP-46")
    print("\n### %s on NLP-46 (informational; only P and C1 have NLP-46 rows)" % a)
    for c in ids:
        if i19_rule.is_strict(h0[c]) and not i19_rule.is_strict(xn[c]):
            print("- LOST %s: %s -> %s" % (cell(c), st(h0[c]), st(xn[c])))
    for c in ids:
        if not i19_rule.is_strict(h0[c]) and i19_rule.is_strict(xn[c]):
            print("- GAINED %s: %s -> %s  (majors %d -> %d, LA %d -> %d)" % (cell(c), st(h0[c]), st(xn[c]), h0[c]["majors"], xn[c]["majors"], h0[c]["la"], xn[c]["la"]))
    print("  strict-optimal count H0 %d, %s %d" % (sum(i19_rule.is_strict(h0[c]) for c in ids), a, sum(i19_rule.is_strict(xn[c]) for c in ids)))

print("\n## 2b. Tail lines (harness tail_guard over the stage-1 corpora incl. SENS-P) reclassified by the H0 status of the cell")
print("Classes: baseline-solved (H0 strict optimal; real tail hit), gained-cell (H0 not strict optimal, arm strict optimal), both-failed (neither strict optimal).")
print("| arm | tail lines | cells | baseline-solved (real) lines/cells | gained-cell lines/cells | both-failed lines/cells | RESULTS lines |")
print("|---|---|---|---|---|---|---|")
RES_TAIL = {"P": 0, "G1": 5, "G2": 2, "G3": 1, "G4": 0, "G5": 2, "G6": 3, "G7": 3, "C1": 50, "C2": 38}
tail_detail = {}
for a in ARMS:
    x, xn = view(a)
    tg = i19_rule.tail_guard(H0s, x, sorted(H0s), params["tail_add"], params["tail_mult"])
    cl = {"real": [], "gained": [], "both-failed": []}
    for (c, cnt, hv, xv) in tg:
        k = "real" if i19_rule.is_strict(H0s[c]) else ("gained" if i19_rule.is_strict(x[c]) else "both-failed")
        cl[k].append((c, cnt, hv, xv))
    tail_detail[a] = cl
    cells = lambda L: len({t[0] for t in L})
    print("| %s | %d | %d | %d / %d | %d / %d | %d / %d | %d |" % (a, len(tg), len({t[0] for t in tg}), len(cl["real"]), cells(cl["real"]), len(cl["gained"]), cells(cl["gained"]), len(cl["both-failed"]), cells(cl["both-failed"]), RES_TAIL[a]))
for a in ("P", "G1", "G2", "G3", "G4", "G5", "G6", "G7"):
    x, xn = view(a)
    print("\n### %s tail lines" % a)
    for k, L in tail_detail[a].items():
        for (c, cnt, hv, xv) in L:
            print("- [%s] %s %s %d -> %d   (H0 %s, %s %s)" % (k, cell(c), cnt, hv, xv, st(H0s[c]), a, st(x[c])))
for a in ("C1", "C2"):
    x, xn = view(a)
    print("\n### %s tail lines by class: real %d, gained %d, both-failed %d; real-class lines on cells (distinct problems): %s" % (
        a, len(tail_detail[a]["real"]), len(tail_detail[a]["gained"]), len(tail_detail[a]["both-failed"]),
        sorted({t[0][2] for t in tail_detail[a]["real"]})))
# NLP-46 tail lines P and C1
for a in ("P", "C1"):
    x, xn = view(a)
    ids = sorted(c for c in h0 if c[0] == "NLP-46")
    tg = i19_rule.tail_guard(h0, xn, ids, params["tail_add"], params["tail_mult"])
    print("\n### %s NLP-46 tail lines (informational)" % a)
    for (c, cnt, hv, xv) in tg:
        k = "real" if i19_rule.is_strict(h0[c]) else ("gained" if i19_rule.is_strict(xn[c]) else "both-failed")
        print("- [%s] %s %s %d -> %d (H0 %s, %s %s)" % (k, cell(c), cnt, hv, xv, st(h0[c]), a, st(xn[c])))

print("\n## 3. Objective differences: both strict optimal, |f_arm - f_H0| > 1e-6 * max(1, |f_H0|)")
objdiff = {}
for a in ARMS:
    x, xn = view(a)
    pairs = [(c, H0s[c], x[c]) for c in sorted(H0s)] + ([(c, h0[c], xn[c]) for c in sorted(c for c in h0 if c[0] == "NLP-46")] if a in ("P", "C1") else [])
    L = []; miss = 0
    for c, hr, xr in pairs:
        if i19_rule.is_strict(hr) and i19_rule.is_strict(xr):
            fh, fx = fnum(hr["objective"]), fnum(xr["objective"])
            if fh is None or fx is None:
                miss += 1; continue
            if abs(fx - fh) > 1e-6 * max(1.0, abs(fh)):
                L.append((c, fh, fx))
    objdiff[a] = L
    print("\n### %s: %d cells%s" % (a, len(L), ("  (%d both-optimal cells had no parseable objective)" % miss) if miss else ""))
    for c, fh, fx in L:
        print("- %s: f_H0 = %.10g, f_%s = %.10g  (%s; |df|/max(1,|f_H0|) = %.3g)" % (cell(c), fh, a, fx, "arm lower" if fx < fh else "arm higher", abs(fx - fh) / max(1.0, abs(fh))))
print("\nUnion of problem names with an objective difference (any arm): %s" % sorted({c[2] for L in objdiff.values() for c, _, _ in L}))

# ------------------------------------------------------------------ section 5 (exploratory)
print("\n## 5. Exploratory re-read on placebo-stable problems (ONE placebo draw only; NOT the ensemble rule, NOT a re-verdict)")
print("A problem is 'placebo-stable' when P shows no outcome-level move on any of its TRAIN-238 / CAP-238-1 / CAP-238-01 cells (%d of %d names)." % (len({c[2] for c in H0s if c[0] in LIVE}) - len(unstable), len({c[2] for c in H0s if c[0] in LIVE})))
stable_ids = lambda corpus: [c for c in sorted(H0s) if c[0] == corpus and c[2] not in unstable]
def diffsol(a):
    return {c for c, _, _ in objdiff[a]}
def tally_ids(a, ids, drop_hs025=False, drop_diffsol=False):
    x, _ = view(a)
    if drop_hs025:
        ids = [c for c in ids if c[2] != "hs025"]
    if drop_diffsol:
        ids = [c for c in ids if c not in diffsol(a)]
    t = i19_rule.tally(H0s, x, ids)
    return t["LA"]["wins"], t["LA"]["losses"], t["EV"]["wins"], t["EV"]["losses"], len(ids)
print("\nWins-losses over placebo-stable cells (cell rule: 10%% integer rule, both strict optimal). hs025 note: its cells tie at zero factorizations; the 'no hs025' columns remove it.")
print("| arm | corpus | stable cells | LA W-L | EV W-L | LA W-L no hs025 | EV W-L no hs025 | LA W-L excl. different-solution cells | EV W-L excl. different-solution cells |")
print("|---|---|---|---|---|---|---|---|---|")
for a in ARMS:
    for corpus in LIVE:
        if a == "P" or (a == "G6" and corpus == "TRAIN-238"): continue   # P is stable by construction; G6's live set is the CAP corpora
        ids = stable_ids(corpus)
        lw, ll, ew, el, n = tally_ids(a, ids)
        lw2, ll2, ew2, el2, n2 = tally_ids(a, ids, True)
        lw3, ll3, ew3, el3, n3 = tally_ids(a, ids, False, True)
        print("| %s | %s | %d | %d-%d | %d-%d | %d-%d | %d-%d | %d-%d | %d-%d |" % (a, corpus, n, lw, ll, ew, el, lw2, ll2, ew2, el2, lw3, ll3, ew3, el3))
print("\nPer-arm effects split by placebo stability of the problem (SENS-P cells are outside the TRAIN/CAP stability set and are listed on their own)")
def klass(c):
    return "sens" if c[0] == "SENS-P" else ("unstable" if c[2] in unstable else "stable")
print("| arm | lost: stable problems | lost: unstable | lost: SENS-P | gained: stable | gained: unstable | gained: SENS-P | real tail lines: stable | real tail lines: unstable | real tail lines: SENS-P |")
print("|---|---|---|---|---|---|---|---|---|---|")
for a in ARMS:
    L = lost_detail[a]; G = gained_detail[a]; R = tail_detail[a]["real"]
    cnt = lambda X, k, f=lambda t: t: sum(klass(f(t)) == k for t in X)
    lost_stable = ", ".join(sorted({cell(c) for c in L if klass(c) == "stable"})) or "0"
    gst = ", ".join(sorted({cell(c) for c in G if klass(c) == "stable"})) or "0"
    rs = "; ".join("%s %s %d->%d" % (cell(c), cnt_, hv, xv) for c, cnt_, hv, xv in R if klass(c) == "stable") or "0"
    print("| %s | %s | %d | %d | %s | %d | %d | %s | %d | %d |" % (a, lost_stable, cnt(L, "unstable"), cnt(L, "sens"), gst, cnt(G, "unstable"), cnt(G, "sens"), rs, cnt(R, "unstable", lambda t: t[0]), cnt(R, "sens", lambda t: t[0])))
print("\nDistinct problem names behind each arm's V1 losses (one vote per problem name, B4 rule 5): ")
for a in ARMS:
    print("- %s: %s" % (a, sorted({c[2] for c in lost_detail[a] if c[0] != 'SENS-P'}) or "none"))

print("\nStable-problem cost LOSSES (10% integer cell rule, both strict optimal) for G3, G5, G6 (the arms the review's B3 discusses):")
for a in ("G3", "G5", "G6"):
    x, _ = view(a)
    for c in sorted(H0s):
        if c[2] in unstable or c[0] == "SENS-P" or c[0] not in LIVE: continue
        if i19_rule.is_strict(H0s[c]) and i19_rule.is_strict(x[c]):
            for cur in ("LA", "EV"):
                if i19_rule.cell_verdict(i19_rule.cur_value(x[c], cur), i19_rule.cur_value(H0s[c], cur)) == i19_rule.LOSS:
                    print("- %s %s %s: H0 %d -> %s %d" % (a, cell(c), cur, i19_rule.cur_value(H0s[c], cur), a, i19_rule.cur_value(x[c], cur)))
