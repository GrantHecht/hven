#!/usr/bin/env bash
# usage: manifest.sh <dir>   -> <dir>/MANIFEST.txt (sha256  size  path), every file under <dir> except MANIFEST.txt
cd "$1" || exit 1
find . -type f ! -name MANIFEST.txt | LC_ALL=C sort | while read -r f; do printf '%s  %s  %s\n' "$(sha256sum "$f" | cut -d' ' -f1)" "$(stat -c %s "$f")" "${f#./}"; done > MANIFEST.txt
wc -l MANIFEST.txt
