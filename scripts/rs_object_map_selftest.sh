#!/usr/bin/env bash
# Self-test of scripts/rs_object_map.py on synthetic manifests: it proves the
# generator discriminates (pairs what it should, refuses every gap) before any
# build is trusted to it.  No build tree is needed.
#   scripts/rs_object_map_selftest.sh
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GEN="${HERE}/rs_object_map.py"
T="$(mktemp -d "${TMPDIR:-/tmp}/rs-objmap-selftest.XXXXXX")"
trap 'rm -rf "${T}"' EXIT
fail=0
check() { # name, condition-exit-code
    if [ "$2" -eq 0 ]; then echo "PASS  $1"; else echo "FAIL  $1"; fail=1; fi
}

cat > "${T}/before.txt" <<'EOM'
CMakeFiles/hven.dir/src/core/version.cpp.o
CMakeFiles/hven.dir/src/drivers/ipm_solver.cpp.o
CMakeFiles/hven.dir/src/drivers/solve_status.cpp.o
CMakeFiles/hven.dir/src/interior/utils/color_text.cpp.o
CMakeFiles/hven.dir/src/kkt/kkt_calls.cpp.o
tests/interior/CMakeFiles/hven_interior_tests.dir/test_soc.cpp.o
tests/interior/CMakeFiles/hven_interior_tests.dir/Unity/unity_0_cxx.cxx.o
tests/CMakeFiles/hven_fault_injection_tests.dir/__/src/kkt/kkt_calls.cpp.o
libhven.a
EOM
cat > "${T}/map.txt" <<'EOM'
# comment
src/drivers/ipm_solver.cpp -> src/ipm/ipm_solver.cpp
src/drivers/solve_status.cpp -> src/ipm/ipm_phase_status.cpp
src/interior/utils/color_text.cpp -> DELETE (owner)
src/kkt/kkt_calls.cpp -> src/kkt/kkt_factor.cpp
tests/interior/test_soc.cpp -> tests/ipm/test_soc.cpp
EOM
# after: the sources moved; the interior test target was renamed hven_ipm_tests
# and its directory moved; the unity object keeps its path.
cat > "${T}/after.txt" <<'EOM'
CMakeFiles/hven.dir/src/core/version.cpp.o
CMakeFiles/hven.dir/src/ipm/ipm_solver.cpp.o
CMakeFiles/hven.dir/src/ipm/ipm_phase_status.cpp.o
CMakeFiles/hven.dir/src/kkt/kkt_factor.cpp.o
tests/ipm/CMakeFiles/hven_ipm_tests.dir/test_soc.cpp.o
tests/ipm/CMakeFiles/hven_ipm_tests.dir/Unity/unity_0_cxx.cxx.o
tests/CMakeFiles/hven_fault_injection_tests.dir/__/src/kkt/kkt_factor.cpp.o
libhven.a
EOM
# The unity object is renamed together with its target (path differs), so this
# first run must REFUSE it -- that is the unity rule working.
python3 "${GEN}" --move-map "${T}/map.txt" --before "${T}/before.txt" --after "${T}/after.txt" \
    --target-map hven_interior_tests=hven_ipm_tests -o "${T}/out1.txt" 2> "${T}/err1.txt"
rc=$?
[ "${rc}" -eq 1 ] && grep -q '^ERROR UNITY-UNMAPPED tests/interior/CMakeFiles/hven_interior_tests.dir/Unity/unity_0_cxx.cxx.o' "${T}/err1.txt"
check "a unity object whose path changed is refused" $?


# --unity-dir-move (RS-2): a unity object may differ by exactly a declared directory prefix; nothing looser.
printf 'tests/interior/CMakeFiles/t.dir/Unity/unity_0_cxx.cxx.o\ntests/interior/CMakeFiles/t.dir/a.cpp.o\n' > "${T}/ub.txt"
printf 'tests/ipm/CMakeFiles/t.dir/Unity/unity_0_cxx.cxx.o\ntests/ipm/CMakeFiles/t.dir/a.cpp.o\n' > "${T}/ua.txt"
printf 'tests/interior/a.cpp -> tests/ipm/a.cpp\n' > "${T}/um.txt"
python3 "${GEN}" --move-map "${T}/um.txt" --before "${T}/ub.txt" --after "${T}/ua.txt" -o /dev/null 2> "${T}/erru1.txt"
rc=$?
[ "${rc}" -eq 1 ] && grep -q '^ERROR UNITY-UNMAPPED tests/interior/CMakeFiles/t.dir/Unity/unity_0_cxx.cxx.o' "${T}/erru1.txt"
check "a unity object that moved directory is refused without the declaration" $?
python3 "${GEN}" --move-map "${T}/um.txt" --before "${T}/ub.txt" --after "${T}/ua.txt" --unity-dir-move tests/interior=tests/ipm -o "${T}/outu.txt" 2> "${T}/erru2.txt"
rc=$?
[ "${rc}" -eq 0 ] && grep -qxF 'tests/interior/CMakeFiles/t.dir/Unity/unity_0_cxx.cxx.o => tests/ipm/CMakeFiles/t.dir/Unity/unity_0_cxx.cxx.o' "${T}/outu.txt"
check "the declared unity directory move resolves and is written to the map" $?
python3 "${GEN}" --move-map "${T}/um.txt" --before "${T}/ub.txt" --after "${T}/ua.txt" --unity-dir-move tests/other=tests/ipm -o /dev/null 2> "${T}/erru3.txt"
rc=$?
[ "${rc}" -eq 1 ] && grep -q '^ERROR UNITY-UNMAPPED' "${T}/erru3.txt"
check "a different declared directory move does not license it" $?

# Same run with the unity object path unchanged in both manifests.
sed -i 's#tests/interior/CMakeFiles/hven_interior_tests.dir/Unity#tests/ipm/CMakeFiles/hven_ipm_tests.dir/Unity#' "${T}/before.txt"
python3 "${GEN}" --move-map "${T}/map.txt" --before "${T}/before.txt" --after "${T}/after.txt" \
    --target-map hven_interior_tests=hven_ipm_tests -o "${T}/out2.txt" 2> "${T}/err2.txt"
check "synthetic restructure resolves with exit 0" $?
cat > "${T}/expect2.txt" <<'EOM'
CMakeFiles/hven.dir/src/drivers/ipm_solver.cpp.o => CMakeFiles/hven.dir/src/ipm/ipm_solver.cpp.o
CMakeFiles/hven.dir/src/drivers/solve_status.cpp.o => CMakeFiles/hven.dir/src/ipm/ipm_phase_status.cpp.o
CMakeFiles/hven.dir/src/kkt/kkt_calls.cpp.o => CMakeFiles/hven.dir/src/kkt/kkt_factor.cpp.o
tests/CMakeFiles/hven_fault_injection_tests.dir/__/src/kkt/kkt_calls.cpp.o => tests/CMakeFiles/hven_fault_injection_tests.dir/__/src/kkt/kkt_factor.cpp.o
tests/interior/CMakeFiles/hven_interior_tests.dir/test_soc.cpp.o => tests/ipm/CMakeFiles/hven_ipm_tests.dir/test_soc.cpp.o
EOM
diff -u "${T}/expect2.txt" "${T}/out2.txt" > "${T}/diff2.txt"
check "map lines are exactly the expected five" $?
grep -q '^DELETED CMakeFiles/hven.dir/src/interior/utils/color_text.cpp.o' "${T}/err2.txt"
check "a deleted source is reported, not dropped" $?

# identity: a manifest against itself yields an empty map
python3 "${GEN}" --move-map /dev/null --before "${T}/after.txt" --after "${T}/after.txt" -o "${T}/id.txt" 2> "${T}/errid.txt"
rc=$?
[ "${rc}" -eq 0 ] && [ ! -s "${T}/id.txt" ]
check "a manifest compared with itself gives an empty map" $?

# --phase: only the listed entries move; the rest are treated as unmoved, so the
# other moved objects then show up as MISSING/NEW (the phase did not cover them).
python3 "${GEN}" --move-map "${T}/map.txt" --phase src/kkt/ --before "${T}/before.txt" --after "${T}/after.txt" \
    --target-map hven_interior_tests=hven_ipm_tests -o /dev/null 2> "${T}/errp.txt"
rc=$?
[ "${rc}" -eq 1 ] && grep -q '^ERROR MISSING CMakeFiles/hven.dir/src/drivers/ipm_solver.cpp.o' "${T}/errp.txt"
check "a phase leaves other moves unmapped and the gap is an error" $?

# negatives
grep -v 'kkt_factor.cpp' "${T}/after.txt" | grep -v '^libhven' > "${T}/after-missing.txt"
python3 "${GEN}" --move-map "${T}/map.txt" --before "${T}/before.txt" --after "${T}/after-missing.txt" \
    --target-map hven_interior_tests=hven_ipm_tests -o /dev/null 2> "${T}/errm.txt"
rc=$?
[ "${rc}" -eq 1 ] && grep -q '^ERROR MISSING CMakeFiles/hven.dir/src/kkt/kkt_calls.cpp.o' "${T}/errm.txt"
check "a vanished object is an error" $?

{ cat "${T}/after.txt"; echo CMakeFiles/hven.dir/src/ipm/brand_new.cpp.o; } > "${T}/after-new.txt"
python3 "${GEN}" --move-map "${T}/map.txt" --before "${T}/before.txt" --after "${T}/after-new.txt" \
    --target-map hven_interior_tests=hven_ipm_tests -o /dev/null 2> "${T}/errn.txt"
rc=$?
[ "${rc}" -eq 1 ] && grep -q '^ERROR NEW CMakeFiles/hven.dir/src/ipm/brand_new.cpp.o' "${T}/errn.txt"
check "an unclaimed new object is an error" $?

printf 'src/drivers/ipm_solver.cpp -> src/ipm/ipm_solver.cpp\nsrc/drivers/solve_status.cpp -> src/ipm/ipm_solver.cpp\n' > "${T}/map-m2o.txt"
head -3 "${T}/before.txt" > "${T}/b3.txt"
printf 'CMakeFiles/hven.dir/src/core/version.cpp.o\nCMakeFiles/hven.dir/src/ipm/ipm_solver.cpp.o\n' > "${T}/a3.txt"
python3 "${GEN}" --move-map "${T}/map-m2o.txt" --before "${T}/b3.txt" --after "${T}/a3.txt" -o /dev/null 2> "${T}/errx.txt"
rc=$?
[ "${rc}" -eq 1 ] && grep -q '^ERROR MANY-TO-ONE' "${T}/errx.txt"
check "two objects mapped onto one is an error" $?

if [ "${fail}" -ne 0 ]; then echo "rs_object_map_selftest: FAILED"; exit 1; fi
echo "rs_object_map_selftest: ALL PASS"
