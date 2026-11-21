# DECLARATION-RS2-r2 (successor of DECLARATION-RS2.md; DECLARATION-RS2.md stays frozen; its run-1 data is kept in gates/RS-2-run1/)
Run 1 of the RS-2 gate (move commit 43f5573f9) built clean (G1 rc 0) and passed the lists leg (20 executables raw-identical to the RS-1e measurement; the 12 test
executables raw-identical to the committed RS-0 lists; ctest -N identical modulo the byte-dump class, 133 = 133), but did not meet G2'/G3' as worded, for reasons that
are instrument or wording defects, not tree defects:
1. G2' unity: `scripts/rs_object_map.py` refuses a unity object whose path differs between the manifests (RS-0's rule). The unity target hven_interior_tests lives in the
   CMake directory that RS-2 renames (tests/interior -> tests/ipm), so its four unity objects moved directory by construction: 4 UNITY-UNMAPPED + 4 NEW.
2. G2' `__FILE__` length: every gtest TEST registers through a dynamic initialiser `_GLOBAL__sub_I_<file>` that builds a std::string from `__FILE__` (CodeLocation); the
   compiler copies the constant-length literal inline, so a move that changes the LENGTH of a source file's absolute path changes displacements in that one function
   (e.g. vmovups %ymm0,0x33(%rax) -> 0x32). Run 1: 70 objects, all in `_GLOBAL__sub_I_*` (the design anticipated that `__FILE__` strings change in object bytes, §5 "The claim",
   but not that this reaches instruction operands). The RS-1 gates never met it because no test source moved.
3. G2' define literals: a path-valued -D define (HVEN_SRC_ROOT, HVEN_GOLDEN_DIR, HVEN_TRACE_FIXTURE_DIR) moved with its directory; a test that builds a std::filesystem::path from
   the literal instantiates `path::path<char [N]>` named by the literal's size N, so the instantiation's NAME (and the relocation target of its callers) changes. Run 1: the 4
   objects with SYMBOLS findings (human_trace_goldens, message_sites, trace_off_path, trace_v1_fixtures) plus the callers' RELOC lines.
4. Gate-script defects (no measurement content): flags_diff2.py was invoked with `--allow-removed '-I@/...'` (argparse reads the leading dash as an option; rc 2) and
   `tail -2` was given two files.
Amended criteria and measurement (everything else in DECLARATION-RS2.md unchanged):
- G2'-r2  P-SYM is measured on two NEW arms, built for this purpose with `__FILE__` neutralised: tools/neutral_flags.py emits one flag list -- `-fmacro-prefix-map=<root>/<d>/=` for every
          directory <d> under tests/ in either tree (so `__FILE__` is the bare file name), plus an exact-file map to one fixed token for each of the three S6 renames (clang takes the longest
          matching prefix; verified) -- and BOTH arms are configured with that same list in `CMAKE_CXX_FLAGS` (legs/psym_arms.sh): the before arm is the RS-1e head (3edf5ede4), the after arm
          is this branch's HEAD, each configured and built clean at the same build path (`build-n`, P-SYM's precondition) and captured. The flag list is identical in the two arms by
          construction, so it cannot itself create a difference; it removes only the dependence of the gtest registration initialisers on the length of a source path (class 2).
          The compare uses the generated object map with `--unity-dir-move tests/interior=tests/ipm` (class 1; scripts/rs_object_map_selftest.sh covers it and its negatives: a unity object may
          differ by exactly that declared directory prefix and by nothing else) and the symbol map symbol-map-rs2.txt (RS-0) + the DERIVED path<char [N]> renames of class 3
          (tools/gen_symbol_map_defines.py reads the before/after compile definitions, requires every changed path-valued define to be explained by the move map / the golden directory move,
          and derives N from the literals; a define that changed for any other reason fails it). Criterion: 0 UNCLASSIFIED, 0 MISSING, 0 COLLISION, and the exceptions file is EMPTY
          (restoring the design's wording; no exception of any kind is declared for RS-2).
          The real build (no neutral flags) is what ctest, lists, flags, install and replay run on; the neutral arms exist only for P-SYM.
- G3''    = G3' with the flag spellings fixed (`--allow-removed=REGEX`, `--allow-added=TOKEN`), on the real builds: run-1's `build` (the move commit, no neutral flags) against the RS-1e
          compile_commands captured by baseline2.sh.
Instruments changed for the rerun, committed before it: scripts/rs_object_map.py (+ scripts/rs_object_map_selftest.sh), tools/gen_symbol_map_defines.py (new), tools/neutral_flags.py (new),
legs/psym_arms.sh (new), legs/gate2.sh, legs/env.sh (WT may be preset; used because psym_arms.sh and gate2.sh run from copies outside the tree: the arm builder checks the tree out at the
before commit and back). scripts/psym_compare.sh is unchanged. The rerun is gates/RS-2/. The real build of run 1 is the build under test for the non-P-SYM legs (sources unchanged between run 1
and the rerun; only docs/instrument files differ, none read by a test); G1 stands on run 1's clean build; the Debug build and G5-G9 run on the rerun's tree.
