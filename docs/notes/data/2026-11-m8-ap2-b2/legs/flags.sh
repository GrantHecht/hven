#!/usr/bin/env bash
# AP-2 B2-d: compile-flags diff, pin (build-b2/pin, built from the exported copy
# build-b2/pin-src) vs B2 (build-b2/rel). The pin's compile_commands.json is first
# re-rooted -- the exported source root replaced by the worktree root, the pin build
# dir by the B2 build dir -- which normalizes the -I / source / object spellings and
# nothing else; then flags_diff.py (the B1 instrument, copied verbatim).
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-aba3deeb97313f444
EV=$WT/docs/notes/data/2026-11-m8-ap2-b2
python3 - "$WT" <<'PY'
import sys
wt = sys.argv[1]
s = open(wt + '/build-b2/pin/compile_commands.json').read()
s = s.replace(wt + '/build-b2/pin-src', wt).replace(wt + '/build-b2/pin/', wt + '/build-b2/rel/')
open(wt + '/build-b2/pin-cc-rerooted.json', 'w').write(s)
PY
python3 "$EV/flags_diff.py" "$WT/build-b2/pin-cc-rerooted.json" "$WT/build-b2/rel/compile_commands.json"
