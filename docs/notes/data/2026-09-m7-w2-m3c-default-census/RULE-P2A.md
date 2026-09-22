# M7 W2 M3c — RULE-P2A.md: is the PASS-2 INSTRUMENT SOUND? The discriminating experiment

**Committed BEFORE any leg of it ran and before either of its two probe builds
was configured.** `RULE.md` (pass 1) and `RULE-P2.md` (pass 2) are unchanged and
are never edited; this file governs one bounded experiment and nothing else. It
decides whether pass 2's traces may be read at all — it reads no trace itself,
selects no solve, and answers none of `RULE-P2.md` §0's question.

---

## §0 The fact this rule exists to explain

`RULE-P2.md` §5.1's identity gate STOPPED all ten pass-2 cells. Nine differ from
their committed pass-1 rows on integer counters; `ACOPP57` agrees on every byte
field the pass-2 instrument prints and differs only in the last digits of its
residuals (`tables/mapping-p2.txt`).

The offline forensics (`logs/why-p2-differs.txt`) established three things, each
out of artefacts that already existed and with no solver run:

1. the pass-2 binary is **bit-identical across processes** (its smoke and its
   chain leg agree to the last digit on `ACOPP57`), so heap-address sensitivity
   inside MKL is not what moved the numbers;
2. these cells **are** reproducible across processes and across binaries — pass
   1's own gate reproduced the harness's committed rows at the pin with
   `IDENTITY_DIFFERS=0` and zero near-ulp acceptances;
3. therefore the cause is **in the pass-2 binary itself**, which differs from the
   install the pass-1 probe linked in two ways at once: its recompile of hven's
   42 sources did not use hven's own flag regime, and it defines `HVEN_TESTING=1`
   target-wide.

**Which of the two moved the numbers was NOT determined**, and cannot be without
building and running again. That is what this rule pre-registers.

### 0.1 A fourth fact, found offline while writing this rule, and bigger than the flag diff

The two builds did not use the same **compiler**.

    .scratch/m7w2m3c/rel/CMakeCache.txt          CMAKE_CXX_COMPILER:STRING=/usr/bin/clang++
    .scratch/m7w2m3c/p2/probebuild/CMakeCache.txt CMAKE_CXX_COMPILER:FILEPATH=/usr/bin/c++

Pass 1's install build was configured with `-DCMAKE_CXX_COMPILER=/usr/bin/clang++`
(`wrappers/chain_census.sh`). `wrappers/prepare_p2.sh` did not pass that flag, so
the pass-2 probe configure took CMake's default C++ compiler on this host,
`/usr/bin/c++`. Every flag the forensics listed as "missing" follows from that
one fact and from how those flags are attached:

* `-mllvm -inline-threshold=225` is appended by
  `cmake/hven_compile_options.cmake:233` inside a
  `$<$<STREQUAL:${CMAKE_CXX_COMPILER_ID},Clang>:…>` generator expression, and
  `-fopenmp=libiomp5` by `CMakeLists.txt:173` behind the same compiler-id test —
  both evaluate to nothing when the compiler is not Clang, which is also why the
  pass-2 command line carries a plain `-fopenmp` instead;
* `-Wabsolute-value` is added by `add_compile_options()` in hven's own DIRECTORY
  scope (`cmake/hven_compile_options.cmake:434`), so it is a directory property
  and is not on the `hven` target's `COMPILE_OPTIONS` at all — a harvest that
  reads target properties cannot see it;
* `-fPIC` comes from `set(CMAKE_POSITION_INDEPENDENT_CODE ON)` in that same
  macro — a target PROPERTY on `hven`, again not a compile option;
* `-Xclang -fno-pch-timestamp` is `target_compile_options(hven PRIVATE …)`
  (`src/CMakeLists.txt:349`), which IS a target property and IS harvestable.

So the pass-2 recompile was not hven's build with a define added: it was a
**different compiler** applying a **different flag set**. Under `-ffast-math`
that is licence to contract and reassociate differently in every hot loop.

This is an observation about two build trees that already exist. No leg produced
it and no number below is written after a leg was seen.

---

## §1 The three arms, and what each one isolates

Each arm is a probe binary that runs the SAME source, `wrappers/m3c_p2_probe.cpp`,
on the same cells at DEFAULT options. They differ only in how hven's 42 sources
underneath were compiled, and in whether the observer is armed at run time.

| arm | hven's 42 TUs | observer | probe CLI |
|-----|---------------|----------|-----------|
| **A** | hven's EXACT install compile commands, **no `HVEN_TESTING`** | not compiled in | `--trace off` |
| **B** | the SAME commands **plus `-DHVEN_TESTING=1`** and nothing else | compiled in, **not armed** | `--trace off` |
| **C** | the SAME binary as arm B | compiled in, **ARMED** | `--trace on --capture-solve -1` |

Arms B and C share one build; "arm C" is that build run with the observer armed.
So there are **two builds and six legs**.

* **A vs pass 1** isolates the BUILD. A reproduces ⇒ the recompile pipeline is
  sound and the flag/compiler regime was the whole of the difference.
* **B vs A** isolates `HVEN_TESTING` COMPILED IN — the `#ifdef`-gated hook sites
  inside `src/qp/qp_engine.cpp` and friends, present but never entered.
* **C vs B** isolates ARMING — the observer actually recording, on whole default
  runs rather than on a first QP.

The probe's own TU is compiled with `-DHVEN_TESTING=1` in **every** arm, arm A
included: the source carries `#error "…must be compiled with HVEN_TESTING…"` and
cannot be built without it. That is sound and not a contamination of arm A,
because every `HVEN_TESTING`-gated header body in this repository is **purely
additive** — `include/hven/detail/qp/qp_walk_trace.h`,
`include/hven/detail/linear/fault_injection.h`,
`include/hven/detail/qp/ipqp_fault_injection.h` and
`include/hven/detail/linear/pardiso_session.h` each wrap their entire test-only
body in `#ifdef HVEN_TESTING` and change no entity that exists without it. In arm
A the observer's `static inline thread_local` members are therefore defined in the
probe's own TU alone, referenced by nothing in hven's objects, and no inline
function has two different bodies across the link. This is an argument from the
headers' text, made before the experiment, and it is falsifiable: if arm A's hven
objects are byte-identical to the install's (§3.4), no hven code in arm A can
have seen the define at all.

---

## §2 What is fixed before anything runs

* **No file under `src/` or `include/` is changed. No default is changed. No
  remedy is selected.** Nothing in this rule licenses a line of code.
* **`RULE.md` and `RULE-P2.md` are not edited.** Their bytes are pins. This rule
  does not reinterpret them; it asks whether pass 2's INSTRUMENT was sound.
* **No pass-2 trace is read.** `wrappers/readouts_p2.py` is not run on the
  stopped traces, here or later, unless a gate this rule defines opens.
* **Wall-clock is informational only** (CLAUDE.md §7). The budgets in §4 are
  STOPS, not measurements.
* **HOLDOUT is never run.** `/home/ghecht/Projects/hven-bench` is READ-ONLY and
  its CMake is never entered. The SNOPT source firewall is absolute; PIQP source
  is never read.

---

## §3 The instrument: an EXACT REPLAY, not a second construction of the flags

### 3.1 Why the CMake harvest cannot do this, stated as a residual

`wrappers/probe-p2-CMakeLists.txt` harvests hven's build settings by reading the
`hven` target's `COMPILE_OPTIONS` / `COMPILE_DEFINITIONS` / `INTERFACE_*`
properties and hven's directory-scope `CMAKE_CXX_FLAGS`. §0.1 shows that
construction **cannot** reach three of the five differing flags:

* a DIRECTORY property set by `add_compile_options()` (`-Wabsolute-value`) is not
  a target property and is not read by that harvest;
* `CMAKE_POSITION_INDEPENDENT_CODE` (`-fPIC`) is a target PROPERTY, not a compile
  option, and is not read either;
* the two compiler-id generator expressions (`-mllvm -inline-threshold=225`,
  `-fopenmp=libiomp5`) evaluate against the CONSUMING project's compiler, so they
  can only be recovered by configuring the consumer with the same compiler.

Pinning `CMAKE_CXX_COMPILER` would recover the last two and `-Xclang
-fno-pch-timestamp`; the first two would still have to be re-derived by hand,
which is exactly the "second, driftable copy" that CMakeLists' own header warns
against. **This rule therefore does not approximate the install's commands and
does not re-derive them: it REPLAYS them.**

### 3.2 The replay

`wrappers/replay_build_p2a.py` reads the install build's own Ninja files —

    build tree   /home/ghecht/Projects/hven/.scratch/m7w2m3c/rel
    from         chain_census.sh's configure, HEAD d6eb24de, CMAKE_CXX_COMPILER=/usr/bin/clang++
    installed    /home/ghecht/Projects/hven/.scratch/m7w2m3c/install/lib64/libhven.a
    sha256       18a71b727533e9231fbdeb4e6bf0c9bb2d77aad6ab886a0f2e5729f1169eee10
                 (recorded by pass 1 in .scratch/m7w2m3c/library-sha256.txt and in logs/prepare.txt)

— expands each `CXX_COMPILER__hven_*` edge against its rule's `command`
template exactly as Ninja does (`$DEFINES $INCLUDES $FLAGS -MD -MT $out -MF
$DEP_FILE -o $out -c $in`, `${LAUNCHER}` included), and runs the resulting string
through `/bin/sh -c`, which is how Ninja runs it. The arm directory MIRRORS the
install build tree's relative object layout (`CMakeFiles/hven.dir/src/…`) and the
replay runs with that directory as its cwd, so **the command STRING is identical,
output path included**, rather than merely equivalent.

The archive is replayed the same way, from the `CXX_STATIC_LIBRARY_LINKER__hven`
edge, so its member names and member order are the install's.

The probe executable's own three harness bridge TUs, its own TU and its link are
replayed the same way from pass 1's own probe build tree
(`.scratch/m7w2m3c/probebuild`), so the only compilation anywhere in arm A that
is not byte-identical to something pass 1 ran is the probe's own source file,
which necessarily differs (it is the pass-2 probe, not the pass-1 probe).

**HEAD d6eb24de to HEAD b637e714 touches no file under `src/`, `include/`,
`cmake/` or either `CMakeLists.txt`** — `git diff --stat` between them over those
paths is empty — so the install build tree is at this HEAD's library sources.

### 3.3 The ONE permitted difference, and its proof

Arms B and C add **`-DHVEN_TESTING=1` and nothing else** to each of the 42
compile commands, inserted at the position CMake's own alphabetical `DEFINES`
ordering would have put it.

`replay_build_p2a.py` writes `logs/harvest-diff-p2a.txt`: for every one of the 42
TUs, the install command line and the replay command line, tokenized on
whitespace, compared after removing that single token on arms B/C. **The
transcript must be EMPTY of differences, and the script exits non-zero if it is
not.** A TU count below the install archive's member count, or a mismatch between
the set of TUs compiled and the set of members in the install archive, is a
REFUSAL.

If the replay cannot reproduce a command exactly, the run STOPS and the residual
is reported. Nothing is approximated.

### 3.4 Three structural checks, each able to fail

1. **Object identity (arm A).** Every replayed object's sha256 is compared to the
   install build's own object of the same relative path, and the replayed archive's
   sha256 to the installed `libhven.a`. Arm A is EXPECTED to be byte-identical;
   this is reported as the numbers (how many of 42 matched), never as a bool, and
   a mismatch is a finding, not a silent pass.
2. **The seam is where it is claimed to be.** `nm` over each arm's archive:
   `QpWalkObserver` symbol count must be **0** on arm A's and **≥ 1** on arm B's.
   Both directions are asserted, so a check that cannot fail is not counted.
3. **No production library in the link.** Neither arm's link line may name the
   INSTALLED `libhven.a`; each links its own replayed archive. The pattern used is
   exercised against a constructed link line that DOES name the installed path,
   so the check is not vacuous (`RULE-P2.md` §1's repaired-check lesson).

---

## §4 The legs

**Cells: `ACOPP57` and `DALLASM`.** Both are cheap at pass-1 wall (informational:
seconds, and under a minute), both are `QP_CAP_BUDGET` cells, and `DALLASM`
additionally has a restoration entry and an elastic activation in its pass-1 row —
so between them the two cells exercise the main walk, the restoration sub-driver
and a lazy engine. `ACOPP57` is the cell whose pass-2 row differed ONLY in
residual digits, and `DALLASM` the cheapest cell that differed in counters; a
rule that used only one of them could not tell a residual-class move from a
counter move.

**Six legs**, in this order: `A:ACOPP57 A:DALLASM B:ACOPP57 B:DALLASM C:ACOPP57
C:DALLASM`.

Terms, identical to `RULE-P2.md` §4 and to pass 1's own:

* DEFAULT options throughout — `--tr-init` is never passed, so `SqpOptions::tr_init`
  is not assigned at all and the row prints `tr_init_touched=0`;
* `MKL_NUM_THREADS=1 OMP_NUM_THREADS=1`, `taskset -c 2`, one solver process at a
  time, serial;
* the box lock (`flock /tmp/box-build.lock`, **no timeout, no fallback**) held per
  leg and released between legs, with a `pgrep` for a foreign cell binary as its
  own command before the lock AND again inside it, and `set -o pipefail`
  everywhere;
* **the SOLVER deadline is 600 s**, enforced by the probe's iteration callback,
  the same value pass 1 and pass 2 ran these two cells at;
* **the OUTER stop is `timeout 900`** per leg; a leg it kills, or a leg exiting
  non-zero for any other reason, is **CENSORED** — no row, no trace, nothing
  filled in later;
* arm C sets `--max-records 2000000`, as pass 2 did; arms A and B pass
  `--trace off`, which leaves `QpWalkObserver::active` false, and that — not the
  absence of a trace file — is what "not armed" means;
* the whole of it is ONE DETACHED CHAIN (`setsid`) with its pid in
  `.superpowers/m7-w2-m3c-STATE.md`.

---

## §5 The IDENTITY GATE — every field, never a subset

This is the process rule the pass-2 smoke's failure taught, written down. That
smoke checked **eight integer counters on one cell** and reported the seam
neutral; `f`, `stationarity` and `feasibility_e` already differed from the pass-1
row in that same transcript, and it did not compare them. **A gate that chooses
its fields can only find what it chose to look at.**

`wrappers/gate_p2a.py` compares each leg's row against the COMMITTED pass-1 row
for the same cell, read from the bytes of `rows/<CELL>.txt`, as follows.

**5.1 The field set is not chosen. It is the INTERSECTION** of the two rows' own
`key=value` field sets, computed mechanically, every member compared. Fields the
pass-1 row carries and the pass-2 instrument does not print are reported by name
in an `ABSENT` list — **an absent field is never a pass** — and fields only the
pass-2 instrument prints (its trace and summary bookkeeping) are listed too, so a
reader can see the whole of what was and was not compared. The gate prints the
resulting field count and the full field list.

**5.2 The class of a field is decided by its VALUE TEXT, not by a hand-kept
list.** A field is **residual class** iff either row's value text is parseable as
a floating-point number AND contains one of `.`, `e`, `E`, `nan`, `inf`.
Everything else — integers, statuses, names — is **byte class**.

* **byte class: exact byte equality.** No tolerance of any kind.
* **residual class: CLAUDE.md §7's near-ulp gate**, declared here as
  `|a − b| ≤ 1e-12 · max(1, |a|, |b|)`. This is the SAME constant `RULE-P2.md`
  §5.1 declared, deliberately: the two gates must be comparable, and a gate
  loosened after seeing what it rejected is not a gate. `nan` is matched as a
  BYTE fact and never numerically; a `nan` against a number is a DIFFERENCE.
* Every residual-class field's verdict is printed as `EQUAL_BYTES` / `WITHIN` /
  `EXCEEDS` **with both values and the absolute difference**, so "identical" and
  "within tolerance" are never conflated. A leg that passes only by tolerance is
  reported as such, with the count of near-ulp acceptances.

**5.3 Wall-clock fields are excluded BY NAME and the exclusion is printed**
(CLAUDE.md §7: wall-clock is informational and is never asserted). The excluded
names are exactly those matching `wall`, `seconds_elapsed` or `time`; the
deadline SETTING (`deadline_seconds`) and the deadline OUTCOME (`deadline_fired`)
are NOT wall-clock readings and ARE compared.

**5.4 A leg REPRODUCES iff every byte-class field is byte-equal and every
residual-class field is `EQUAL_BYTES` or `WITHIN`.** Anything else is a
NON-REPRODUCTION, reported with every differing field, both values, and the
class each fell into.

**5.5 The gate's own folds.** `gate_p2a.py` runs, on CONSTRUCTED inputs, before
it reads any leg, and prints the transcript: identical rows PASS; a single
changed integer counter STOPS and names it; a float inside the tolerance is
`WITHIN`; a float outside it STOPS (this is the `stationarity` case the smoke
missed); `nan` vs `0` STOPS; `nan` vs `nan` is `EQUAL_BYTES`; a field present in
one row only is reported ABSENT and never counted as agreement; a missing row
file is REFUSED. **A checker that has never been seen to fail is not a checker**
(`RULE-P2.md` §7.4).

---

## §6 The PRE-REGISTERED OUTCOMES

Fixed here, before the builds, with what each licenses and what it forbids.

1. **A, B and C all reproduce** ⇒ the flag-and-compiler regime was the whole
   cause, and the seam is neutral both compiled-in and ARMED on whole default
   runs at DEFAULT options. ⇒ **The ten pass-2 legs are RE-TAKEN** under arm C's
   build: a NEW chain, the same terms as pass 2 (`RULE-P2.md` §3 stage 1 and §4,
   unchanged), with §5's identity gate armed on every cell. The existing pass-2
   traces stay stopped and unread, and the committed pass-2 rows keep their
   provenance as what a differently-built binary produced.
2. **A reproduces, B does not** ⇒ compiling the seam IN is not record-only on the
   default path. ⇒ **STOP.** This is a finding about hven — a test seam the
   repository's own convention calls inert changing a default-path trajectory —
   and it is reported to the settler with the differing fields per cell. No code
   changes here.
3. **A and B reproduce, C does not** ⇒ ARMING the observer is not neutral. ⇒
   **STOP likewise**, reported the same way. Pass 2's instrument cannot be used
   to answer `RULE-P2.md` §0's question by whole-run capture, and any successor
   design is a separate task with its own pre-registered rule.
4. **A does not reproduce** ⇒ the replay is still not the install's build, or the
   difference lies somewhere this rule has not looked. ⇒ **STOP and report the
   residual** — the exact fields, and the remaining construction difference
   (which, by §3.3's transcript, cannot be a compile command of hven's 42 TUs).
   Arms B and C are then uninterpretable and are reported as such, whatever they
   did.

A cell that CENSORS (§4) leaves its arm undecided; an arm with a censored cell is
reported as `INCOMPLETE` and never as a reproduction.

**The two cells are reported separately.** If one reproduces and the other does
not, that is the outcome — it is not averaged, and the arm is not called
reproducing.

---

## §7 What no outcome here licenses

Nothing in this experiment licenses a behaviour change, a default change, a line
of code, or a reading of any pass-2 trace beyond what §6 arm 1 opens. It cannot
close `RULE-P2.md` §0's question, it cannot re-open a cell `RULE-P2.md` §5.1
stopped, and its own wall figures are informational. Algorithmic references are
open-literature papers only; GPL/LGPL-licensed solvers' source is never read
(CLAUDE.md §6).
