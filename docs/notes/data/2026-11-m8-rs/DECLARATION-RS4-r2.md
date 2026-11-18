# RS-4 -- DECLARATION r2 (base moved to GL-0; order-dependent includes surfaced by the sort)
Committed before any edit of the re-based RS-4 code commits; DECLARATION-RS4.md is unchanged. Two facts change the plan's inputs, no criterion:
1. BASE. m8 moved from 4064ff97a to ca358873524a1ef5c8d750c29266b26989919736 (GL-0, globalization diagnostics; 133 files, src/sqp, include/hven/{solver,detail/sqp}, tests/sqp, tests/trace).
   The RS-4 code commits are re-made on the new base (the respell and the format are re-run on it, so GL-0's files are normalised too); the "before" arm, the lists, the replay reference and the
   flags reference are re-measured on ca3588735 (legs/baseline2.sh in a separate detached worktree). gate4.sh's default base is ca3588735. Where DECLARATION-RS4.md says 4064ff97a read ca3588735.
2. ORDER DEPENDENCE (found by an earlier gate run 1 on the pre-rebase head ded1b1c30, evidence kept in gates/RS-4-run1/): the sorted tree did not compile. Three objects failed:
   src/sqp/assembly_eval_seam.cpp (library + the seam-test copy) and tests/solver/test_infinite_bound.cpp. Sorting put detail/sqp/assembly_eval_seam.h and
   detail/sqp/globalization/restoration.h ABOVE detail/sqp/sqp_solver_impl.h, which defines NlpEval; both headers are NOT self-contained BY DOCUMENTED DESIGN (they name NlpEval and
   sqp_solver_impl.h includes them itself, mid-file, after NlpEval). A "missing include in the dependent header" is not available: the header would have to include sqp_solver_impl.h, which
   includes it back (a cycle); the real remedy, extracting NlpEval into its own header, is a structural change outside RS-4 and is registered for RS-7.
   Declared fix (DECLARATION-RS4.md item 3's remedy class, no clang-format off block): in each failing consumer the dependent include sits in its own include block BELOW sqp_solver_impl.h,
   separated by an explanatory comment line (clang-format ends an include block at a comment line, so sorting cannot hoist it; formatting the fixed files is a no-op). Any further order
   dependence the re-based build exposes is fixed the same way and listed in RESULT.txt.
Commit structure: (a) instruments, (b) respell + exception 4, (c1) .clang-format, (c2) pure-format commit, (c3) order-dependence fix, (d) blame-ignore, (e) evidence.
G1-G9 are exactly as declared; the gate is run in full on the final head.
