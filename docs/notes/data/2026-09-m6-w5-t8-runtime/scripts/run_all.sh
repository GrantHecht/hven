#!/bin/bash
# W5 T8.9r fix1 -- THE TOP-LEVEL RUNNER (astra I6).
#
# Round 1 had no script that invoked BOTH the legs and the comparator, so the
# claim that "every leg folds its comparator status" was false: the fold
# function was proved on a stub, and no leg ever called the comparator. This
# script is the missing one. It invokes every leg script AND `idle_proof.py`
# AND `comparator.py`, and worst-folds EVERY status into ONE exit.
#
#   ./run_all.sh                      the whole round: legs, idle proof, comparator
#   ./run_all.sh --skip-legs          idle proof + comparator over what is on disk
#   ./run_all.sh --comparator CMD     substitute CMD for the comparator (the fold proof)
#
# EVERY leg runs under `flock /tmp/box-build.lock`, one at a time, in the
# foreground. The `--skip-legs` form is how this script was actually exercised
# in the fix round: the timed legs together run for about an hour and a half,
# longer than this session's per-command cap, so each batch was invoked
# separately under the lock -- the same script, the same arguments, the same
# lock, the same log. That is declared in PROVENANCE.txt's fix1 block.
R=/home/ghecht/Projects/hven/.scratch/w5t89r
. $R/fix1/common.sh
ART=${ART:-$R/art2}
LOCK=/tmp/box-build.lock
SKIP_LEGS=0
COMPARATOR="python3 -B $ART/comparator.py --root $ART --out $R/logs2/comparator-output.md"
while [ $# -gt 0 ]; do
    case "$1" in
        --skip-legs)  SKIP_LEGS=1; shift ;;
        --comparator) COMPARATOR="$2"; shift 2 ;;
        *) echo "run_all.sh: unknown argument '$1'" >&2; exit 64 ;;
    esac
done
mkdir -p $R/logs2
echo "RUN_ALL_START $(utc) skip_legs=$SKIP_LEGS"

leg() {  # leg <script> <args...>
    local name=$1; shift
    echo "LEG $(utc) $name $*"
    pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' \
        > $R/logs2/$(printf '%s' "$name-$*" | tr ' /' '--').pgrep 2>&1
    flock $LOCK bash $R/fix1/$name "$@"
    local rc=$?
    fold $rc
    echo "LEG_DONE $(utc) $name $* rc=$rc LEG_RC=$LEG_RC"
}

if [ $SKIP_LEGS -eq 0 ]; then
    for m in ipm ssn walk; do
        for r in 1 2 3; do leg leg1_wall.sh $m $r; done
    done
    for m in ipm ssn walk; do leg leg1_perf.sh $m; done
    for spec in "ipm off 1000" "ipm sink 1000" "ssn off 2000" "ssn sink 2000" \
                "walk off 1000" "walk sink 1000"; do
        set -- $spec
        leg leg2.sh $1 $2 $3 A
        leg leg2.sh $1 $2 $3 B
    done
    for p in wall perfA perfB; do leg leg_interior.sh $p; done
    for r in 1 2 3; do leg leg_interior_cells.sh $r; done
fi

echo "IDLE_PROOF $(utc)"
run python3 -B $R/fix1/idle_proof.py $ART/logs/L*.log $ART/logs/IDLE-PROOF.md
echo "   LEG_RC after the idle proof = $LEG_RC"

echo "COMPARATOR $(utc): $COMPARATOR"
run $COMPARATOR
echo "   LEG_RC after the comparator = $LEG_RC"

echo "RUN_ALL_END $(utc) WRAPPER_EXIT=$LEG_RC"
exit $LEG_RC
