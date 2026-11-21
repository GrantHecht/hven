# DECLARATION-r2 (successor; DECLARATION.md stays frozen; its run-1 data for RS-1a is kept in gates/RS-1a-run1/)
Run 1 of the RS-1a gate (P-SYM and flags steps) did not meet G2/G3 as worded, for two reasons that are instrument/wording defects, not tree defects:
1. G2 said "empty exceptions file". The design itself mandates one real test-code edit in RS-1a (M4: test_core_layering scans detail/core and asserts
   the 11+6 count), so that test object differs structurally by construction; and P-SYM cannot map an object to nothing, so the deleted color_text object
   shows as MISSING (run 1: 1 MISSING, 1 unclassified object = test_core_layering, 3 noise-only objects).
2. flags_diff.py was invoked with `--expect-removed -I@/...`, which argparse reads as an option (usage error, rc=2).
Amended criteria (everything else in DECLARATION.md unchanged):
- G2'  P-SYM with the generated --object-map: 0 UNCLASSIFIED once the group's exceptions file is applied. A group's exceptions file may name ONLY the findings of
       objects of test sources the design mandates editing in that group (RS-1a: tests/core/test_core_layering.cpp.o; later groups' analogues are named in
       their own gate directory with the design clause that mandates the edit), each entry carrying that reason. No library, bench or unedited-test object may be excepted.
       Objects the map deletes are removed from the before arm and their defined symbols recorded in gates/<g>/removed-symbols.txt (a deletion is accounted for in the
       open as removed symbols only); every other object must pass without exception.
- G3'  as G3, with the expected removal passed as `--expect-removed=-I@/src/interior/utils`.
The rerun is gates/RS-1a/. Run 1 stays as evidence of what the unamended instrument said.
