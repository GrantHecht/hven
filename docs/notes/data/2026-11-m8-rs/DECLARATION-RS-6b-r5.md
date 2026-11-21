# RS-6b -- DECLARATION r5 (successor round on the new base; -r1..-r4 stay frozen)
Written after rs6b was rebased (clean, no conflicts) onto origin/m8 afc9744bf (examples re-cut: examples/, the root CMakeLists HVEN_BUILD_EXAMPLES default, docs; no library source changed) and before any leg of this round. Code under test: the rebased head; commits after this declaration touch only docs/notes/data evidence. A later change to tests/, src/, include/, bench/, scripts/, .github/ or a CMakeLists.txt voids the round.
## Scope (limited)
R1 Linux Release ctest, R2 Linux Debug ctest: expected the earlier 3925 plus the examples' ctest entries (10), 0 failed.
R3 `ctest -N` of the head equals a base list taken at afc9744bf (same P-SYM arm instrument: base and head built at one build path); every executable's gtest list identical.
R4 libhven.a members equal to afc9744bf's and every library object (CMakeFiles/hven.dir) byte-identical (comparison by gates/RS-6b/objcmp_r3.py; the three bench objects that embed the commit stamp are normalised as in r3 and reported).
R5 a four-lane CI run on the rebased head, all green, with the examples' ctest entries run on the Windows lane (they already ran green on Windows on the examples branch; this confirms it on this head).
## Carried forward unchanged
The r1-r4 mutation evidence (nine isolated single-fault Windows runs plus the core2 replacement) and the Windows gate run on 72cc139df: the code under test is unchanged by the rebase (the base moved only in examples/, the root default and docs, all disjoint from tests/ CMake and the test sources RS-6b changed), so those results are not re-run. Install smoke is not re-run in this round (not in scope; the examples change is the base's own).
