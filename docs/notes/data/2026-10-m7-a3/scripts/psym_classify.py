#!/usr/bin/env python3
"""A2 P-SYM classification: every differing object is A (its own source changed in base..head),
B (source untouched, includes a changed header per the build's own Ninja dependency log), or
UNEXPLAINED (the finding). Also the negative control: includers of a changed header that are
byte/instruction identical. Usage: psym_classify.py OUT BASE HEAD"""
import os
import re
import subprocess
import sys

W = "/home/ghecht/Projects/hven/.claude/worktrees/agent-afecbe0a5611f9eff"


def changed(base, head):
    out = subprocess.run(["git", "-C", W, "diff", "--name-only", f"{base}..{head}", "--",
                          "include", "src", "tests", "bench"], capture_output=True, text=True)
    return [l for l in out.stdout.split() if l]


def deps(path):
    table, cur = {}, None
    for line in open(path):
        if not line.strip():
            continue
        if not line.startswith(" "):
            m = re.match(r"^(\S+): #deps", line)
            cur = m.group(1) if m else None
            if cur:
                table[cur] = []
        elif cur:
            table[cur].append(line.strip())
    return table


def manifest(d):
    p = os.path.join(d, ".psym-manifest")
    return [l.split()[-1] if l.strip() else "" for l in open(p)] if os.path.exists(p) else []


def main(argv):
    out, base, head = argv[1:4]
    ch = changed(base, head)
    hdrs = [c for c in ch if c.endswith(".h")]  # A3: tests/sqp/support/indefinite_fixtures.h too
    srcs = [c for c in ch if not c.endswith(".h")]
    print("CHANGED HEADERS:", hdrs)
    print("CHANGED SOURCES:", srcs)
    total_bad = 0
    for cfg in ("Release", "Debug"):
        text = open(os.path.join(out, f"compare-{cfg}.txt")).read()
        differing = sorted(set(re.findall(r"^DIFFERS\s+(\S+\.o)", text, re.M)))
        dep = deps(os.path.join(out, f"deps-after-{cfg}.txt"))

        def rel_deps(obj):
            return [os.path.relpath(d, W) if d.startswith(W) else d for d in dep.get(obj, [])]

        a, b, x = [], [], []
        for obj in differing:
            ds = rel_deps(obj)
            if not ds:
                x.append((obj, "no deps entry"))
                continue
            src = ds[0]
            if src in srcs:
                a.append(obj)
            elif any(h in ds for h in hdrs):
                b.append(obj)
            else:
                x.append((obj, src))
        includers = [o for o in dep if any(h in rel_deps(o) for h in hdrs)]
        identical_includers = [o for o in includers if o not in differing]
        print(f"\n=== {cfg}: {len(differing)} differing objects")
        print(f"  class A (source changed): {len(a)}")
        for o in a:
            print("     ", o)
        print(f"  class B (includer of a changed header, source untouched): {len(b)}")
        print(f"  UNEXPLAINED: {len(x)}")
        for o, why in x:
            print("     ", o, "::", why)
        print(f"  negative control: {len(identical_includers)} includer objects of a changed header "
              f"are IDENTICAL (of {len(includers)} includers in the dependency log)")
        tail = re.findall(r"^P-SYM: .*$", text, re.M)
        print("  compare verdict line:", tail[-1] if tail else "(none)")
        total_bad += len(x)
    print(f"\nCLASSIFICATION: {'PASS' if total_bad == 0 else 'FINDING'} -- {total_bad} unexplained")
    return 0 if total_bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
