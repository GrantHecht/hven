# Shared constants for every W6 T6 leg.  Sourced, never executed.
R=/home/ghecht/Projects/hven
F=$R/.scratch/w6t6
B=$F/build-release
# BIN_OVERRIDE is the FOLD PROOF's only hook: it lets fold_proof.sh point every
# leg script at a binary that fails, so each wrapper is proven to carry a failing
# status out rather than reporting its last echo. Unset in every measured run.
BIN=${BIN_OVERRIDE:-$B/bench/hven_sqp_corpus}
# The uniform flag regime, spelled as CMakePresets.json's linux-clang-release
# preset spells it (the presets themselves cannot be used: their binaryDir is
# fixed under ${sourceDir}).
UNIFORM=(-G Ninja
         -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE
         -DHVEN_FP_MODE=SAFER_FAST
         -DCMAKE_CXX_COMPILER=/usr/bin/clang++)
# The pinned measurement core and its SMT sibling (cpu2 <-> cpu10 on this box,
# /sys/devices/system/cpu/cpu2/topology/thread_siblings_list = 2,10).  The same
# pair T8.9r reserved.
CORE=2
SIB=10
# pass A, T8.9r's instrument (ownership doc section 11.1 / 11.4).
PA=instructions:u,branches:u,cycles:u,branch-misses:u,L1-icache-load-misses:u
