#!/usr/bin/env bash
# usage: format.sh <base>: clang-format --dry-run on every .cpp/.h the range <base>..HEAD touches; 0 diagnostics each.
source "$(dirname "$0")/common.sh"
cd "$WT" || fin 1
clang-format --version
tot=0
for f in $(git diff --name-only "${1:?}"..HEAD -- '*.cpp' '*.h'); do n=$(clang-format --dry-run "$f" 2>&1 | grep -c 'warning:'); echo "$n $f"; tot=$((tot+n)); done
echo "diagnostics=$tot"
[ $tot -eq 0 ]; fin $?
