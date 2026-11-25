#!/usr/bin/env python3
"""TU-3 reader (DECLARATION-TU-3.md section 7). Pure function of the I19-2a store; reads no HOLDOUT; runs no solver.

    python3 -B tu3_read.py --store STORE-H --harness HARNESS-CHECKOUT --baseline H0 --out DIR
Case H0: baseline arm H0. rule_tu1.py is `git show tu1:tu/rule.py` verbatim (the TU-1 cell rule).
The loader is the harness's own `arm_rows` / `rec_from_rows`; unlike `load_arm_data` it does not stop on a
results row with no counters side row (only dnf_wall rows lack one; such a row never reaches, so no side column is read).
"""
import argparse, csv, hashlib, os, subprocess, sys, collections

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import rule_tu1 as tu1rule  # noqa: E402

CAND = ["G1", "G2", "G3", "G4", "G5", "G6", "G7", "C1", "C2"]
LABELS = {"G1": "trust_region_growth=kRatioByStepType", "G2": "trust_region_growth_after_soc=false",
          "G3": "trust_region_rejection_memory=3", "G4": "trust_region_growth_ratio_max=3",
          "G5": "trust_region_shrink_to_step=true", "G6": "trust_region_growth_on_step_limit=false",
          "G7": "trust_region_growth_funnel_guard=true", "C1": "SOC off + kAnyAccepted (TU-2 A4)", "C2": "SOC off (TU-2 A1)"}
CLUSTERS = {  # declared members, section 3 verbatim
    "K1": "BIGBANK DALLASM SMBANK STEENBRB STEENBRC STEENBRD STEENBRE STEENBRG",
    "K2": "ACOPP118 ACOPP57 ACOPR118 ACOPR57 SAWPATH",
    "K3": "ACOPP118 ACOPP57 ACOPR118 ACOPR57 NET3 SAWPATH TAX213322 TAX53322",
    "K4": "BIGBANK MSS2 MSS3 READING8 SAWPATH STEENBRC STEENBRE STEENBRG TAX213322",
    "K5": "YORKNET hs011 hs014 hs025 hs027 hs028"}
CLUSTERS = {k: v.split() for k, v in CLUSTERS.items()}
BASES = ("default", "sqp-qn")
TAILS = ("majors", "EV", "rejected_steps", "reject_funnel_ceiling")
FLOOR_NET, FLOOR_NAMES = 3, 2
OUT = []


def p(*a):
    OUT.append(" ".join(str(x) for x in a))


def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def load(store, corpus, arm, i19_run, i19_rule, files):
    rows, thrown = i19_run.arm_rows(store, corpus, arm)
    out = {}
    for r, side, d in rows:
        if r.get("split", "TRAIN") != "TRAIN":
            raise SystemExit("STOP: non-TRAIN row (HOLDOUT is never read)")
        files.add(os.path.join(d, "results.csv"))
        if side is not None:
            files.add(os.path.join(d, "counters-hven-sqp.csv"))
        base = tu1rule.base_label(r["options_label"])
        key = (r["problem_set"], r["problem"], base)
        if key in out:
            raise SystemExit("STOP: duplicate cell %s %s %s" % (corpus, arm, key))
        rec = i19_rule.rec_from_rows(r, side)
        rec["outcome"] = r["outcome"]
        rec["has_side"] = side is not None
        rec["fact"] = r.get("factorizations")
        out[key] = rec
    return out


def reached(rec):
    return rec["outcome"] == "optimal"


def verdict(c, b):
    cr, br = reached(c), reached(b)
    return tu1rule.cell_verdict(cr, int(c["fact"]) if cr else None, br, int(b["fact"]) if br else None)


def strict(r):
    return r["status"] == "optimal"


def verdict_strict(c, b):
    """I19-2a strict-optimal reading (acceptable does not reach); reported alongside, not decisive."""
    cr, br = strict(c), strict(b)
    return tu1rule.cell_verdict(cr, int(c["fact"]) if cr else None, br, int(b["fact"]) if br else None)


def acc(r):
    return r["status"] in ("optimal", "acceptable")


def tailcols(r):
    return {"majors": r["majors"], "EV": r["eval_f"] + r["eval_jac"], "rejected_steps": r["rejected_steps"],
            "reject_funnel_ceiling": r["reject_funnel_ceiling"]}


def tally(items):
    t = {"WIN": 0, "LOSS": 0, "TIE": 0}
    for v in items:
        t[v] += 1
    return t["WIN"], t["LOSS"], t["TIE"]


def sg(w, l):
    return tu1rule.family_sign(w, l)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--store", required=True)
    ap.add_argument("--harness", required=True)
    ap.add_argument("--baseline", default="H0")
    ap.add_argument("--out", required=True)
    ap.add_argument("--binary", default=None)
    ap.add_argument("--binaries-txt", default=None)
    a = ap.parse_args()
    sys.dont_write_bytecode = True
    sys.path.insert(0, os.path.join(a.harness, "tu"))
    import i19_run, i19_rule  # noqa: E402
    store, base_arm = a.store, a.baseline
    TR, NL = "TRAIN-238", "NLP-46"

    # ---- section 8 stop checks: pin / harness sha uniform across every row file, binary sha vs BINARIES
    pins, hsha = set(), set()
    for dp, _, fs in os.walk(store):
        if "results.csv" in fs and ("/%s/" % TR in dp + "/" or "/%s/" % NL in dp + "/"):
            for ln in open(os.path.join(dp, "results.csv")):
                if ln.startswith("# hven.pin.sha:"):
                    pins.add(ln.split(":", 1)[1].strip())
                if ln.startswith("# harness.sha:"):
                    hsha.add(ln.split(":", 1)[1].strip())
    p("PIN", sorted(pins), "HARNESS_SHA", sorted(hsha))
    if len(pins) != 1 or len(hsha) != 1:
        raise SystemExit("STOP: store pin/harness sha not uniform")
    if a.binary and a.binaries_txt:
        want = [ln.split()[4] for ln in open(a.binaries_txt) if ln.startswith("I19 lane H ")][0]
        got = sha(a.binary)
        p("LANE-H BINARY sha256", got, "BINARIES.txt", want, "MATCH" if got == want else "MISMATCH")
        if got != want:
            raise SystemExit("STOP: binary sha disagrees with BINARIES.txt")
    # ---- clusters verify
    r = subprocess.run([sys.executable, "-B", os.path.join(a.harness, "tu", "clusters.py"), "--verify"], capture_output=True, text=True,
                       env=dict(os.environ, PYTHONDONTWRITEBYTECODE="1"))
    p("clusters.py --verify rc", r.returncode, r.stdout.strip())
    if r.returncode:
        raise SystemExit("STOP: clusters --verify failed")
    spec_members = {}
    for ln in subprocess.run([sys.executable, "-B", os.path.join(a.harness, "tu", "clusters.py")], capture_output=True, text=True,
                             env=dict(os.environ, PYTHONDONTWRITEBYTECODE="1")).stdout.splitlines():
        head, rest = ln.split(":", 1)
        spec_members[head.split()[0][:2]] = sorted(x.split("/")[1] for x in rest.split())
    for k, v in CLUSTERS.items():
        if sorted(v) != spec_members[k]:
            raise SystemExit("STOP: %s declared members disagree with clusters.py" % k)
    p("declared member lists == clusters.py members for K1-K5")

    files = set()
    data = {}
    arms = [base_arm, "P"] + CAND
    for corpus in (TR, NL):
        for arm in arms:
            data[(corpus, arm)] = load(store, corpus, arm, i19_run, i19_rule, files)
    for corpus in (TR, NL):
        for arm in arms:
            p("ROWS", corpus, arm, len(data[(corpus, arm)]), "no-side-row", sum(1 for v in data[(corpus, arm)].values() if not v["has_side"]))

    # sd-roster collision check (membership by name)
    allnames = {n for v in CLUSTERS.values() for n in v}
    sd_hits = sorted({k[1] for k in data[(TR, base_arm)] if k[0] != "hs" and k[1] in allnames})
    p("TRAIN-238 sd-roster cells whose problem name is a cluster member:", sd_hits or "none")

    def cluster_cells(k):
        cells = []
        for name in CLUSTERS[k]:
            if name.startswith("hs"):
                for b in BASES:
                    cells.append((TR, "hs", name, b))
            else:
                for b in BASES:
                    cells.append((NL, "cutest", name, b))
        return cells

    def key_of(c):
        return (c[1], c[2], c[3])

    # baseline coverage (stop)
    for k in CLUSTERS:
        for c in cluster_cells(k):
            if key_of(c) not in data[(c[0], base_arm)]:
                raise SystemExit("STOP: baseline row missing for %s %s" % (k, c))
    p("baseline coverage: every cluster cell has a %s row" % base_arm)
    cells_all = {k: cluster_cells(k) for k in CLUSTERS}
    p("realized cell counts:", {k: len(v) for k, v in cells_all.items()})

    # families
    fam_cells = {"HS": [(TR, "hs", n[2], n[3]) for n in [(0, 0, x[1], x[2]) for x in data[(TR, base_arm)] if x[0] == "hs"]],
                 "CUTEst": [(NL,) + k for k in sorted(data[(NL, base_arm)])]}
    fam_cells["HS"] = [(TR,) + k for k in sorted(k for k in data[(TR, base_arm)] if k[0] == "hs")]
    p("family sizes: HS", len(fam_cells["HS"]), "CUTEst", len(fam_cells["CUTEst"]))

    def get(arm, c):
        return data[(c[0], arm)].get(key_of(c))

    def score(arm, cells):
        """list of dict per scored cell; list of NOT SCORED cells."""
        rows, missing = [], []
        for c in cells:
            x, h = get(arm, c), get(base_arm, c)
            if x is None:
                missing.append(c)
                continue
            v = verdict(x, h)
            rows.append({"cell": c, "x": x, "h": h, "v": v})
        return rows, missing

    def tails(rows, both_def):
        out = []
        for r in rows:
            x, h = r["x"], r["h"]
            if both_def == "reach" and not (reached(x) and reached(h)):
                continue
            if both_def == "ended" and not (x["ended"] and h["ended"]):
                continue
            tx, th = tailcols(x), tailcols(h)
            for n in TAILS:
                if tx[n] >= max(th[n] + 20, 3 * th[n]):
                    out.append((r["cell"], n, th[n], tx[n]))
        return out

    def v1(rows):
        lost, ok = [], True
        detail = {}
        for b in BASES:
            rb = [r for r in rows if r["cell"][3] == b]
            sx, sh = sum(strict(r["x"]) for r in rb), sum(strict(r["h"]) for r in rb)
            ax, ah = sum(acc(r["x"]) for r in rb), sum(acc(r["h"]) for r in rb)
            detail[b] = (sh, sx, ah, ax)
            if not (sx >= sh and ax >= ah):
                ok = False
        for r in rows:
            if strict(r["h"]) and not strict(r["x"]):
                lost.append((r["cell"], "strict", r["h"]["status"], r["x"]["status"]))
            elif acc(r["h"]) and not acc(r["x"]):
                lost.append((r["cell"], "acceptable", r["h"]["status"], r["x"]["status"]))
        return ok, detail, lost

    def floor(rows):
        net_by_name = collections.Counter()
        win_names = set()
        for r in rows:
            n = r["cell"][2]
            if r["v"] == "WIN":
                net_by_name[n] += 1
                win_names.add(n)
            elif r["v"] == "LOSS":
                net_by_name[n] -= 1
        pos = sorted(n for n, v in net_by_name.items() if v > 0)
        return pos, sorted(win_names)

    def family_read(arm, fam):
        rows, missing = score(arm, fam_cells[fam])
        w, l, t = tally(r["v"] for r in rows)
        res = {"w": w, "l": l, "t": t, "sign": sg(w, l), "missing": len(missing), "n": len(fam_cells[fam]),
               "full": not missing}
        for b in BASES:
            wb, lb, _ = tally(r["v"] for r in rows if r["cell"][3] == b)
            res[b] = (wb, lb)
        return res

    def evaluate(arm, k, famreads):
        cells = cells_all[k]
        rows, missing = score(arm, cells)
        w, l, t = tally(r["v"] for r in rows)
        net = w - l
        pos_names, win_names = floor(rows)
        tl_reach, tl_ended = tails(rows, "reach"), tails(rows, "ended")
        v1ok, v1det, lost = v1(rows)
        per_base = {b: tally(r["v"] for r in rows if r["cell"][3] == b) for b in BASES}
        covered = not missing and rows
        fh, fc = famreads["HS"], famreads["CUTEst"]
        fam_ok = fh["full"] and fc["full"] and fh["sign"] >= 0 and fc["sign"] >= 0
        reasons = []
        if not rows:
            reasons.append("NOT COVERED (no rows for this arm on the cluster's cells)")
        elif missing:
            reasons.append("PARTIAL: %d of %d cluster cells NOT SCORED" % (len(missing), len(cells)))
        if sg(w, l) != 1:
            reasons.append("cluster sign %+d (needs +1)" % sg(w, l))
        if not (fh["full"] and fc["full"]):
            reasons.append("family sign UNEVALUABLE (HS %d/%d cells scored, CUTEst %d/%d): no label without the two family reads" % (
                fh["n"] - fh["missing"], fh["n"], fc["n"] - fc["missing"], fc["n"]))
        else:
            if fh["sign"] < 0:
                reasons.append("SQP-HS family sign -1")
            if fc["sign"] < 0:
                reasons.append("SQP-CUTEst family sign -1")
        if tl_reach:
            reasons.append("R1 tail breach (%d)" % len(tl_reach))
        if not v1ok:
            reasons.append("V1 loss (%d lost cell(s))" % len(lost))
        if net < FLOOR_NET:
            reasons.append("net %d < %d" % (net, FLOOR_NET))
        if len(pos_names) < FLOOR_NAMES:
            reasons.append("net-positive problem names %d < %d" % (len(pos_names), FLOOR_NAMES))
        return {"arm": arm, "k": k, "rows": rows, "missing": missing, "w": w, "l": l, "t": t, "net": net, "sign": sg(w, l),
                "pos_names": pos_names, "win_names": win_names, "tl_reach": tl_reach, "tl_ended": tl_ended, "v1ok": v1ok,
                "v1det": v1det, "lost": lost, "per_base": per_base, "reasons": reasons, "pass": not reasons}

    famreads = {arm: {f: family_read(arm, f) for f in ("HS", "CUTEst")} for arm in ["P"] + CAND}
    p("")
    p("== FAMILY READS (whole families, cell rule, vs %s; wall-dependent dnf_wall cells are not reached)" % base_arm)
    for arm in ["P"] + CAND:
        for f in ("HS", "CUTEst"):
            r = famreads[arm][f]
            p("FAMILY", arm, f, "scored %d/%d" % (r["n"] - r["missing"], r["n"]), "W/L/T %d/%d/%d" % (r["w"], r["l"], r["t"]),
              "sign %+d" % r["sign"], "default(W/L) %s sqp-qn(W/L) %s" % (r["default"], r["sqp-qn"]),
              "" if r["full"] else "NOT FULLY SCORED")

    results = {}
    for arm in ["P"] + CAND:
        for k in CLUSTERS:
            results[(arm, k)] = evaluate(arm, k, famreads[arm])
    p("")
    p("== PER CANDIDATE x CLUSTER")
    for arm in ["P"] + CAND:
        for k in CLUSTERS:
            e = results[(arm, k)]
            n = len(cells_all[k])
            p("%s %s scored %d/%d W/L/T %d/%d/%d net %+d sign %+d | per-base (W,L,T) default %s sqp-qn %s | net-positive names %s | win names %s" % (
                arm, k, len(e["rows"]), n, e["w"], e["l"], e["t"], e["net"], e["sign"], e["per_base"]["default"], e["per_base"]["sqp-qn"],
                e["pos_names"], e["win_names"]))
            sw, sl, _ = tally(verdict_strict(r["x"], r["h"]) for r in e["rows"])
            p("   strict-optimal reading (informational) W/L %d/%d net %+d" % (sw, sl, sw - sl))
            p("   R1 breaches (both reach): %d %s ; (informational, both-ended definition: %d)" % (
                len(e["tl_reach"]), [(c[2] + "/" + c[3], n_, h, x) for c, n_, h, x in e["tl_reach"]], len(e["tl_ended"])))
            if e["tl_ended"] != e["tl_reach"]:
                p("   both-ended-only breaches:", [(c[2] + "/" + c[3], n_, h, x) for c, n_, h, x in e["tl_ended"] if (c, n_, h, x) not in e["tl_reach"]])
            p("   V1 (strict base->arm, acc base->arm) per base label:", e["v1det"], "lost:", [(c[2] + "/" + c[3], lv, h, x) for c, lv, h, x in e["lost"]])
            if e["missing"]:
                p("   NOT SCORED cells (%d): %s" % (len(e["missing"]), ", ".join("%s/%s" % (c[2], c[3]) for c in e["missing"])))
            p("   decision:", "LABEL" if e["pass"] else "NONE", "; ".join(e["reasons"]))
    # placebo control
    p("")
    p("== P CONTROL (section 4 item 5): P through items 1-4 on each cluster")
    noise = {}
    for k in CLUSTERS:
        e = results[("P", k)]
        noise[k] = e["sign"] == 1 and e["net"] >= 3
        p("P", k, "scored %d/%d" % (len(e["rows"]), len(cells_all[k])), "W/L/T %d/%d/%d" % (e["w"], e["l"], e["t"]), "net %+d sign %+d" % (e["net"], e["sign"]),
          "NOISE-LIMITED (sign +1 and net>=3)" if noise[k] else "not noise-limited", "| P items 1-4 would pass:", e["pass"], "; ".join(e["reasons"]))
    # overlap
    p("")
    p("== K1/K4 OVERLAP (section 5)")
    ov = ["BIGBANK", "STEENBRC", "STEENBRE", "STEENBRG"]
    k1o = [n for n in CLUSTERS["K1"] if n not in ov]
    k4o = [n for n in CLUSTERS["K4"] if n not in ov]
    p("overlap members", ov, "K1-only", k1o, "K4-only", k4o)
    for arm in CAND:
        out = []
        for lab, names in (("overlap", ov), ("K1-only", k1o), ("K4-only", k4o)):
            cs = [c for n in names for c in cells_all["K1"] + cells_all["K4"] if c[2] == n]
            cs = sorted(set(cs))
            rows, miss = score(arm, cs)
            w, l, _ = tally(r["v"] for r in rows)
            out.append("%s %d/%d scored W/L %d/%d sign %+d" % (lab, len(rows), len(cs), w, l, sg(w, l)))
        p("OVERLAP", arm, "; ".join(out))
    p("")
    p("== VERDICT LINES")
    shipped = []
    for k in CLUSTERS:
        ok = [arm for arm in CAND if results[(arm, k)]["pass"] and not noise[k]]
        for arm in ok:
            shipped.append((k, arm))
        p("VERDICT", k, "LABEL (%s)" % ", ".join("%s: %s" % (x, LABELS[x]) for x in ok) if ok else "NONE")
    p("K (composite): not run (no I19-2a arm passed stage 1)")

    # cells csv
    os.makedirs(a.out, exist_ok=True)
    with open(os.path.join(a.out, "tu3_cells.csv"), "w", newline="") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(["candidate", "cluster", "corpus", "problem", "base_label", "baseline", "cand_status", "cand_outcome", "cand_fact",
                    "cand_majors", "cand_EV", "cand_rejected_steps", "cand_reject_funnel_ceiling", "base_status", "base_outcome", "base_fact",
                    "base_majors", "base_EV", "base_rejected_steps", "base_reject_funnel_ceiling", "verdict", "r1_breach_columns"])
        for arm in CAND:
            for k in CLUSTERS:
                e = results[(arm, k)]
                scored = {r["cell"]: r for r in e["rows"]}
                brk = collections.defaultdict(list)
                for c, n_, _, _ in e["tl_reach"]:
                    brk[c].append(n_)
                for c in cells_all[k]:
                    r = scored.get(c)
                    h = get(base_arm, c)
                    th = tailcols(h)
                    if r is None:
                        w.writerow([arm, k, c[0], c[2], c[3], base_arm, "NOT_SCORED", "", "", "", "", "", "", h["status"], h["outcome"], h["fact"],
                                    th["majors"], th["EV"], th["rejected_steps"], th["reject_funnel_ceiling"], "NOT_SCORED", ""])
                        continue
                    tx = tailcols(r["x"])
                    w.writerow([arm, k, c[0], c[2], c[3], base_arm, r["x"]["status"], r["x"]["outcome"], r["x"]["fact"], tx["majors"], tx["EV"],
                                tx["rejected_steps"], tx["reject_funnel_ceiling"], h["status"], h["outcome"], h["fact"], th["majors"], th["EV"],
                                th["rejected_steps"], th["reject_funnel_ceiling"], r["v"], "+".join(brk.get(c, []))])
    # store sha256s
    with open(os.path.join(a.out, "store-sha256.txt"), "w") as f:
        for fp in sorted(files):
            f.write("%s  %s\n" % (sha(fp), os.path.relpath(fp, store)))
    agg = hashlib.sha256(open(os.path.join(a.out, "store-sha256.txt"), "rb").read()).hexdigest()
    p("")
    p("PROVENANCE harness_sha", sorted(hsha)[0], "pin", sorted(pins)[0], "store row files", len(files), "store-sha256.txt sha256", agg)
    with open(os.path.join(a.out, "tu3_read.out"), "w") as f:
        f.write("\n".join(OUT) + "\n")
    print("\n".join(OUT))


if __name__ == "__main__":
    main()
