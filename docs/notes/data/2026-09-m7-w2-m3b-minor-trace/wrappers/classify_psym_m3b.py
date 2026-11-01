#!/usr/bin/env python3
"""Classify every object P-SYM reports as differing, against the build's own
Ninja dependency log.

THE GATE IS THE CLASSIFICATION: every differing object either has changed
source or includes a changed header, with UNEXPLAINED at 0, and the NEGATIVE
CONTROL beside it -- objects that ALSO include those headers and are identical
anyway -- so that the differing set is never just "everything that sees the
header".

FOR M7 W2 M3b THE DECLARED COUNT IS ZERO, which is a different claim from the
rounds this script was written for and a stronger one. Those rounds changed a
struct's layout or a field's type and OWED an explanation for the movers that
followed. This round adds a test seam whose every line is `#ifdef
HVEN_TESTING`-ed away in the production build, and whose one `#include` is
placed where it cannot move a `__LINE__` immediate; what it owes is that
NOTHING in the captured universe moved at all. The classification is still the
gate -- the negative control is what makes "0 differing" mean "the header was
seen and changed nothing" rather than "the capture saw nothing".

FIX ROUND 1 (M7 W2 M2b, review finding "the classifier can pass vacuously");
this file is that landed version at `252201c7`, copied for M7 W2 M3a task A,
copied AGAIN for M7 W2 M3a FIX ROUND 1, and copied a THIRD time here for M7 W2
M3b. M3a's two copies are NOT edited -- they produced committed classifications
and a gate's evidence must stay reproducible from the script that produced it.

WHAT MOVES IN THIS ROUND: nothing in the P-SYM capture set is expected to. The
changed surface is `src/qp/qp_engine.cpp` (hooks, every one of them inside
`#ifdef HVEN_TESTING` and every one of them BELOW that TU's one `assert()`),
`include/hven/detail/qp/qp_engine.h` (one `#include`),
`include/hven/detail/qp/qp_walk_trace.h` (new, and empty without
`HVEN_TESTING`), `tests/CMakeLists.txt`, `tests/qp/test_qp_walk_seam.cpp` and
`docs/testing.md`. The new test source belongs to `hven_qp_walk_seam_tests`,
which -- like `hven_fault_injection_tests` and `hven_ipqp_seam_tests` before it
-- `scripts/psym_compare.sh` deliberately does NOT capture, so no object of it
can appear in this comparison under any outcome.

The first round's classifier returned failure ONLY for unexplained objects, so
an EMPTY or MALFORMED comparison -- the file the compare step failed to write,
or wrote a usage error into -- returned 0 and the gate read green over nothing
at all. That is the same class of defect as the wrong-binary incident this
task's own report declares (a green rc over zero tests). Three refusals now
stand between a broken input and a green exit, each of them FOLD-PROVEN in
`gates/fold-proofs.txt`:

  R1  no `P-SYM: ` summary line in the comparison  -> MALFORMED, exit 3
  R2  a differing count other than `--expect-differing` -> exit 4
  R3  negative control below `--min-negative-control` (default 1), i.e. the
      differing set IS "everything that sees the header" and the classification
      explains nothing                             -> exit 5
  R4  (pre-existing) any unexplained object        -> exit 1

R2 WAS unconditional in every earlier copy ("zero movers is always a failed
capture"), with a closing note that "a future task whose P-SYM run is
legitimately clean does not need a classifier at all -- it has byte-identity".
M7 W2 M3b IS that task, and the note was half right: it still needs the
classifier, because a clean run has to be PROVED clean over the capture's own
universe with the negative control beside it, not merely observed to print
nothing. So R2 becomes a comparison against a count this round DECLARES
(`--expect-differing`, required, no default). Declaring 0 and finding one mover
fails; declaring 12 and finding 10 fails, which is M3a fix1's G5 exactly.

M7 W2 M3b'S TWO CHANGES TO THIS FILE, both in `differing_objects` and in R2,
are described at their sites. The HEADERS list is this round's two headers under
`include/`: the walk engine's header, which gains one `#include`, and the seam
header it now includes, which is new.
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
    "include/hven/detail/qp/qp_engine.h",
    "include/hven/detail/qp/qp_walk_trace.h",
)


def differing_objects(compare_path):
    """Every object P-SYM reports as NOT identical, by EITHER route.

    THE REGISTERED DEFECT THIS FIXES (M7 W2 M3a fix1's gates-fix1/README.md,
    "A defect this exposed, registered and NOT silently fixed"). Every earlier
    copy of this script harvested the differing set with `^DIFFERS` alone. An
    object whose P-SYM verdict is `NOISE-ONLY` PLUS a `RELOCATIONS` line --
    Release's `trace_writer.cpp.o` in that round, whose only residue was one
    relocation target renamed -- was therefore never classified at all, and
    fell through into the NEGATIVE CONTROL, which prints its members under the
    heading "objects that ALSO include one of those headers and ARE
    BYTE-IDENTICAL". Its bytes were not identical. The label was wrong.

    It is fixed HERE and not in M3a's copies, which produced committed
    evidence and are never edited. It matters more for this round than it did
    for that one: this round declares ZERO movers, so an object that escapes
    the harvest is exactly an object that would let a false zero pass.
    """
    objs, summary = [], ""
    for line in open(compare_path, errors="replace"):
        if line.startswith("P-SYM: "):
            summary = line.strip()
        m = re.match(r"^(?:DIFFERS|RELOCATIONS)\s+(\S+)", line)
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
    ap.add_argument("--expect-differing", type=int, required=True,
                    help="how many differing objects THIS ROUND declares, before the run. "
                         "R2 below is asserted against it, so a round whose P-SYM run is "
                         "legitimately CLEAN declares 0 and a single mover fails the gate")
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
    if len(objs) != args.expect_differing:
        print(summary)
        print("DIFFERING OBJECTS: {}".format(len(objs)))
        for o in sorted(objs):
            print("      " + o)
        print("REFUSED (R2): {} names {} differing object(s) and this round DECLARED {} -- a "
              "shape that is not the declared one is re-derived and declared again, never "
              "discovered".format(args.compare, len(objs), args.expect_differing))
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
