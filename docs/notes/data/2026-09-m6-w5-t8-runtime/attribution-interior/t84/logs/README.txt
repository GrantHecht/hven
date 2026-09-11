W5 T8.9r-attrib3 -- WHAT IS IN THIS DIRECTORY
=============================================
Every batch this leg ran, as its script wrote it, unedited. The naming:

  B0-predeclaration.log   the scoring rules' hash and its UTC stamp, taken BEFORE
                          the first timed sample existed
  B1-build.log            the six bisect arms' builds (extract, submodule pins,
                          configure, build, sha256, source count)
  B2-smoke.log            the untimed smoke: every arm accepts the pin's
                          invocation, and the row ORDER -- the R3 positional
                          rule's premise -- is the same at all six
  B3-identity.log         the binary identity and the build-recipe calibration
  B4-build-exp.log        the experiment-1 and experiment-2 arms' builds
  B5-build-exp3.log       experiment 3's three padded arms' builds
  F1-fold-proof-{fail,pass}.log
                          the worst-fold proof, run through the SAME common.sh
                          the timed legs source. `fail` stubs three steps at rc
                          2/5/3 in that order and the wrapper exits 5 -- the
                          WORST, not the last, and a later rc 0 does not clear
                          it; `pass` runs the identical sequence at rc 0 and
                          exits 0.
  W{1..5}-wall-r{1..5}.log      Step A, the six-arm wall rounds
  X{1..5}-xwall-r{1..5}.log     experiments 1 and 2, the four-arm wall rounds
  Y{1..5}-ywall-r{1..5}.log     experiment 3, the five-arm wall rounds
  F{1..5}-wallpf-r{1..5}.log    the fault count in the wall leg's own condition
  A{1..5}-alloc-r{1..5}.log     experiment 4, the allocator intervention
  M{1..6}-mem-r{1..6}.log       the memory-behaviour leg (page faults under perf)
  P1-perf.log, P2-perfstat-r{1..3}.log, P3-perfrec-r{1..3}.log
                          the perf legs
  S1-screen-malloc.log    a SCREENING probe, kept because it is the reason the
                          fault count was re-taken without perf: it read the
                          culprit's faults at ~301 k where the mem leg reads
                          361 k, under a different perf event list and therefore
                          a different child argv+environ footprint. Nothing in
                          any table is taken from it.
  IDLE-PROOF.md           R2's arithmetic over the Step A wall batches
  IDLE-PROOF-X.md         ... over the experiment-1/2 wall batches
  IDLE-PROOF-Y.md         ... over the experiment-3 wall batches
  IDLE-PROOF-A.md         ... over experiment 4's wall batches
  IDLE-PROOF-F.md         ... over the wallpf batches
  All five exit 0: twenty-five timed wall batches, all PINNED-CLEAN.

EVERY BATCH LOG CARRIES, IN THIS ORDER: the `pgrep -af 'cmake|ninja|ctest|hven_|
codex|clang'` audit taken OUTSIDE the box lock by the caller, verbatim; the same
audit taken again INSIDE the lock; `FOREGROUND_START <utc>`; the batch's
`PS_SNAPSHOT` blocks with every foreign pid's `ps` row and its utime+stime in
clock ticks; a `CPUSTAT` bracket around EVERY timed run reading `cpu2`, `cpu10`
and the whole machine; `FOREGROUND_END <utc>`; and `WRAPPER_EXIT=<worst rc>`.

NINE BATCHES WERE RE-RUN, as R2 requires. xwall-r1..r4 failed the PINNED-CORE
test on their first pass (0.29-0.35 s of foreign un-niced user time on cpu2,
1.09-1.32 % of the window). alloc-r3 and wallpf-r1..r4 failed on the SMT SIBLING
(cpu10, 0.50-1.19 % of the window) with cpu2 at exactly zero -- the wallpf
batches are short, 13 s, so a few hundredths of a second there is over 1 %. All
nine were re-run under the same recipe, and the re-run overwrote each batch's
log and CSVs, which is what "the batch is re-run" means. NOTHING WAS EVER
SIGNALLED.

The `FOREIGN_PS` rows are long because several of this box's resident processes
are shells with very long command lines. They are retained as the snapshot
printed them; `scripts/idle_proof.py` reads the `FOREIGN_TICK` lines beside them.
