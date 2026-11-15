#!/usr/bin/env python3
"""M8 zero-step gate (../DECLARATION.md, criterion I4): the TRAIN lane, arm A (origin/m8) vs arm B (the code head).

Per roster cell (the 238 TRAIN cells) and label (D = default, K = never-default), the two rows are compared: every
key=value of the .out except `options_label` and `hven_pin_sha`, plus the lane exit code; integers and strings
byte-equal; residual-class values byte-equal or within hven-bench docs/corun.md's calibrated gate
|a-b| <= max(1e-12*max(|a|,|b|), 1e-13); the .optdump byte-equal. A cell whose rows differ is a MOVER and is classified:
  IMPROVED  outcome improves: (reference_ok, converged) strictly better, converged = status in {optimal, acceptable}
  WORSE     outcome worsens, or a converged cell changes status
  EQUAL_CLASS  status and outcome unchanged (counter / iteration / residual movement only)
  STATUS_SHIFT  status string differs but the outcome pair is unchanged (e.g. numerical_error -> infeasible)
Exit 0 iff no row is missing or capped, no mover is WORSE, and the mover set equals DECLARED exactly (one tag.arm per
line, '#' comments; none undeclared, none declared-but-unmoved). With DEV_A / DEV_B (the disclosed dev runs), every
row must also equal the dev run's (I4R: the movers reproduce). HS battery = the roster's 'hs' population, reported on
its own.
Usage: tally.py A_DIR B_DIR ROSTER DECLARED [DEV_A DEV_B]
"""
import math
import os
import sys

SKIP = {"options_label", "hven_pin_sha"}
GATE_REL, GATE_ABS = 1e-12, 1e-13
STRINGS = {"status", "outcome", "problem", "lane", "problem_set", "set_version", "source_path", "hessian_provenance"}
CONVERGED = {"optimal", "acceptable"}
SHOW = ("status", "iterations", "objective", "reference_ok", "_cell_rc", "outcome")


def load(prefix):
    row, present = {}, False
    res = prefix + ".result"
    capped = os.path.exists(res) and "CAPPED" in open(res).read()
    if os.path.exists(res):
        for tok in open(res).read().split():
            if tok.startswith("rc="):
                row["_cell_rc"] = tok[3:]
    if os.path.exists(prefix + ".out"):
        for line in open(prefix + ".out"):
            line = line.rstrip("\n")
            if "=" in line:
                k, v = line.split("=", 1)
                row[k] = v
                present = True
    return row, present and not capped and "status" in row


def is_int(v):
    try:
        int(v)
        return True
    except ValueError:
        return False


def diffs(a, b):
    out = []
    for k in sorted(set(a) | set(b)):
        if k in SKIP:
            continue
        va, vb = a.get(k), b.get(k)
        if va == vb:
            continue
        if va is None or vb is None or k in STRINGS or k == "_cell_rc" or is_int(va) or is_int(vb):
            out.append((k, va, vb))
            continue
        try:
            fa, fb = float(va), float(vb)
        except ValueError:
            out.append((k, va, vb))
            continue
        if not (math.isfinite(fa) and math.isfinite(fb)):
            out.append((k, va, vb))
            continue
        if abs(fa - fb) <= max(GATE_REL * max(abs(fa), abs(fb)), GATE_ABS):
            continue
        out.append((k, va, vb))
    return out


def outcome(r):
    return (r.get("reference_ok") == "1", r.get("status") in CONVERGED)


def classify(a, b):
    oa, ob = outcome(a), outcome(b)
    if ob > oa:
        return "IMPROVED"
    if ob < oa:
        return "WORSE"
    if a.get("status") != b.get("status"):
        return "WORSE" if a.get("status") in CONVERGED else "STATUS_SHIFT"
    return "EQUAL_CLASS"


def main():
    a_dir, b_dir, roster, perm_file = sys.argv[1:5]
    dev_a, dev_b = (sys.argv[5], sys.argv[6]) if len(sys.argv) > 6 else (None, None)
    permitted = set()
    for line in open(perm_file):
        line = line.split("#", 1)[0].strip()
        if line:
            permitted.add(line)
    tags = []
    for line in open(roster):
        pop, lane, prob = line.rstrip("\n").split("\t")
        tags.append((pop, f"{pop}-{lane}-{prob}"))
    ok = True
    movers = []
    n_cmp = 0
    for pop, t in tags:
        for arm in "DK":
            ra, pa = load(f"{a_dir}/cells/{t}.{arm}")
            rb, pb = load(f"{b_dir}/cells/{t}.{arm}")
            if not (pa and pb):
                print(f"MISSING_OR_CAPPED {t}.{arm} A={pa} B={pb}")
                ok = False
                continue
            n_cmp += 1
            d = diffs(ra, rb)
            da = open(f"{a_dir}/cells/{t}.{arm}.optdump", "rb").read()
            db = open(f"{b_dir}/cells/{t}.{arm}.optdump", "rb").read()
            if da != db:
                d.append(("optdump", "A", "B"))
            if d:
                movers.append((pop, f"{t}.{arm}", classify(ra, rb), d, ra, rb))
    print(f"compared {n_cmp} rows ({len(tags)} cells x 2 labels), {len(movers)} movers")
    by = {}
    for pop, tag, cls, d, ra, rb in movers:
        by.setdefault(cls, []).append(tag)
        keys = ", ".join(f"{k} {va}->{vb}" for k, va, vb in d if k in SHOW or k == "optdump")
        extra = "" if tag in permitted else " UNDECLARED"
        print(f"MOVER [{pop}] {tag} {cls}{extra}: {len(d)} keys; {keys}")
    for cls in ("IMPROVED", "EQUAL_CLASS", "STATUS_SHIFT", "WORSE"):
        print(f"CLASS {cls}: {len(by.get(cls, []))} {sorted(by.get(cls, []))}")
    hs = [m for m in movers if m[0] == "hs"]
    print(f"HS battery: {sum(1 for p, _ in tags if p == 'hs')} cells x 2 labels, {len(hs)} movers {[m[1] for m in hs]}")
    moved = {m[1] for m in movers}
    undeclared = sorted(moved - permitted)
    unmoved = sorted(permitted - moved)
    worse = [m[1] for m in movers if m[2] == "WORSE"]
    print(f"declared {len(permitted)}; undeclared movers {undeclared}; declared but unmoved {unmoved}; WORSE {worse}")
    r_bad = 0
    if dev_a is not None:
        for pop, t in tags:
            for arm in "DK":
                for side, cur, dev in (("A", a_dir, dev_a), ("B", b_dir, dev_b)):
                    rc, pc = load(f"{cur}/cells/{t}.{arm}")
                    rd, pd = load(f"{dev}/cells/{t}.{arm}")
                    dd = diffs(rd, rc) if pd and pc else [("missing", None, None)]
                    if dd:
                        r_bad += 1
                        print(f"I4R_DIFF {side} {t}.{arm}: {dd[:6]}")
        print(f"I4R (rows vs the dev runs): {r_bad} differing rows")
    if ok and not worse and not undeclared and not unmoved and r_bad == 0:
        print("I4_PASS")
        return 0
    print("I4_FAIL")
    return 1


if __name__ == "__main__":
    sys.exit(main())
