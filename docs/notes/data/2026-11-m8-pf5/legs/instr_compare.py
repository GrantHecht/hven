#!/usr/bin/env python3
# PF-5 (../DECLARATION-PF-5.md): copy of ../../2026-11-m8-pf4/r2/legs/instr_compare.py re-pointed to the pf5b worktree (paths only); older lines are historical.
# PF-4 r2 (../../DECLARATION-PF-4-r2.md): copy of ../../legs/instr_compare.py re-pointed to the pf4-r2 worktree and r2/ (paths only); older lines are historical.
"""PF-4: per-row instructions:u, arm B vs arm A, from cell.sh MODE stat's .stat files (perf stat -x,).

usage: instr_compare.py <A cells dir> <B cells dir> [focus cells...]
Rows are <tag>.<arm> prefixes present in both; a row whose .result says CAPPED or whose .stat lacks a count is skipped
and listed. Prints per-lane totals and B/A, the focus rows (default: the PF-3 add_border-dominated cells), and the ten
largest relative savings and the ten largest relative increases. Informational table plus one asserted line:
`TOTAL B/A` (the criterion is applied by the declaration, not here)."""
import glob, os, sys

FOCUS = ["SPANHYD", "MESH", "DNIEPER", "CRESC50", "CRESC100", "MSS1"]


def instr(prefix):
    st = prefix + ".stat"
    if not os.path.exists(st):
        return None
    if os.path.exists(prefix + ".result") and "CAPPED" in open(prefix + ".result").read():
        return None
    for line in open(st):
        p = line.strip().split(",")
        if len(p) > 2 and p[2].startswith("instructions"):
            try:
                return int(p[0])
            except ValueError:
                return None
    return None


def main():
    a_dir, b_dir = sys.argv[1], sys.argv[2]
    focus = sys.argv[3:] or FOCUS
    rows = {}
    skipped = []
    for f in sorted(glob.glob(os.path.join(b_dir, "*.stat"))):
        pre = f[:-5]
        name = os.path.basename(pre)
        ia, ib = instr(os.path.join(a_dir, name)), instr(pre)
        if ia is None or ib is None:
            skipped.append(name)
            continue
        rows[name] = (ia, ib)
    tot = {}
    for name, (ia, ib) in rows.items():
        lane = name.split("-")[1] + "-" + name.split("-")[2]
        arm = name.rsplit(".", 1)[1]
        for key in (lane, f"{lane}.{arm}", "ALL"):
            t = tot.setdefault(key, [0, 0, 0])
            t[0] += ia; t[1] += ib; t[2] += 1
    print(f"rows compared {len(rows)}; skipped {len(skipped)} {' '.join(skipped)}")
    for key in sorted(tot):
        a, b, n = tot[key]
        print(f"TOTAL {key:18s} rows {n:4d}  A {a:.6e}  B {b:.6e}  B/A {b / a:.6f}")
    print("\nfocus rows:")
    for name in sorted(rows):
        if any(f"-{p}." in name for p in focus):
            ia, ib = rows[name]
            print(f"  {name:40s} A {ia:.6e} B {ib:.6e} B/A {ib / ia:.4f}")
    rel = sorted(rows.items(), key=lambda kv: kv[1][1] / kv[1][0])
    print("\nlargest savings:")
    for name, (ia, ib) in rel[:10]:
        print(f"  {name:40s} A {ia:.6e} B {ib:.6e} B/A {ib / ia:.4f}")
    print("largest increases:")
    for name, (ia, ib) in rel[-10:]:
        print(f"  {name:40s} A {ia:.6e} B {ib:.6e} B/A {ib / ia:.4f}")


if __name__ == "__main__":
    main()
