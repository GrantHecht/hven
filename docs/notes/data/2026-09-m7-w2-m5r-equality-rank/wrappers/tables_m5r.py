#!/usr/bin/env python3
"""M7 W2 M5r -- fold the 23 row files into tables/rank.md. Pure file folding.

Every number in the table is read from `rows/<CELL>.txt` and the cell name in
the first column is that citation. The STRUCTURAL READING column is the one
thing not read from a row: it is this task's interpretation of the row's own
dependent-row lines (`eq_DEP`, `skel_DEP`), labelled INTERPRETATION in the
table and argued in README.md.
"""
import json
import os
import sys

ART = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROWS = os.path.join(ART, "rows")
PIN = "/home/ghecht/Projects/hven-bench/sets/split-v1.json"

READING = {
    "SMBANK": "network balance, ONE redundant row: all 64 rows sum to 0 with +-1 coefficients (linear 64/64)",
    "DALLASM": "network balance, ONE redundant row: all 151 rows, +-1 (linear 151/151)",
    "BIGBANK": "network balance, ONE redundant row in Ae (+-1 over 1112 rows); the skeleton adds 149 more: every one a +-1 combination of node balances and FIXED arc flows, i.e. fixed arcs cut the network and each cut piece carries its own redundant balance",
    "STEENBRB": "12 disjoint 9-node networks (one per period, rows N<node><period>): one redundant balance per copy, each a +-1 sum of its 9 rows (linear 108/108)",
    "STEENBRD": "as STEENBRB (the identical 12 dependent rows by name)",
    "STEENBRC": "14 disjoint 9-node networks, one redundant balance per copy, +-1 (linear 126/126)",
    "STEENBRE": "as STEENBRC (the identical 14 dependent rows by name)",
    "STEENBRG": "as STEENBRC (the identical 14 dependent rows by name)",
    "MSS2": "NONLINEAR rows (0 linear); 325 dependent at x0 AND at the perturbed point; combinations are not +-1 (mostly support 7, coefficients 0.5/1); structure NOT identified",
    "MSS3": "NONLINEAR rows (0 linear); 946 dependent at x0 AND at the perturbed point; same shape as MSS2; structure NOT identified",
    "SAWPATH": "NO exact dependence: full rank at eps (QR and SVD agree), but ILL-CONDITIONED and BADLY SCALED -- row norms 1.41 .. 6.0e10, R-diagonal/singular values decay smoothly (no gap) to 3.5e-12 of the largest; at the arm's 1e-8 cut 575/578 rows read dependent (linear 578/578)",
    "NET3": "x0 UNEVALUABLE (Jacobian non-finite); at the projected start Ae and the skeleton are full rank; the entry-set deficiency equals its row-count excess",
    "YORKNET": "Ae and skeleton full rank; entry-set deficiency 40 at x0 (16 of it row-count excess), 64 at the projected start (56 excess)",
}


def blocks(path):
    """Header keys, and one dict per POINT block."""
    head, pts, cur, name = {}, {}, None, None
    for line in open(path):
        line = line.rstrip("\n")
        if line.startswith("POINT_BEGIN "):
            name = line.split()[1]
            cur = {}
            continue
        if line.startswith("POINT_END "):
            pts[name] = cur
            cur = None
            continue
        if "=" not in line or " " in line.split("=")[0]:
            continue
        k, v = line.split("=", 1)
        (cur if cur is not None else head).setdefault(k, v)
    return head, pts


def g(d, k, default="-"):
    return d.get(k, default)


def defi(p, tag):
    """dflt / 1e-10 / 1e-8, raw; with the row-equilibrated triple where it differs."""
    raw = [g(p, f"{tag}_{t}_deficiency") for t in ("dflt", "alt", "arm")]
    eqz = [g(p, f"{tag}_eqz_{t}_deficiency") for t in ("dflt", "alt", "arm")]
    if raw[0] == "-":
        return "n/a"
    s = "/".join(raw)
    if eqz != raw:
        s += f" (eqz {'/'.join(eqz)})"
    return s


def ratio(p, tag):
    v = g(p, f"{tag}_dflt_cond_ratio")
    try:
        return f"{float(v):.3g}"
    except ValueError:
        return v


def dep_names(cell, point, tag, limit=14):
    out, inside = [], False
    for line in open(os.path.join(ROWS, f"{cell}.txt")):
        line = line.rstrip("\n")
        if line == f"POINT_BEGIN {point}":
            inside = True
            continue
        if line == f"POINT_END {point}":
            break
        if inside and line.startswith(f"{tag}_DEP "):
            out.append(line.split()[1])
    if len(out) > limit:
        return f"{len(out)} rows (first: {', '.join(out[:4])}, ...)"
    return ", ".join(out) if out else "none"


def main():
    d = json.load(open(PIN))
    cells = [a["problem"] for a in d["assignments"]
             if a["problem_set"] == "cutest" and a["split"] == "TRAIN"]
    assert len(cells) == 23, cells
    m5_q1 = {"SMBANK", "SAWPATH", "DALLASM", "STEENBRB", "STEENBRC", "STEENBRG"}
    o = []
    o.append("# M7 W2 M5r -- the equality-Jacobian rank table\n")
    o.append("Every number is read from `rows/<CELL>.txt` by `wrappers/tables_m5r.py`; the "
             "cell name IS the citation. No solver ran.\n")
    o.append("**Deficiency triples** are `Eigen default / 1e-10 / 1e-8` relative thresholds "
             "on the SAME R (ColPivHouseholderQR of the transpose). 1e-8 is M5's arm "
             "`dependency_tol` (added as a third read, declared). `(eqz a/b/c)` is the same "
             "triple on the ROW-EQUILIBRATED rows, shown only where it differs from the raw one. "
             "**skel** = [Ae; e_i, l_i = u_i]; **entry** = [Ae; e_i for every bound active "
             "within 1e-8 absolute, fixed included]. **excess** = max(0, rows - n): the part "
             "of a deficiency that the row COUNT alone forces. **cond** = |R_11|/|R_rr| at the "
             "default-threshold rank. Cells marked * are M5 Q1's six.\n")
    o.append("## 1. At the decisive point (x0; the projected start where x0 is unevaluable)\n")
    o.append("| cell | pt | n | me | mi | fixed | finite bnds | rank(Ae) | Ae deficiency | "
             "skel deficiency | entry deficiency (excess) | cond Ae | cond skel | "
             "Ae linear rows (cstats, all m) | dependent Ae rows (default cut) |")
    o.append("|" + "---|" * 15)
    for c in cells:
        h, pts = blocks(os.path.join(ROWS, f"{c}.txt"))
        pt = "x0"
        if pts.get("x0", {}).get("jac_finite") != "1":
            pt = "xproj"
        p = pts.get(pt, {})
        star = "*" if c in m5_q1 else ""
        me = h.get("me")
        rank = g(p, "eq_dflt_rank") if me != "0" else "-"
        o.append(
            f"| `{c}`{star} | {pt} | {h['n']} | {me} | {h['mi']} | {h['fixed_bounds']} | "
            f"{h['finite_bounds']} | {rank} | {defi(p, 'eq') if me != '0' else 'me=0'} | "
            f"{defi(p, 'skel') if me != '0' else 'me=0'} | {defi(p, 'entry')} "
            f"({g(p, 'entry_count_excess')}) | {ratio(p, 'eq') if me != '0' else '-'} | "
            f"{ratio(p, 'skel') if me != '0' else '-'} | "
            f"{g(h, 'cstats_linear_constraints')} | "
            f"{dep_names(c, pt, 'eq') if me != '0' else '-'} |")
    o.append("")
    o.append("## 2. Structural reading (INTERPRETATION of each row's own `*_DEP` lines)\n")
    o.append("| cell | reading |")
    o.append("|---|---|")
    for c in cells:
        o.append(f"| `{c}` | {READING.get(c, 'full rank in Ae and the skeleton; nothing to read')} |")
    o.append("")
    o.append("## 3. Point dependence: x0, the projected start, and a perturbed point\n")
    o.append("`xpert` = x0 moved by 1e-3*(1+|x0_i|), alternating sign, projected into the box -- "
             "NOT a solve iterate; read ONLY for the equality Jacobian's deficiency and for how "
             "many equality rows' Jacobian values moved. No row carries the OFF solution's final x "
             "(M3c/M4/M5 rows carry counters, not x), so brief item 7 is SKIPPED.\n")
    o.append("| cell | f(x0) finite | Jac(x0) finite | outside box at x0 | projected start | "
             "Ae def. x0 | Ae def. xproj | entry def. xproj (excess) | eq rows moved at xpert | "
             "Ae def. xpert (dflt/1e-10/1e-8) |")
    o.append("|" + "---|" * 10)
    for c in cells:
        h, pts = blocks(os.path.join(ROWS, f"{c}.txt"))
        x0, xp = pts.get("x0", {}), pts.get("xproj", {})
        ps = h.get("projected_start", "EVALUATED" if xp else "-").split(" ")[0]
        xpd = "/".join(g(h, f"xpert_eq_deficiency_{t}") for t in ("dflt", "alt", "arm"))
        o.append(
            f"| `{c}` | {g(x0, 'f_finite')} | {g(x0, 'jac_finite')} | {g(x0, 'outside_box')} | "
            f"{ps} | {defi(x0, 'eq') if h.get('me') != '0' and x0.get('jac_finite') == '1' else ('UNEVALUABLE' if x0.get('jac_finite') != '1' else 'me=0')} | "
            f"{defi(xp, 'eq') if xp and h.get('me') != '0' else '-'} | "
            f"{(defi(xp, 'entry') + ' (' + g(xp, 'entry_count_excess') + ')') if xp else '-'} | "
            f"{g(h, 'xpert_eq_rows_with_moved_jacobian')} | "
            f"{xpd if h.get('me') != '0' and 'xpert_eq_deficiency_dflt' in h else h.get('xpert', '-')} |")
    o.append("")
    o.append("## 4. Identity check and threshold agreement (per row file)\n")
    o.append("`identity` = rank([Ae; E_S]) == |S| + rank(Ae restricted to the columns not in S), "
             "computed by a SECOND QR on a different matrix, at default / 1e-10. "
             "`agree` = default and 1e-10 give the same rank on the raw rows.\n")
    o.append("| cell | pt | eq agree | skel agree | skel identity | entry agree | entry identity |")
    o.append("|---|---|---|---|---|---|---|")
    for c in cells:
        h, pts = blocks(os.path.join(ROWS, f"{c}.txt"))
        for pt, p in pts.items():
            if p.get("jac_finite") != "1":
                continue
            o.append(f"| `{c}` | {pt} | {g(p, 'eq_thresholds_agree')} | "
                     f"{g(p, 'skel_thresholds_agree')} | {g(p, 'skel_identity_holds')} | "
                     f"{g(p, 'entry_thresholds_agree')} | {g(p, 'entry_identity_holds')} |")
    o.append("")
    sys.stdout.write("\n".join(o) + "\n")


if __name__ == "__main__":
    main()
