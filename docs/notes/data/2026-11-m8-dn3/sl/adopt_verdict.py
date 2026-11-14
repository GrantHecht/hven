#!/usr/bin/env python3
"""M8 DN-3 (../DECLARATION-r1.md, criterion A): the adoption verdict, applied mechanically.

Inputs under EV: roster.tsv; enum/ (candidates.tsv, enum-analysis.txt's E3 identity); stat/<tag>.<arm>.r<k>.{stat,out,
result} (every cell, arms N = `default` kNever and A = `auto-default` kAuto); record/<tag>.<arm>.attr.json (candidates).

Per cell and arm: raw = the median instructions:u of the three stat rounds; solver = raw x (1 - s_init - s_proc) for a
candidate (adopt_attr.py's shares). converged = status optimal or acceptable in all three rounds' rows (a CAPPED round, a
threw=1 row, a missing row or outcome=error is not converged).
  A4  win:  A converges and N does not, or both converge and solver_A <= 0.90 solver_N; loss: the mirror; else tie.
      A NON-candidate is a tie by identity (E3: its A row equals its N row in every printed key) and is never measured
      against the bar.
  A5  per engine E, per family F in {HS, SD}: sign = sign(wins - losses). kAuto becomes E's default iff at least one
      family has sign +1 and none has sign -1.
STOP / VOID checks: S-A3 (s_init == 0 on every IPM candidate's N arm), S-A4 (R-d > 0.01 of solver instructions on a
median cell that is a candidate), S-A5 (a status differs between one arm's three stat rounds; listed for a solo re-run).

Usage: adopt_verdict.py EV   -> stdout (the per-cell table, the R items, the verdict); exit 0 when the verdict is
computable (no STOP/VOID), 7 otherwise.
"""
import collections
import json
import math
import os
import statistics
import sys

CONV = {"optimal", "acceptable"}
MEDIANS = {("hven-ipm", "hs026"), ("hven-ipm", "HS67"), ("hven-sqp", "hs043"), ("hven-sqp", "HS84")}
NAMED = {("hven-ipm", "ALLINITC"), ("hven-sqp", "HS91")}
DENSE_COLS = {
    "hven-sqp": ["side.dn3_dense_factorizations", "side.dn1c_dense_condition_fallbacks",
                 "side.dn1d_dense_flagged_factorizations", "side.dn1d_dense_singular_refactors",
                 "side.dn1d_dense_shifted_solve_refusals", "side.dn1d_dense_shifted_k0_fallbacks",
                 "side.dn1d_dense_shifted_terminations", "side.dn3a_dense_solve_residual_refusals"],
    "hven-ipm": ["side.dn3_dense_factorizations", "side.dn1b_dense_residual_ratio_triggers",
                 "side.dn3b_kkt_dense_singular_count", "side.dn3b_kkt_dense_flagged_count"],
}


def kv(path):
    d = {}
    if os.path.exists(path):
        for line in open(path):
            line = line.rstrip("\n")
            if "=" in line:
                k, v = line.split("=", 1)
                d[k] = v
    return d


def instr(path):
    if not os.path.exists(path):
        return None
    for line in open(path):
        parts = line.split(",")
        if len(parts) > 2 and parts[2].startswith("instructions"):
            try:
                return int(parts[0])
            except ValueError:
                return None
    return None


def arm_rows(ev, tag, arm):
    rows = []
    for k in (1, 2, 3):
        pre = os.path.join(ev, "stat", f"{tag}.{arm}.r{k}")
        res = open(pre + ".result").read() if os.path.exists(pre + ".result") else ""
        o = kv(pre + ".out")
        rows.append({"capped": "CAPPED" in res, "out": o, "instr": instr(pre + ".stat")})
    return rows


def converged(rows):
    for r in rows:
        o = r["out"]
        if r["capped"] or not o or o.get("threw") == "1" or o.get("outcome") == "error" or o.get("status") not in CONV:
            return False
    return True


def main():
    ev = sys.argv[1]
    roster = [l.rstrip("\n").split("\t") for l in open(os.path.join(ev, "roster.tsv")) if l.strip()]
    cands = {tuple(l.rstrip("\n").split("\t")) for l in open(os.path.join(ev, "enum", "candidates.tsv")) if l.strip()}
    enum_txt = open(os.path.join(ev, "enum", "enum-analysis.txt")).read()
    e3_ok = "E3_NONCANDIDATE_MOVED: none" in enum_txt and "ENUM_PASS" in enum_txt
    tally = collections.defaultdict(collections.Counter)
    stops = []
    s_a5 = []
    ipm_cand_init = []
    named = []
    print("cell | cand | N status (it, fac) | A status (it, fac) | f rel diff | raw N | raw A | s_init N/A | s_proc N/A |"
          " solver N | solver A | A/N | margin SE | outcome | dense columns A")
    for pop, lane, prob in roster:
        tag = f"{pop}-{lane}-{prob}"
        eng = "IPM" if lane == "hven-ipm" else "SQP"
        fam = "HS" if pop == "hs" else "SD"
        is_c = (pop, lane, prob) in cands
        R = {a: arm_rows(ev, tag, a) for a in ("N", "A")}
        for a in ("N", "A"):
            sts = {r["out"].get("status", "CAPPED" if r["capped"] else "NO_ROW") for r in R[a]}
            if len(sts) > 1:
                s_a5.append(f"{tag}.{a}: {sorted(sts)}")
        cv = {a: converged(R[a]) for a in ("N", "A")}
        raw = {}
        for a in ("N", "A"):
            v = [r["instr"] for r in R[a] if r["instr"] is not None]
            raw[a] = int(statistics.median(v)) if len(v) == 3 else None
        o = {a: R[a][0]["out"] for a in ("N", "A")}
        try:
            fn, fa = float(o["N"]["objective"]), float(o["A"]["objective"])
            rel = abs(fa - fn) / max(1.0, abs(fn))
            reltxt = f"{rel:.2e}{' FLAG' if rel > 1e-8 else ''}"
        except (KeyError, ValueError):
            reltxt = "-"
        sh = {}
        solver = {}
        se = {}
        if is_c:
            for a in ("N", "A"):
                p = os.path.join(ev, "record", f"{tag}.{a}.attr.json")
                if not os.path.exists(p) or raw[a] is None:
                    stops.append(f"MISSING_RECORD_OR_STAT {tag}.{a}")
                    continue
                j = json.load(open(p))
                sh[a] = j
                solver[a] = raw[a] * (1.0 - j["s_init"] - j["s_proc"])
                se[a] = raw[a] * j["se_share"]
                if (lane, prob) in MEDIANS and solver[a] > 0 and j["rd_share"] * raw[a] / solver[a] > 0.01:
                    stops.append(f"S-A4 {tag}.{a} R-d {j['rd_share'] * raw[a] / solver[a]:.4f}")
            if eng == "IPM" and "N" in sh:
                ipm_cand_init.append(sh["N"]["s_init"])
        margin = "-"
        if not is_c:
            outcome = "tie (identity)" if e3_ok else "UNRESOLVED (E3 failed)"
        elif cv["A"] and not cv["N"]:
            outcome = "win"
        elif cv["N"] and not cv["A"]:
            outcome = "loss"
        elif cv["N"] and cv["A"] and "N" in solver and "A" in solver:
            n_, a_ = solver["N"], solver["A"]
            if a_ <= 0.90 * n_:
                outcome = "win"
            elif n_ <= 0.90 * a_:
                outcome = "loss"
            else:
                outcome = "tie"
            dw = (0.90 * n_ - a_) / math.sqrt((0.90 * se["N"]) ** 2 + se["A"] ** 2)
            dl = (0.90 * a_ - n_) / math.sqrt((0.90 * se["A"]) ** 2 + se["N"] ** 2)
            margin = f"{dw:+.1f}/{dl:+.1f}"
        else:
            outcome = "tie"
        tally[(eng, fam)][outcome.split(" ")[0]] += 1
        dcols = " ".join(f"{c.split('.')[-1]}={o['A'].get(c, '-')}" for c in DENSE_COLS[lane]) if is_c else "-"
        line = (f"{tag} | {'Y' if is_c else 'n'} | {o['N'].get('status', '-')} ({o['N'].get('iterations', '-')}, "
                f"{o['N'].get('factorizations', '-')}) | {o['A'].get('status', '-')} ({o['A'].get('iterations', '-')}, "
                f"{o['A'].get('factorizations', '-')}) | {reltxt} | {raw['N']} | {raw['A']} | "
                + (f"{sh['N']['s_init']:.4f}/{sh['A']['s_init']:.4f} | {sh['N']['s_proc']:.4f}/{sh['A']['s_proc']:.4f} | "
                   f"{solver['N']:.0f} | {solver['A']:.0f} | {solver['A'] / solver['N']:.3f}"
                   if "N" in sh and "A" in sh else "- | - | - | - | "
                   + (f"{raw['A'] / raw['N']:.3f} (raw)" if raw["N"] and raw["A"] else "-"))
                + f" | {margin} | {outcome} | {dcols}")
        print(line)
        if (lane, prob) in NAMED:
            named.append(line)
    print()
    print("== R-b (named)")
    for l in named:
        print("  " + l)
    print()
    if ipm_cand_init and all(x == 0 for x in ipm_cand_init):
        stops.append("S-A3 s_init == 0 on every IPM candidate's N arm (VOID)")
    if not e3_ok:
        stops.append("E3 identity not established by the enumeration leg")
    print("== FAMILY TALLIES (wins / losses / ties)")
    verdict = {}
    for eng in ("IPM", "SQP"):
        signs = {}
        for fam in ("HS", "SD"):
            t = tally[(eng, fam)]
            s = (t["win"] > t["loss"]) - (t["win"] < t["loss"])
            signs[fam] = s
            print(f"  {eng} {fam}: wins {t['win']}, losses {t['loss']}, ties {t['tie']} -> sign {s:+d}")
        verdict[eng] = any(v == 1 for v in signs.values()) and not any(v == -1 for v in signs.values())
    print()
    print("== S-A5 (status differs between one arm's stat rounds; each re-run alone before it counts)")
    print("  " + ("; ".join(s_a5) if s_a5 else "none"))
    print("== STOP / VOID")
    print("  " + ("; ".join(stops) if stops else "none"))
    print()
    print("== VERDICT")
    for eng in ("IPM", "SQP"):
        print(f"  {eng}: {'ADOPT (kAuto becomes the default, subject to the I set)' if verdict[eng] else 'NOT ADOPTED (the default stays kNever; kAuto ships opt-in)'}")
    sys.exit(0 if not stops and not s_a5 else 7)


if __name__ == "__main__":
    main()
