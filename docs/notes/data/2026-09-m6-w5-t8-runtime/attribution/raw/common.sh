# Shared leg mechanics for W5 T8.9r. Sourced by every leg script AND by the
# fold-proof script, so the proof exercises the SAME code the timed legs use.
set -o pipefail
LEG_RC=0
# Worst-fold: LEG_RC ends at the WORST (numerically largest) status any step
# returned. Never resets, never masks.
fold() {
    local rc=$1
    if [ "$rc" -gt "$LEG_RC" ]; then LEG_RC=$rc; fi
    return 0
}
# Run a command, fold its status. No pipes inside: a pipe would report the
# LAST stage's status and hide the one that matters.
run() {
    "$@"
    fold $?
    return 0
}
utc() { date -u +%Y-%m-%dT%H:%M:%SZ; }
box_pgrep() {
    echo "PGREP_INLOCK $(utc)"
    pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' || echo PGREP_EMPTY
}
