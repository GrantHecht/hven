# M7 W2 M3c — the DEFAULT-PATH failure census: what is here, and what this round cost to learn

The task's question is `tables/census.md`'s: **what actually kills each failing
`hven-sqp` CUTEst TRAIN cell at DEFAULT options?** This file is the map of the
directory and the record of the PROCESS findings — the instrument defects this
round found, three of them by a refusal rather than by a reading, and what each
one changes about how the next measurement is built.

Nothing here licenses a behaviour change, a default change, or a line of code.
No file under `src/` or `include/` was touched by any of it.

---

## The rules, and the order they were committed in

Each was committed **before** the legs it governs, and **none is ever edited
after its legs** — a pre-registered rule's bytes are the pin.

| file | governs | committed |
|------|---------|-----------|
| `RULE.md` | PASS 1: the public-surface + ledger census of all twenty failing cells, its taxonomy and its identity requirement | before any leg |
| `RULE-P2.md` | PASS 2: the per-minor walk trace at DEFAULT options — the mapping, the selection, the identity gate, the three answers | `176da6cf`, before any pass-2 leg, the smoke included |
| `RULE-P2A.md` | the discriminating experiment that decides whether pass 2's INSTRUMENT is sound | `d190df90`, amended `c456331d`, both before any P2A leg |

## What happened, in order

1. **PASS 1 closed.** Twelve identity-comparable cells reproduce the harness's
   committed rows at the pin on all 24 fields (`logs/identity-EI.txt`), and
   `tables/census.md` is the deliverable. Four defects OF `RULE.md` are recorded
   in that table rather than fixed in the rule.
2. **PASS 2 STOPPED, all ten cells.** `RULE-P2.md` §5.1's gate refused every
   traced cell: nine differ on integer counters, `ACOPP57` only in the last
   digits of its residuals (`tables/mapping-p2.txt`).
3. **The offline forensics** (`logs/why-p2-differs.txt`) located the cause in the
   pass-2 BINARY — bit-identical across processes, while the cells themselves are
   reproducible across processes AND binaries — but could not say whether the
   flag regime or `HVEN_TESTING` moved the numbers.
4. **P2A answered it.** Three arms over two builds, both built by an exact replay
   of the install build's own compile commands; six legs; all three arms
   reproduce the committed pass-1 rows on **every** compared field with **zero**
   near-ulp acceptances (`logs/gate-p2a.txt`). RULE-P2A §6's OUTCOME 1.
5. **PASS 2B: the re-take** of the ten legs under arm C's build, on `RULE-P2.md`
   §3/§4's unchanged terms, with P2A's gate applied per leg
   (`wrappers/chain_p2b.sh`). **REPRODUCES 10 / 10**, DIFFERS 0, CENSORED 0,
   near-ulp acceptances 0 (`logs/gate-p2b.txt`).
6. **PASS 2 COMPLETED on those traces.** `RULE-P2.md` §2's mapping is UNIQUE on
   all ten cells and §5.3's two conservation sums have difference 0 on all ten
   (`tables/mapping-p2b.txt`); §3 stage 2 selected 21 solves mechanically and
   their slices are in `traces-p2b/`; §6's read-outs are machine-written in
   `tables/readouts-p2b.md` and §6.1's three-way answer, per traced solve, is
   in `verdict-p2.txt`. **19 `CYCLE`, 1 `LONG WALK`, 1 `SOMETHING ELSE`.**

---

## The process findings

### F1 — The pass-2 probe was built by a DIFFERENT COMPILER, and nothing said so

Pass 1's install build was configured with `-DCMAKE_CXX_COMPILER=/usr/bin/clang++`
(`wrappers/chain_census.sh`). `wrappers/prepare_p2.sh` passed no such flag, so the
pass-2 probe configure took CMake's default C++ compiler on this host,
`/usr/bin/c++`. Every flag the forensics listed as "missing" from the pass-2
command line follows from that single fact:

* `-mllvm -inline-threshold=225` and `-fopenmp=libiomp5` sit behind
  `$<$<STREQUAL:${CMAKE_CXX_COMPILER_ID},Clang>:…>` generator expressions
  (`cmake/hven_compile_options.cmake:233`, `CMakeLists.txt:173`) and evaluate to
  nothing when the consumer is not Clang — which is why a plain `-fopenmp`
  appeared in their place;
* `-Xclang -fno-pch-timestamp` is Clang-gated in `src/CMakeLists.txt`.

Under `-ffast-math` a different compiler is licence to contract and reassociate
differently in every hot loop. **A probe that recompiles a library's sources
pins the compiler, or it is not that library.**

### F2 — A flag harvest by TARGET PROPERTY cannot reach a build's flags

`wrappers/probe-p2-CMakeLists.txt` harvested hven's settings the careful way —
reading the `hven` target's `COMPILE_OPTIONS`, `COMPILE_DEFINITIONS`,
`INTERFACE_*` and hven's directory-scope `CMAKE_CXX_FLAGS` — precisely so that
nothing would be retyped and drift. It still could not see:

* `-Wabsolute-value`, added by `add_compile_options()` in hven's own DIRECTORY
  scope (`cmake/hven_compile_options.cmake:434`) — a directory property, not a
  target property;
* `-fPIC`, from `set(CMAKE_POSITION_INDEPENDENT_CODE ON)` — a target PROPERTY,
  not a compile option;
* anything behind a compiler-id generator expression, which evaluates against the
  CONSUMER's compiler.

**The finding, general:** a target-property harvest reconstructs a command line;
it does not reproduce one. When a measurement needs the SAME code, **replay the
build's own compile commands** — `wrappers/replay_build_p2a.py` reads the install
build's Ninja files, expands each edge against its rule exactly as Ninja does
(the ccache `${LAUNCHER}` included), and runs the string through `/bin/sh` in a
directory that mirrors the install tree's relative object layout, so the command
STRING is identical, output path included, rather than merely equivalent.

Its proof is in `logs/harvest-diff-p2a.txt`, and it is the strongest form
available: **arm A's replayed archive sha256 IS the installed `libhven.a`'s**
(`18a71b727533e9231fbdeb4e6bf0c9bb2d77aad6ab886a0f2e5729f1169eee10`), all 42 of
its objects byte-identical to the install build's own, its PCH byte-identical
too. Arm A's hven code is not LIKE the library pass 1 linked; it IS it.

### F3 — A gate that chooses its fields can only find what it chose to look at

Pass 2's smoke (`logs/smoke-p2.txt`) checked **eight integer counters on one
cell** and reported the seam neutral. `f`, `stationarity` and `feasibility_e`
already differed from the pass-1 row **in that same transcript**, by 5.8e-11,
6.8e-11 and 6.9e-15 — the evidence that the binary had moved was on the page, and
the smoke was not asked the question that would have surfaced it. A ten-cell chain later,
`RULE-P2.md` §5.1's full gate found it.

`wrappers/gate_p2a.py` cannot repeat that. **It does not choose its fields:** it
takes the INTERSECTION of the two rows' own `key=value` sets — 58 fields on these
rows, chosen by neither the rule's author nor the reader — and compares every
member. A field's class is decided by its value TEXT, not by a hand-kept list;
byte-class fields are byte-equal or the leg fails, with no tolerance of any kind;
residual-class fields carry `RULE-P2.md` §5.1's own constant
`|a−b| ≤ 1e-12·max(1,|a|,|b|)`, unchanged, because a bar loosened after seeing
what it rejected is not a bar. A field one row does not print is reported ABSENT
and is **never** counted as agreement. Ten folds on constructed inputs run before
any leg is read — including the exact 6.8e-11 case the smoke missed — and the
gate exits non-zero if any is red.

### F4 — The 43rd compile edge: a precompiled header, found by a refusal

The replay REFUSED on its first run: 43 `CXX_COMPILER__hven` edges against 42
archive members. The forty-third is `cmake_pch.hxx.pch` —
`src/CMakeLists.txt:332` gives the `hven` target a precompiled header with a
per-source opt-in list — and **four TUs read it by ABSOLUTE path into the install
build tree**: `src/drivers/ipm_solver.cpp`, `src/drivers/ipm_options.cpp`,
`src/model/non_linear_program.cpp` and `src/model/nlp_adapter.cpp`, the last two
on the SQP path. A replay that had silently dropped the unmatched edge would have
built four TUs against a header state it did not control.

`RULE-P2A.md` §3.3a (AMENDMENT 1, committed before any leg) records the handling:
arm A replays all 43 commands unchanged and reads the install's own `.pch`; arm B
rebuilds the PCH from the same command plus the same single define — clang
rejects a TU whose macro state differs from the PCH it includes — and re-points
those four commands' one path token, which is the second and only other declared
difference on arms B/C.

### F5 — Two smaller instrument defects, both found by a failure rather than a reading

* The replay's Ninja reader stripped a variable's TRAILING whitespace. CMake
  relies on `LAUNCHER = /usr/bin/ccache ` keeping its space to separate the
  launcher from the compiler in `${LAUNCHER}${CODE_CHECK}/usr/bin/clang++`; the
  first build died with `/usr/bin/ccache/usr/bin/clang++: Not a directory`.
* Pass 2's first launch carried a PROSE third field in its leg list and
  `for spec in $LEGS` word-split it, so every word became a leg refused in
  milliseconds, each printing a `CENSORED <word>` line a reader could mistake for
  a real censored cell. No cell was affected and no row was wrong; the run was
  stopped and relaunched, a malformed token is now REFUSED rather than handed to
  the probe, and the aborted log is kept.

---

## What the seam is now proven to be, and the SCOPE of that claim

M3b's claim was about the FIRST QP at `tr_init = +inf`, a configuration hven does
not ship. P2A extends it:

> On `ACOPP57` and `DALLASM`, at DEFAULT options, over the WHOLE run, the walk
> test seam changes nothing — neither compiled in (arm B) nor ARMED at
> `--capture-solve -1` and recording every minor of every `QpEngine::run` on the
> thread (arm C). Each arm reproduces the committed pass-1 row on all 58 shared
> fields with **byte** equality; near-ulp acceptances: **0**.

The scope, stated so it is not over-read: **two cells, one host, one build of
each arm, at DEFAULT options.** Between them the two cells exercise the main
walk, the restoration sub-driver and a lazy engine (`DALLASM`'s pass-1 row has one
restoration entry and one elastic activation), and both are `QP_CAP_BUDGET`
cells. It is not a claim about every cell: pass 2B applies the same gate to each
of the ten re-taken legs, and a cell that differs is a STOP for that cell,
reported, never relabelled.

What makes the arm-A comparison worth anything is F2's identity: arm A's hven
objects ARE the install's, so "arm B reproduces arm A" is a statement about the
define and nothing else.

---

## The files

```
RULE.md          RULE-P2.md      RULE-P2A.md     the three pre-registered rules; never edited after their legs
tables/census.md                 THE DELIVERABLE: pass 1's table, plus pass 2's section appended to it
tables/mapping-p2.txt            PASS 2's mapping, gate and (not-run) selection -- the ten STOPPED cells
tables/mapping-p2b.txt           PASS 2B's mapping, gate, conservation sums and the SELECTION that was run
tables/readouts-p2b.md           RULE-P2 section 6's nine read-outs, per traced solve, machine-written
verdict-p2.txt                   RULE-P2 section 6.1's three-way answer, per traced solve
PROVENANCE.txt                   the binaries, the build, the host and the terms every leg ran under
rows/                            PASS 1's fifteen committed rows: the bytes every later gate compares against
rows-p2/                         PASS 2's ten rows + its smoke: what the differently-built binary produced
rows-p2a/                        P2A's six rows, three arms x two cells
rows-p2b/                        PASS 2B's ten rows: the ones the read-outs' traces belong to
traces-p2/                       PASS 2's trace manifest (its slices were never cut: the cells STOPPED)
traces-p2b/                      PASS 2B's 21 committed per-solve slices + the sha256 of their ten parents
logs/why-p2-differs.txt          the offline forensics that located the cause in the binary
logs/harvest-diff-p2a.txt        the replay's per-TU command-line proof and its structural checks
logs/gate-p2a.txt                P2A's identity gate, its folds, and RULE-P2A section 6's outcome
logs/gate-p2b.txt                the SAME gate over the ten re-taken legs: REPRODUCES 10 / 10
logs/chain-p2a.log               P2A's six legs
logs/chain-p2b.log               the re-take's ten legs
logs/binaries-p2a.txt            the two arm binaries' sha256
logs/fold-map-p2b.txt            map_p2.py's 38 folds, re-run after the re-pointing, byte-identical
logs/fold-readouts-p2b.txt       readouts_p2b.py's folds: all THREE of section 6.1's answers proved reachable
wrappers/                        every probe, chain, classifier, mapper, reader and gate this task ran
```

**The ten FULL flat traces are not committed** -- 217 MB, and `RULE-P2.md` §3
said so in advance. They are at `.scratch/m7w2m3c/p2b/traces/<CELL>-all.csv` on
the box that produced them, and `traces-p2b/SHA256-all-traces-p2b.txt` records
each one's sha256 and size, so any committed slice can be checked against the
bytes it was cut from.

**Not read, and deliberately so:** PASS 2's full traces
(`.scratch/m7w2m3c/p2/traces/`) and `wrappers/readouts_p2.py`, the read-out
script an earlier round wrote for them. `RULE-P2.md` §5.1 stopped those cells,
and a stopped cell's traces are not traces of the pass-1 row. They are left in
place as evidence of what the differently-built binary did, and
`readouts_p2.py` was never run on them; the read-outs this census reports come
from `wrappers/readouts_p2b.py` over the RE-TAKEN traces, whose rows reproduce
the pass-1 rows byte for byte. P2A's own arm-C traces
(`.scratch/m7w2m3c/p2a/traces/`) are an instrument check, not the census's
evidence, and are not read either.

---

## What the census ANSWERS, in plain language

Two passes, twenty cells, 21 traced solves. The short version: **at the options hven actually ships, the SQP lane's failing cells die inside the QP walk, and the walk is cycling.**

| cell | what kills it at DEFAULT options (pass 1, OBSERVED) | are its capped QPs M3b's cycle? (pass 2) |
|---|---|---|
| `ACOPP57` | 3 steps accepted, then 4 QP solves in a row spend their full 1 795-minor cap and the driver's 3-failure budget ends the solve; never restores | **YES** — 2 of 2 traced solves are `CYCLE`, period 2, tail 98–99% of the passes, Σ|Δx|∞ exactly 0. |
| `DALLASM` | restores once (sub-status optimal) and activates the elastic tier once; capped 2 150- and 1 960-minor solves spend the budget | **YES** — 2 of 2 traced solves are `CYCLE`, period 2, tail 93–94% of the passes, Σ|Δx|∞ exactly 0. |
| `STEENBRB` | restores from h≈2.9e3 to h≈0 and lands f at 4.09e12; the QPs after that excursion are capped and spend the budget | **SPLIT.** Its FIRST capped solve is a `LONG WALK` that ends `numerical_error`; its LAST is a period-2 `CYCLE`. |
| `ACOPP118` | 3 steps accepted, then 4 capped 4 710-minor solves and the budget; never restores | **YES** — 2 of 2 traced solves are `CYCLE`, period 2, tail 98% of the passes, Σ|Δx|∞ exactly 0. |
| `ACOPR57` | 8 steps accepted, then capped 2 080-minor solves and the budget; never restores | **SPLIT.** Its FIRST capped solve is a period-2 `CYCLE`; its LAST is an exact period-6 cycle that MOVES and returns — `SOMETHING ELSE` under the rule, a stall in practice. |
| `STEENBRC` | restores and lands f at 1.83e14; capped 5 400-minor solves spend the budget | **YES** — 2 of 2 traced solves are `CYCLE`, period 2/4, tail 85–97% of the passes, Σ|Δx|∞ exactly 0. |
| `STEENBRG` | restores and lands f at 1.83e14; capped 5 400-minor solves spend the budget | **YES** — 2 of 2 traced solves are `CYCLE`, period 2/4, tail 85–97% of the passes, Σ|Δx|∞ exactly 0. |
| `YORKNET` | one capped 5 080-minor main-engine solve, 19 elastic activations, then restoration runs 18 iterations and returns `infeasible` with h unchanged bit for bit | **YES** — 2 of 2 traced solves are `CYCLE`, period 2/4, tail 89–100% of the passes, Σ|Δx|∞ exactly 0. One of them (solve 145) is a NOT-LEDGERED solve — the restoration sub-driver's or a lazy engine's, which the public ledger never sees. |
| `SAWPATH` | the 300 s wall, inside ONE major that has already spent 77 037 minors; its restoration sub-solve ends `numerical_error` | **YES** — 2 of 2 traced solves are `CYCLE`, period 2, tail 91–97% of the passes, Σ|Δx|∞ exactly 0. One of them (solve 10) is a NOT-LEDGERED solve — the restoration sub-driver's or a lazy engine's, which the public ledger never sees. |
| `BRITGAS` | the budget is spent over majors 0–2, then major 3's subproblem returns `infeasible`, the elastic ladder runs and returns unusable, and the RESTORATION exit ends the solve `numerical_error` (pass-1 finding R2: the class name says budget, the mechanism is the restoration exit) | **YES** — 3 of 3 traced solves are `CYCLE`, period 2, tail 82–95% of the passes, Σ|Δx|∞ exactly 0. One of them (solve 11) is a NOT-LEDGERED solve — the restoration sub-driver's or a lazy engine's, which the public ledger never sees. |

**OBSERVED vs INFERRED.** Everything in the middle column is OBSERVED, from rows that reproduce the harness's committed rows at the pin on all 24 compared fields. Everything in the right column is OBSERVED too, from traces whose own rows reproduce those same pass-1 rows BYTE for byte on all 58 shared fields. What is INFERRED is named as such wherever it appears, and the biggest inference pass 1 carried — "the counter shape looks like a cycle" — is exactly the one pass 2 converted into an observation.

Three more cells (`NET3`, `STEENBRD`, `STEENBRE`) never build a subproblem at all: the objective is NaN at the CUTEst start point. Five (`MSS2`, `MSS3`, `READING8`, `TAX213322`, `TAX53322`) are CENSORED by the outer timeout and are never filled in. Two more (`ACOPR118`, `BIGBANK`) are truncated by the wall; `BIGBANK` is accepting every step it takes and is simply slow.

## The rule defects this round recorded, and did not repair

A pre-registered rule's bytes are the pin. Seven defects were found by APPLYING the three rules, and **not one rule file was edited**:

| defect | rule | what it is |
|---|---|---|
| R1 | `RULE.md` §5.2 #7 | two different predicates in one clause; the ambiguity decides `YORKNET`'s class. Both readings printed. |
| R2 | `RULE.md` §5.2 #4 | fires on `BRITGAS`, whose exit is the restoration path and not the failure budget. Label stands; mechanism printed beside it. |
| R3a | `RULE.md` F2 | the h-band degenerates to `0 >= 0` when `h_in == 0`, which is `SAWPATH`. The pair is printed rather than the ratio. |
| R3 | `RULE.md` F3, F4 | dead on the whole population: no cell reaches the trust-region floor and essentially every `Reject` is a routed QP failure. |
| D1 | `RULE-P2.md` §6.1 | "a tail" is never defined, and the inherited detector is period-2 only. Four solves cycle at period 4 or 6 and it sees nothing. |
| D2 | `RULE-P2.md` §6.1 | `dx_inf == 0` on every tail pass is unreachable read strictly: drop passes record no `dx_inf` at all. |
| D3 | `RULE-P2.md` §6.1 | the `LONG WALK` clause parses two ways and "does not repeat" is undefined. Changed no answer here. |

A fourth `RULE-P2` defect, of §5.1: two of its fourteen named byte fields are not printed by the pass-2 probe and are reported ABSENT rather than as agreement.

---

## Engine change ⇒ cells it plausibly converts — a RECOMMENDATION to the settler

**A RECOMMENDATION, and nothing more.** `RULE.md` §0/§10 and `RULE-P2.md` §0/§8 bind every line: no remedy is selected, designed, built or run here, and each candidate below is its own task with its own pre-registered rule and its own gates. OBSERVED and INFERRED are marked separately on every line, and what is NOT KNOWN is stated rather than left out.

### Candidate A — an ANTI-CYCLING rule in the QP walk

*Would plausibly convert, or at least stop wasting: `ACOPP57`, `ACOPP118`, `ACOPR57`, `DALLASM`, `STEENBRB`, `STEENBRC`, `STEENBRG` (7 cells whose terminator is `QP_CAP_BUDGET`), and is implicated in `BRITGAS`, `SAWPATH` and `YORKNET` (3 more). Pass 1 listed the same ten as INFERRED; pass 2 makes them OBSERVED.*

**OBSERVED.** 19 of 21 traced solves end in an exactly periodic, zero-motion tail; one more (`ACOPR57` solve 16) ends in an exactly periodic tail that moves and returns. The tail starts EARLY — its start minor over the 21 solves runs 0 … 1570 — so **101752 of the 109489 recorded minors in the traced solves, 92.9%, are spent after the walk has already begun repeating itself.** Those solves account for 109489 of the 362599 minors the ten cells spend in total. The MODAL re-admitted constraint of the tail is a BOUND on 21 of the 21 solves and an inequality row on none of them.

**OBSERVED, and it CONSTRAINS the remedy rather than supporting one.** The engine's own inertia test reports NO excess negatives anywhere in the tail on 14 of the 21 solves. **An inertia-CONTROLLING remedy would not fire on most of this population**, because by the engine's own algebraic verdict the working set is not the problem it is looking for. Where the tail excess is non-zero it is small and constant (`ACOPP57`'s −6/−5, `ACOPR57` solve 16's −19/−18/0, the ±1 alternation on `STEENBRC`/`STEENBRG`/`BRITGAS`), and M3b C11 says those are ALGEBRAIC verdicts about the bordered system, not fresh factorizations. An ANTI-STALL remedy keyed on repetition — the working-set signature repeating with no motion — fires on every one of the 19 `CYCLE` solves by construction.

**OBSERVED.** The cycle is RE-ENTERED, not INHERITED: on all eight cells where a later capped solve exists, its incoming working-set signature is NOT any of the earlier solve's tail signatures. A per-solve anti-cycling rule needs no cross-solve state.

**INFERRED, and NOT established.** That breaking the cycle would make these QPs terminate usefully. Nothing here measures what the walk does after an anti-cycling intervention, and nothing here says the subproblem has a solution the walk could reach: a cycling active-set walk can be cycling on a degenerate vertex of a subproblem that is itself the wrong subproblem. **NOT KNOWN:** why the drop rule selects the constraint it just admitted — the multiplier sign and the ratio test are recorded per pass, but no experiment here varies either.

### Candidate A′ — a QP-CAP TERMINATION REASON (cheap, and strictly diagnostic)

*Would convert: probably nothing on its own. Would make every cell in candidate A legible to the driver and to the next census.*

**OBSERVED.** A capped walk returns `max_iter` today, and the driver routes it as one more consecutive failure until the budget ends the solve. Pass 1's finding R3 is that essentially every `Reject` verdict in these histories is a ROUTED QP FAILURE rather than a merit rejection. Pass 2 shows those same solves were provably stalled for 82–99.9%% of their minors before the cap was reached.

**INFERRED.** That a distinct exit reason — "the walk repeated a working set with zero motion" — would let the driver do something better than shrink and retry. It is a CHEAP change to measure and it commits to no numerical remedy. **NOT KNOWN:** what the driver should do differently on such an exit; this census does not design one.

### Candidate B — the SCHUR REBUILD cost of the cycle

*Would convert: nothing by itself. Would remove a large constant factor from every cell in candidate A, and it is the reason those cells are also slow.*

**OBSERVED, and this is new in pass 2.** The per-pass cost of a cycling minor is not uniform, and the traces split the population in two:

| solve | tail passes | factorizations in the tail | Schur updates | border refines | `eqp_path` in the tail |
|---|---|---|---|---|---|
| `ACOPP57` solve 3 | 1776 | 0 | 1775 | 1776 | {'border_schur': 1776} |
| `ACOPP57` solve 6 | 1760 | 0 | 1759 | 1760 | {'border_schur': 1760} |
| `DALLASM` solve 1 | 1990 | 3980 | 203975 | 0 | {'eliminated': 1990} |
| `DALLASM` solve 7 | 1853 | 0 | 1853 | 1853 | {'border_schur': 1853} |
| `STEENBRB` solve 1 | 414 | 509 | 248 | 0 | {'eliminated': 414} |
| `STEENBRB` solve 16 | 4531 | 4531 | 0 | 0 | {'eliminated': 4531} |
| `ACOPP118` solve 3 | 4607 | 4607 | 0 | 0 | {'eliminated': 4607} |
| `ACOPP118` solve 6 | 4606 | 4606 | 0 | 0 | {'eliminated': 4606} |
| `ACOPR57` solve 4 | 1900 | 0 | 1899 | 1900 | {'border_schur': 1900} |
| `ACOPR57` solve 16 | 2010 | 670 | 20100 | 2680 | {'border_schur': 1675, 'eliminated': 335} |
| `STEENBRC` solve 0 | 4592 | 4592 | 0 | 0 | {'eliminated': 4592} |
| `STEENBRC` solve 27 | 5233 | 5233 | 0 | 0 | {'eliminated': 5233} |
| `STEENBRG` solve 0 | 4577 | 4577 | 0 | 0 | {'eliminated': 4577} |
| `STEENBRG` solve 27 | 5242 | 5242 | 0 | 0 | {'eliminated': 5242} |
| `YORKNET` solve 7 | 5077 | 5077 | 0 | 0 | {'eliminated': 5077} |
| `YORKNET` solve 145 | 7990 | 7990 | 0 | 0 | {'eliminated': 7990} |
| `SAWPATH` solve 6 | 6655 | 13310 | 49913 | 0 | {'eliminated': 6655} |
| `SAWPATH` solve 10 | 15855 | 15855 | 0 | 0 | {'eliminated': 15855} |
| `BRITGAS` solve 0 | 4283 | 4283 | 0 | 0 | {'eliminated': 4283} |
| `BRITGAS` solve 4 | 5727 | 5727 | 0 | 0 | {'eliminated': 5727} |
| `BRITGAS` solve 11 | 11074 | 11074 | 0 | 0 | {'eliminated': 11074} |

On the `eliminated` rows the walk pays **one full sparse KKT factorization for every wasted minor**; on the `border_schur` rows it pays one Schur update and one border refine and NO factorization. `ACOPP118` spends 20 501 factorizations on 19 917 minors; `BRITGAS` spends 93 749 on 92 309. `DALLASM` solve 1 is the extreme: 2 factorizations and ~102 Schur updates per tail pass.

**INFERRED, and NOT established.** That the `eliminated` path is avoidable on these working sets, or that keeping the border stack alive would be correct there. The trace records `border_latched` and `border_dim = -1` on exactly those passes; what those states mean for the engine's own correctness is not read out here and no hven source was changed to find out. **NOT KNOWN:** whether the rebuild is a consequence of the cycle or a contributor to it.

### Candidate C — `project_start_point` for the three non-finite-start cells

*Would plausibly convert: `STEENBRD`, `STEENBRE`; would not convert `NET3` (3 cells in the class, 2 candidates). Unchanged from pass 1 — pass 2 traced none of them, because they build no subproblem at all.*

**OBSERVED.** All three die at major 0 with `f(x0) = NaN`, an empty ledger and zero factorizations. `STEENBRD` and `STEENBRE` are solved to optimality by hven's OWN IPM engine on the same model through the same bridge, and by SNOPT; `NET3` defeats all four lanes. The option exists, is shipped OFF, and every leg's `OPTIONS` block proves it OFF.

**INFERRED, explicitly not established.** That the start point lies outside the declared box and that the projection is what the IPM's interior push supplies. **NOT KNOWN:** which of `f`, `grad`, `cE`, `cI` is non-finite — the driver's gate conjoins all four into one bool. The census's own three-step measurement for this is written out in `tables/census.md`; it is cheap and touches no QP code.

### Candidate D — the restoration excursion

*Would plausibly convert: `STEENBRB`, `STEENBRC`, `STEENBRG` (3 cells, all of them also in candidate A).*

**OBSERVED, pass 1.** Each restores from a large violation to `h_out ≤ 5e-12` and lands the objective nine to eleven orders of magnitude above where it started. **OBSERVED, pass 2, and it is the new part:** the QPs AT that restored point are the cycling ones — `STEENBRC` and `STEENBRG` both cycle at period 4 on their first capped solve and at period 2 on their last, and `STEENBRB`'s last capped solve cycles at period 2 with a 4 531-pass tail. So the excursion and the cycle are not competing explanations on these three: the excursion supplies the point, and the walk cycles once it is there.

**INFERRED.** That a proximity-weighted restoration (M2b built it, shipped OFF at `restoration_proximity = 0`) would land somewhere the walk does not cycle. **NOT KNOWN:** whether the cycling is a property of the restored point or of the walk; the same walk cycles on `ACOPP57`, `ACOPP118` and `ACOPR57`, which never restore at all. That is evidence AGAINST the excursion being the cause of the cycling, and FOR it being an independent second problem on those three cells.

### Candidate E — a drop-rule TIE-BREAK

*Would plausibly touch: `YORKNET` (1 cell). Newly suggested by the traces, and small.*

**OBSERVED.** On `YORKNET` solve 7 every one of the 2 538 tail drop passes records `drop_tied = 8`: eight candidates tie on the drop rule's own criterion, and the same one is chosen every pass, and the walk cycles on it. Everywhere else in this population `drop_tied` is `1`. **INFERRED:** that a different tie-break would leave the cycle. **NOT KNOWN:** what the eight tied candidates are — the trace records the COUNT and the chosen identity, not the tied set's members.

### What NOT to build from this census

* **Trust-region floor work and merit/filter acceptance work** — pass 1's finding R3 stands: `F3_TR_AT_FLOOR` is false on every cell with a row and judged rejections are 0 or 2 everywhere.
* **An inertia-controlled correction as the PRIMARY anti-stall** — pass 2's own read-out says the inertia excess in the tail is exactly zero on 14 of 21 traced solves. It would be building for a condition most of this population does not report.
* **A ride/curvature fix as the primary remedy** — on 14 of the 21 solves the ride is never attempted in the tail at all, and on three of the solves where it IS attempted it reports `stepped` with a step of exactly zero. The ride is not what is holding these walks up.
