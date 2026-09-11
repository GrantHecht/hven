# W5 T8.9r-attrib3 -- the SIX intra-T8.4 arms, in commit order. NN label sha7.
#   b01 = T8.3 head (the base of T8.4)          510a4bb
#   b02 = 8ae1618  SolveResult/SolveBudget/declared diagnostics (a PURE ADDITION:
#                  one new header, one new TU, src/CMakeLists.txt 42 -> 43 sources;
#                  interior_point_solver.cpp is NOT touched)
#   b03 = 9cebbbe  IpmResult; solve(model, x0, budget) with phases; the model
#                  borrowed per call; the five entries removed
#   b04 = 9ce9bb2  the SQP side (SqpResult, SolveBudget on the SQP overloads)
#   b05 = 5124aa1  T8.4 fix1 (IPM evaluation provenance, the clock at the
#                  public boundary, the analysis owner id)
#   b06 = 3c8e43b  T8.4 head -- == attribution/arms.txt's a05
#
# THE TWO BENCH/TEST-ONLY COMMITS ARE NOT SEPARATE ARMS, and the reason is that an
# arm is a BUILD: eb8f157 (bench corpus rows) and f607c2c (one test fix) change no
# library source at all, so an arm at either would carry a libhven.a byte-identical
# to its parent's and could only measure this instrument's floor -- which
# attribution/arms.txt already measured on its own control arm (T8.1). They are
# ANCESTORS of b05, so their bench-side work IS present from b05 on; the b04 -> b05
# step therefore carries the harness change as well as fix1, and the report says so.
ARMS="01:T8.3:510a4bb 02:8ae1618:8ae1618 03:9cebbbe:9cebbbe 04:9ce9bb2:9ce9bb2 05:fix1:5124aa1 06:T8.4:3c8e43b"
ARMBIN=/home/ghecht/Projects/hven/.scratch/w5t89ra3/bin
# THE ARGV LOCK (attribution.md section 2/section 6): every path component that
# varies between arms is FIXED WIDTH -- the sha is seven characters at all six
# arms, the arm number two digits, the round one digit -- so the measured child's
# argv byte length is identical at every arm by construction, and the leg PRINTS it.
CELLS="f7_n1000_bound_physics,f7_n5000_bound_physics,f7_n10000_bound_neutral,f7_n20000_bound_neutral"
