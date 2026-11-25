#!/usr/bin/env bash
# usage: WT=<worktree> bash mutation_rsr1.sh <build-dir-name>   (run under the box lock by gate_rsr1.sh; needs the Release build dir already built)
# RS-R1 mutation proofs (DECLARATION-RS-R1.md "Mutations"): plant a defect, the named check must FAIL with the expected message; restore it, it must PASS.
# R1/R2/R6 are configure-only. R3/R4 run the built hven_core_tests (Layering.* reads the source tree at run time, so no rebuild). R5 compiles the one generated self-containment TU of qp_options.h with the compile
# command of the build, so only that TU is parsed. Every plant is restored by `git checkout` even on error.
set -o pipefail
source "$(dirname "$0")/env.sh"
export GIT_DIR=$(git -C "$WT" rev-parse --absolute-git-dir) GIT_WORK_TREE=$WT
BD=$WT/${1:?build dir}
E=$WT/docs/notes/data/2026-11-m8-rs; MD=$E/gates/RS-R1/mutation; mkdir -p "$MD"; V=$MD/MUTATION-VERDICTS.txt; : > "$V"
cd "$WT" || exit 1
[ -z "$(git status --porcelain --untracked-files=no)" ] || { echo "tracked tree not clean"; exit 2; }
D=$WT/.scratch/mut-build
restore() { git checkout -q -- tests include src 2>/dev/null; rm -f tests/core/test_planted_orphan.cpp; }
trap restore EXIT
cfg() { rm -rf "$D"; cmake -S "$WT" -B "$D" -G Ninja -DCMAKE_BUILD_TYPE=Release -DHVEN_FP_MODE=SAFER_FAST -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON \
    -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/home/ghecht/Projects/hven/build/_deps/googletest-src > "$1" 2>&1; }
verdict() { echo "$1" | tee -a "$V"; }
expect_fail() { cfg "$MD/$1-planted.txt"; rc=$?
  if [ $rc -ne 0 ] && tr -s ' \n' ' ' < "$MD/$1-planted.txt" | grep -Eq "$3"; then verdict "$1 $2: planted -> configure FAILED with the expected message (rc=$rc) [KILLED]"; else verdict "$1 $2: planted -> rc=$rc, expected /$3/ absent [SURVIVED]"; fi; }
expect_pass() { cfg "$MD/$1-restored.txt"; rc=$?; if [ $rc -eq 0 ]; then verdict "$1 restored -> configure passes (rc=0)"; else verdict "$1 restored -> rc=$rc [BAD RESTORE]"; fi; }
cfg "$MD/00-clean.txt"; verdict "clean tree: configure rc=$? (want 0)"
# R1: a seam executable taking a source from outside tests/<dir>/seam/
python3 - <<'PY'
p = 'tests/ipm/CMakeLists.txt'; s = open(p).read(); o = 'SOURCES seam/test_ipm_dense_guard_seams.cpp)'
assert o in s; open(p, 'w').write(s.replace(o, 'SOURCES seam/test_ipm_dense_guard_seams.cpp test_soc.cpp)', 1))
PY
expect_fail R1-seam-takes-ordinary-source "hven_ipm_seam_tests also lists tests/ipm/test_soc.cpp" 'is a seam executable .HVEN_TESTING. but takes the source tests/ipm/test_soc.cpp'
git checkout -q -- tests; expect_pass R1-seam-takes-ordinary-source
# R2: an ordinary executable taking a source from inside seam/
python3 - <<'PY'
p = 'tests/ipm/CMakeLists.txt'; s = open(p).read(); o = 'hven_add_test_executable(hven_ipm_tests\n    SOURCES ${_hven_ipm_sources}'
assert o in s; open(p, 'w').write(s.replace(o, o + ' seam/test_ipm_dense_guard_seams.cpp', 1))
PY
expect_fail R2-ordinary-takes-seam-source "hven_ipm_tests also lists seam/test_ipm_dense_guard_seams.cpp" 'is an ordinary executable but takes the source tests/ipm/seam/test_ipm_dense_guard_seams.cpp'
git checkout -q -- tests; expect_pass R2-ordinary-takes-seam-source
# R6 (carried from RS-6a): orphan source / duplicate claim / seam links hven::hven
printf '#include <gtest/gtest.h>\nTEST(PlantedOrphan, Never) {}\n' > tests/core/test_planted_orphan.cpp
expect_fail R6a-orphan "a tests/core source in no executable" 'is in no test executable'
rm -f tests/core/test_planted_orphan.cpp; expect_pass R6a-orphan
printf '#include <gtest/gtest.h>\nTEST(PlantedSeamOrphan, Never) {}\n' > tests/ipm/seam/test_planted_orphan.cpp
expect_fail R6b-seam-orphan "a tests/ipm/seam source in no executable" 'is in no test executable'
rm -f tests/ipm/seam/test_planted_orphan.cpp; expect_pass R6b-seam-orphan
printf '\ntarget_link_libraries(hven_ipm_seam_tests PRIVATE hven::hven)\n' >> tests/ipm/CMakeLists.txt
expect_fail R6c-seam-links-hven "hven_ipm_seam_tests linked to hven::hven" 'seam executable .HVEN_TESTING. and links hven::hven'
git checkout -q -- tests; expect_pass R6c-seam-links-hven
rm -rf "$D"
# R3: the funnel include re-added to the JSON-lines sink -> an upward include (trace -> engine implementation)
runcore() { "$BD/tests/core/hven_core_tests" --gtest_filter="$1" > "$2" 2>&1; }
runcore 'Layering.*' "$MD/R3-clean.txt"; verdict "R3 clean: Layering.* rc=$? (want 0)"
sed -i 's|#include <hven/trace/json_lines_trace_sink.h>|#include <hven/detail/sqp/globalization/funnel.h>\n#include <hven/trace/json_lines_trace_sink.h>|' src/trace/json_lines_trace_sink.cpp
runcore 'Layering.*' "$MD/R3-planted.txt"; rc=$?
if [ $rc -ne 0 ] && grep -q 'json_lines_trace_sink.cpp.*funnel.h' "$MD/R3-planted.txt"; then verdict "R3 funnel.h re-added to json_lines_trace_sink.cpp: Layering.NoIncludeGoesUpTheLayerOrder FAILED naming the include (rc=$rc) [KILLED]"; else verdict "R3 funnel.h re-added: rc=$rc, expected message absent [SURVIVED]"; fi
git checkout -q -- src; runcore 'Layering.*' "$MD/R3-restored.txt"; verdict "R3 restored: Layering.* rc=$? (want 0)"
# R4: a new unlisted forward declaration of an upward type (core/types.h forward-declares SqpOptions)
printf '\nnamespace hven {\nstruct SqpOptions;\n} // namespace hven\n' >> include/hven/core/types.h
runcore 'Layering.*' "$MD/R4-planted.txt"; rc=$?
if [ $rc -ne 0 ] && grep -q "types.h.*forward-declares 'SqpOptions'" "$MD/R4-planted.txt"; then verdict "R4 unlisted upward forward declaration in core/types.h: Layering.NoForwardDeclarationGoesUpTheLayerOrder FAILED naming it (rc=$rc) [KILLED]"; else verdict "R4 upward forward declaration: rc=$rc, expected message absent [SURVIVED]"; fi
git checkout -q -- include; runcore 'Layering.*' "$MD/R4-restored.txt"; verdict "R4 restored: Layering.* rc=$? (want 0)"
# R5: qp_options.h made non-self-contained (its core/types.h include removed); parse its generated standalone TU with the build's own compile command
TU=$(python3 - "$BD/compile_commands.json" <<'PY'
import json, sys
for e in json.load(open(sys.argv[1])):
    if e['file'].endswith('header_selfcheck/hven_qp_qp_options.h.cpp'): print(e['directory'] + '|' + (e.get('command') or ' '.join(e['arguments'])))
PY
)
[ -n "$TU" ] || { verdict "R5 FAILED TO FIND the generated TU for qp_options.h in the compile database"; exit 1; }
CDIR=${TU%%|*}; CCMD=${TU#*|}; CCMD=$(echo "$CCMD" | sed -E 's/ -o [^ ]+//; s/ -MD//; s/ -MT [^ ]+//; s/ -MF [^ ]+//')
selfcheck() { (cd "$CDIR" && eval "$CCMD -fsyntax-only") > "$1" 2>&1; }
selfcheck "$MD/R5-clean.txt"; verdict "R5 clean: qp_options.h standalone TU rc=$? (want 0)"
sed -i '/#include <hven\/core\/types.h>/d' include/hven/qp/qp_options.h
selfcheck "$MD/R5-planted.txt"; rc=$?
if [ $rc -ne 0 ] && grep -q "qp_options.h" "$MD/R5-planted.txt"; then verdict "R5 qp_options.h without core/types.h: its standalone TU FAILED to compile, naming the header (rc=$rc) [KILLED]"; else verdict "R5 non-self-contained qp_options.h: rc=$rc, expected message absent [SURVIVED]"; fi
git checkout -q -- include; selfcheck "$MD/R5-restored.txt"; verdict "R5 restored: qp_options.h standalone TU rc=$? (want 0)"
grep -c KILLED "$V" | xargs echo "killed:"; grep -c SURVIVED "$V" | xargs echo "survived:"
! grep -q 'SURVIVED\|BAD RESTORE\|rc=[1-9].*(want 0)' "$V"
