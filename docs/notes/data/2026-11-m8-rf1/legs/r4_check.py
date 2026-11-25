#!/usr/bin/env python3
"""RF-1 gate A+B2 r4 (../DECLARATION-RF-1-AB2-r4.md section 3): the bars of legs T and P read off the cell rows.
usage: r4_check.py train NEW_DIR C_DIR      -> every certified-infeasible row per arm; FALSE_CERT = a NEW row certified
                                               (status infeasible or side.sqp_infeasibility_certified=1) where C' is not;
                                               LOST = C' optimal/acceptable -> other.
       r4_check.py pilot NEW_DIR C_DIR R3_DIR -> LOST_VS_C / LOST_VS_ABR3 per cell; the 054-YORKNET row; solved counts.
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


def train(ndir, cdir):
    bad = 0
    names = sorted(os.path.basename(f)[:-4] for f in glob.glob(os.path.join(ndir, "cells", "*.out")))
    for nm in names:
        n = load(os.path.join(ndir, "cells", nm + ".out"))
        c = load(os.path.join(cdir, "cells", nm + ".out"))
        if c is None:
            print("MISSING_C", nm); bad += 1; continue
        if cert(n) or cert(c):
            tag = "FALSE_CERT" if cert(n) and not cert(c) else "CERT_BOTH" if cert(n) else "CERT_C_ONLY"
            print(f"{tag} {nm} C {c.get('status')} cert={c.get('side.sqp_infeasibility_certified')} | NEW {n.get('status')} "
                  f"cert={n.get('side.sqp_infeasibility_certified')} f={n.get('objective')} h={n.get('kkt_primal_eq')}")
            bad += tag == "FALSE_CERT"
        if c.get("status") in SOLVED and n.get("status") not in SOLVED:
            print(f"LOST {nm} {c.get('status')}->{n.get('status')}"); bad += 1
    print(f"rows {len(names)}")
    print("VERDICT", "TRAIN_BARS_MET" if bad == 0 else f"TRAIN_BARS_FAILED({bad})")


def pilot(ndir, cdir, rdir):
    bad = 0; s = {"C": 0, "R3": 0, "NEW": 0}
    for f in sorted(glob.glob(os.path.join(ndir, "cells", "*.out"))):
        nm = os.path.basename(f)[:-4]
        n, c, r = load(f), load(os.path.join(cdir, "cells", nm + ".out")), load(os.path.join(rdir, "cells", nm + ".out"))
        if c is None or r is None:
            print("MISSING", nm); bad += 1; continue
        if n.get("threw") == "1" or c.get("threw") == "1":
            print("THREW", nm); bad += 1
        for k, x in (("C", c), ("R3", r), ("NEW", n)):
            s[k] += x.get("status") in SOLVED
        if c.get("status") in SOLVED and n.get("status") not in SOLVED:
            print(f"LOST_VS_C {nm} {c.get('status')}->{n.get('status')}"); bad += 1
        if r.get("status") in SOLVED and n.get("status") not in SOLVED:
            print(f"LOST_VS_ABR3 {nm} {r.get('status')}->{n.get('status')}"); bad += 1
        if cert(n) and not cert(c):
            print(f"FALSE_CERT {nm} C {c.get('status')} | NEW {n.get('status')} cert={n.get('side.sqp_infeasibility_certified')}"); bad += 1
        if nm.startswith("054-YORKNET"):
            print(f"CELL_054 C {c.get('status')} | ABr3 {r.get('status')} cert={r.get('side.sqp_infeasibility_certified')} | NEW "
                  f"{n.get('status')} cert={n.get('side.sqp_infeasibility_certified')} it={n.get('iterations')} "
                  f"fact={n.get('factorizations')} f={n.get('objective')}")
            bad += cert(n)
    print(f"SOLVED C' {s['C']} ABr3 {s['R3']} NEW {s['NEW']}")
    print("VERDICT", "PILOT_BARS_MET" if bad == 0 else f"PILOT_BARS_FAILED({bad})")


if __name__ == "__main__":
    if sys.argv[1] == "train":
        train(*sys.argv[2:4])
    else:
        pilot(*sys.argv[2:5])
