# Shared leg mechanics for W5 T8.9r-attrib3 (the interior leg's per-task
# attribution). This file is the T8.9r FIX-ROUND-1 common.sh
# (docs/notes/data/2026-09-m6-w5-t8-runtime/scripts/common.sh) with the two scratch paths
# repointed at this leg's scratch root and the BOX_PAUSE_GIVEUP message's wording
# shortened -- and NOTHING ELSE.  common-vs-fix1.diff in this directory is that
# diff, retained: the R2 discipline is reproduced by running that file's code.
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

# ---------------------------------------------------------------------------
# R2 -- THE SOLO PROOF IS BY CPU TIME, NOT BY PROCESS NAMES.  (See the fix1
# common.sh header and reading.md 8.1 for the full argument.)  Two lines per
# foreign process: the verbatim `ps` row, and the same pid's utime+stime in
# CLOCK TICKS from /proc/<pid>/stat, because `ps` resolves CPU time only to
# whole seconds.  NOTHING IS EVER SIGNALLED: a runnable foreign process PAUSES
# the batch and is re-snapshotted.
# ---------------------------------------------------------------------------
CLK_TCK=$(getconf CLK_TCK 2>/dev/null || echo 100)
SNAPBUF=${SNAPBUF:-/home/ghecht/Projects/hven/.scratch/w5t89ra3/snap/buf}
mkdir -p "$(dirname "$SNAPBUF")"

box_snapshot() {
    local tag=$1
    echo "PS_SNAPSHOT $tag $(utc) mono=$(cut -d' ' -f1 /proc/uptime) clk_tck=$CLK_TCK"
    ps -eo pid,ppid,stat,pcpu,cputimes,etimes,comm,args --no-headers 2>/dev/null \
      | awk -v me=$$ '
        {
          n++; pid[n]=$1; raw[n]=$0; par[$1]=$2
          a=""; for (i=8;i<=NF;i++) a = a $i " "
          ar[n]=a
        }
        END {
          p=me
          while (p != "" && p+0 > 0) { anc[p]=1; if (p+0 == 1) break; p=par[p] }
          for (i=1; i<=n; i++) {
            if (ar[i] ~ /^\[/) continue                    # kernel thread
            own = (pid[i] in anc)
            if (!own) {
              q=pid[i]
              while (q != "" && q+0 > 0) {
                if (q+0 == me+0) { own=1; break }
                if (q+0 == 1) break
                q=par[q]
              }
            }
            if (own) continue
            print "FOREIGN_PS " raw[i]
            print "FOREIGN_PID " pid[i]
          }
        }' > "$SNAPBUF.raw" 2>/dev/null
    local nf=0
    while read -r kind rest; do
        if [ "$kind" = "FOREIGN_PS" ]; then
            echo "FOREIGN_PS $rest"
        elif [ "$kind" = "FOREIGN_PID" ]; then
            local p=$rest
            if [ -r "/proc/$p/stat" ]; then
                local st
                st=$(sed 's/.*) //' "/proc/$p/stat" 2>/dev/null \
                     | awk '{printf "%s %d", $1, $12 + $13}')
                [ -n "$st" ] && echo "FOREIGN_TICK $p $st" && nf=$((nf+1))
            fi
        fi
    done < "$SNAPBUF.raw"
    echo "PS_SNAPSHOT_END $tag foreign_pids=$nf"
}

box_guard() {
    local tag=$1 tries=0
    while : ; do
        box_snapshot "$tag" | tee "$SNAPBUF.cur"
        if ! grep -qE '^FOREIGN_TICK [0-9]+ R' "$SNAPBUF.cur"; then
            break
        fi
        tries=$((tries+1))
        echo "BOX_PAUSE $(utc) tag=$tag try=$tries -- a foreign process is in state R."
        echo "           PAUSING 5 s and re-snapshotting. Nothing is signalled."
        command sleep 5
        if [ "$tries" -ge 1 ]; then
            echo "BOX_PAUSE_GIVEUP $(utc) tag=$tag -- still R after $tries pauses."
            echo "           Same finding as reading.md 8.1: the desktop's resident processes"
            echo "           are transiently R at any moment on this box, so R2's 'no R in any"
            echo "           snapshot' condition cannot be reached by waiting. The pause and"
            echo "           the gap are RECORDED; the batch continues, and the PINNED-CORE"
            echo "           test is what proves this window. Nothing was signalled."
            break
        fi
    done
}

# ---------------------------------------------------------------------------
# R2, THE PART THAT ACTUALLY DECIDES IT: THE PINNED CORE'S OWN ACCOUNTING.
#   foreign CPU time on cpu2 = cpu2's busy jiffies across the run
#                              MINUS the measured process's own user+sys
#   SMT contention           = cpu10's busy jiffies across the run
# Bracketed around EACH timed run, not around the batch.
# ---------------------------------------------------------------------------
TIMEBUF=${TIMEBUF:-/home/ghecht/Projects/hven/.scratch/w5t89ra3/snap/time}
mkdir -p "$(dirname "$TIMEBUF")"

# AND THE DRIVING SHELL IS PINNED OFF THE MEASUREMENT CORE AND ITS SIBLING.
HARNESS_MASK=${HARNESS_MASK:-0,1,3,4,5,6,7,8,9,11,12,13,14,15}
taskset -cp "$HARNESS_MASK" $$ > /dev/null 2>&1

cpu_stat() {
    echo "CPUSTAT $1 $2 $(utc) mono=$(cut -d' ' -f1 /proc/uptime)"
    awk '$1=="cpu"||$1=="cpu2"||$1=="cpu10"{print "CPUSTAT_LINE " $0}' /proc/stat
}

# timed_run <tag> <stdout-file> -- <command...>
timed_run() {
    local tag=$1 red=$2; shift 2
    [ "$1" = "--" ] && shift
    cpu_stat "$tag" pre
    /usr/bin/time -f "CPUTIME_SELF tag=$tag user=%U sys=%S real=%e" -o "$TIMEBUF" -- "$@" \
        > "$red" 2>&1
    fold $?
    cpu_stat "$tag" post
    cat "$TIMEBUF"
}

batch_start() {
    echo "BATCH_START $1 $(utc)"
    box_guard "$1/open"
}
batch_end() {
    box_guard "$1/close"
    echo "BATCH_END $1 $(utc)"
}
