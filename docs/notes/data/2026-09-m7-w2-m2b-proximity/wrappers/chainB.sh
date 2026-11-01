#!/usr/bin/env bash
# M7 W2 M2b chain B: THE CALIBRATION LEGS -- the screen arm (zeta = 0) and the
# four candidate arms, seven cells each, one leg per cell per candidate, every
# leg ALONE under the box lock.
#
# TERMS, declared here and repeated in the artifact: one cell per process,
# serial, MKL_NUM_THREADS=1, OMP_NUM_THREADS=1, pinned to one physical core,
# each ARM holding /tmp/box-build.lock for its own seven legs -- no timeout, no
# fallback -- and RELEASING between arms so the harness lane's own cells can
# interleave. Wall is INFORMATIONAL (CLAUDE.md section 7) and the 900 s per-leg
# cap is a SAFETY STOP, not a DNF condition: a leg that hits it is a finding,
# reported as CENSORED and never as a row.
#
# THE ZETA VALUES ARE PRE-REGISTERED, in RULE.md section 3, and are reproduced
# here verbatim rather than recomputed: h_k is M2's committed restoration_h_in
# and n is the cell's variable count, both from
# docs/notes/data/2026-09-m7-w2-m2-restoration/ladder/<CELL>.txt.
set -o pipefail
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m2b
LOG=$S/logs/chainB.log
mkdir -p "$S/logs" "$S/out/screen" "$S/out/c1" "$S/out/c2" "$S/out/c3" "$S/out/c4"
exec >"$LOG" 2>&1
echo "CHAINB_START $(date -Is)"

CELLS="STEENBRB STEENBRC STEENBRG ZAMB2-8 YORKNET BRITGAS READING6"
zeta_of() { # cell, member(0=screen,1..4)
  case "$2" in
    0) echo 0 ;;
    *) case "$1" in
         STEENBRB) set -- 0.04230769230769231 19.8 42.30769230769231 19800 ;;
         STEENBRC) set -- 0.07740740740740741 41.8 77.4074074074074 41800 ;;
         STEENBRG) set -- 0.07740740740740741 41.8 77.4074074074074 41800 ;;
         ZAMB2-8)  set -- 0.00017857694781702878 0.02464361879874997 0.1785769478170288 24.643618798749973 ;;
         YORKNET)  set -- 0.15542110239428375 48.49138394701652 155.42110239428374 48491.383947016526 ;;
         BRITGAS)  set -- 0.0002556444444444444 0.11503999999999996 0.25564444444444434 115.03999999999996 ;;
         READING6) set -- 0 0 0 0 ;;
       esac
       eval echo "\${$MEMBER}" ;;
  esac
}

# --- segment 0: install, probe build, smoke ------------------------------
flock /tmp/box-build.lock bash -c '
set -o pipefail
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m2b
echo "FOREGROUND_START chainB-seg0 $(date -Is)"
cd "$ROOT" || exit 90
echo "=== HEAD $(git rev-parse --short HEAD)  dirty-files=$(git status --porcelain | wc -l) ==="
[ -z "$(git status --porcelain)" ] || { echo "REFUSED: the tree is not clean -- a stamped leg runs on a committed tree"; exit 2; }
rm -rf "$S/rel" "$S/install" "$S/probebuild"
cmake -S "$ROOT" -B "$S/rel" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_INSTALL_PREFIX="$S/install" \
  -DHVEN_BUILD_TESTS=OFF -DHVEN_BUILD_BENCH=OFF > "$S/logs/install-configure.log" 2>&1 || exit 91
cmake --build "$S/rel" -j "$(nproc)" > "$S/logs/install-build.log" 2>&1 || { tail -30 "$S/logs/install-build.log"; exit 91; }
cmake --install "$S/rel" > "$S/logs/install.log" 2>&1 || exit 91
echo "--- LIBRARY SHA256 (Fedora installs to lib64)"
sha256sum "$S/install/lib64/libhven.a"
grep -n "restoration_proximity" "$S/install/include/hven/drivers/sqp_solver_types.h" | head -3
# A FRESH probe configure every time: `hven_DIR` is a cache variable that
# find_package sets on first success and then reuses.
cmake -S "$S/probe" -B "$S/probebuild" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_PREFIX_PATH="$S/install" \
  > "$S/logs/probe-configure.log" 2>&1 || { tail -20 "$S/logs/probe-configure.log"; exit 92; }
echo "--- THE hven PACKAGE THIS PROBE RESOLVED"
grep -m1 "^hven_DIR:" "$S/probebuild/CMakeCache.txt"
cmake --build "$S/probebuild" --target m2b_probe -j "$(nproc)" > "$S/logs/probe-build.log" 2>&1 \
  || { grep -E "error:|FAILED" "$S/logs/probe-build.log" | head -20; exit 92; }
echo "--- PROBE SHA256"
sha256sum "$S/probebuild/m2b_probe"
echo "--- SMOKE: ZAMB2-8 at zeta = 0 and at candidate 2"
cd /home/ghecht/Projects/hven-bench || exit 93
for z in 0 0.02464361879874997; do
  MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 timeout 600 \
    "$S/probebuild/m2b_probe" --problem ZAMB2-8 --proximity "$z" > "$S/logs/smoke-$z.txt" 2>&1
  echo "    zeta=$z rc=$?"
  grep -E "^(problem|proximity|status|f|restoration_entries|restoration_h_in|restoration_h_out|infeasibility_certified)=" "$S/logs/smoke-$z.txt" | sed "s/^/      /"
done
echo "FOREGROUND_END chainB-seg0 $(date -Is) rc=0"
'
rc=$?
echo "WRAPPER_RC chainB-seg0 rc=${rc}"
[ $rc -eq 0 ] || { echo "CHAINB_ABORTED_AT_SEG0"; echo "CHAINB_END $(date -Is)"; exit $rc; }

# --- segments 1..5: one ARM per lock hold --------------------------------
worst=0
for MEMBER in 0 1 2 3 4; do
  dir=screen; [ "$MEMBER" != 0 ] && dir="c$MEMBER"
  # The zeta table is resolved OUTSIDE the lock (pure string work) and handed in.
  args=""
  for cell in $CELLS; do args="$args $cell:$(zeta_of "$cell" "$MEMBER")"; done
  echo "=== ARM $dir: $args"
  flock /tmp/box-build.lock bash -c '
set -o pipefail
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m2b
BIN=$S/probebuild/m2b_probe
dir="$1"; shift
echo "FOREGROUND_START chainB-$dir $(date -Is)"
cd /home/ghecht/Projects/hven-bench || exit 90
echo "probe sha256: $(sha256sum "$BIN")"
echo "hven library sha256: $(sha256sum "$S/install/lib64/libhven.a")"
w=0
for spec in "$@"; do
  cell=${spec%%:*}; z=${spec#*:}
  foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
  if [ -n "$foreign" ]; then
    echo "ABORTED before $cell: a foreign cell binary is alive while this run holds the lock: $foreign"
    exit 95
  fi
  t0=$(date +%s.%N)
  MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 timeout 900 \
    "$BIN" --problem "$cell" --proximity "$z" > "$S/out/$dir/$cell.txt" 2>"$S/out/$dir/$cell.stderr"
  rc=$?
  t1=$(date +%s.%N)
  printf "LEG %-10s %-24s rc=%-3s wall_s=%.1f (INFORMATIONAL)\n" "$cell" "zeta=$z" "$rc" "$(echo "$t1 - $t0" | bc)"
  if [ "$rc" = 124 ]; then
    echo "    CENSORED: the 900 s safety stop fired -- a finding, not a row"; w=1
  elif [ "$rc" != 0 ]; then
    echo "    LEG FAILED rc=$rc"; head -5 "$S/out/$dir/$cell.stderr"; w=1
  else
    grep -E "^(proximity|status|restoration_entries|restoration_h_in|restoration_h_out|restoration_f_out|infeasibility_certified|restoration_iters)=" "$S/out/$dir/$cell.txt" | sed "s/^/    /"
  fi
done
echo "FOREGROUND_END chainB-$dir $(date -Is) rc=$w"
exit $w
' _ "$dir" $args
  rc=$?
  echo "WRAPPER_RC chainB-$dir rc=${rc}"
  [ $rc -eq 0 ] || worst=1
done

echo "LEGS_WORST=$worst"
echo "--- SUMMARISE (outside the lock: no solver runs, pure file folding)"
python3 -B "$S/artifact/summarise_m2b.py"
echo "SUMMARISE_RC=$?"
echo "CHAINB_END $(date -Is)"
exit $worst
