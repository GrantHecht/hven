#!/usr/bin/env bash
# RF-1c r3 leg V (../DECLARATION-RF-1c-r3.md section 3): the NET3 restart's claimed dependence on keeping the running best, measured.
# Builds a LOCAL-ONLY probe arm P = the committed head with mutation R1 applied (new_run also forgets the best) as a loose commit on the
# local branch rf1c-r1probe (never pushed, deleted by the settler at close), the TRAIN-lane hven install at that commit
# (lane_build.sh, unpatched), the TU-2 harness binary at that pin (tu2_build.sh), then the 48 TU-2 cells (tu2_run.sh) on the four
# pinned physical cores of one box lane, and tu2_report_r3.py P vs H and P vs R. Counters, statuses and objectives only.
# Run under: ~/.local/bin/box-lane -- bash rf1c_r1probe.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/rf1c
R=/home/ghecht/Projects/hven/.scratch/rf1c-build-legs
D=$WT/docs/notes/data/2026-11-m8-rf1; L=$D/legs-rf1c; E=$D/gate-RF-1c/r1probe
echo "FOREGROUND_START $(date -Is) lane=$BOX_LANE phys=$BOX_LANE_PHYS"
cd "$WT" || exit 1
git diff --quiet --ignore-submodules=all -- src include tests || { echo TREE_DIRTY_BEFORE; exit 2; }
mkdir -p "$E" "$R/tmp"
F=include/hven/detail/sqp/globalization/restoration.h
OLD='    void new_run() { samples_.clear(); }'
NEW='    void new_run() { samples_.clear(); best_ = std::numeric_limits<double>::infinity(); }'
base=$(git rev-parse HEAD)
export GIT_INDEX_FILE=$R/tmp/probe.index; rm -f "$GIT_INDEX_FILE"
git read-tree "$base" || exit 1
python3 - "$F" "$OLD" "$NEW" > "$R/tmp/restoration.probe.h" <<'PY' || { echo ANCHOR_FAIL; exit 3; }
import sys
f, old, new = sys.argv[1:4]
s = open(f).read()
assert s.count(old) == 1, "anchor"
sys.stdout.write(s.replace(old, new))
PY
blob=$(git hash-object -w "$R/tmp/restoration.probe.h")
git update-index --cacheinfo 100644,$blob,$F || exit 1
tree=$(git write-tree) || exit 1
unset GIT_INDEX_FILE
P=$(git -c user.name="Grant Hecht" -c user.email=grantrhecht@icloud.com commit-tree "$tree" -p "$base" -m "EVIDENCE-ONLY (local, never pushed): RF-1c R1 probe -- new_run also forgets the running best") || exit 1
git branch -f rf1c-r1probe "$P" || exit 1
echo "PROBE_COMMIT $P (base $base)"
git diff "$base" "$P" --stat
bash $L/lane_build.sh P1 "$P" > $E/lane-build.out 2>&1; echo "lane_build rc=$?"; tail -5 $E/lane-build.out
bash $L/tu2_build.sh P1 "$P" > $E/tu2-build.out 2>&1; echo "tu2_build rc=$?"; tail -3 $E/tu2-build.out
sha256sum $R/bin/tu2-P1/hven_bench_cell
read -r p1 p2 p3 p4 _ <<< "$BOX_LANE_PHYS"
bash $L/tu2_run.sh P1 $E/P $p1 $p2 $p3 $p4 > $E/run-P.out 2>&1; echo "tu2 P rc=$?"
python3 $L/tu2_report_r3.py $E/P $D/gate-RF-1c/tu2/H > $E/report-vs-H.txt 2>&1; echo "report vs H rc=$?"; tail -2 $E/report-vs-H.txt
python3 $L/tu2_report_r3.py $E/P $D/gate-RF-1c/tu2/R > $E/report-vs-R.txt 2>&1; echo "report vs R rc=$?"; tail -2 $E/report-vs-R.txt
echo "FOREGROUND_END $(date -Is)"
