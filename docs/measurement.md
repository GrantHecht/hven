# Measurement discipline

CLAUDE.md §7 (`CLAUDE.md:207-252`) is the rule; this page is how it is executed here.
Nothing on it is new: every rule is followed by where it is written, and where two sources
disagree that is said, not smoothed over. Rulings attributed to the settler are recorded in
`docs/notes/2026-09-m7-ledger.md`'s W0 T6 CLOSED entry, which is their source — this page
only cites them.

---

## 1. What is asserted and what is informational

**Counters and statuses are the currency.** CLAUDE.md §7's first line: "Counters are the
asserted currency of correctness and performance claims; wall-clock timings are
informational only." The M6 close gate states the consequence: "NOTHING IN THIS GATE ASSERTS
WALL-CLOCK. Counters, statuses and test counts are the asserted currency; every duration
recorded here is INFORMATIONAL"
(`docs/notes/data/2026-09-m6-close-gate/PROVENANCE.txt:116-120`). A quoted wall-clock number
is single-threaded unless stated otherwise (§7).

**Residual-class columns are the one exemption.** `CLAUDE.md:219-224`: they "are exempt from
that determinism" because MKL's kernels are address-sensitive, and cross-process agreement
on them "is asserted by a calibrated near-ulp gate rather than by byte equality — counters
and statuses remain exact." The gate's two numbers, derived at source, with byte equality
checked FIRST and the gate engaging only on digits that provably vary
(`tests/sqp/test_corpus_cells.cpp:932-934`):

- **1e-5 relative** — `tests/sqp/test_corpus_cells.cpp:935`, derived at `:908-934` as "~5.6x
  the largest spread on record, taken from the worst regime rather than the quietest because
  this pin runs wherever the suite runs, threaded and CI included" (readings 1.0e-7 serial /
  8.6e-7 CI cross-runner / 1.63e-6 CI / 1.79e-6 MKL-threaded).
- **A 1e-13 absolute floor** — `:3618`, `:3809`, reasoned at `:3607-3617`: these are
  residual norms "formed by cancellation from O(1) data", so a purely relative test measures
  noise against noise; worst observed drift 4.5e-15, 22x head-room, six orders below the
  smallest genuine movement.

**Which columns the gate reaches is the comparators' scope, not §7's.** §7 says
"residual-class columns"; the widening to **twelve non-wall floats** of the 76-column schema
is `compare_replay.py`'s, at M6 W6 T6b — `scripts/census_compare.py:35-43` states it: within
the census comparator the gate reaches "EXACTLY ONE COLUMN -- index 12, `kkt_residual`",
while `scripts/compare_replay.py`, "which compares the whole 76-column schema, gates ALL
TWELVE of its floating measure columns". The twelve are enumerated by index at
`tests/sqp/test_corpus_cells.cpp:938-951`. `wall_s` is a thirteenth float, excluded for a
different reason — "timing noise, never a regression contract".

**Both comparators are byte-exact by default; the gate is opt-in.** `census_compare.py`
excludes `wall_s` — "this is a counter replay, not a timing measurement (CLAUDE.md section
7)" — and without `--residual-gate` is "EXACT STRING EQUALITY on all 13 asserted columns";
`compare_replay.py` is "EXACT STRING EQUALITY on every column but `wall_s`" unless the flag
is passed. Non-numeric and non-finite spellings "NEVER pass the gate: they fall back to the
byte comparison, so an `inf` or a `nan` is reported, not waved through"
(`census_compare.py:55-57`; `compare_replay.py:95-96`).

So the gates as run assert *more* than §7 describes: the M6 close gate ran the census
comparator "at its BYTE-EXACT default -- its opt-in `--residual-gate` is NOT passed"
(`…/2026-09-m6-close-gate/PROVENANCE.txt:138-140`). **Settler ruling (M7 ledger, W0 T6
CLOSED entry):** §7's near-ulp gate is the **ceiling** a census may assert at;
byte-exact-by-default is the gates' stricter practice and it stands. `--residual-gate` is
opt-in **by declaration** for a cross-process run.

**Absent is never zero.** An unobserved Apple/Accelerate value is `UNOBSERVED` (CLAUDE.md
§6), an unmeasured diagnostic `NaN`, an engine-absent event field `std::optional`
(`docs/notes/2026-09-m6-w5-t8-plan.md:20`).

**A pin is broken on purpose or not at all** — "Intentional breaks of a pinned/reproduced
value are declared and re-derived explicitly — never silent" (§7). The flag-unification
event, phase C's U0, is the worked example of a declared mass re-derivation rather than an
exception (`CLAUDE.md:244-252`).

---

## 2. The serial rule: anything that asserts wall-clock

CLAUDE.md §7: "any measurement that ASSERTS wall-clock — a benchmark, a timing baseline, a
throughput or build-time comparison — must serialize its suite: one solve at a time, alone
on the machine. **Nothing produced any other way is a quotable timing.**"

The terms a serial leg declares, as the M4 task-7 wall leg declares them
(`docs/notes/data/2026-08-22-m4-task7-wall-leg/provenance_sqp.txt`, 42 lines): toolchain,
MKL, CPU, RAM, kernel and date named literally (`:1-7`); the protocol — "3 cells, 7 reps,
base/head alternated per rep, each run pinned to one core under MKL_NUM_THREADS=1"
(`:9-10`); the commits (`:11`); **the binary identity**, `md5 hven_sqp_bench base 4a65c612…
/ head 9c021305…` (`:12-13`); **the box state**, "box at leg start: loadavg 0.49, nothing
else compiling" (`:14`); and the lock, "every build and every leg ran under flock
/tmp/box-build.lock" (`:15`). Its earlier leg adds the wait condition — the leg "holds flock
/tmp/box-build.lock and **waits for loadavg[1] < 0.6 before starting**" (`:30-31`) — and the
pin runs down to the SMT sibling, "left idle and … MEASURED, not assumed"
(`…/2026-09-m6-w6-attached-cost/PROVENANCE.txt:107-111`).

**The counter-example, recorded rather than hidden.** That directory's original SQP leg is
marked `PROVENANCE PARTIAL` (`:34-42`): "the two bench binaries that produced those cells
were NOT checksummed at the time, and they no longer exist … only the binary identity is
unverifiable." The precedent's closing rule: "Every later leg in this directory records its
binaries' checksums at run time." A binary md5 plus the compiled-in stamp is part of a
complete declaration.

---

## 3. Co-runs: the parallel-tiered protocol

**What may co-run.** CLAUDE.md §7: "A replay asserting only deterministic per-process
columns — counters, statuses, and residuals computed at a fixed thread count — may co-run."
The argument is the gate-B review request's, approved by the owner 2026-08-14 and declared
"because a protocol change that is not declared is exactly the kind of silent break §7
forbids" (`docs/notes/2026-08-14-m3-gate-b-review-request.md:250-254`). Its four points
(`:256-273`): the census asserts counters and excludes `wall_s` by construction; counters
are scheduling-invariant at `MKL_NUM_THREADS=1`; "contention biases in one direction only" —
it "can never be pushed toward a false `Optimal` or a false counter value"; and §7's
serialization clause is about wall-clock validity.

**The fixed terms** (§7): one process per pinned **PHYSICAL** core with SMT siblings left
idle, `MKL_NUM_THREADS=1` in every process, and wall-clock from such a run "stays
informational — it is never quoted as a measurement and never compared against a serial
timing." Wall-clock-**dependent** statuses — deadline-truncated outcomes — are not
scheduling-invariant and "must run solo, or co-run only with their full budget honored."

**The tiering, assigned from the FROZEN baseline** so the schedule "is fixed before a single
solve starts and cannot be tuned by what the fresh run does"
(`…/2026-08-14-m3-gate-b-review-request.md:278-289`):

| Tier | Rule | Width |
|---|---|---|
| **T1** | everything else: solved, comfortably sub-deadline | 6-wide, core-pinned |
| **T2** | wall-sensitive: any terminating baseline cell with `wall_s ≥ 1800 s`, plus `f7_n10000_path_physics` | **solo** |
| **T3** | deep-DNF: every other baseline `dnf_*` cell, at its full per-phase budget | 5-wide |

Every cell process is "`MKL_NUM_THREADS=1` and `taskset`-pinned to one logical CPU of a
**distinct physical core** — SMT siblings are never both used" (`:280-282`). Order is T1 →
T2 → T3, tier 2 starting only after every tier-1 worker is reaped. Runner:
`scripts/run_walk_census.sh` (`PROTOCOL_VERSION="parallel-tiered v1"`, `:127`).

**The declaration goes in the artifact.** §7: "A co-run protocol is declared in the evidence
artifact it produces — the tiering rule, the widths, and the pinning." The census writes it
into the merged CSV's header, verbatim
(`docs/notes/data/2026-08-21-m3-gate-c/provenance_header.txt:1-15`):

```
# census.protocol: parallel-tiered v1 (T1 6-wide pinned / T2 solo / T3 5-wide full-budget)
# census.binary_stamp: binary: fb45a000c095
# census.binary_stamp: schema: 37
# census.binary_stamp: budget_table_hash: 0x357aee91dee27391
# census.topology: 8 physical cores; representatives (one sibling each): 0,1,2,3,4,5,6,7
# census.  counts: T1=44  T2=3  T3=10  (total 57)
```

The M6 close gate's prose form adds the comparator and its setting, the read-only baseline
and the tier windows as run (`…/2026-09-m6-close-gate/PROVENANCE.txt:122-147`).

**The serial-confirm rule.** §7: "A deviation observed under a co-run is a candidate, not a
regression. Re-run the deviating cell ALONE … and reproduce the deviation before calling it
one." The runner mechanizes it: a disagreeing cell "is written to serial_confirm_list.txt
and is NOT a regression until it has been re-run ALONE and still disagrees"
(`scripts/run_walk_census.sh:40-44`).

### 3.1 The per-leg concurrency declaration

When counter-only legs run while an **uncontrolled foreign lane** holds the box, the
declaration is per leg, not per directory — a blanket "SOLO" over a directory containing any
co-run leg is the failure mode, and it occurred. M7 W0 T3's header field reads "MIXED, and the split is per leg"
(`docs/notes/data/2026-09-m7-w0-t3-elastic-rung/PROVENANCE.txt:64`), and the block opens by
naming its own earlier error: "An earlier version of this file said 'SOLO. Every leg ran
alone … This is NOT a co-run' of the whole directory. That was true of the legs whose
numbers are QUOTED and false of three later diagnostic legs, and §7 requires the co-run to
be declared in the artifact it produces rather than only in a report." Both halves are then
named by file:

- **SOLO** — alone under `flock /tmp/box-build.lock`, `taskset -c 2`, `MKL_NUM_THREADS=1`,
  `OMP_NUM_THREADS=1`, empty pre-leg `pgrep`, each leg listed by its raw log, closed by
  "EVERY WALL-CLOCK AND INSTRUCTION NUMBER IN THIS DIRECTORY COMES FROM THAT SET"
  (`:75-85`).
- **CO-RUN** — the four legs that ran while "the hven-bench harness lane was running its own
  cells on this box, outside the box lock and outside this implementer's control", each
  named, terms exact: one MKL thread and `taskset -c 2` on **this** process only, "the
  foreign lane's placement was NOT controlled, so this is not the 'one process per pinned
  PHYSICAL core' discipline §7 defines for a measured co-run and nothing here is offered as
  one"; counters, statuses and a classification are what is read; "NO WALL NUMBER AND NO
  INSTRUCTION COUNT FROM THESE FOUR LEGS IS QUOTED ANYWHERE"; and the closing check — "every
  counter column these legs print is byte-identical to the SOLO measurement's" (`:87-104`).

---

## 4. The instruction-count protocol

Instructions are scheduling-invariant, so a claim about *work* is read from them rather than
from seconds. W5 T8.9r's leg 1 "is the WALL-CLOCK leg"
(`docs/notes/data/2026-09-m6-w5-t8-runtime/PROVENANCE.txt:184`), with instructions taken as
pass A on three designated cells per mode (`:188-190`); W6 T6 is the leg that asserts
instructions and nothing else — "the wall is carried beside it and is informational"
(`…/2026-09-m6-w6-attached-cost/PROVENANCE.txt:125-129`).

**The instrument** (`:131-135`), the ownership doc's pass A unchanged:

```
perf stat -e instructions:u,branches:u,cycles:u,branch-misses:u,L1-icache-load-misses:u
```

Everything is `:u` (user space) because "perf_event_paranoid = 2" on this box (`:89`) — an
environmental fact, stated, not worked around.

**The shape.** "Three rounds; the arms are ALTERNATED WITHIN a round" (`:135`), so an A and
a B reading of the same cell are adjacent in time. Each cell runs as **one process**
(`--internal-run-one`) rather than through the forking leg, because "a `perf stat` over the
leg would count 27 forks, 27 execs and a parent poll loop beside the solve" (`:207-214`).
Per arm, the **per-cell MEDIAN** of the runs; "corpus = the SUM of the per-cell medians,
never a mean of ratios" (`…/2026-09-m6-w5-t8-runtime/PROVENANCE.txt:183-184`).

**An untimed warm-up, so no round is a batch's first.** A batch's first process is an
allocator/page-cache outlier (one read +2.1 % in round 1 over its own rounds 2 and 3, all
six processes writing byte-identical rows); the median of three discards it and the re-take
additionally runs "ONE UNTIMED, UNRECORDED warm-up process", each arm then internally stable
to 0.27 % and 0.04 %. Neither batch is discarded
(`…/2026-09-m6-w6-attached-cost/PROVENANCE.txt:230-241`).

**The band, FLAT** — `docs/notes/2026-09-m6-w5-t6-ownership.md` §11.1 governs it
(`:1248-1249`):

> **FLAT** — per-cell median ratio in **0.99–1.01** AND whole-corpus within **±0.5 %**, in
> all three modes, on BOTH legs.

"BOTH legs" is §11.3's two (`:1347-1360`): **leg 1**, the U0 27-cell corpus, serialized, 3×
alternating; **leg 2**, "an HS-scale leg, which (d) cannot do without", because every corpus
cell is F7 at n ≥ 800 "where the QP dominates and a de-inlined per-major helper … is
invisible". FLAT is a **timing** band there. The instruction side has its own bands in the
same section (`:1260-1266`): every bench leg carries the `perf stat` line "BESIDE wall time,
on the same discipline as the timing"; **LAYOUT-MOVED** is "instruction and branch counts
identical within **1e-4**" with the cycle delta accounted for by the miss deltas — reported,
not a redraw trigger; **WORK-MOVED** is "instructions UP" and "Remains the veto". Anything
not FLAT is "UNRESOLVED pending re-measurement" (`:1252-1256`). Both bars are required: the
T6 close reading's ssn row is corpus-inside / cells-outside, in a wall reading
(`:1745-1751`).

W6 T6 **borrowed** the timing band for an instruction reading, per its plan clause — the
instruction reading "reported against the ±0.5 % corpus bar as a finding"
(`docs/notes/2026-09-m6-w6-plan.md:11`, §0 J.5; the leg says so at
`…/2026-09-m6-w6-attached-cost/PROVENANCE.txt:125-127`). That leg's own header, titled "THE
BAND (ownership doc §11.1, quoted)", drops the "on BOTH legs" clause without giving a reason
(`:137-140`); separately it notes that "§11.1's veto machinery is a COMMIT-PAIR rule and
does not apply: this leg has no pair" (`:141-142`). **Settler ruling (M7 ledger, W0 T6
CLOSED entry):** §11.1 governs; the header's omission is a misquote to be **noted**, and the
artifact is never edited.

**The idle proof, computed not assumed.** "A batch is one lock acquisition and is the unit
the idle proof is computed over" (`:222`). R2' as written
(`…/2026-09-m6-w5-t8-runtime/PROVENANCE.txt:531-535`): foreign TASK time on the measurement
core **and** on its SMT sibling, counted user+nice+steal+guest from `/proc/stat` across the
batch, "must be under 0.5 % of the batch's wall; every foreign pid and state seen in any
snapshot is listed, transients included; a foreign 'R' is a pause and a RE-SNAPSHOT; **a
batch that cannot be proven is re-run**." Two failure classes: `UNPROVEN-FRACTION` (a core
over 0.5 %) and `UNPROVEN-EVIDENCE` (alternation snapshots missing)
(`…/w6-attached-cost/PROVENANCE.txt:415-423`). The tool is `idle_proof.py`, copied between
artifacts byte-identically with its recorded sha256 (`:243-250`). W6 T6 instead marked three
failing wall batches NON-QUOTABLE — "The settler ruled that marking discharges it **here**:
nothing in this leg asserts a wall … A number that is to be QUOTED must come from a re-run
batch" (`:424-431`). That is a discharge granted on that leg's facts, not a replacement for
R2'.

**The counter check is the STOP condition.** Every column of every arm's CSV is compared
with `compare_replay.py`; if instructions move and a counter moves too, this is not an
instruction measurement of the same computation. The attached-cost leg records "43 cells x
30 columns, 0 differences, every pair" (`:232-236`).

---

## 5. The census cost policy

**The price is ~6 h.** `docs/notes/2026-08-15-m3-phase-c-plan.md:1512` (row O3): "P-CENSUS
is ~6 h. Running it on all six relocation batches costs ~36 h for commits that, by
construction, cannot change codegen."

**The ruling, and the amendment over it.** The ratification's clause list reads "P-SUITE +
P-SYM per batch; P-CENSUS once at the relocation-group boundary (R6), then on every content
change (B2, S1, S2, S3, all T-tasks, H3) and at Gate C". The 2026-08-15 amendment in the
same row "fixes the census schedule at one post-H3 counter census plus Gate C, **replacing
this row's 'P-CENSUS once at the relocation-group boundary (R6)' clause**", and quotes its
operative sentence verbatim: "Gate C (and every future gate) runs the full 57; the post-H3
counter census is the ONE intermediate census; **any further scope or frequency reduction is
pre-refused** — a proposal to run Gate C itself reduced would break ratified gate currency
and does not need to be asked." The amendment's stated scope names only the R6 clause, yet
"ONE intermediate census" also displaces "on every content change"; the two are in tension,
reported. **Settler ruling (M7 ledger, W0 T6 CLOSED entry):** the amendment governs in full
— one intermediate census plus every gate at the full 57, the pre-refusal sentence standing;
the older "every content change" clause is superseded by it.

**The argument P-SYM rests on** (same row): over the object set it compares — which includes
the census binary and the bench binaries — per-symbol disassembly identity is "strictly
stronger than a census for the Linux/MKL build … in one build instead of six hours", with
two bounded limits stated rather than implied (the Accelerate `#ifdef` arms are not compiled
on Linux; P-SYM is Release-only, Debug correctness being P-SUITE's). **Its tripwire is part
of the rule, not advice:** "any non-`__FILE__` P-SYM difference ⇒ the batch is not
relocation-only and P-CENSUS becomes mandatory for it."

**What P-SYM draws** (`scripts/psym_compare.sh:5-12`): object bytes are not expected to be
identical after a relocation — `__FILE__` and `__LINE__` both reach the object — and "what
must be identical is the INSTRUCTIONS, per symbol. This script draws that line in one place
so every batch draws it the same way, instead of each agent re-inventing a comparison and
then widening it by hand when the first legitimate difference appears." Exceptions are keyed
`<object>::<symbol>` with a stated reason; "the object qualifier is MANDATORY" (`:21-22`).

**The current instance** is the M6 close gate: the full 57-cell walk census, "all 57 cells,
the 13 the M5 close discharged INCLUDED" (`…/2026-09-m6-close-gate/PROVENANCE.txt:5-11`),
one detached leg, "57 baseline cells, 57 fresh cells, 0 mismatches",
`serial_confirm_list.txt` empty — no cell became a candidate, so no solo re-run was owed
(`:142-153`).

---

## 6. The box protocol

One box, several lanes; these rules are what keeps a measurement on it meaningful.

- **Builds are serialized against source edits.** CLAUDE.md §8 (`:258-261`): "do not edit
  source files while a build of that source is in flight, and do not start a build while
  another agent's edits to the same tree are in progress."
- **Every build and every leg runs under `flock /tmp/box-build.lock`**, foreground where it
  can be, with `PGREP_EMPTY` inside the lock (`docs/notes/2026-09-m6-w5-t8-plan.md:21`) and
  `FOREGROUND_START` / `FOREGROUND_END` written inside it too
  (`…/2026-09-m6-w6-attached-cost/PROVENANCE.txt:104-105`;
  `…/2026-09-m7-w0-t4-reseed/raw/gates/README.md:20`).
- **The `pgrep` goes in its own command**, before the lock and again inside it, both landing
  verbatim in the retained log (`…/w6-attached-cost/PROVENANCE.txt:95-101`).
- **Every wrapper is fold-proven.** `set -o pipefail`; the exit status is "the WORST status
  any step returned and a later success never clears it", proven against stubbed failures
  INSIDE the lock (`:110-116`).
- **Never kill or signal a process another lane started**; stop only by the pid your own
  launcher's log names, never `pgrep -f | kill`; flock-waiters are idle and are not a
  problem to solve. The settler's standing box-protocol rules (every M6/M7 brief §0;
  recorded in the M7 ledger's W0 T6 CLOSED entry), whose precedent is
  `docs/notes/2026-08-m6-ledger.md:3440-3441` — legs "queued on `flock` behind the lane
  twice, nothing killed", and "background chains are not reliable for legs; foreground under
  600 s adopted".
- **Long legs detached and ALONE, pid retained** — the one >600 s Debug test runs under
  `nohup`, alone, its pid in the log (`docs/notes/2026-09-m6-w5-t8-plan.md:21`;
  `…/t4-reseed/raw/gates/README.md`).
- **Nothing under `/tmp` except the lock** (`…/w6-attached-cost/PROVENANCE.txt:116`), and
  scratch under one git-ignored parent (the standing brief §0 rule).

**What the collisions cost.** The remedy the M6 ledger records: an orphaned ninja held "the
lock fd for 2 h 11 min against hven's queued verify leg; the protocol now requires verifying
a 'lock released' claim with `fuser` and killing the process GROUP when stopping a flock'd
gate" (`docs/notes/2026-08-m6-ledger.md:1634-1637`, restated `:2350-2353`; the earlier
collision that produced the announce-before-every-solo-leg protocol is `:1489-1490`).

---

## 7. The provenance stamp

CLAUDE.md §7: "Every benchmark artifact carries a provenance stamp (toolchain, hardware,
date, commit)." The corpus binary writes eight header lines into every CSV
(`bench/bench_corpus.cpp:868-878`) — `# hven_sqp_corpus provenance`, then `# binary:` (`git
describe --always --dirty --abbrev=12`), `# schema:` (`kIpqpColumns`),
`# budget_table_hash:`, `# invocation:` (argv joined), `# MKL_NUM_THREADS:` (env or `<unset>`),
`# host:` and `# generated:` (UTC) — plus the lever state, unconditionally: "an arm that is
UNATTACHED is one half of this leg's comparison, so its file has to say so in its own bytes"
(`:879-882`).

**The stamp is resolved at CONFIGURE time, and that is the trap.**
`bench/CMakeLists.txt:81-90`: the string is resolved "at CONFIGURE time (not build time)
because that is when CMake can shell out"; `--dirty` is "deliberately included: a baseline
produced from an uncommitted tree must say so"; and "THE TRAP THIS CARRIES, and it bit once
(T10b fix round 1): a tree configured while clean and REBUILT after an edit emits the stale,
clean stamp."

It has since bitten twice more, in the trap's two directions, both in M7 W0:

1. **From tracked artifact PROSE under edit.** T3's Debug arm stamped `40ef105f-dirty`: the
   commit landed 21:33:39, the Debug build and suites ran 21:41:34–21:42:21 clean,
   `PROVENANCE.txt` — "a TRACKED artifact file" — was rewritten 21:42:27, and the arm
   started 29 s later; "the dirt was artifact PROSE." The leg "was RE-RUN on a clean tree
   and the dirty log is kept beside it as raw/debug-both-states.dirty.log rather than
   deleted" (`…/2026-09-m7-w0-t3-elastic-rung/PROVENANCE.txt:132-153`).
2. **From a tree that had moved since configure.** T4's replay arm stamped
   `0be12ec390c5-dirty`, "which is NOT the tree they were produced from" — the code was the
   clean `39746705`, the string was the build directory's configure-time one. Re-configured
   and re-taken; both arms kept, and "THE RE-TAKEN ARM IS THE AUTHORITY"
   (`…/2026-09-m7-w0-t4-reseed/PROVENANCE.txt:112-126`; `raw/gates/README.md:14`, 27 / 75 /
   0 ×3).

**The rules that follow, in their sources' words:**

- **An artifact edit lands before a stamped leg.** The settler's standing dispatch rule —
  first written in the T3 fix-2 dispatch, recorded in the M7 ledger's W0 T6 CLOSED entry —
  whose instance is T3's clean re-run above (`…/t3-elastic-rung/PROVENANCE.txt:132-138`).
- **Re-configure before producing a measurement CSV** (`bench/CMakeLists.txt:89-90`). The
  harness's form is "re-configure **per pin** (M7-16)"
  (`docs/notes/2026-09-m7-ledger.md:100-101`); the brief's is "the harness checks CLEAN
  before a sweep" (`docs/notes/2026-09-m7-brief.md:215-216`). That the configure-time stamp
  is stale once a commit lands is the observed consequence of the trap, not a fourth
  trigger.
- **A `-dirty` stamp "fails the gate's provenance requirement"**, and the census runner
  checks before starting a multi-hour sweep, with the preflight command spelled out
  (`scripts/run_walk_census.sh:51-61`; the M6 close gate's clean reading at
  `…/2026-09-m6-close-gate/PROVENANCE.txt:147-150`). A retained `-dirty` artifact is
  **named**, and is never the authority — T4's stale-stamped arm above is kept and the
  re-take is the authority.
- **A leg that must run dirty says so and why.** W6 T6 was dirty by exactly two files
  because "THE LEVERS ARE THE INSTRUMENT: the leg cannot be run at a tree without them"; its
  CSVs carry `b0a7ffaa4342-dirty`, "which is that fact, stamped by the binary itself"
  (`…/2026-09-m6-w6-attached-cost/PROVENANCE.txt:25-32`). A stamp is reconciled, never
  edited: "the stamp names the checked-out commit and is not edited to match prose"
  (`…/2026-09-m6-close-gate/PROVENANCE.txt:53-58`).

**A full artifact stamp** carries, beyond the CSV header: compiler, CMake, Ninja and perf
versions; MKL path and version; kernel, host, CPU with its physical/logical split and the
pinned core's SMT sibling named, memory, governor and boost state; run date in UTC; the
build directory and whether it was empty at configure; the effective compile flags **read
from the build's own `compile_commands.json` rather than from the configuration**; and the
binaries' checksums (`…/2026-09-m6-w6-attached-cost/PROVENANCE.txt:37-92`).

**The harness.** `hven-bench` carries the same block plus its own sha and the hven sha it
pins — "every artifact stamped (toolchain, hardware, date, hven sha + harness sha)"
(`docs/notes/2026-09-m7-brief.md:60-64`). It "pins hven by `find_package(hven)` against an
install prefix built from a named sha" (`:47-50`), so the hven pin in its stamp is that
prefix's provenance, not a branch name.

---

## 8. Apple/Accelerate and Windows

**Never fabricate a value.** CLAUDE.md §6: "A value that has not been observed on real Mac
hardware is recorded as `UNOBSERVED`, never zero-filled or interpolated. Absent evidence is
reported as absent." Windows has the same standing (`docs/concurrency.md:294-295`), and an
evidence directory says so explicitly rather than by omission: "APPLE / WINDOWS: UNOBSERVED.
Nothing in this directory was taken on Mac or Windows hardware and no value here is
interpolated to them" (`…/2026-09-m6-w6-attached-cost/PROVENANCE.txt:88-92`).

**What a green macOS CI lane does and does not license** (`docs/ci.md:498-509`): "A green
macOS lane licenses a per-backend *counter* arm and does not license a per-backend *float*
arm." Each backend asserts its own observed value — "the MKL side is never relaxed and the
pair is never replaced by a range that admits both". The CI-side half: a run on
`github-macos26-arm64` is a real observation, a counter that reproduces across runs is
committable, and a float needs **two-run byte agreement** because nothing on that lane pins
the reduction order.

---

## 9. Who cites this page

- `docs/concurrency.md` — the wall-clock-under-contention bullet (`:296-301`) and the
  residual-gate note (`:80-91`).
- `scripts/run_walk_census.sh` (`:2`, `:36-61`), where the co-run declaration and the
  clean-stamp preflight are enforced; `scripts/census_compare.py` and
  `scripts/compare_replay.py`, byte-exact by default with `--residual-gate` opt-in.
- `hven-bench`, the private benchmark harness — brief §0, "hven grows one
  `docs/measurement.md` page the harness cites" (`docs/notes/2026-09-m7-brief.md:64`).
