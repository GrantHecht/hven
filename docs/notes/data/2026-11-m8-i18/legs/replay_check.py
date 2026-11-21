#!/usr/bin/env python3
"""I18 replay check: for each mode, scripts/compare_replay.py A vs B (exact), and B vs B_DEV.
I18_REPLAY_PASS iff, in every mode, the cells differing A vs B are exactly MOVERS' cells for that mode (lines `<mode> <cell>`,
'#' comments) and B vs B_DEV reads "differences: 0".   usage: replay_check.py WT A_DIR A_LABEL B_DIR B_LABEL BDEV_DIR BDEV_LABEL MOVERS"""
import re
import subprocess
import sys


def diff_cells(wt, a, b, label):
    out = subprocess.run([sys.executable, f"{wt}/scripts/compare_replay.py", a, b, label], capture_output=True, text=True).stdout
    print(out.rstrip())
    m = re.search(r"differences: (\d+)", out)
    if not m or "UNMATCHED" in out or "SCHEMA" in out:
        return None, out
    return {l.split()[0] for l in out.splitlines()[1:] if l.startswith("    ")}, out


def main():
    wt, adir, al, bdir, bl, ddir, dl, movers = sys.argv[1:9]
    declared = {}
    for line in open(movers):
        line = line.split("#", 1)[0].strip()
        if line:
            mode, cell = line.split()
            declared.setdefault(mode, set()).add(cell)
    fails = []
    for mode in ("walk", "ssn", "ipm"):
        cells, _ = diff_cells(wt, f"{adir}/{al}-{mode}.csv", f"{bdir}/{bl}-{mode}.csv", f"A-vs-B {mode}")
        if cells is None:
            fails.append(f"{mode}: compare failed")
            continue
        if cells != declared.get(mode, set()):
            fails.append(f"{mode}: moved {sorted(cells)} declared {sorted(declared.get(mode, set()))}")
        dcells, dout = diff_cells(wt, f"{ddir}/{dl}-{mode}.csv", f"{bdir}/{bl}-{mode}.csv", f"Bdev-vs-B {mode}")
        if dcells is None or "differences: 0" not in dout:
            fails.append(f"{mode}: B differs from the dev run")
    for f in fails:
        print("  FAIL " + f)
    print("I18_REPLAY_PASS" if not fails else "I18_REPLAY_FAIL")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main())
