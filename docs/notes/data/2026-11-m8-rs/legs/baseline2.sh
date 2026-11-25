#!/usr/bin/env bash
# RS-2 baseline: the RS-1e head (the commit RS-2 branches from) measured with the same legs the gate uses, into .scratch/ (ignored).
# usage: flock /tmp/box-build.lock setsid -w bash baseline2.sh      steps: build capture lists replay ctest
set -o pipefail
source "$(dirname "$0")/env.sh"
STEPS=${*:-build capture lists replay ctest}
E=$WT/docs/notes/data/2026-11-m8-rs; L=$E/legs
cd "$WT" || exit 1
has() { case " $STEPS " in *" $1 "*) return 0;; esac; return 1; }
mkdir -p .scratch/base
echo "== baseline head $(git rev-parse HEAD) $(date -Is)"
if has build; then bash $L/build.sh Release build clean > .scratch/base/build.txt 2>&1; echo "build rc=$?"; tail -2 .scratch/base/build.txt; fi
if has capture; then
  rm -rf .scratch/psym-base; scripts/psym_compare.sh capture .scratch/psym-base build > .scratch/base/psym-capture.txt 2>&1; echo "capture rc=$?"
  cp build/compile_commands.json .scratch/cc-base.json
fi
if has lists; then rm -rf .scratch/base/lists; bash $L/lists.sh build .scratch/base/lists > .scratch/base/lists-run.txt 2>&1; echo "lists rc=$?"
  python3 $E/tools/lists_compare.py $E/baseline .scratch/base/lists | tail -3; fi
if has replay; then bash $L/replay.sh build .scratch/base/replay > .scratch/base/replay-run.txt 2>&1; echo "replay rc=$?"; fi
if has ctest; then bash $L/ctest.sh build .scratch/base/ctest-release.log > .scratch/base/ctest-run.txt 2>&1; echo "ctest rc=$?"; grep -E 'tests passed|tests failed' .scratch/base/ctest-release.log; fi
echo "== baseline done $(date -Is)"
