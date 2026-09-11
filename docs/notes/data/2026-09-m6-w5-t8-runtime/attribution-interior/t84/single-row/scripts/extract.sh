#!/bin/bash
# Extract ONE arm from git archive (repo + both pinned submodules), by the
# T8.9r artifact's recipe. $1 = sha, $2 = dest dir.
set -o pipefail
REPO=/home/ghecht/Projects/hven
EIGEN=bc3b39870ecb690a623a3f49149a358b95c5781d
FMT=407c905e45ad75fc29bf0f9bb7c5c2fd3475976f
SHA=$1 DEST=$2
rm -rf "$DEST"; mkdir -p "$DEST"
git -C $REPO archive --format=tar $SHA | tar -x -C "$DEST" || exit 1
mkdir -p "$DEST/dep/eigen" "$DEST/dep/fmt"
git -C $REPO/dep/eigen archive --format=tar $EIGEN | tar -x -C "$DEST/dep/eigen" || exit 1
git -C $REPO/dep/fmt   archive --format=tar $FMT   | tar -x -C "$DEST/dep/fmt"   || exit 1
echo "arm eigen gitlink: $(git -C $REPO rev-parse $SHA:dep/eigen)  expected $EIGEN"
echo "arm fmt   gitlink: $(git -C $REPO rev-parse $SHA:dep/fmt)    expected $FMT"
echo "extracted: $(find "$DEST" -type f | wc -l) files"
