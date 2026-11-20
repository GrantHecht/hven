#!/usr/bin/env python3
"""I18 measurement-phase table from corpus/<label>.corpus (rows: name status minors facts eq stat) and suite/<label>.{issue,failed}.txt."""
import os
H = os.path.dirname(os.path.abspath(__file__))
def load(l):
    r = {}
    for line in open(f"{H}/corpus/{l}.corpus"):
        p = line.split()
        if len(p) >= 4 and p[1].startswith("status="):
            r[p[0]] = (p[1][7:], int(p[2][7:]), int(p[3][6:]))
        else:
            r[p[0]] = ("TO", 0, 0)
    return r
base = load("base")
order = ["base", "N1P", "N1Psite", "N1Pbox", "N1Ptr", "N1G", "N1", "N1PtrG", "N2box", "N2tr", "Rbox01", "Rbox05",
         "Rbox05c01", "Rbox05c01b", "Rbox05c02", "Rbox05c005", "Rbox01c01", "N2boxc01", "N2trc01", "Rtr05c01", "Rall05c01",
         "Rall01c01", "N1PtrRtr05c01"]
print("| candidate | issue opt (b9 b10 r9 r10) | corpus opt | gained | lost | other status moves | both-opt minors | seam fails |")
print("|---|---|---|---|---|---|---|---|")
for l in order:
    if not os.path.exists(f"{H}/corpus/{l}.corpus"):
        continue
    c = load(l)
    iss = ""
    p = f"{H}/suite/{l}.issue.txt"
    if os.path.exists(p):
        d = {tuple(x.split()[1:3]): x.split()[4] for x in open(p) if x.strip()}
        iss = " ".join("Y" if d.get(k, "") == "status=0" else "n" for k in
                       [("border", "1e-9"), ("border", "1e-10"), ("refac", "1e-9"), ("refac", "1e-10")])
    opt = sum(1 for v in c.values() if v[0] == "0")
    gained = sum(1 for k in c if c[k][0] == "0" and base[k][0] != "0")
    lost = sum(1 for k in c if c[k][0] != "0" and base[k][0] == "0")
    other = sum(1 for k in c if c[k][0] != base[k][0] and c[k][0] != "0" and base[k][0] != "0")
    ma = sum(base[k][1] for k in c if c[k][0] == "0" == base[k][0]); mb = sum(c[k][1] for k in c if c[k][0] == "0" == base[k][0])
    f = f"{H}/suite/{l}.failed.txt"
    nf = sum(1 for x in open(f) if x.strip()) if os.path.exists(f) else "-"
    print(f"| {l} | {iss} | {opt} | {gained} | {lost} | {other} | {100.0 * (mb - ma) / ma:+.2f}% | {nf} |")
