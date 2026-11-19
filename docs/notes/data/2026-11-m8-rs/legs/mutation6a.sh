#!/usr/bin/env bash
# usage: WT=<worktree> bash mutation6a.sh      (configure-only proofs; no compile; run under the box lock by gate6a.sh)
# RS-6a mutation proofs (DECLARATION-RS6a.md G8): plant a defect in the build description, the configure (or generate) must FAIL with the expected message; restore it, the configure must PASS.
# Every plant is restored by `git checkout`/rm even on error. Output: gates/RS-6a-r6/mutation/*.txt and MUTATION-VERDICTS.txt.
set -o pipefail
source "$(dirname "$0")/env.sh"
export GIT_DIR=$(git -C "$WT" rev-parse --absolute-git-dir) GIT_WORK_TREE=$WT
E=$WT/docs/notes/data/2026-11-m8-rs; MD=$E/gates/RS-6a-r6/mutation; mkdir -p "$MD"; V=$MD/MUTATION-VERDICTS.txt; : > "$V"
cd "$WT" || exit 1
[ -z "$(git status --porcelain --untracked-files=no)" ] || { echo "tracked tree not clean"; exit 2; }
D=$WT/.scratch/mut-build
restore() { git checkout -q -- tests 2>/dev/null; rm -f tests/core/test_planted_orphan.cpp; }
trap restore EXIT
cfg() { # <log> : configure (and generate) into $D; rc 0 = pass
  rm -rf "$D"
  cmake -S "$WT" -B "$D" -G Ninja -DCMAKE_BUILD_TYPE=Release -DHVEN_FP_MODE=SAFER_FAST -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON \
    -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/home/ghecht/Projects/hven/build/_deps/googletest-src > "$1" 2>&1
}
verdict() { echo "$1" | tee -a "$V"; }
expect_fail() { # <id> <what> <regex>   (CMake wraps its message text across lines: the regex is matched on the whitespace-joined log)
  cfg "$MD/$1-planted.txt"; rc=$?
  if [ $rc -ne 0 ] && tr -s ' \n' ' ' < "$MD/$1-planted.txt" | grep -Eq "$3"; then verdict "$1 $2: planted -> configure FAILED with the expected message (rc=$rc) [KILLED]"; else verdict "$1 $2: planted -> rc=$rc, expected message /$3/ absent [SURVIVED]"; fi
}
expect_pass() { cfg "$MD/$1-restored.txt"; rc=$?; if [ $rc -eq 0 ]; then verdict "$1 restored -> configure passes (rc=0)"; else verdict "$1 restored -> rc=$rc [BAD RESTORE]"; fi; }
cfg "$MD/00-clean.txt"; verdict "clean tree: configure rc=$? (want 0)"
# (a) a test source that no executable builds
printf '#include <gtest/gtest.h>\nTEST(PlantedOrphan, Never) {}\n' > tests/core/test_planted_orphan.cpp
expect_fail M1-orphan "a tests/core source in no executable" 'is in no test executable'
rm -f tests/core/test_planted_orphan.cpp; expect_pass M1-orphan
# (b) a source in two executables (linear lists core's test_version.cpp too)
python3 - <<'PY'
p = 'tests/linear/CMakeLists.txt'; s = open(p).read()
o = 'hven_add_test_executable(hven_linear_tests\n    SOURCES ${_hven_linear_sources}'
assert o in s
open(p, 'w').write(s.replace(o, o + ' ../core/test_version.cpp', 1))
PY
expect_fail M2-duplicate "core/test_version.cpp also in hven_linear_tests" 'is built by two executables'
git checkout -q -- tests; expect_pass M2-duplicate
# (c) a seam executable linked to hven::hven
printf '\ntarget_link_libraries(hven_ipm_seam_tests PRIVATE hven::hven)\n' >> tests/ipm/CMakeLists.txt
expect_fail M3-seam-links-hven "hven_ipm_seam_tests linked to hven::hven" 'seam executable .HVEN_TESTING. and links hven::hven'
git checkout -q -- tests; expect_pass M3-seam-links-hven
# (c2) an ordinary executable that defines HVEN_TESTING
printf '\ntarget_compile_definitions(hven_ipm_tests PRIVATE HVEN_TESTING=1)\n' >> tests/ipm/CMakeLists.txt
expect_fail M3b-ordinary-defines-testing "hven_ipm_tests defines HVEN_TESTING" 'links hven::hven and defines HVEN_TESTING'
git checkout -q -- tests; expect_pass M3b-ordinary-defines-testing
# (d) a compile_fail probe path left pointing at the stale location (a half-done move: the loop in tests/model reads tests/ipm/compile_fail)
python3 - <<'PY'
p = 'tests/model/CMakeLists.txt'; s = open(p).read()
o = '        compile_fail/${_probe}.cpp)'
assert s.count(o) >= 1
open(p, 'w').write(s.replace(o, '        ../ipm/compile_fail/${_probe}.cpp)', 1))
PY
expect_fail M4-compile-fail-stale-path "compile_fail probe source read from the stale ../ipm/compile_fail" 'Cannot find source file|No SOURCES given|CMake Generate step failed'
git checkout -q -- tests; expect_pass M4-compile-fail-stale-path
# (e) Q9 / DECLARATION-RS6a-r6: a whole-library seam executable that compiles a library TU of its own instead of using the shared hven_testing_library
printf '\ntarget_sources(hven_ipm_seam_tests PRIVATE ${HVEN_SRC_DIR}/core/ledger.cpp)\n' >> tests/ipm/CMakeLists.txt
expect_fail M5a-whole-seam-own-library-tu "hven_ipm_seam_tests also compiles src/core/ledger.cpp itself" 'whole-library seam executable and compiles the library TU'
git checkout -q -- tests; expect_pass M5a-whole-seam-own-library-tu
# (e2) the same defect in its other form: the executable declares the whole library as its own LIBRARY_SOURCES (a private recompile of every hven source)
python3 - <<'PY'
p = 'tests/ipm/CMakeLists.txt'; s = open(p).read()
o = 'hven_add_seam_executable(hven_ipm_seam_tests\n    WHOLE_LIBRARY\n'
assert o in s
open(p, 'w').write(s.replace(o, 'get_target_property(_hven_all_lib hven SOURCES)\nhven_add_seam_executable(hven_ipm_seam_tests\n    LIBRARY_SOURCES ${_hven_all_lib}\n', 1))
PY
expect_fail M5b-seam-recompiles-whole-library "hven_ipm_seam_tests declared with every hven source as LIBRARY_SOURCES" 'narrow seam executable but recompiles'
git checkout -q -- tests; expect_pass M5b-seam-recompiles-whole-library
rm -rf "$D"
grep -c KILLED "$V" | xargs echo "killed:"; grep -c SURVIVED "$V" | xargs echo "survived:"
! grep -q 'SURVIVED\|BAD RESTORE' "$V"
