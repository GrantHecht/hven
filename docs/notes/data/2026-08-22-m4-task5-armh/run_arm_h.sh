#!/bin/bash
# =============================================================================
# ARM-H (the "attribution reversal mechanism" discriminator, header-only).
#
# WHAT THIS ISOLATES. Task 5's Level 2 consumption switch (base 07d5ee1 ->
# landed HEAD) grew src/model/candidate_point.h (three new EvalRequest shape
# constants + extended is_legal_request + mapping-table text) AND changed
# src/model/non_linear_program.cpp's assemble_impl dispatch tail from a bare
# `} else { <full-KKT body> }` to
# `} else if (request == kRequestFullKkt) { <same body> } else { <refuse
# throw> }` (landed at 0451f35). The standing IPM wall leg found the n=240
# IPM cells OUT OF BAND (docs/notes/data/2026-08-21-m4-task5-wall/README.md)
# and named the mechanism as code LAYOUT from the header's growth (an
# inlining input on two already-timed-path objects, not the new work itself,
# which is a handful of nanoseconds of integer compares against a multi-ms
# cell). ARM-H is the discriminator that tests that attribution directly:
# apply arm_h.patch, which reverts ONLY the dispatch tail to its pre-task
# bare-else form, on top of the LANDED header. If the header alone
# reproduces the band trip, the mechanism is confirmed; if reverting the
# dispatch code (with the header left exactly as landed) brings the number
# back in-band, the dispatch edit -- not the header -- was responsible.
#
# FOUR ARMS:
#   BASE    07d5ee1  (pre-task: neither the header nor the dispatch changed)
#   ARM-H   8503f39 + arm_h.patch (header exactly as landed; dispatch reverted)
#   LANDED  8503f39, unmodified (both landed as-is) -- the adjudicated state
#   FIXED   main-tree HEAD, unmodified: the post-fix-batch state, which is what
#           actually ships. The fix batch rewrote the dispatch tail's refuse
#           message (it now enumerates the eight supported shapes inline) and
#           added a per-entry coordinate check to the bridge's scatter, so the
#           question LANDED answered has to be re-asked of the state that
#           replaces it: the leg reports FIXED against BASE on the same
#           occasion and by the same estimator as the other three.
#
# WHY LANDED AND ARM-H ARE NOW PINNED WORKTREES. Both were built from the main
# tree when the tree still sat at the landed commit. It no longer does, and
# arm_h.patch reverts the exact text the fix batch rewrote -- it does not apply
# at the new HEAD and must not be made to. LANDED is 8503f39 by commit, ARM-H
# is 8503f39 plus the patch, and the main tree supplies the FIXED arm alone.
#
# PROTOCOL, mirroring docs/notes/data/m4-ipm-wall-leg/ipm_wall_leg.sh and
# docs/notes/data/2026-08-21-m4-task5-wall/wall_leg.sh conventions:
#   - serial   MKL_NUM_THREADS=1, pinned to one physical core (taskset -c 2).
#   - threaded default thread count, unpinned, machine otherwise idle.
#   - sides alternated per rep (base/armh/landed/fixed, in that order) each
#     rep, so machine drift lands on all four sides equally.
#   - both estimators (median-of-per-rep-medians, minimum-of-per-rep-minimums)
#     are reported. THE MINIMUM ESTIMATOR GOVERNS every classification
#     comparison -- each delta is read off it; the median is reported beside
#     it as the second estimator and never drives a verdict (the standing
#     leg's own convention: the median's rep-to-rep noise at these problem
#     sizes is comparable to the effects being looked for).
#   - THE SPREAD TERM IS MAX-OF-REP-SPREADS, PER ARM. Each arm's spread is
#     the max over its own reps' spreads, reported per arm; the within-spread
#     test for a delta uses the max of the two arms it compares. This
#     replaces the earlier base-arm-only rep-spread, which described one
#     arm's stability and was then applied to comparisons involving three
#     others.
#   - THREE repetitions (three independent invocations, three log files),
#     pre-declared for this session: the sticky-vs-scattered read lives at
#     the 1-2% scale, where a two-rep spread is the weakest term in the
#     comparison. See the SDD ledger's pre-declaration.
#   - quiet-machine pre-check (quiet_check.sh, the task5-wall convention)
#     recorded before the build and before each repetition.
#
# ADJUSTED SCOPE vs the standing leg: the standing probe (ipm_time.cpp) times
# n=60/120/240 every invocation; ARM-H's adjudicated cell count is EIGHT
# (4 arms x 2 modes) at n=240, the only size that cleared the standing
# precedent. The n=60/120 rows still print (the probe is reused verbatim,
# unmodified, per the "match its conventions" brief) and are a free
# consistency check, but are not part of the six adjudicated cells.
#
# SAFETY. BASE, ARM-H and LANDED are built in throwaway `git worktree`
# checkouts under .scratch/task5-armh/ (the same shape the standing legs use
# for their base arm) -- the patch never touches the main tree. FIXED builds
# directly from the main tree (HEAD, unmodified), the same shape the standing
# legs use for their head arm, since the main tree already sits at the wanted
# commit. Nothing here pushes, commits, or mutates the main tree's working
# state.
#
# THIS SCRIPT DOES NOT RUN ITSELF. It refuses to start while a file named
# HOLD sits beside it -- the controller removes HOLD at the measurement
# window's end mark. It is prepared, not executed, by the agent that wrote
# it (ABSOLUTE CONSTRAINT: no build/cmake/ninja/test/benchmark from that
# agent's own hands).
#
# USAGE: ./run_arm_h.sh [REPS] [INNER]   (defaults 9 and 15, matching the
#                                          standing IPM leg's own defaults)
# =============================================================================
set -u
set -o pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"

if [ -e "$HERE/HOLD" ]; then
    echo "REFUSING TO START: $HERE/HOLD is present." >&2
    echo "The controller removes HOLD at the measurement window's end mark; this" >&2
    echo "script does not run while it is there." >&2
    exit 1
fi

REPO=/home/ghecht/Projects/hven
ROOT=$REPO/.scratch/task5-armh
BASE_COMMIT=07d5ee1
LANDED_COMMIT=8503f39
PATCH="$HERE/arm_h.patch"
IPM_SRC_CANON=$REPO/docs/notes/data/m4-ipm-wall-leg/ipm_time.cpp
REPS=${1:-9}
INNER=${2:-15}
JOBS=$(nproc)

mkdir -p "$HERE/runs" "$ROOT/ab"

echo "=== quiet check before build"
"$HERE/quiet_check.sh" | tee "$HERE/pgrep-before-build.txt"

setup_worktree() { # $1=path $2=commitish
    local path=$1 ref=$2
    if [ -d "$path" ]; then
        git -C "$REPO" worktree remove --force "$path" 2>/dev/null
        rm -rf "$path"
    fi
    git -C "$REPO" worktree prune
    git -C "$REPO" worktree add --detach "$path" "$ref"
    # Worktrees do not populate submodules; the vendored dep/ pins are
    # verified identical across all four arm commits (git diff --stat
    # 07d5ee1..HEAD -- dep/ is empty), so the main tree's checkouts are
    # content-exact for every arm and are shared by symlink.
    local sub
    for sub in eigen fmt; do
        rmdir "$path/dep/$sub" 2>/dev/null || rm -rf "$path/dep/$sub"
        ln -s "$REPO/dep/$sub" "$path/dep/$sub"
    done
}

echo "=== preparing BASE worktree ($BASE_COMMIT)"
setup_worktree "$ROOT/basesrc" "$BASE_COMMIT"

echo "=== preparing LANDED worktree ($LANDED_COMMIT)"
setup_worktree "$ROOT/landedsrc" "$LANDED_COMMIT"

echo "=== preparing ARM-H worktree ($LANDED_COMMIT + arm_h.patch)"
setup_worktree "$ROOT/armhsrc" "$LANDED_COMMIT"
if ! git -C "$ROOT/armhsrc" apply --check "$PATCH"; then
    echo "FATAL: arm_h.patch does not apply cleanly to the ARM-H worktree" >&2
    echo "($LANDED_COMMIT). The patch reverts the landed dispatch tail verbatim and is" >&2
    echo "pinned to that commit; it is NOT expected to apply at the current HEAD." >&2
    exit 1
fi
git -C "$ROOT/armhsrc" apply "$PATCH"

HEAD_SHA=$(git -C "$REPO" rev-parse HEAD)
echo "=== FIXED arm builds the main tree at HEAD $HEAD_SHA (unmodified)"
if [ -n "$(git -C "$REPO" status --porcelain -- include src bench tests)" ]; then
    echo "FATAL: the main tree has uncommitted changes under include/src/bench/tests." >&2
    echo "The FIXED arm must measure a commit, not a working state." >&2
    exit 1
fi

build_one() { # $1=src $2=build $3=label
    local src=$1 bld=$2 label=$3
    echo "=== configuring $label"
    cmake -S "$src" -B "$bld" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang \
        -DHVEN_BUILD_TESTS=OFF
    echo "=== building $label"
    cmake --build "$bld" --target hven -- "-j${JOBS}"
}

build_one "$ROOT/basesrc"   "$ROOT/basebuild"   "BASE ($BASE_COMMIT)"
build_one "$ROOT/armhsrc"   "$ROOT/armhbuild"   "ARM-H ($LANDED_COMMIT + arm_h.patch)"
build_one "$ROOT/landedsrc" "$ROOT/landedbuild" "LANDED ($LANDED_COMMIT, unpatched)"
build_one "$REPO"           "$ROOT/fixedbuild"  "FIXED (HEAD $HEAD_SHA, main tree, unpatched)"

echo "=== building the four IPM probes (standing instrument, unmodified)"
cp -f "$IPM_SRC_CANON" "$ROOT/ab/ipm_time.cpp"
build_ipm() { # $1=srcroot $2=blddir(with libhven.a) $3=outbin
    /usr/bin/clang++ -DEIGEN_DONT_PARALLELIZE -DEIGEN_INITIALIZE_MATRICES_BY_ZERO \
        -DEIGEN_MAX_ALIGN_BYTES=32 -DFMT_HEADER_ONLY -DFMT_USE_LOCALE=0 \
        -DHVEN_DEFAULT_QP_THREADS=8 \
        -I"$1/include" -I/opt/intel/oneapi/mkl/latest/include \
        -isystem "$1/dep/eigen" -isystem "$1/dep/fmt/include" \
        -DMKL_LP64 -m64 -O3 -DNDEBUG -std=c++20 -pthread -march=native -mtune=native \
        -ffast-math -fno-finite-math-only -fopenmp=libiomp5 \
        -L/opt/intel/oneapi/compiler/latest/lib \
        "$ROOT/ab/ipm_time.cpp" -o "$3" \
        -Wl,-rpath,/opt/intel/oneapi/mkl/latest/lib:/opt/intel/oneapi/compiler/latest/lib \
        "$2/libhven.a" -Wl,--start-group \
        /opt/intel/oneapi/mkl/latest/lib/libmkl_intel_lp64.a \
        /opt/intel/oneapi/mkl/latest/lib/libmkl_intel_thread.a \
        /opt/intel/oneapi/mkl/latest/lib/libmkl_core.a \
        /opt/intel/oneapi/compiler/latest/lib/libiomp5.so -Wl,--end-group -ldl -lm
}
build_ipm "$ROOT/basesrc"   "$ROOT/basebuild"   "$ROOT/ab/ipm_base"
build_ipm "$ROOT/armhsrc"   "$ROOT/armhbuild"   "$ROOT/ab/ipm_armh"
build_ipm "$ROOT/landedsrc" "$ROOT/landedbuild" "$ROOT/ab/ipm_landed"
build_ipm "$REPO"           "$ROOT/fixedbuild"  "$ROOT/ab/ipm_fixed"

run_leg() { # $1 = output log path
    local out=$1
    : > "$out"
    for mode in serial threaded; do
        echo "=== arm: $mode" >> "$out"
        for r in $(seq 1 "$REPS"); do
            for side in base armh landed fixed; do
                bin="$ROOT/ab/ipm_$side"
                if [ "$mode" = serial ]; then
                    o=$(MKL_NUM_THREADS=1 taskset -c 2 "$bin" "$INNER")
                else
                    o=$("$bin" "$INNER")
                fi
                echo "$o" | sed "s/^/rep$r $side /" >> "$out"
            done
        done
    done
}

# Three repetitions, each its own invocation, its own log file and its own
# quiet check. Pre-declared for this session; see the header's protocol block.
for rep in 1 2 3; do
    echo "=== quiet check before repetition $rep"
    "$HERE/quiet_check.sh" | tee "$HERE/pgrep-before-armh-$rep.txt"
    run_leg "$HERE/runs/armh_wall_$rep.log"
done

for rep in 1 2 3; do
    python3 "$HERE/aggregate_armh.py" "$HERE/runs/armh_wall_$rep.log" |
        tee "$HERE/armh_wall_${rep}_aggregate.txt"
done

echo "=== ARM-H wall leg complete."
echo "=== Run bit_identity_check.sh next, then fill README.md's four-way read"
echo "=== table and calibration output from these three aggregate files. The"
echo "=== accept-vs-mitigate decision tree is SUPERSEDED: this session is a"
echo "=== noise-floor calibration (docs/notes/2026-08-m4-ledger.md, 2026-08-22)."
