#!/usr/bin/env python3
"""Classify every object P-SYM reports as differing, against the build's own
Ninja dependency log.

THE GATE IS THE CLASSIFICATION, NOT BYTE-IDENTITY (the ledger's W0 T3 statement,
and M7 W2 M4's worked instance): a commit that adds a field to `SqpCounters`
moves `sizeof` from 848 to 856 in every TU that lays out or initialises it, so
exit 1 is the EXPECTED status. What is owed is
that every differing object either has changed source or includes a changed
header -- and the NEGATIVE CONTROL, that objects which also include those
headers and are byte-identical exist, so the differing set is not "everything
that sees the header".

FIX ROUND 1 (M7 W2 M2b, review finding "the classifier can pass vacuously");
this file is that landed version at `252201c7`, copied for M7 W2 M3a task A
and copied AGAIN for M7 W2 M3a FIX ROUND 1 (this file) with only its HEADERS
list and its struct-name prose re-pointed. `classify_psym.py` beside it is task
A's copy and is NOT edited -- it is what produced the committed `gates/`
classification, and a gate's evidence must stay reproducible from the script
that produced it.

WHAT MOVES IN THIS ROUND, AND WHY IT IS NOT A `sizeof` CHANGE. fix1 replaces
`SqpCounters::restoration_sub_status`'s TYPE (`SolveStatus` ->
`RestorationSubStatus`, same underlying type, same width), so NO offset and no
`sizeof` moves and the "every TU that lays the struct out" universe of task A is
NOT the universe here. What moves is narrower and has two causes: the TUs whose
SOURCE changed, and the TUs that compare or print that field through the
counters X-macro table -- a `gtest` `EXPECT_EQ` on the field instantiates
template symbols carrying the TYPE'S NAME, so renaming the type adds symbols to
those objects. Both causes are enumerated in the round's declared mover shape at
the top of `gates-fix1.sh`.
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
this artifact runs it on a comparison that MUST have movers (task A grew the
counters struct; fix1 changed three sources under src/ and the type of a counters
field), so "no movers" here is always either a failed capture or a failed
compare. A future task whose P-SYM run is legitimately clean does not need a
classifier at all -- it has byte-identity.
"""
import argparse
import os
import re
import subprocess
import sys

# THIS ROUND'S OWN CHANGED HEADER LIST, and it is exactly the TWO headers M7 W2
# M3a FIX ROUND 1 edits under `include/`: the counters contract, whose
# `restoration_sub_status` changes TYPE, and the solve-outcome vocabulary, which
# gains the `RestorationSubStatus` enum and the total map into it. Task A's copy
# of this script carried the ONE header task A touched; copying its list here
# would have credited this round's differing objects to a header it did not
# edit, and would have left the enum's own header unaccounted for.
# `--changed-headers` makes the list a CHECKED fact rather than a claim: the
# runner passes `git diff --name-only <base> -- include/` and this script
# refuses if the two lists disagree.
HEADERS = (
    "include/hven/core/solver_counters.h",
    "include/hven/core/solver_status.h",
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
              "round's comparison MUST have movers (three sources under src/ changed and the "
              "counters field changed type), so an empty differing set is a failed capture, "
              "not a clean one".format(args.compare, summary))
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
