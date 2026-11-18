# DECLARATION-RS2-r3 (successor of DECLARATION-RS2-r2; DECLARATION-RS2.md and -r2 stay frozen)
The rerun gate (gates/RS-2/VERDICTS.txt, steps psym flags lists) met G2'-r2 (P-SYM PASS: 198 objects, 188 byte-identical, 3 noise-class, 0 unclassified, 0 missing, empty exceptions) and the
lists leg, and flags_diff2 reported `TUs whose flag list differs beyond the expected: 0` -- but its RESULT was DIFFERENT because it keys a unity TU by its source path, and the four unity
sources of hven_interior_tests live under the build tree of the CMake directory RS-2 renames (build/tests/interior/CMakeFiles/.../Unity/unity_N_cxx.cxx -> build/tests/ipm/...): 4 only-before + 4
only-after. Same family as r2's class 1, in the flags instrument. Amended (G3'''): flags_diff2.py gains `--key-dir-move OLD=NEW`, applied to the SOURCE KEY of a BEFORE-arm TU only (never to a flag
token), invoked as `--key-dir-move build/tests/interior=build/tests/ipm`; the four unity TUs then pair by key and their flag lists are compared like every other TU. Nothing else changes: the TU
sets must be identical and every flag list identical except the declared deltas of DECLARATION-RS2.md G3'. Instrument changed: tools/flags_diff2.py, legs/gate2.sh (committed before the re-run of the
flags step; the flags step needs no rebuild: it compares the RS-1e compile_commands.json against run 1's build, which is the real build under test).
