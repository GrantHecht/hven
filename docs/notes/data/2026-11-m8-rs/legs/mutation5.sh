#!/usr/bin/env bash
# usage: WT=<worktree> flock /tmp/box-build.lock setsid -w bash mutation5.sh     (needs the Release build in $WT/build and a smoke install tree in $WT/.scratch/smoke-default)
# RS-5 mutation proofs (DECLARATION-RS5.md item 5): plant a defect, the matching check must FAIL; restore it, the check must PASS.
# Every plant is restored by `git checkout`/rm even on error. Output: gates/RS-5/mutation/*.txt and MUTATION-VERDICTS.txt.
set -o pipefail
source "$(dirname "$0")/env.sh"
box_check
export GIT_DIR=$(git -C "$WT" rev-parse --absolute-git-dir) GIT_WORK_TREE=$WT
E=$WT/docs/notes/data/2026-11-m8-rs; MD=$E/gates/RS-5/mutation; mkdir -p "$MD"; V=$MD/MUTATION-VERDICTS.txt; : > "$V"
cd "$WT" || exit 1
[ -z "$(git status --porcelain --untracked-files=no)" ] || { echo "tracked tree not clean"; exit 2; }
T=build/tests/hven_tests
restore() { git checkout -q -- include src 2>/dev/null; rm -f include/hven/stray.h include/hven/detail/util/planted_not_selfcontained.h; }
trap restore EXIT
lay() { "$T" --gtest_filter='Layering.*:CoreLayering.*' 2>&1; }
verdict() { echo "$1" | tee -a "$V"; }

# --- baseline: all checks pass on the clean tree
lay > "$MD/00-layering-clean.txt"; rc=$?; verdict "clean tree: layering gtests rc=$rc (want 0)"
SMK=$(find .scratch/smoke-default -name hvenTargets.cmake -path '*hven-install*' | head -1); PFX=$(dirname "$(dirname "$(dirname "$(dirname "$SMK")")")")
ALIGN=$(grep -oE 'resolved to [0-9]+' .scratch/smoke-default/hven-configure.log | head -1 | grep -oE '[0-9]+')
bash scripts/check_export_contract.sh "$SMK" linux "$ALIGN" > "$MD/00-export-clean.txt" 2>&1; rc=$?; verdict "clean install tree: export contract rc=$rc (want 0)"

# --- 1. planted layering violations (three kinds)
plant() { # <file> <include line>
  printf '\n#include <%s>\n' "$2" >> "$1"; }
for spec in "L1-core-up|include/hven/core/types.h|hven/trace/trace.h" \
            "L2-mid-up|include/hven/detail/barrier/barrier_math.h|hven/detail/kkt/kkt_assembly.h" \
            "L3-sibling|src/ipm/ipm_options.cpp|hven/sqp/sqp_options.h"; do
  IFS='|' read -r tag file inc <<< "$spec"
  plant "$file" "$inc"
  lay > "$MD/$tag-planted.txt"; rc=$?; verdict "$tag planted ($file -> $inc): layering gtests rc=$rc (want nonzero)"; grep -E "^\s+(include|src)/.*#include" "$MD/$tag-planted.txt" | head -3 | tee -a "$V"
  git checkout -q -- "$file"
  lay > "$MD/$tag-restored.txt"; rc=$?; verdict "$tag restored: layering gtests rc=$rc (want 0)"
done

# --- 2. planted root header: caught by the export contract on an install tree carrying it, and by the layering scan on the source tree
cp -r "$PFX" "$WT/.scratch/mut-prefix"; touch "$WT/.scratch/mut-prefix/include/hven/stray.h"
bash scripts/check_export_contract.sh "$WT/.scratch/mut-prefix/lib/cmake/hven/hvenTargets.cmake" linux "$ALIGN" > "$MD/R1-export-planted.txt" 2>&1; rc=$?; verdict "R1 planted root header (install tree): export contract rc=$rc (want nonzero)"; tail -2 "$MD/R1-export-planted.txt" | tee -a "$V"
rm -f "$WT/.scratch/mut-prefix/include/hven/stray.h"; mkdir "$WT/.scratch/mut-prefix/include/hven/not_a_component"
bash scripts/check_export_contract.sh "$WT/.scratch/mut-prefix/lib/cmake/hven/hvenTargets.cmake" linux "$ALIGN" > "$MD/R2-export-planted-dir.txt" 2>&1; rc=$?; verdict "R2 planted unknown directory (install tree): export contract rc=$rc (want nonzero)"; tail -2 "$MD/R2-export-planted-dir.txt" | tee -a "$V"
rmdir "$WT/.scratch/mut-prefix/include/hven/not_a_component"
bash scripts/check_export_contract.sh "$WT/.scratch/mut-prefix/lib/cmake/hven/hvenTargets.cmake" linux "$ALIGN" > "$MD/R3-export-restored.txt" 2>&1; rc=$?; verdict "R3 restored: export contract rc=$rc (want 0)"
rm -rf "$WT/.scratch/mut-prefix"
touch include/hven/stray.h
lay > "$MD/R4-layering-planted-root.txt"; rc=$?; verdict "R4 planted root header (source tree): layering gtests rc=$rc (want nonzero)"; grep -E "stray.h" "$MD/R4-layering-planted-root.txt" | head -2 | tee -a "$V"
rm -f include/hven/stray.h
lay > "$MD/R5-layering-restored.txt"; rc=$?; verdict "R5 restored: layering gtests rc=$rc (want 0)"

# --- 3. planted non-self-contained headers: an existing header that loses its include, and a new header
HS() { cmake --build build --target hven_header_selfcheck > "$1" 2>&1; }
ctest_hs() { ( cd build && ctest -R '^HeaderSelfContained$' --output-on-failure ) > "$1" 2>&1; }
ctest_hs "$MD/S0-selfcontained-clean.txt"; rc=$?; verdict "S0 clean tree: HeaderSelfContained rc=$rc (want 0)"
sed -i '/#include <hven\/core\/types.h>/d' include/hven/detail/sqp/nlp_eval.h
ctest_hs "$MD/S1-planted-edit.txt"; rc=$?; verdict "S1 planted (nlp_eval.h loses its core/types.h include): HeaderSelfContained rc=$rc (want nonzero)"; grep -E "error:" "$MD/S1-planted-edit.txt" | head -2 | tee -a "$V"
git checkout -q -- include/hven/detail/sqp/nlp_eval.h
ctest_hs "$MD/S2-restored-edit.txt"; rc=$?; verdict "S2 restored: HeaderSelfContained rc=$rc (want 0)"
printf '#pragma once\nnamespace hven { inline Vec planted_uses_vec_without_including_it() { return Vec(); } }\n' > include/hven/detail/util/planted_not_selfcontained.h
ctest_hs "$MD/S3-planted-new.txt"; rc=$?; verdict "S3 planted NEW header (uses Vec, includes nothing): HeaderSelfContained rc=$rc (want nonzero)"; grep -E "error:" "$MD/S3-planted-new.txt" | head -2 | tee -a "$V"
rm -f include/hven/detail/util/planted_not_selfcontained.h
ctest_hs "$MD/S4-restored-new.txt"; rc=$?; verdict "S4 restored: HeaderSelfContained rc=$rc (want 0)"
git status --short --untracked-files=no | tee -a "$V"
