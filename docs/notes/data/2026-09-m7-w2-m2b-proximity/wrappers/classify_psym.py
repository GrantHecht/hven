#!/usr/bin/env python3
"""Classify every object P-SYM reports as differing, against the build's own
Ninja dependency log.

THE GATE IS THE CLASSIFICATION, NOT BYTE-IDENTITY (the ledger's W0 T3 statement,
and M7 W2 M4's worked instance): a commit that adds a public field to
`SqpOptions` moves `sizeof` from 400 to 416 in every TU that lays out or
initialises it, so exit 1 is the EXPECTED status. What is owed is
that every differing object either has changed source or includes a changed
header -- and the NEGATIVE CONTROL, that objects which also include those
headers and are byte-identical exist, so the differing set is not "everything
that sees the header".

FIX ROUND 1 (M7 W2 M2b, review finding "the classifier can pass vacuously").
The first round's classifier returned failure ONLY for unexplained objects, so
an EMPTY or MALFORMED comparison -- the file the compare step failed to write,
or wrote a usage error into -- returned 0 and the gate read green over nothing
at all. That is the same class of defect as the wrong-binary incident this
task's own report declares (a green rc over zero tests). Three refusals now
stand between a broken input and a green exit, each of them FOLD-PROVEN in
`gates-fix1/fold-proofs.txt`:

  R1  no `P-SYM: ` summary line in the comparison  -> MALFORMED, exit 3
  R2  zero `DIFFERS` objects in the comparison     -> VACUOUS,   exit 4
  R3  negative control below `--min-negative-control` (default 1), i.e. the
      differing set IS "everything that sees the header" and the classification
      explains nothing                             -> exit 5
  R4  (pre-existing) any unexplained object        -> exit 1

R2 is deliberately unconditional rather than a flag: every use of this script in
this artifact runs it on a comparison that MUST have movers (the options struct
grew), so "no movers" here is always either a failed capture or a failed
compare. A future task whose P-SYM run is legitimately clean does not need a
classifier at all -- it has byte-identity.
"""
import argparse
import os
import re
import subprocess
import sys

# THIS TASK'S OWN CHANGED HEADER LIST, and it is exactly the two headers this
# task edits under `include/`: the restoration wrapper's declaration and
# `SqpOptions`/`SqpResult`. M2's copy of this script carried ONE header
# (`core/solver_counters.h`) because that is what M2 touched; copying its list
# here would have credited this task's differing objects to a header it never
# edited, and copying M1's would have done the same in the other direction.
#
# FIX ROUND 1 edits the SAME two headers (the certificate's doc on
# `SqpResult::infeasibility_certified` and on `SqpOptions::restoration_proximity`;
# the removed `proximity_residual_inf` declaration on the wrapper), so the list
# is unchanged -- and `--changed-headers` now makes that a CHECKED fact rather
# than a claim: the runner passes `git diff --name-only <base> -- include/` and
# this script refuses if the two lists disagree.
HEADERS = (
    "include/hven/detail/globalization/sqp/restoration.h",
    "include/hven/drivers/sqp_solver_types.h",
)


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
    ap.add_argument("--changed-headers", nargs="*", default=None,
                    help="the round's header list, normally `git diff --name-only <base> "
                         "-- include/`; must equal this script's declared HEADERS")
    ap.add_argument("--min-negative-control", type=int, default=1)
    args = ap.parse_args()

    if args.changed_headers is not None:
        got = tuple(sorted(args.changed_headers))
        want = tuple(sorted(HEADERS))
        if got != want:
            print("REFUSED: the round's changed-header list is {} but this script declares {}"
                  .format(list(got), list(want)))
            return 2

    if not os.path.isfile(args.compare):
        print("REFUSED: no comparison file at {}".format(args.compare))
        return 3

    objs, summary = differing_objects(args.compare)
    if not summary:
        print("REFUSED (R1): {} carries no `P-SYM: ` summary line -- the comparison is empty or "
              "malformed, and a classification of nothing is not a pass".format(args.compare))
        return 3
    if not objs:
        print("REFUSED (R2): {} reports the summary {!r} but names no DIFFERS object -- this "
              "task's comparison MUST have movers (the options struct grew), so an empty "
              "differing set is a failed capture, not a clean one"
              .format(args.compare, summary))
        return 4

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

    if unexplained:
        print("REFUSED (R4): {} differing objects are explained by neither a changed source nor a "
              "changed header".format(len(unexplained)))
        return 1
    if len(control) < args.min_negative_control:
        print("REFUSED (R3): the negative control is {} (< {}) -- the differing set is "
              "'everything that sees the header' and this classification explains nothing"
              .format(len(control), args.min_negative_control))
        return 5
    return 0


if __name__ == "__main__":
    sys.exit(main())
