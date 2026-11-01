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

M7 W2 M3b FIX ROUND 1 COPIED THIS AGAIN and repaired the NEGATIVE CONTROL's
label. See `noise_only_objects` below: the control used to be the complement of
the MOVER set alone, so an object P-SYM reported as NOISE-ONLY -- bytes differ,
differences are `__LINE__`-class noise -- was printed under a heading that said
"byte-identical". Three of M3b's own Debug objects were. The labelling is now
three-way (mover / noise-only / byte-identical), the mover set and therefore the
`--expect-differing` declaration are UNCHANGED, and R5 asserts that no object is
reported as both. M3b's own copy is not edited: it produced committed
classifications and a gate's evidence must stay reproducible from the script
that produced it.

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
  R6  (M4) the header derivation cannot be taken, or comes back EMPTY, so the
      classification could not be non-vacuous                -> exit 7

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
are described at their sites.

M7 W2 M4'S ONE CHANGE, AND WHY IT EXISTS. Every copy of this script down to and
including M3b's CARRIED its changed-header list as a literal tuple, and M4's
chain inherited M3b's tuple -- `qp_walk_trace.h` alone -- while M4's own
`git diff --name-only f5205770..196f0ef0 -- include/` names TWO headers, the
walk seam and the new `qp_anti_cycling_arm.h`. The `--changed-headers`
cross-check did its job and REFUSED (rc 2), so M4's G5 failed against its own
machinery rather than against the tree; but the refusal was not a finding about
the code, it was the third instance of one error class -- A DECLARATION THAT WAS
CARRIED INSTEAD OF DERIVED (M7 W2 M3a fix1 and M3b fix1 were the first two).
M3b's own copy said so in the comment above its tuple and still kept the tuple.

So this copy DERIVES the list and carries nothing. `--base` and `--head` are
required, the script runs `git -C <repo> diff --name-only <base>..<head> --
include/` itself, PRINTS the derivation (the command, the two revisions as the
full sha they resolve to, and the list) into its own transcript, and refuses if
the derivation cannot be taken or comes back empty. `--changed-headers` stays,
and is now what it always should have been: the RUNNER's independently-derived
list, checked against this script's, so that a disagreement between two
derivations is still a refusal. A literal can go stale; `base..head` cannot.

THE DERIVATION IS `base..head`, NOT `base`. `git diff --name-only <base>` compares
the base against the WORKING TREE, which is the right thing for a gate that runs
on a clean checkout of head and the wrong thing for one run afterwards, offline,
over captures already taken: the tree can have moved on. Naming both endpoints
makes the classification reproducible from the artifact forever.
"""
import argparse
import os
import re
import subprocess
import sys

# NO LITERAL HEADER LIST LIVES IN THIS FILE. `derive_headers` below takes it
# from the round's own `base..head` diff; see the last paragraphs of the module
# docstring for why the literal was removed rather than corrected.


def derive_headers(repo, base, head):
    """This round's changed headers, DERIVED from its own base..head diff.

    Returns a sorted tuple, or None if the derivation could not be taken. The
    derivation is PRINTED -- the command, both revisions resolved to full shas,
    and the resulting list -- so a reader of the transcript can re-run it.
    """
    def rev(r):
        p = subprocess.run(["git", "-C", repo, "rev-parse", r],
                           capture_output=True, text=True)
        return p.stdout.strip() if p.returncode == 0 else None

    base_sha, head_sha = rev(base), rev(head)
    print("HEADER DERIVATION")
    print("  repo : {}".format(repo))
    print("  base : {} -> {}".format(base, base_sha or "UNRESOLVED"))
    print("  head : {} -> {}".format(head, head_sha or "UNRESOLVED"))
    print("  cmd  : git -C {} diff --name-only {}..{} -- include/".format(repo, base, head))
    if base_sha is None or head_sha is None:
        print("REFUSED (R6): a revision this round names does not resolve in {}".format(repo))
        return None
    p = subprocess.run(["git", "-C", repo, "diff", "--name-only",
                        "{}..{}".format(base, head), "--", "include/"],
                       capture_output=True, text=True)
    if p.returncode != 0:
        print("REFUSED (R6): the header derivation failed (git exited {}): {}"
              .format(p.returncode, p.stderr.strip()))
        return None
    headers = tuple(sorted(x for x in p.stdout.split("\n") if x.strip()))
    print("  list : {}".format(list(headers) if headers else "EMPTY"))
    if not headers:
        print("REFUSED (R6): the derivation names NO header under include/. This script "
              "classifies differing objects into 'changed source' and 'includes a changed "
              "header'; with no header there is no class B and no negative control, and a "
              "classification that cannot be non-vacuous is not a gate.")
        return None
    return headers


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


def noise_only_objects(compare_path):
    """Every object P-SYM reports as NOISE-ONLY: its BYTES DIFFER, and every
    difference its disassembly comparison found was `__LINE__`-class noise.

    THE DEFECT THIS CLOSES (M7 W2 M3b review, Important 1). The differing set
    above is the MOVER set, and it is the right thing to declare against. But
    every object NOT in it was then printed by the negative control under the
    heading "objects that ALSO include one of those headers and are
    BYTE-IDENTICAL" -- and a NOISE-ONLY object is not byte-identical. In M3b's
    own Debug classification that heading claimed byte-identity for
    `sqp_solver.cpp.o`, `ipqp_engine.cpp.o` and `qp_engine.cpp.o`, all three of
    which differ in bytes. The mover count was right; the label beside it was
    false, and a false label under a zero-mover declaration is exactly the kind
    of thing that makes a reader believe a stronger claim than was measured.

    THE LABELLING IS THEREFORE THREE-WAY, not two: MOVER (P-SYM found a real
    difference), NOISE-ONLY (bytes differ, differences are noise), and
    BYTE-IDENTICAL (neither). An object can never be in more than one, and the
    third is now the complement of the first two rather than of the first
    alone.

    NOTE WHAT THIS DOES NOT CHANGE: the mover set, and so the
    `--expect-differing` declaration, is untouched. This is a labelling repair,
    not a loosening of the gate -- and R5 below turns it into an assertion.
    """
    objs = []
    for line in open(compare_path, errors="replace"):
        m = re.match(r"^NOISE-ONLY\s+(\S+)", line)
        if m and m.group(1) not in objs:
            objs.append(m.group(1))
    return objs


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
    ap.add_argument("--repo", required=True,
                    help="the git worktree the header derivation is taken in")
    ap.add_argument("--base", required=True,
                    help="the round's BASE revision; the header list is derived from "
                         "`git diff --name-only <base>..<head> -- include/`")
    ap.add_argument("--head", required=True, help="the round's HEAD revision")
    ap.add_argument("--changed-headers", nargs="*", default=None,
                    help="the RUNNER's own independently-derived header list. Optional. "
                         "When given it must equal the list this script derives, so that "
                         "two derivations disagreeing is still a refusal")
    ap.add_argument("--expect-differing", type=int, required=True,
                    help="how many differing objects THIS ROUND declares, before the run. "
                         "R2 below is asserted against it, so a round whose P-SYM run is "
                         "legitimately CLEAN declares 0 and a single mover fails the gate")
    ap.add_argument("--min-negative-control", type=int, default=1)
    args = ap.parse_args()

    headers = derive_headers(args.repo, args.base, args.head)
    if headers is None:
        return 7

    if args.changed_headers is not None:
        got = tuple(sorted(args.changed_headers))
        if got != headers:
            print("REFUSED: the runner's changed-header list is {} but this round's own "
                  "{}..{} diff derives {}".format(list(got), args.base, args.head,
                                                  list(headers)))
            return 2
    print("HEADER LIST ACCEPTED: {}".format(list(headers)))

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
        hits = [h for h in headers if any(x.endswith(h) or h in x for x in d)]
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

    # THE LABELLING, THREE-WAY. MOVER / NOISE-ONLY / BYTE-IDENTICAL, and an
    # object is in exactly one of them. The negative control is the third, and
    # only the third: it used to be the complement of the movers alone, which
    # printed every NOISE-ONLY object as byte-identical.
    differing = set(objs)
    noise = noise_only_objects(args.compare)
    noise_set = set(noise)

    # MOVER TAKES PRECEDENCE, and this is not a detail. P-SYM's per-object
    # VERDICT line is `DIFFERS` or `NOISE-ONLY`, and a `RELOCATIONS` line can
    # accompany EITHER -- M3a fix1's Release `trace_writer.cpp.o` was exactly
    # `NOISE-ONLY` plus `RELOCATIONS`, which is the shape the harvest repair
    # exists for. Such an object is a MOVER: its relocations really moved, and
    # the noise verdict describes only its disassembly. It is therefore removed
    # from the NOISE-ONLY label here, so the three classes stay disjoint and no
    # object can be counted twice.
    noise_set -= differing
    noise = [o for o in noise if o not in differing]

    # R5 -- the one overlap that would be incoherent rather than merely
    # layered: a single object carrying BOTH verdict lines. P-SYM emits one
    # verdict per object, so this cannot happen; if it ever did, the harvest
    # above would be reading a file it does not understand and no labelling
    # taken from it could be trusted. Asserted rather than assumed.
    verdicts = {}
    for line in open(args.compare, errors="replace"):
        m = re.match(r"^(DIFFERS|NOISE-ONLY)\s+(\S+)", line)
        if m:
            verdicts.setdefault(m.group(2), set()).add(m.group(1))
    both = sorted(o for o, v in verdicts.items() if len(v) > 1)
    if both:
        print("REFUSED (R5): {} object(s) carry BOTH a DIFFERS and a NOISE-ONLY verdict line, "
              "which P-SYM does not emit -- the comparison is not the format this script "
              "harvests: {}".format(len(both), both))
        return 6

    noise_here = [o for o in noise if includes_changed_header(o) or
                  any(os.path.basename(sr) == os.path.basename(o)[:-2]
                      for sr in args.changed_sources)]
    noise_lib = [o for o in noise if o.startswith("CMakeFiles/hven.dir/")]
    print("\n  NOISE-ONLY -- objects whose BYTES DIFFER and whose every disassembly "
          "difference P-SYM classified as __LINE__-class noise: {} ({} of them in the "
          "PRODUCTION LIBRARY, {} explained by this round's changed source or header)"
          .format(len(noise), len(noise_lib), len(noise_here)))
    for o in sorted(noise_lib):
        print("      LIB   " + o)
    for o in sorted(o for o in noise if o not in set(noise_lib)):
        print("      test  " + o)

    control = [o for o in all_objects(args.build)
               if o not in differing and o not in noise_set and includes_changed_header(o)]
    print("\n  NEGATIVE CONTROL -- objects that ALSO include one of those headers and are "
          "BYTE-IDENTICAL (neither a mover nor NOISE-ONLY): {}".format(len(control)))
    for o in sorted(control):
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
