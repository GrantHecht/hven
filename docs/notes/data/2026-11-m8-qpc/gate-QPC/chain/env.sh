# QPC runner glue (NOT a declared leg script; recorded in gate-QPC/DEVIATIONS.md). Sourced by every driver.
export W=/home/ghecht/Projects/hven/.claude/worktrees/qpc
export G=$W/docs/notes/data/2026-11-m8-qpc
export HB=/home/ghecht/Projects/hven-bench-qpc
export PY=$HB/.scratch/venv/bin/python
export STORE=$HB/.scratch/qpc-store          # ensemble store (census H0 member dirs copied in + arm rows)
export DEV=$HB/.scratch/qpc-m0dev-store      # member-0 development store for N2 / P-POWER (never reused as ensemble rows)
export RUN=$W/.scratch/qpcrun                # logs, leg B scratch, N0 outputs
export B=$RUN/B
export BIN=$G/BINARIES.txt
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
export R_PIN=99712ec324716e6d08ee0545a43d595ca15eec3f
export H_PIN=f0ba4e1010000000000000000000000000000000   # replaced below by rev-parse
H_PIN=$(git -C $W rev-parse f0ba4e101); C1_PIN=$(git -C $W rev-parse 8923ae130); C2_PIN=$(git -C $W rev-parse 82b8c3112)
export H_PIN C1_PIN C2_PIN
arm_pin()  { case $1 in QPC) echo $H_PIN;; QPC-C1) echo $C1_PIN;; QPC-C2) echo $C2_PIN;; esac; }
arm_lane() { echo $B/$1/lane/lanes/hven/hven_bench_cell; }
arm_inst() { echo $B/$1/install; }
arm_grid() { echo $B/$1/grid/hven_qp_dump_grid; }
glock() { flock $RUN/git.lock "$@"; }                 # serialize git writes with the other writers (the driver and the settler's agent)
gcommit() { # gcommit "<message>" path...
  local msg=$1; shift
  glock git -C $W add -- "$@" && glock git -C $W commit -q -m "$msg" && glock git -C $W push -q origin qpc
}
stop() { echo "STOP $(date -Is) $*" | tee -a $RUN/STOP.txt; exit 3; }
# D (V-PDIV) runs LAST: the last finisher of {driverY, driverEA x3} launches it (atomic mkdir, no polling, no waiting).
maybe_launch_D() {
  for f in Y.ALLDONE EA-QPC.ALLDONE EA-QPC-C1.ALLDONE EA-QPC-C2.ALLDONE; do [ -f $RUN/$f ] || return 0; done
  mkdir $RUN/D.launch 2>/dev/null || return 0
  setsid nohup bash $G/gate-QPC/chain/driverD.sh > $RUN/logs/driverD.log 2>&1 < /dev/null &
  echo $! > $RUN/driverD.pid
}
