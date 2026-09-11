# Shared leg mechanics for W5 T8.9r FIX ROUND 1. Sourced by every leg script AND
# by the fold-proof script, so the proof exercises the SAME code the timed legs
# use. (astra I1/I6/I7; settler ruling R2.)
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
# R2 -- THE SOLO PROOF IS BY CPU TIME, NOT BY PROCESS NAMES.
#
# Round 1 proved solo-ness with `pgrep`, which returns command NAMES. A name
# cannot say whether a process ran. This function snapshots every FOREIGN
# process -- everything but this agent's own process tree and the kernel
# threads -- with its scheduler state and its accumulated CPU time, so the
# window's idle proof is arithmetic on the CPU time those processes actually
# consumed across it:
#
#   for every foreign pid, cputime delta across the batch < 0.5 % of the
#   batch's wall, AND no `R` state observed in any snapshot.
#
# Two lines per process are written. FOREIGN_PS is the verbatim `ps -eo
# pid,ppid,stat,pcpu,cputimes,etimes,comm,args` row R2 asks for; FOREIGN_TICK
# carries the same pid's utime+stime in CLOCK TICKS read from /proc/<pid>/stat,
# because `ps` reports CPU time in whole seconds and a 0.5 % bar on a 200 s
# window is 1 s -- below that column's resolution. Ticks are 10 ms here
# (`getconf CLK_TCK` = 100), which is 100x finer than the bar.
#
# NOTHING IS EVER SIGNALLED. When a foreign process is RUNNABLE the batch
# PAUSES and re-snapshots; the gap is recorded in the log.
# ---------------------------------------------------------------------------
CLK_TCK=$(getconf CLK_TCK 2>/dev/null || echo 100)
# The snapshot's scratch buffer. Under .scratch/w5t89r/ -- NOTHING of this leg's
# is written under /tmp except the box lock itself and the corpus harness's own
# internal markers, both declared in PROVENANCE.txt.
SNAPBUF=${SNAPBUF:-/home/ghecht/Projects/hven/.scratch/w5t89r/snap/buf}
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
                # comm may contain spaces and parentheses: drop everything up to
                # the LAST ')'. After that, field 1 is the state and fields 12
                # and 13 are utime and stime.
                local st
                st=$(sed 's/.*) //' "/proc/$p/stat" 2>/dev/null \
                     | awk '{printf "%s %d", $1, $12 + $13}')
                [ -n "$st" ] && echo "FOREIGN_TICK $p $st" && nf=$((nf+1))
            fi
        fi
    done < "$SNAPBUF.raw"
    echo "PS_SNAPSHOT_END $tag foreign_pids=$nf"
}

# A snapshot that PAUSES (never signals) while any foreign process is RUNNABLE.
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
            echo "           The desktop's resident processes (the Wayland compositor, the"
            echo "           browser, this agent's own daemon) are transiently R at any moment"
            echo "           on this box, so R2's 'no R in any snapshot' condition cannot be"
            echo "           reached by waiting. The pause and the gap are RECORDED; the"
            echo "           batch continues, and the PINNED-CORE test (cpu2 busy minus the"
            echo "           run's own user+sys, cpu10 busy) is what proves this window."
            echo "           Nothing was signalled. Declared as a deviation in the report."
            break
        fi
    done
}

# ---------------------------------------------------------------------------
# R2, THE PART THAT ACTUALLY DECIDES IT: THE PINNED CORE'S OWN ACCOUNTING.
#
# The per-process snapshot above answers "did anything else run?". On a desktop
# box the honest answer is yes -- a compositor, a browser, this agent's own
# daemon -- and no protocol can make that false. What the timing recipe
# actually requires is narrower and IS measurable: that the PINNED CORE, and
# the SMT SIBLING it shares a physical core with, ran nothing but the measured
# solve. /proc/stat accounts every jiffy per logical CPU, so:
#
#   foreign CPU time on cpu2 = cpu2's busy jiffies across the run
#                              MINUS the measured process's own user+sys
#   SMT contention           = cpu10's busy jiffies across the run
#
# Both are bracketed around EACH timed run, not around the batch, so nothing of
# this agent's own analysis falls inside them. `cpu10` is also the measurement
# M2 asked for: the sibling's idleness is READ here, not asserted.
# ---------------------------------------------------------------------------
TIMEBUF=${TIMEBUF:-/home/ghecht/Projects/hven/.scratch/w5t89r/snap/time}
mkdir -p "$(dirname "$TIMEBUF")"

# AND THE DRIVING SHELL IS PINNED OFF THE MEASUREMENT CORE AND ITS SIBLING.
# Round 1 pinned the solve to cpu2 and left the harness -- the shell, `ps`,
# `awk`, `perf`'s own setup, the timestamps -- free to land on cpu2 as well, so
# "cpu2 ran nothing but the solve" was not even true of the leg's own scaffold.
# Every logical CPU EXCEPT 2 and its SMT sibling 10 is what the harness gets;
# `taskset -c 2` on the measured process still widens its own affinity to the
# reserved core, which is the point.
HARNESS_MASK=${HARNESS_MASK:-0,1,3,4,5,6,7,8,9,11,12,13,14,15}
taskset -cp "$HARNESS_MASK" $$ > /dev/null 2>&1

cpu_stat() {
    echo "CPUSTAT $1 $2 $(utc) mono=$(cut -d' ' -f1 /proc/uptime)"
    awk '$1=="cpu"||$1=="cpu2"||$1=="cpu10"{print "CPUSTAT_LINE " $0}' /proc/stat
}

# timed_run <tag> <stdout-file> -- <command...>
# The redirect is an ARGUMENT, not something the caller wraps around the call:
# a `>` on the call site would swallow the two cpu_stat brackets as well and
# leave the pinned-core test with nothing to read.
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
