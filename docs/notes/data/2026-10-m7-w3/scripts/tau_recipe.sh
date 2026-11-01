#!/bin/bash
# tau_bar_sweep_probe's documented recipe (bench/tau_bar_sweep_probe.cpp:28-55), on the head's OWN compile and
# link lines (read from ninja, so every ABI/numerics flag is the build's): the probe TU and a copy-compiled
# src/globalization/sqp/funnel.cpp see a PATCHED globalization.h placed first on the include path; the
# head's libhven.a resolves everything else (funnel.cpp.o is never pulled: the explicit object defines it first).
# SELFTEST = the copy UNPATCHED: must print the in-tree probe's output byte for byte.
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/w3
S=$W/.scratch/w3; B=$S/build-head-rel; M=$S/meas; R=$S/tau-recipe
rm -rf "$R"; mkdir -p "$R"
cd "$B" || exit 90
CMDS="$(ninja -t commands bench/hven_sqp_tau_bar_sweep_probe)"
strip_dep() { sed -E 's# -MD##; s# -MT [^ ]+##; s# -MF [^ ]+##'; }
CP="$(grep -F 'tau_bar_sweep_probe.cpp' <<< "$CMDS" | grep -F ' -c ' | strip_dep)"
CF="$(grep -F 'src/globalization/sqp/funnel.cpp' <<< "$CMDS" | grep -F ' -c ' | strip_dep)"
CL="$(tail -1 <<< "$CMDS")"
[ -n "$CP" ] && [ -n "$CF" ] && grep -q 'libhven.a' <<< "$CL" || { echo "RECIPE: could not read compile/link lines"; exit 91; }
ANCHOR='inline constexpr double kFunnelTauBar = 100.0;'
for TAU in SELFTEST 1.0 10.0 1000.0 100000.0; do
    P=$R/$TAU/inc/hven/detail/globalization/sqp; mkdir -p "$P"
    cp "$W/include/hven/detail/globalization/sqp/globalization.h" "$P/"
    test "$(grep -c -F "$ANCHOR" "$P/globalization.h")" = 1 || { echo "anchor not unique"; exit 92; }
    [ "$TAU" != SELFTEST ] && sed -i "s/kFunnelTauBar = 100.0;/kFunnelTauBar = $TAU;/" "$P/globalization.h"
    grep -n "kFunnelTauBar =" "$P/globalization.h"
    OP=$R/$TAU/probe.o; OF=$R/$TAU/funnel.o; EXE=$R/$TAU/probe
    # first token is the compiler; inject the patched include dir right after it, retarget -o
    c1="$(sed -E "s#clang\+\+ #clang++ -I$R/$TAU/inc #; s# -o [^ ]+# -o $OP#" <<< "$CP")"
    c2="$(sed -E "s#clang\+\+ #clang++ -I$R/$TAU/inc #; s# -o [^ ]+# -o $OF#" <<< "$CF")"
    l1="$(sed -E "s#bench/CMakeFiles/hven_sqp_tau_bar_sweep_probe.dir/tau_bar_sweep_probe.cpp.o#$OP $OF#; s# -o [^ ]+# -o $EXE#" <<< "$CL")"
    eval "$c1" && eval "$c2" && eval "$l1" || { echo "RECIPE BUILD FAILED $TAU"; exit 93; }
    taskset -c 2 "$EXE" > "$M/tauprobe-recipe-$TAU.txt" || exit 94
    echo "== $TAU"; cat "$M/tauprobe-recipe-$TAU.txt"
done
if cmp -s "$M/tauprobe-recipe-SELFTEST.txt" "$M/tauprobe-intree-head.txt"; then echo "RECIPE_SELFTEST PASS (byte-identical to the in-tree probe)"; else echo "RECIPE_SELFTEST FAIL"; diff "$M/tauprobe-intree-head.txt" "$M/tauprobe-recipe-SELFTEST.txt"; fi
