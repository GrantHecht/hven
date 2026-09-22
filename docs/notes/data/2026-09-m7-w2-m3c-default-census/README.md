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
   (`wrappers/chain_p2b.sh`).

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
tables/census.md                 PASS 1's deliverable, generated by wrappers/census_table.py
tables/mapping-p2.txt            PASS 2's mapping, gate and (not-run) selection -- the ten STOPPED cells
rows/                            PASS 1's fifteen committed rows: the bytes every later gate compares against
rows-p2/                         PASS 2's ten rows + its smoke: what the differently-built binary produced
rows-p2a/                        P2A's six rows, three arms x two cells
traces-p2/                       PASS 2's committed trace slices
logs/why-p2-differs.txt          the offline forensics that located the cause in the binary
logs/harvest-diff-p2a.txt        the replay's per-TU command-line proof and its structural checks
logs/gate-p2a.txt                P2A's identity gate, its folds, and RULE-P2A section 6's outcome
logs/chain-p2a.log               P2A's six legs
logs/binaries-p2a.txt            the two arm binaries' sha256
wrappers/                        every probe, chain, classifier, mapper and gate this task ran
```

**Not read, and deliberately so:** pass 2's full traces
(`.scratch/m7w2m3c/p2/traces/`) and `wrappers/readouts_p2.py`. `RULE-P2.md` §5.1
stopped those cells, and a stopped cell's traces are not traces of the pass-1 row.
They are left in place as evidence of what the differently-built binary did.
P2A's own arm-C traces (`.scratch/m7w2m3c/p2a/traces/`) are an instrument check,
not the census's evidence, and are not read either.
