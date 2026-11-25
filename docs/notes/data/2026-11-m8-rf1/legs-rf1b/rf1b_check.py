#!/usr/bin/env python3
"""RF-1b (../DECLARATION-RF-1b.md section 2): the bars of legs T and P read off the cell rows (r4_check.py re-pointed: reference R).
usage: rf1b_check.py train NEW_DIR R_DIR   -> LOST (R optimal/acceptable -> other), FALSE_CERT (NEW certified infeasible where R is not),
                                              MISSING rows; every status mover is listed.
       rf1b_check.py pilot NEW_DIR R_DIR   -> LOST_VS_R per cell, FALSE_CERT, THREW, cells 055-057 (must not end at iterations == 1),
                                              solved counts; every cell whose status/iterations/factorizations differ is listed (MOVER).
A row is solved iff status is optimal or acceptable. Prints a final VERDICT line."""
import glob, os, sys

SOLVED = ("optimal", "acceptable")


def load(f):
    r = {}
    if not os.path.exists(f):
        return None
    for line in open(f, errors="replace"):
        if "=" in line:
            k, v = line.rstrip("\n").split("=", 1)
            r.setdefault(k, v)
    return r


def cert(r):
    return r is not None and (r.get("status") == "infeasible" or r.get("side.sqp_infeasibility_certified") == "1")


def fmt(r):
    return f"{r.get('status')} it={r.get('iterations')} fact={r.get('factorizations')} f={r.get('objective')} cert={r.get('side.sqp_infeasibility_certified')}"


def train(ndir, rdir):
    bad = 0
    names = sorted(os.path.basename(f)[:-4] for f in glob.glob(os.path.join(ndir, "cells", "*.out")))
    for nm in names:
        n = load(os.path.join(ndir, "cells", nm + ".out"))
        c = load(os.path.join(rdir, "cells", nm + ".out"))
        if c is None:
            print("MISSING_R", nm); bad += 1; continue
        if c.get("status") != n.get("status"):
            print(f"STATUS_MOVER {nm} R {fmt(c)} | NEW {fmt(n)}")
        if cert(n) and not cert(c):
            print(f"FALSE_CERT {nm} R {c.get('status')} | NEW {n.get('status')}"); bad += 1
        if c.get("status") in SOLVED and n.get("status") not in SOLVED:
            print(f"LOST {nm} {c.get('status')}->{n.get('status')}"); bad += 1
    print(f"rows {len(names)}")
    print("VERDICT", "TRAIN_BARS_MET" if bad == 0 else f"TRAIN_BARS_FAILED({bad})")


def pilot(ndir, rdir):
    bad = 0; s = {"R": 0, "NEW": 0}
    for f in sorted(glob.glob(os.path.join(ndir, "cells", "*.out"))):
        nm = os.path.basename(f)[:-4]
        n, r = load(f), load(os.path.join(rdir, "cells", nm + ".out"))
        if r is None:
            print("MISSING_R", nm); bad += 1; continue
        if n.get("threw") == "1" or r.get("threw") == "1":
            print("THREW", nm); bad += 1
        for k, x in (("R", r), ("NEW", n)):
            s[k] += x.get("status") in SOLVED
        if r.get("status") in SOLVED and n.get("status") not in SOLVED:
            print(f"LOST_VS_R {nm} {r.get('status')}->{n.get('status')}"); bad += 1
        if cert(n) and not cert(r):
            print(f"FALSE_CERT {nm} R {r.get('status')} | NEW {n.get('status')}"); bad += 1
        if (r.get("status"), r.get("iterations"), r.get("factorizations")) != (n.get("status"), n.get("iterations"), n.get("factorizations")):
            print(f"MOVER {nm} {n.get('label')} | R {fmt(r)} | NEW {fmt(n)}")
        if nm.split("-")[0] in ("055", "056", "057"):
            ok = n.get("iterations") != "1"
            print(f"CELL_{nm.split('-')[0]} {'OK' if ok else 'ENDED_AT_MAJOR_1'} R {fmt(r)} | NEW {fmt(n)}")
            bad += not ok
    print(f"SOLVED R {s['R']} NEW {s['NEW']}")
    print("VERDICT", "PILOT_BARS_MET" if bad == 0 else f"PILOT_BARS_FAILED({bad})")


if __name__ == "__main__":
    (train if sys.argv[1] == "train" else pilot)(*sys.argv[2:4])
