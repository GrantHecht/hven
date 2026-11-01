#!/usr/bin/env python3
"""Classify every object P-SYM reports as differing, against the build's own
Ninja dependency log.

THE GATE IS THE CLASSIFICATION, NOT BYTE-IDENTITY (the ledger's W0 T3 statement,
and M7 W2 M4's worked instance): a commit that adds five public fields to
`SqpCounters` moves `sizeof` from 808 to 848 in every TU that lays out or
initialises it, so exit 1 is the EXPECTED status. What is owed is
that every differing object either has changed source or includes a changed
header -- and the NEGATIVE CONTROL, that objects which also include those
headers and are byte-identical exist, so the differing set is not "everything
that sees the header".
"""
import argparse
import os
import re
import subprocess
import sys

# THIS TASK'S OWN CHANGED HEADER LIST, and it is exactly ONE: M7 W2 M2 touches
# `core/solver_counters.h` and nothing else under `include/`. (M1's list carried
# three because it also moved `SqpOptions`; copying that list here would have
# credited this task's differing objects to headers it never edited.)
HEADERS = ("include/hven/core/solver_counters.h",)


def differing_objects(compare_path):
    objs, summary = [], ""
    for line in open(compare_path, errors="replace"):
        if line.startswith("P-SYM: "):
            summary = line.strip()
        m = re.match(r"^DIFFERS\s+(\S+)", line)
        if m and m.group(1) not in objs:
            objs.append(m.group(1))
    return objs, summary


def dep_map(build_dir):
    """object -> set of dependency paths, from `ninja -t deps`."""
    out = subprocess.run(["ninja", "-C", build_dir, "-t", "deps"],
                         capture_output=True, text=True).stdout
    deps, cur = {}, None
    for line in out.splitlines():
        if line and not line.startswith(" "):
            m = re.match(r"^(\S+):", line)
            cur = m.group(1) if m else None
            if cur:
                deps[cur] = set()
        elif line.startswith("    ") and cur:
            deps[cur].add(line.strip())
    return deps


def all_objects(build_dir):
    objs = []
    for root, _, files in os.walk(build_dir):
        for f in files:
            if f.endswith(".o") or f.endswith(".cxx.o"):
                objs.append(os.path.relpath(os.path.join(root, f), build_dir))
    return objs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--compare", required=True)
    ap.add_argument("--build", required=True)
    ap.add_argument("--changed-sources", nargs="*", default=[])
    args = ap.parse_args()

    objs, summary = differing_objects(args.compare)
    deps = dep_map(args.build)
    print(summary)
    print("DIFFERING OBJECTS: {}".format(len(objs)))

    def includes_changed_header(obj):
        d = deps.get(obj, set())
        hits = [h for h in HEADERS if any(x.endswith(h) or h in x for x in d)]
        return hits

    class_a, class_b, unexplained = [], [], []
    for obj in sorted(objs):
        base = os.path.basename(obj)[:-2]
        if any(os.path.basename(s) == base for s in args.changed_sources):
            class_a.append(obj)
        elif includes_changed_header(obj):
            class_b.append(obj)
        else:
            unexplained.append(obj)

    print("  class A -- SOURCE CHANGED by this task: {}".format(len(class_a)))
    for o in class_a:
        print("      " + o)
    print("  class B -- INCLUDERS of a changed header, source untouched: {}".format(len(class_b)))
    for o in class_b:
        print("      " + o)
    print("  UNEXPLAINED: {}".format(len(unexplained)))
    for o in unexplained:
        print("      " + o)

    # THE NEGATIVE CONTROL.
    differing = set(objs)
    control = [o for o in all_objects(args.build)
               if o not in differing and includes_changed_header(o)]
    print("\n  NEGATIVE CONTROL -- objects that ALSO include one of those headers "
          "and are byte-identical: {}".format(len(control)))
    for o in sorted(control)[:80]:
        print("      " + o)
    return 1 if unexplained else 0


if __name__ == "__main__":
    sys.exit(main())
