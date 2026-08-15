# The Accelerate divergence register (hven)

A **divergence register entry** records a place where Apple Accelerate and
Intel MKL Pardiso give this library measurably different numbers, together
with what was observed, where it was observed, and what the tests were
changed to assert as a result. An entry is not a bug report: most of what is
here is a trajectory that forked without changing an answer.

## Why this file exists at this path

The SQP suite that arrived with M3 cites an older register — `D14`-`D19` and
`D22` appear in test comments pointing at
`docs/notes/2026-07-28-accelerate-audit-checklist.md`,
`2026-07-29-accelerate-audit-results.md`,
`2026-07-31-accelerate-second-pass-results.md`, and
`2026-08-01-accelerate-register-3.md`. **None of those four notes migrated
into this repository.** Roughly a dozen source files still name them, and
every one of those references is dangling here. The observations they carry
are not lost — they are quoted in full in the comments that cite them, which
is where a reader will actually meet them — but the notes themselves are not
readable from this tree, and this file does not reconstruct text nobody here
has seen.

What it does instead: it is the register **from gate B onward**, and it keeps
its own numbering (`M3-n`) rather than continuing the origin's `D`-series,
because the origin's highest number is unknown from inside this repository and
a guessed `D20` could silently collide with an entry that already means
something else. Origin numbers are cited as origin numbers where a test
already carries one.

**The `D`-series citations were repointed at phase C, task C0.3.** Each one now
either names the register row that succeeds it — only `D19` has one, `M3-2` —
or carries an explicit marker saying the origin note did not migrate and that
the quoting comment is the citable record here, since the observation is
quoted there in full. The repointed sites are `test_sqp_restoration.cpp`
(`D14`, two blocks), `test_warm_start.cpp` (`D15`), `test_b1_gate.cpp`
(`D16`), `test_hs_sweeps.cpp` (`D17`, and the `D16` cross-reference),
`test_predictor.cpp` (`D18`), `include/hven/detail/sqp/predictor.h`
(`D16`/`D17`/`D18`), `test_continuation.cpp` (`D19`, repointed at gate B) and
`test_warm_start_battery.cpp` (`D22`). The disposition was repoint rather than
migrate: the observations are quoted at the citing sites, and the origin notes
are a citable archive that the M3 plan explicitly does not migrate.

**Wider, and deliberately out of that task's scope:** those same four notes are
also cited from roughly a dozen comments that carry no `D` number (audit
checklist items, standing rules, contrast paragraphs), and the suite cites
about thirty further non-migrated `docs/notes/2026-0[78]-*` files besides —
around 180 references in all. Those are algorithm-design and results notes with
no successor artifact in this repository, so there is nothing here to repoint
them *to*; this paragraph is their standing disclosure. Read any
`docs/notes/2026-07-*` or pre-`2026-08-14` `docs/notes/2026-08-*` citation in
`include/hven/detail/sqp/`, `tests/sqp/` or `bench/` as naming an origin
document that is not readable from this tree, with the quoting comment as the
record.

## What may be committed here, and on what evidence

The macOS CI lane (`macos-clang-release`, runner class
`github-macos26-arm64`) is the only Apple hardware this project reaches, and a
run on it **is** a real observation — the never-fabricate rule bars invented
values, not measured ones. Two standing limits on what a run can support:

- **Counters, states and lists** carry no machine, thread or configuration
  context, so a count that reproduces across CI runs is committable as an
  asserted per-backend value.
- **Floats** are not. hven's Accelerate session stores `num_threads` without
  applying it to any backend call, so nothing pins the reduction order on this
  lane; a float row needs two-run byte agreement before it is committed, and a
  column that fails that bar is reported, never asserted (`docs/testing.md`,
  "A float expectation is context-pinned three ways").

## Entries

### M3-1 — HS battery crash-basis minors are one lighter per arm

`tests/sqp/test_hs_battery.cpp`,
`HsBattery.CrashBasisIsANullResultOnTheBatteryWithTwoBoundSeededExceptions`.

| quantity | MKL Pardiso | Accelerate |
| --- | --- | --- |
| `minors_off` | 862 | **861** |
| `minors_on` | 859 | **858** |
| `facts_off` / `facts_on` | 320 / 323 | 320 / 323 (identical) |
| seeded rows / bounds, seeding list, changed list | 7 / 5, `{10,11,14,15,22,30,33,45}`, `{30,33}` | identical |

One HS problem spends one fewer QP minor on Accelerate, in both the
seeding-off and seeding-on arms. Factorizations do not move, and the test's
answer-invariance half (status and objective per problem) passes untouched, so
this is a trajectory fork rather than a different answer.

Provenance: stable across every m3 CI run carrying the suite, from
[31736708703](https://github.com/GrantHecht/hven/actions/runs/31736708703)
(0ba1b9f) through
[31824897327](https://github.com/GrantHecht/hven/actions/runs/31824897327)
(6535566) — ten agreeing runs, Release, AppleClang, `github-macos26-arm64`.

Disposition: per-backend arm on the two minor counts, taken at gate B. The
pin's own comment had asked for exactly this if the counts moved.

### M3-2 — origin `D19` re-adjudicated: the failed proposal costs 304

`tests/sqp/test_continuation.cpp`,
`Continuation.ProbeBudgetBoundsAFailingProposal`, arm A.

`off.steps[1].counters.factorizations` reads **304** on Accelerate against MKL
Pardiso's 303, on the failed, thrown-away proposal; majors (3), minors (1002),
status, and every other claim in the file match exactly. This is origin entry
`D19` (Grant's Mac pass at BASE fe47aef), which was deliberately left as an
MKL-only pin "so the next Mac pass re-adjudicates a number rather than a
range".

The Mac pass has now happened ten times, on CI, and re-adjudicates the same
number: 304, every run, Release
([31736708703](https://github.com/GrantHecht/hven/actions/runs/31736708703)
through
[31824897327](https://github.com/GrantHecht/hven/actions/runs/31824897327)).
Origin `D19` also claimed Debug byte-identity; this lane has no Debug leg, so
that half stays on the origin's authority and is not re-observed here.

Disposition: the per-backend arm the comment was structured to receive.

### M3-3 — the task-6b float columns cannot be pinned on this lane

`tests/sqp/test_corpus_cells.cpp`,
`CorpusTask6bPhaseB.TheShippedKSsnConfigurationIsUnmovedByTheFourLevers`.

On Accelerate every **integer** column, the status, and the per-QP
factorization shape reproduce the committed MKL artifact exactly, for both
compared cells. Only the five `{:.9e}` float residual columns differ, in the
6th-10th significant digit (e.g. `kkt_residual` 6.295824606e-14 observed
against the artifact's 6.295816157e-14).

Those floats **fail the two-run bar**. `f7_n1000_path_neutral`'s
`kkt_residual` reads 3.053657327e-10, 3.053661768e-10 and 3.053663988e-10 on
three separate runs
([31817935342](https://github.com/GrantHecht/hven/actions/runs/31817935342),
[31824897327](https://github.com/GrantHecht/hven/actions/runs/31824897327),
[31812442998](https://github.com/GrantHecht/hven/actions/runs/31812442998));
`f7_n1000_bound_neutral` is byte-stable across the same runs. The test pins
`MKL_NUM_THREADS=1` for exactly this reason on MKL, and that pin does nothing
on a backend that stores the thread count without applying it.

Disposition: the five float compares are asserted on MKL and **reported, not
asserted** on Accelerate. No Accelerate byte value is committed, and none may
be until the lane's threading is pinnable.

### M3-4 — the weakly-active tie lands the other way, and flips four times — **RULED**

`tests/sqp/test_ssn_engine.cpp`, `SsnEngineLocal.WeaklyActiveRowFinishesUncertain`.

The fixture is built on a tie (row 1 has both `c* = 0` and `lambda* = 0`), so
which way the activity coin lands is decided by rounding. On Accelerate every
coin lands opposite to MKL: the default run reports row 1 active and not
uncertain, bare mode reports it inactive, and full mode reports it active and
not uncertain. `ssn_uncertain_peak` still reads 1.

Stable across all ten suite-carrying runs. The coin outcomes themselves are
not a defect — the test's own banner says both readings are correct readings
of a tie. What the test asserts on this backend is strictly weaker than "a
tie must end UNCERTAIN": `ssn_uncertain_peak == 1` (the third, weakly-active
set fired at some point during the solve, both backends);
`res.ineq_active[1] || res.ineq_uncertain[1]` (row 1 is never
inactive-and-certain, both backends); and, in the coin-flip cell,
`bool(bare.ineq_active[1]) != bool(full.ineq_active[1])` (bare and
safeguarded disagree on row 1, both backends). It does **not** assert that
the tie ends uncertain on Accelerate — it cannot: on this backend the
default run's end state reports `ineq_uncertain[1] == false`, so the tied
row *leaves* the uncertain set before the solve finishes, the opposite of
MKL's end state.

**Ruled by the gate-B execution review (2026-08-15), Verdict 2.** Both
questions this entry left open are answered:

- On the flips (4 vs 1): legitimate, not under-damping. The fixture's flip
  sequence is ulp-sensitive to the backend's factorization rounding by
  construction, because it sits on a knife-edge tie — a tie is the one
  geometry where flip-count portability should NOT be expected. The systemic
  question ("does the oscillation guard under-damp on Accelerate?") is
  answered by the battery-wide evidence, not this fixture: M3-1 records the
  full HS battery at 861/858 minors on Accelerate against MKL's 862/859, with
  factorization counts and problem lists identical — a one-minor difference
  across ~860. Three extra flips confined to a deliberately degenerate cell,
  stable across ten runs, is backend tie resolution, not oscillation.
- On the end state (tie leaves the uncertain set): the weaker portable
  assertion set already in the test IS the right final shape. **End-state tie
  membership is backend-dependent by nature at exact ties** — an
  active-and-certain reading of a tied row is as defensible a reading of the
  tie as an uncertain one, since at `c* = 0` the row IS at its boundary. The
  portable guarantee this leg exists to defend is the
  never-inactive-and-certain property (`res.ineq_active[1] ||
  res.ineq_uncertain[1]`, asserted on both backends): that is what excludes
  the dangerous export (dropping a tied, binding row from the hint silently).
  No portable substitute for end-state uncertainty is required beyond that.

**Disposition**: the Accelerate arm is now PINNED, lifting the `UNOBSERVED`
holds — coin directions as observed, end-state `ineq_uncertain[1] == false`,
and `ssn_bulk_flips == 4`, all per-backend-arm assertions. The ten-run counter
stability meets this register's own committability bar for counters (the
two-run byte bar above is a float rule). The MKL assertions are not relaxed
in any way — the `suspect_escalations` precedent applies: pin each backend's
own value, never widen.

**Named re-open trigger**, quoted from the ruling: "if M5's crossover
measurement ever attributes hint-quality loss to tie misclassification on
Apple — measurably worse warm ingest from Accelerate-exported activity —
this ruling re-opens with that evidence, and the remedy family to evaluate
first is margin-based uncertain membership (conditioning/magnitude-ratio
criteria from the second-pass research, Q2), which would make membership a
function of the geometry rather than the coin." Until that trigger fires, no
algorithmic change is warranted — changing a tie-resolution rule to force
cross-backend agreement would be tuning the algorithm to the test.
