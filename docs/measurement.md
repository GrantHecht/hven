# Measurement discipline

CLAUDE.md §7 (`CLAUDE.md:207-252`) is the rule; this page is how it is executed
here. Nothing on it is new: every rule is followed by where it is written, and
where two sources disagree that is said rather than smoothed over. Where the rule
and the tree disagree, the tree is what is written here — the convention
`docs/concurrency.md:12-14` sets for a contract page.

---

## 1. What is asserted and what is informational

**Counters and statuses are the currency.** CLAUDE.md §7's first line:
"Counters are the asserted currency of correctness and performance claims;
wall-clock timings are informational only." The M6 close gate states the
consequence plainly: "NOTHING IN THIS GATE ASSERTS WALL-CLOCK. Counters, statuses
and test counts are the asserted currency; every duration recorded here is
INFORMATIONAL" (`docs/notes/data/2026-09-m6-close-gate/PROVENANCE.txt:116-120`).
A quoted wall-clock number is single-threaded unless stated otherwise (§7).

**The residual class is the one gated exception.** Counters and statuses are
compared byte-for-byte. Floating-point measure columns are not process-invariant —
MKL's kernels are address-sensitive and addresses differ per process — so §7
exempts them from determinism and asserts cross-process agreement "by a calibrated
near-ulp gate rather than by byte equality — counters and statuses remain exact."
The gate's two numbers, with their derivations at source:

- **1e-5 relative** — `tests/sqp/test_corpus_cells.cpp:935`
  (`kResidualRelativeGate`), derived at `:908-934` as "~5.6x the largest spread
  on record, taken from the worst regime rather than the quietest because this
  pin runs wherever the suite runs, threaded and CI included", from four recorded
  readings (same box serial 1.0e-7; CI cross-runner 8.6e-7; CI path_neutral
  1.63e-6; MKL-threaded 1.79e-6).
- **A 1e-13 absolute floor**, where a purely relative test measures noise against
  noise — `:3618` and `:3809` (`kResidualAbsFloor`), reasoned at `:3607-3617`:
  residual norms "formed by cancellation from O(1) data", absolute accuracy a few
  ulps of 1.0 whatever the norm; worst observed drift 4.5e-15, so 1e-13 leaves
  22x head-room while staying six orders below the smallest genuine movement.

**Byte equality is checked first and is the common outcome; the gate engages only
on the digits that provably vary** (`:932-934`), which `docs/concurrency.md:80-91`
also records: "Byte equality is the observed common outcome; the gate is what the
pin *asserts*."

**The gated column class is twelve non-wall floats** of the corpus's 76-column
schema, enumerated by index at `tests/sqp/test_corpus_cells.cpp:938-951`
(`kkt_residual`, the four other `kkt_*` residuals, `dual_scale`, `x_scale`, the
two `ipqp_rho_demanded_*`, `ipqp_restart_shift_max`, `ipqp_alpha_p_min`,
`ipqp_alpha_d_min`). `wall_s` is a thirteenth float, excluded for a different
reason — "timing noise, never a regression contract".

**Both comparators default to byte-exact and open the gate only on request.**
`scripts/census_compare.py` compares the 13 asserted columns and excludes
`wall_s` — "this is a counter replay, not a timing measurement (CLAUDE.md
section 7)"; without `--residual-gate` it is "EXACT STRING EQUALITY on all 13
asserted columns". `scripts/compare_replay.py` is the whole-schema comparator:
"EXACT STRING EQUALITY on every column but `wall_s`" unless the same opt-in flag
is passed. Non-finite values are refused rather than compared.

**Absent is never zero.** An unobserved Apple/Accelerate value is `UNOBSERVED`
(CLAUDE.md §6), an unmeasured diagnostic is `NaN`, an engine-absent event field is
`std::optional` (`docs/notes/2026-09-m6-w5-t8-plan.md:20`).

**A pin is broken on purpose or not at all.** CLAUDE.md §7: "Intentional breaks
of a pinned/reproduced value are declared and re-derived explicitly — never
silent." The flag-unification event (phase C's U0) is the worked example of a
declared mass re-derivation rather than an exception to the rule (`:246-252`).

---

## 2. The serial rule: anything that asserts wall-clock

CLAUDE.md §7: "Concurrent MKL solvers spin-wait catastrophically against each
other, so any measurement that ASSERTS wall-clock — a benchmark, a timing
baseline, a throughput or build-time comparison — must serialize its suite: one
solve at a time, alone on the machine. **Nothing produced any other way is a
quotable timing.**"

The terms a serial leg declares, as the M4 task-7 wall leg declares them
(`docs/notes/data/2026-08-22-m4-task7-wall-leg/provenance_sqp.txt:1-15`):

| Term | The form it takes | Where |
|---|---|---|
| Toolchain, MKL, CPU, RAM, kernel, date | named literally | `:1-7` |
| The protocol | "3 cells, 7 reps, base/head alternated per rep, each run pinned to one core under MKL_NUM_THREADS=1" | `:9-11` |
| The commits | `BASE 5cd8ae9  HEAD b1b2c4d` | `:12` |
| **The binary identity** | `md5 hven_sqp_bench base 4a65c612… / head 9c021305…` | `:13-14` |
| **The box state** | "box at leg start: loadavg 0.49, nothing else compiling" | `:15` |
| The lock | "every build and every leg ran under flock /tmp/box-build.lock" | `:16` |

The same file's earlier leg adds the wait condition: the leg "holds flock
/tmp/box-build.lock and **waits for loadavg[1] < 0.6 before starting**" (`:31-32`).
**The counter-example, recorded rather than hidden.** The original SQP leg in
that directory is marked `PROVENANCE PARTIAL` (`:37-46`): "the two bench binaries
that produced those cells were NOT checksummed at the time, and they no longer
exist … only the binary identity is unverifiable." The precedent's closing rule:
"Every later leg in this directory records its binaries' checksums at run time."
A binary md5 plus the compiled-in stamp is part of a complete declaration.

Modern serial legs state the pin down to the SMT sibling: `taskset -c 2` on the
measured process, "its SMT sibling cpu10 is left idle and is MEASURED, not
assumed", with the driving shell masked off both cores so the scaffold cannot land
on the reserved one
(`docs/notes/data/2026-09-m6-w6-attached-cost/PROVENANCE.txt:94-121`).

---

## 3. Co-runs: the parallel-tiered protocol

**What may co-run.** CLAUDE.md §7: "A replay asserting only deterministic
per-process columns — counters, statuses, and residuals computed at a fixed
thread count — may co-run." The argument is the gate-B review request's, approved
by the owner 2026-08-14 and declared "because a protocol change that is not
declared is exactly the kind of silent break §7 forbids"
(`docs/notes/2026-08-14-m3-gate-b-review-request.md:250-254`). Its four points
(`:256-273`): the census asserts counters and excludes `wall_s` by construction;
counters are scheduling-invariant at `MKL_NUM_THREADS=1`; "contention biases in
one direction only" — a co-running cell can be pushed *toward* a budget/deadline
outcome, "it can never be pushed toward a false `Optimal` or a false counter
value"; and §7's serialization clause is about wall-clock validity.

**The fixed terms** (§7): one process per pinned **PHYSICAL** core with SMT
siblings left idle, `MKL_NUM_THREADS=1` in every process, and wall-clock from such
a run "stays informational — it is never quoted as a measurement and never compared
against a serial timing." Wall-clock-**dependent** statuses — deadline-truncated
outcomes — are not scheduling-invariant and "must run solo, or co-run only with
their full budget honored."

**The tiering, assigned from the FROZEN baseline** so the whole schedule is fixed
before a single solve starts and cannot be tuned by what the fresh run does
(`docs/notes/2026-08-14-m3-gate-b-review-request.md:277-289`):

| Tier | Rule | Width |
|---|---|---|
| **T1** | everything else: solved, comfortably sub-deadline | 6-wide, core-pinned |
| **T2** | wall-sensitive: any terminating baseline cell with `wall_s ≥ 1800 s`, plus `f7_n10000_path_physics` | **solo** |
| **T3** | deep-DNF: every other baseline `dnf_*` cell, at its full per-phase budget | 5-wide |

Every cell process is "`MKL_NUM_THREADS=1` and `taskset`-pinned to one logical CPU
of a **distinct physical core** — SMT siblings are never both used" (`:278-280`).
Order is T1 → T2 → T3, tier 2 starting only after every tier-1 worker is reaped.
The runner is `scripts/run_walk_census.sh` (`PROTOCOL_VERSION="parallel-tiered v1"`, `:127`).

**The declaration goes in the artifact.** CLAUDE.md §7: "A co-run protocol is
declared in the evidence artifact it produces — the tiering rule, the widths, and
the pinning." The census writes it into the merged CSV's own provenance header
(`docs/notes/data/2026-08-21-m3-gate-c/provenance_header.txt:1-15`):

```
# census.protocol: parallel-tiered v1 (T1 6-wide pinned / T2 solo / T3 5-wide full-budget)
# census.binary_stamp: binary: fb45a000c095 / schema: 37 / budget_table_hash: 0x357aee91dee27391
# census.topology: 8 physical cores; representatives (one sibling each): 0,1,2,3,4,5,6,7
# census.tier assignment (from frozen baseline …/2026-08-16-u0-corpus/walk_baseline.csv)
# census.  counts: T1=44  T2=3  T3=10  (total 57)
```

The current prose form is the M6 close gate's "THE CENSUS'S CO-RUN PROTOCOL,
DECLARED (CLAUDE.md section 7)" block
(`docs/notes/data/2026-09-m6-close-gate/PROVENANCE.txt:122-147`), which adds the
comparator and its setting, the read-only baseline, the tier windows as run, and
"load average equal to the live tier's width at every observation" — the pinning
claim checked rather than asserted.

**The serial-confirm rule.** CLAUDE.md §7: "A deviation observed under a co-run
is a candidate, not a regression. Re-run the deviating cell ALONE, under the
serial rule above, and reproduce the deviation before calling it one." The runner
mechanizes it: a disagreeing cell "is written to serial_confirm_list.txt and is
NOT a regression until it has been re-run ALONE and still disagrees"
(`scripts/run_walk_census.sh:40-44`).

### 3.1 The per-leg concurrency declaration

When counter-only legs run while an **uncontrolled foreign lane** holds the box,
the declaration is per leg, not per directory. The worked example is M7 W0 T3
(`docs/notes/data/2026-09-m7-w0-t3-elastic-rung/PROVENANCE.txt:64,68-104`), whose
header field reads `concurrency  MIXED, and the split is per leg` and which opens
by naming its own earlier error: "An earlier version of this file said 'SOLO. Every
leg ran alone … This is NOT a co-run' of the whole directory. That was true of the
legs whose numbers are QUOTED and false of three later diagnostic legs, and §7
requires the co-run to be declared in the artifact it produces rather than only in
a report." The shape it corrected to, both halves named by file:

- **SOLO** — alone under `flock /tmp/box-build.lock`, `taskset -c 2`,
  `MKL_NUM_THREADS=1`, `OMP_NUM_THREADS=1`, empty pre-leg `pgrep`, each leg listed
  by its raw log, closed by the sentence that carries the declaration: "EVERY
  WALL-CLOCK AND INSTRUCTION NUMBER IN THIS DIRECTORY COMES FROM THAT SET"
  (`:75-85`).
- **CO-RUN** — the four legs that ran while "the hven-bench harness lane was
  running its own cells on this box, outside the box lock and outside this
  implementer's control", each named, with the terms exact: one MKL thread and
  `taskset -c 2` on **this** process only, "the foreign lane's placement was NOT
  controlled, so this is not the 'one process per pinned PHYSICAL core' discipline
  §7 defines for a measured co-run and nothing here is offered as one"; counters,
  statuses and a classification are what is read; "NO WALL NUMBER AND NO
  INSTRUCTION COUNT FROM THESE FOUR LEGS IS QUOTED ANYWHERE"; and the closing
  check — "every counter column these legs print is byte-identical to the SOLO
  measurement's" (`:87-104`).

**A blanket "SOLO" over a directory containing any co-run leg is the failure
mode**, and it is the one that actually occurred. An uncontrolled co-run is not
§7's pinned one-process-per-physical-core discipline and is never dressed as one:
it is declared for what it is, restricted to counters, and shown equal to a solo
run on them.

---

## 4. The instruction-count protocol

Since W5 T8.9r a claim about *work* is made in instructions, not seconds.
Instructions are scheduling-invariant; wall is carried beside them and stays
informational (`docs/notes/data/2026-09-m6-w6-attached-cost/PROVENANCE.txt:123-129`).

**The instrument** (`:131-135`), the ownership doc's pass A unchanged:

```
perf stat -e instructions:u,branches:u,cycles:u,branch-misses:u,L1-icache-load-misses:u
```

Everything is `:u` (user space) because `perf_event_paranoid = 2` on this box
(`:91-92`) — an environmental fact, stated, not worked around.

**The shape.** "Three rounds; the arms are ALTERNATED WITHIN a round" (`:135`),
so an A and a B reading of the same cell are adjacent in time. Each cell runs as
**one process** (`--internal-run-one <cell> --engine <mode>`) rather than through
the forking leg, because "a `perf stat` over the leg would count 27 forks, 27
execs and a parent poll loop beside the solve" (`:207-214`). Ratios are
**per-cell medians**, and the corpus figure is computed over them — "the per-cell
medians, never a mean of ratios"
(`docs/notes/data/2026-09-m6-w5-t8-runtime/PROVENANCE.txt:184`).

**The untimed warm-up.** A batch's first process is an allocator/page-cache
outlier — one batch read +2.1 % instructions in round 1 over its own rounds 2 and
3 while all six processes wrote byte-identical rows. The median of three already
discards it; the re-take additionally runs "ONE UNTIMED, UNRECORDED warm-up
process so that no round is the batch's first", after which each arm is
internally stable to 0.27 % and 0.04 % (`:230-241`). Neither batch is discarded;
both are retained and both are read.

**The band, FLAT** (`docs/notes/2026-09-m6-w5-t6-ownership.md:1248-1249`):

> **FLAT** — per-cell median ratio in **0.99–1.01** AND whole-corpus within
> **±0.5 %**, in all three modes, on BOTH legs.

Anything else is reported as MOVED with its top cells. Both bars are required: a
corpus ratio inside ±0.5 % with cells outside 0.99–1.01 is not FLAT, and the T6
close reading records exactly that case (`ownership.md:1751`). The "on BOTH legs"
clause belongs to a commit-pair reading; a leg with one instrument and no pair
reproduces the band without it, and says so — §11.1's veto machinery "is a
COMMIT-PAIR rule and does not apply" (`:137-140`).

**The idle proof, computed not assumed.** A batch is one lock acquisition and is
the unit the idle proof covers; the pinned core's SMT sibling is measured per
timed run from `/proc/stat` by `idle_proof.py` (the tool of record is
`docs/notes/data/2026-09-m6-w5-t8-runtime/scripts/idle_proof.py`), copied between
artifacts byte-identically with its recorded sha256 (`:243-250`). A batch whose alternation
snapshots are incomplete is marked `UNPROVEN-EVIDENCE` and every wall number taken
from it is marked NON-QUOTABLE rather than tidied away (`:403-431`).

**The counter check is the STOP condition.** Every column of every arm's CSV is
compared with `scripts/compare_replay.py`; if instructions move and a counter
moves too, this is not an instruction measurement of the same computation and the
leg stops. The attached-cost leg records its check as "43 cells x 30 columns, 0
differences, every pair" (`:232-236`).

---

## 5. The census cost policy

**The price is ~6 h.** `docs/notes/2026-08-15-m3-phase-c-plan.md:1512` (row O3):
"P-CENSUS is ~6 h. Running it on all six relocation batches costs ~36 h for
commits that, by construction, cannot change codegen."

**The ruling.** P-SUITE + P-SYM per relocation batch; P-CENSUS on every content
change and at the gate. The argument: over the object set P-SYM compares — which
includes the census binary `hven_sqp_corpus` and the bench binaries — per-symbol
disassembly identity is "strictly stronger than a census for the Linux/MKL build …
in one build instead of six hours", with two bounded limits stated rather than
implied (the Accelerate `#ifdef` arms are not compiled on Linux; P-SYM is
Release-only, Debug correctness being P-SUITE's).

**The tripwire is part of the rule, not advice** (same row): "any non-`__FILE__`
P-SYM difference ⇒ the batch is not relocation-only and P-CENSUS becomes mandatory
for it."

**What P-SYM draws** (`scripts/psym_compare.sh:5-12`): a relocation moves files, so
object bytes are not expected to be identical — `__FILE__` and `__LINE__` both
reach the object — and "what must be identical is the INSTRUCTIONS, per symbol.
This script draws that line in one place so every batch draws it the same way,
instead of each agent re-inventing a comparison and then widening it by hand when
the first legitimate difference appears." Exceptions are per-`<object>::<symbol>`
with a stated reason; the object qualifier is mandatory.

**The frequency is fixed and closed.** The 2026-08-15 amendment in the same row
quotes its operative sentence verbatim: "Gate C (and every future gate) runs the
full 57; the post-H3 counter census is the ONE intermediate census; **any further
scope or frequency reduction is pre-refused** — a proposal to run Gate C itself
reduced would break ratified gate currency and does not need to be asked."

**The current instance** is the M6 close gate: the full 57-cell walk census, "all
57 cells, the 13 the M5 close discharged INCLUDED"
(`docs/notes/data/2026-09-m6-close-gate/PROVENANCE.txt:5-11`), one detached leg
17:17:13Z → 23:04:47Z, "57 baseline cells, 57 fresh cells, 0 mismatches" with an
empty `serial_confirm_list.txt` — no cell became a candidate, so no solo re-run was
owed (`:142-153`).

---

## 6. The box protocol

One box, several lanes; these rules are what keeps a measurement on it meaningful.

- **Builds are serialized against source edits.** CLAUDE.md §8 (`:258-261`): "do
  not edit source files while a build of that source is in flight, and do not
  start a build while another agent's edits to the same tree are in progress."
- **Every build and every leg runs under `flock /tmp/box-build.lock`** —
  foreground where it can be, with the markers written **inside** the lock:
  `PGREP_EMPTY`, `FOREGROUND_START`, `FOREGROUND_END`
  (`docs/notes/2026-09-m6-w5-t8-plan.md:21`).
- **The `pgrep` goes in its own command**, before the lock and again inside it,
  and lands verbatim in the retained log:
  `pgrep -af 'cmake|ninja|ctest|hven_|bench_corpus'`
  (`docs/notes/data/2026-09-m6-w6-attached-cost/PROVENANCE.txt:95-101`).
- **Every wrapper is fold-proven.** `set -o pipefail`; the leg's exit status is
  the worst status any step returned and a later success never clears it; the
  fold is proven against *stubbed failures inside the lock* and the proof is
  retained (`…/2026-09-m6-w6-attached-cost/PROVENANCE.txt:112-116`).
- **Never kill or signal a process another lane started.** Stop only by the pid
  your own launcher's log names — never `pgrep -f | kill`. Flock-waiters are idle
  and are not a problem to solve.
- **Long legs detached and ALONE, pid retained.** The one >600 s Debug test
  (`Continuation.ProbeBudgetBoundsAFailingProposal`) runs under `nohup`, alone, its
  pid in the log (`docs/notes/2026-09-m6-w5-t8-plan.md:21`;
  `docs/notes/data/2026-09-m7-w0-t4-reseed/raw/gates/README.md`). Background chains
  are not reliable for legs; a foreground leg stays under 600 s.
- **Scratch under a git-ignored parent**, never the repo root and never `/tmp`
  except the lock itself.

**What the collisions cost, from the M6 ledger.** A box-sharing collision during
a task replay — "tycho's flock-queued build terminated twice, earlyoom took one
tycho clang under hven's `ctest -j4`" — "produced the announce-before-every-solo-
leg protocol now in force" (`docs/notes/2026-08-m6-ledger.md:1489-1490`). A
second: "the paused tycho lane's gate script was killed but its ninja ran on
orphaned, holding the lock fd for 2 h 11 min against hven's queued verify leg; the
protocol now requires verifying a 'lock released' claim with `fuser` and killing
the process GROUP when stopping a flock'd gate" (`:1634-1637`, restated `:2350-2353`).
And `:3441-3442`: legs "queued on `flock` behind the lane twice, nothing killed" —
the correct outcome — with "background chains are not reliable for legs; foreground
under 600 s adopted".


---

## 7. The provenance stamp

CLAUDE.md §7: "Every benchmark artifact carries a provenance stamp (toolchain,
hardware, date, commit)." What the corpus binary writes into every CSV it produces
(`bench/bench_corpus.cpp:868-878`):

```
# hven_sqp_corpus provenance
# binary: <git describe --always --dirty --abbrev=12>
# schema: <kIpqpColumns>
# budget_table_hash: <budget_table_hash()>
# invocation: <argv, joined>
# MKL_NUM_THREADS: <env or "<unset>">
# host: <hostname>
# generated: <UTC timestamp>
```

The lever state is stamped unconditionally beside it, on the principle that an
UNATTACHED arm is one half of a comparison and must say so
(`bench_corpus.cpp:879-882`).

**The stamp is resolved at CONFIGURE time, and that is the trap.**
`bench/CMakeLists.txt:81-90`: the string is resolved "at CONFIGURE time (not
build time) because that is when CMake can shell out"; `--dirty` is "deliberately
included: a baseline produced from an uncommitted tree must say so"; and then, in
the file's own words, "THE TRAP THIS CARRIES, and it bit once (T10b fix round 1):
a tree configured while clean and REBUILT after an edit emits the stale, clean
stamp. Re-configure before producing a measurement CSV."

**It has since bitten twice more, in the two directions the trap has.** Both are
in this milestone's W0, and both are recorded in the artifact rather than in a
report:

1. **A `-dirty` stamp from tracked artifact PROSE under edit** — T3's Debug leg,
   stamped `40ef105f-dirty`. The retained timeline: the commit landed 21:33:39; the
   Debug build and four Debug suites ran 21:41:34–21:42:21 on a clean tree;
   `PROVENANCE.txt` itself, "a TRACKED artifact file", was rewritten at 21:42:27;
   the Debug vehicle arm started 29 seconds later and stamped itself `-dirty` "for
   exactly that reason. No source file, header, test or build input was modified in
   that window … the dirt was artifact PROSE." The leg was **re-run on a clean
   tree**, the dirty log kept beside it as `raw/debug-both-states.dirty.log` rather
   than deleted
   (`docs/notes/data/2026-09-m7-w0-t3-elastic-rung/PROVENANCE.txt:132-153`).
2. **A stale `-dirty` stamp from a tree that had moved since configure** — T4's
   replay arm produced CSVs stamped `# binary: 0be12ec390c5-dirty`, "which is NOT
   the tree they were produced from": the code was the clean `39746705` the gate
   had just rebuilt, the string was the build directory's configure-time one. The
   CMake comment's remedy was applied — re-configure first, re-take — and both arms
   are kept: "THE RE-TAKEN ARM IS THE AUTHORITY; the stale-stamped one is retained
   rather than deleted because its numbers are what the gate leg actually saw"
   (`docs/notes/data/2026-09-m7-w0-t4-reseed/PROVENANCE.txt:112-126`;
   `raw/gates/README.md`, the `replay-restamp.log` row, 27 / 75 / 0 ×3).

**The rules that follow, and they are not optional:**

- **Every artifact edit lands BEFORE a stamped leg, or the leg is re-run clean.**
  Editing a tracked file — including the artifact's own prose — while a stamped
  leg runs makes the stamp `-dirty`, and a `-dirty` stamp does not name a commit.
- **Reconfigure after every commit**, before producing a measurement CSV.
- **A `-dirty` stamp is never quoted.** The census runner refuses to start a
  multi-hour sweep behind one: "A stamp carrying `-dirty` does not name a real
  commit and fails the gate's provenance requirement", with the preflight command
  spelled out (`scripts/run_walk_census.sh:51-61`). The M6 close gate ran that
  preflight before its sweep and recorded the clean reading
  (`…/2026-09-m6-close-gate/PROVENANCE.txt:51-56`, `:147-150`).
- **A stamp naming a different commit than the prose expects is reconciled, not
  edited.** The M6 close gate's stamp read `15f97cae51ff` where the brief asked for
  `d25a4a6cb3db`; the reconciliation is the docs-only diff between them, under the
  W5 precedent "the stamp names the checked-out commit and is not edited to match
  prose" (`…/2026-09-m6-close-gate/PROVENANCE.txt:38-58`).
- **A leg that cannot run at a clean tree says so and why.** The W6 T6 leg was
  deliberately dirty by exactly two files because those files *were* the
  instrument; the CSV carries `b0a7ffaa4342-dirty` and the PROVENANCE says so
  (`…/2026-09-m6-w6-attached-cost/PROVENANCE.txt:22-33`).

**A full artifact stamp** carries, beyond the CSV header: compiler, CMake, Ninja
and perf versions; MKL path and version; kernel, host, CPU with its
physical/logical split and the pinned core's SMT sibling named, memory, governor
and boost state; run date in UTC; the build directory and whether it was empty at
configure; the effective compile flags **read from the build's own
`compile_commands.json` rather than from the configuration**; and the binaries'
checksums (`…/2026-09-m6-w6-attached-cost/PROVENANCE.txt:37-92`).

**The harness.** `hven-bench` (the private sibling repo) carries the same block
plus its own sha and the hven sha it pins — brief §0, "every artifact stamped
(toolchain, hardware, date, hven sha + harness sha)"
(`docs/notes/2026-09-m7-brief.md:60-64`). It consumes hven through
`find_package(hven)` against an install prefix built from a named sha, so the hven
pin in its stamp is the prefix's provenance, not a branch name.

---

## 8. Apple/Accelerate and Windows

**Never fabricate a value.** CLAUDE.md §6: "A value that has not been observed on
real Mac hardware is recorded as `UNOBSERVED`, never zero-filled or interpolated.
Absent evidence is reported as absent." Windows has the same standing
(`docs/concurrency.md:294-295`).

**What a green macOS CI lane does and does not license** (`docs/ci.md:498-509`):
"A green macOS lane licenses a per-backend *counter* arm and does not license a
per-backend *float* arm." Each backend asserts its own observed value — "the MKL
side is never relaxed and the pair is never replaced by a range that admits both".
The CI-side half: a run on `github-macos26-arm64` is a real observation, a counter
that reproduces across runs is committable, and a float needs **two-run byte
agreement** because nothing on that lane pins the reduction order.

An evidence directory states its Apple standing explicitly rather than by
omission — "APPLE / WINDOWS: UNOBSERVED. Nothing in this directory was taken on Mac
or Windows hardware and no value here is interpolated to them"
(`…/2026-09-m6-w6-attached-cost/PROVENANCE.txt:88-92`).

---

## 9. Who cites this page

- `docs/concurrency.md` — "Wall-clock under contention … CLAUDE.md §7 makes a
  co-run's wall-clock informational and never quotable against a serial timing"
  (`:296-301`), and the residual-gate note at `:80-91`.
- `scripts/run_walk_census.sh` (`:2`, `:36-61`) — where the co-run declaration and
  the clean-stamp preflight are enforced; `scripts/census_compare.py` and
  `scripts/compare_replay.py`, byte-exact by default, `--residual-gate` opt-in.
- `hven-bench`, the private benchmark harness, which is why this exists as a page:
  brief §0, "hven grows one `docs/measurement.md` page the harness cites"
  (`docs/notes/2026-09-m7-brief.md:64`).
