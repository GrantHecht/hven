#!/usr/bin/env bash
# usage: lists.sh <build-dir-name> <out-dir>   (cheap; run under the box lock)
# ctest -N names, and the sorted gtest list of every test executable ctest runs (suite.test per line).
set -o pipefail
source "$(dirname "$0")/env.sh"
box_check
DIR=$WT/${1:?dir}; OUT=$(realpath -m "${2:?out}")
mkdir -p "$OUT/gtest"
cd "$DIR" || exit 1
ctest -N | sed -n 's/^ *Test *#[0-9]*: //p' | LC_ALL=C sort > "$OUT/ctest-N.txt"
wc -l < "$OUT/ctest-N.txt"
ctest -N -V | awk '/^[0-9]+: Test command:/ {print}' > "$OUT/ctest-commands.txt"
n=0
while IFS= read -r line; do
  exe=$(echo "$line" | sed -E 's/^[0-9]+: Test command: *//' | awk '{print $1}')
  name=$(basename "$exe")
  "$exe" --gtest_list_tests > "$OUT/gtest/$name.raw" 2>/dev/null || { rm -f "$OUT/gtest/$name.raw"; echo "NOT_GTEST $name"; continue; }
  awk '/^[^ ]/ {suite=$1; next} /^  / {sub(/^ +/,""); sub(/ +#.*/,""); print suite $0}' "$OUT/gtest/$name.raw" | LC_ALL=C sort > "$OUT/gtest/$name.txt"
  rm -f "$OUT/gtest/$name.raw"
  n=$((n+1))
done < "$OUT/ctest-commands.txt"
echo "gtest executables listed: $n"
wc -l "$OUT"/gtest/*.txt | tail -1
sha256sum "$OUT"/ctest-N.txt "$OUT"/gtest/*.txt > "$OUT/lists.sha256"
