# M7 W2 E1b fix round 3 — gate evidence: DECLARED vs MEASURED

Declaration (written and committed **before** the r2 chain ran, nothing in it
adjusted after): `../wrappers-e1b-fix3/psym-derivation-e1bfix3-r2.txt`, runner
`../wrappers-e1b-fix3/{gates-e1bfix3-r2.sh,chain_gates_e1bfix3_r2.sh}`. Base
`de62a2e7`, code head `9c08cc10`, range `de62a2e7..9c08cc10` for every
source/header derivation; the commits above the code head (`6fbb43d6`,
`20f97328`, this file's commit) are docs/notes/data only. Chain: detached,
started `CHAIN_START 2026-09-24T07:12:45-04:00` at HEAD `20f97328` with 0
dirty files (`chain-e1b-fix3.txt:1-2`; `gates-e1b-fix3.txt:1-2`), ended
`CHAIN_END 2026-09-24T07:42:11-04:00` (`chain-e1b-fix3.txt:23`). Runner
transcript: `gates-e1b-fix3.txt` (copied verbatim from
`.scratch/m7w2e1bfix3gates-r2/logs/gates-e1bfix3-r2.log`). Chain wrapper
transcript: `chain-e1b-fix3.txt` (copied verbatim from
`.scratch/m7w2e1bfix3gates-r2/logs/chain-e1bfix3-r2.log`). Full test and
build transcripts, copied verbatim from `.scratch/m7w2e1bfix3gates-r2/gates/`:
`suite-debug.txt`, `suite-release.txt`, `install-smoke.txt`,
`export-contract.txt`, `fold-proofs.txt`, `psym-*.txt` (from
`gates/psym/`: `classification-{Release,Debug}.txt`,
`compare-{Release,Debug}.txt`, `libobjs-{Release,Debug}.txt`,
`libobjs-both-{Release,Debug}.txt`, `sections-{Release,Debug}.txt`). This
round declares G2 and G4 NOT RUN, so there is no `suite-debug-long.txt` and
no `replay/` — neither a long-test leg nor a U0 replay leg exists in the
source scratch tree. The build trees and object directories (`gates-dbg/`,
`gates-rel/`, `psym-build/`, `gates/psym/{before,after}{,-obj}-*`,
`smoke-work/`) are not copied.

Pre-flight is not a separate transcript file this round: it is embedded
inline in `gates-e1b-fix3.txt:1-27` (`PREFLIGHT_OK: 4 segments (g1 g3 g5 g6)
written and asserted against GATES_SCRATCH, g2/g4 declared not run, before
the first leg`, `gates-e1b-fix3.txt:27`). The P-SYM rehearsal cited by the
declaration (`../wrappers-e1b-fix3/rehearsal/`, `06:18:35-06:32:26`, already
committed at `6fbb43d6`) is carried forward, not re-run: the code range
(`de62a2e7..9c08cc10`) and every declared value are unchanged between the
first runner (`gates-e1bfix3.sh`) and the r2 runner (`gates-e1bfix3-r2.sh`) —
`psym-derivation-e1bfix3-r2.txt` §0c: "its 67 top-level assignments differ
from the first's in exactly two, S= (the scratch tree) and LOG= (the log
name); every EXPECT_*, BEFORE, RENAME_*, NAMES_BASE_DIR is byte-identical."

Every MEASURED cell below cites the file and line it was read from, in the
raw transcript copied alongside this README.

## The first chain (declaration `6fbb43d6`) — refused, then superseded

Before this r2 declaration, the first fix3 chain ran against the same
`6fbb43d6` declaration and range. Its transcripts live outside this
directory, at `.scratch/m7w2e1bfix3gates/logs/{chain,gates}-e1bfix3.log` (not
copied here — that chain did not close green and its tree carries no
"gates-e1b-fix3" evidence dir of its own). It ended with exactly two
`ASSERT FAIL` lines, one per suite, and this GATES_SUMMARY line, quoted
verbatim from `.scratch/m7w2e1bfix3gates/logs/gates-e1bfix3.log:184`:

```
GATES_SUMMARY G1=1 G2=NOT-RUN(declared) G3=1 G4=NOT-RUN(declared) G5=0 G6=0
```

(`GATES_AGGREGATE rc=1 over [1 1 0 0]`, same file line 186.) The two
failures, same file lines 40 and 54:

```
ASSERT FAIL G1 Debug by name: this run does not carry the rename (EqualityBasis.T4NearCapMultiColumnLiftMeetsItsForwardBound absent, EqualityBasis.T4NearCapMultiColumnLiftMeetsItsForwardEstimate present)
ASSERT FAIL G3 Release by name: this run does not carry the rename (EqualityBasis.T4NearCapMultiColumnLiftMeetsItsForwardBound absent, EqualityBasis.T4NearCapMultiColumnLiftMeetsItsForwardEstimate present)
```

Both failures fired on a run that **did** carry the rename — the run's own
`Start` lines for both suites record `RENAME_FROM 0, RENAME_TO 1`
(`psym-derivation-e1bfix3-r2.txt` §0). Root cause
(`psym-derivation-e1bfix3-r2.txt` §0): the by-name check's condition was
`suite_names LOG | grep -qxF FROM || ! suite_names LOG | grep -qxF TO`, run
under `set -o pipefail`. A `grep -q` that **matches** exits as soon as it
sees the match; the still-writing upstream `sort` then takes `SIGPIPE`, and
under `pipefail` the pipeline's reported status is 141 — so on a correct run
(the name present, `grep -q TO` matching), `! ... grep -q TO` evaluated true
by way of the `sort`'s SIGPIPE exit code, not by way of the name being
absent. The runner's own failure text, "(FROM absent, TO present)", stated
the requirement it intended to check, not the measurement it actually took.
This was reproduced offline three-for-three against the first chain's own
`suite-debug.log` (rc 141), per `psym-derivation-e1bfix3-r2.txt` §0.

Every other assertion in the first chain's run matched the declaration: G1
Debug suite failed 0 / total 2817, G3 Release suite failed 0 / total 2818,
G5 P-SYM (differing 1/1 = class A 1/1 + class B 0/0, unexplained 0, negative
control 62 Release / 60 Debug, no library mover, `DEBUG_ONLY` 0 Release / 1
Debug), G6 clean, fold proofs 46/0 — all recorded in
`psym-derivation-e1bfix3-r2.txt` §0a as read from the raw first-chain
transcript. The by-name **set-equality** halves of G1/G3 (2819/2820 started
names) were never reached in that run (the chain stopped at the first
`ASSERT FAIL` per suite); they were exercised **offline**, after the fact,
by running the r2 check directly against the first chain's own
`suite-{debug,release}.log`: rc 0 in both cases, `psym-derivation-e1bfix3-r2.txt`
§0b ("`--check-names debug <first chain>/gates/suite-debug.log` rc 0 (0 / 1;
2819 == 2819)"; release the same, 2820 == 2820).

**The successor runner.** `20f97328`'s runner (`gates-e1bfix3-r2.sh`, E1BFR3r2
(n)) replaces the early-exiting `grep -q` pair with a rename check that
**counts** via `grep -c` (which reads its whole input, so it cannot be cut
short by a downstream reader's SIGPIPE): pass iff the old name is counted 0
times and the new name is counted 1 time in the run's started names, else
`ASSERT FAIL` with the measured counts printed. The fix was exercised
offline before the r2 chain's first leg: rc 0 against the first chain's own
suite logs, rc 2 against three synthetic logs (`TO` replaced by `FROM`;
`TO`'s lines removed; `FROM` added beside `TO`) — `psym-derivation-e1bfix3-r2.txt`
§0b. The 28 declared values themselves are unchanged; this is a
comparison-mechanism fix, not a re-declaration of what the gates expect
(`psym-derivation-e1bfix3-r2.txt` §0c).

## G1 — Debug suite (long test excluded)

| | DECLARED (`psym-derivation-e1bfix3-r2.txt` §3, carried from §0a) | MEASURED |
|---|---|---|
| tests | 2817 | 2817 — `gates-e1b-fix3.txt:39`; `suite-debug.txt:5641` |
| failed | 0 | 0 — `gates-e1b-fix3.txt:38`; `suite-debug.txt:5641` |
| rename (by name) | old name 0 times, new name once | 0 / once — `gates-e1b-fix3.txt:40` ("EqualityBasis.T4NearCapMultiColumnLiftMeetsItsForwardBound 0 times, ...ForwardEstimate once") |
| by-name started count | 2819 (== the fix2 baseline, with the declared rename applied) | 2819 — `gates-e1b-fix3.txt:41` |

## G2 — the long test, alone

Declared and measured **NOT RUN**: no library code, no header code, and
`tests/sqp/test_continuation.cpp` unchanged in the range — checked by value
in the pre-flight (`gates-e1b-fix3.txt:19`) and re-asserted at
`gates-e1b-fix3.txt:42` ("G2 NOT RUN (declared, E1BFR3 (f)): the long test's
source and every library line it runs are unchanged but for comments").
There is no `suite-debug-long.txt` in this evidence set.

## G3 — Release suite

| | DECLARED (§3) | MEASURED |
|---|---|---|
| tests | 2818 | 2818 — `gates-e1b-fix3.txt:54`; `suite-release.txt:5643` |
| failed | 0 | 0 — `gates-e1b-fix3.txt:53`; `suite-release.txt:5643` |
| rename (by name) | old name 0 times, new name once | 0 / once — `gates-e1b-fix3.txt:55` |
| by-name started count | 2820 (== the fix2 baseline, with the declared rename applied) | 2820 — `gates-e1b-fix3.txt:56` |

## G4 — the U0 replay (behavioural gate)

Declared and measured **NOT RUN**: no numeric code changed in the range —
the comment-only proof at `gates-e1b-fix3.txt:10` ("every changed line of
`src/*.cpp` and `include/` ... is a `//` comment") is the pre-flight value
check, re-asserted at `gates-e1b-fix3.txt:57` ("G4 NOT RUN (declared,
E1BFR3 (f)): no numeric code changed (the pre-flight's comment-only proof)").
The replay's inputs are the previous round's (E1b fix2, `de62a2e7`) and are
not re-run here. There is no `replay/` directory in this evidence set.

## G5 — P-SYM (both configurations)

| | DECLARED (§2) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| DIFFERING objects | 1 = 1 A + 0 B | 1 — `gates-e1b-fix3.txt:79`, asserted `:86` | 1 — `gates-e1b-fix3.txt:125`, asserted `:132` |
| class A (source changed) | 1 (`test_equality_basis.cpp.o`) | 1 — `gates-e1b-fix3.txt:80`, asserted `:87`; named in `psym-classification-Release.txt:10-11` | 1 — `gates-e1b-fix3.txt:126`, asserted `:133`; named in `psym-classification-Debug.txt:10-11` |
| class B (includer, source untouched) | 0 | 0 — `gates-e1b-fix3.txt:81`, asserted `:92` | 0 — `gates-e1b-fix3.txt:127`, asserted `:138` |
| UNEXPLAINED | 0 | 0 — `gates-e1b-fix3.txt:82`, asserted `:88` | 0 — `gates-e1b-fix3.txt:128`, asserted `:134` |
| negative control | ≥ 1 | 62 — `gates-e1b-fix3.txt:83,89`; `psym-classification-Release.txt:20` | 60 — `gates-e1b-fix3.txt:129,135`; `psym-classification-Debug.txt:22` |
| compare rc | 1 (expected; the gate is the classification) | 1 — `gates-e1b-fix3.txt:68,84` | 1 — `gates-e1b-fix3.txt:114,130` |
| capture pair | after = before = 151 | 151 — `gates-e1b-fix3.txt:62-64` | 151 — `gates-e1b-fix3.txt:108-110` |
| outside-universe seam object | built, 0 in the manifest | `gates-e1b-fix3.txt:65-66` | `gates-e1b-fix3.txt:111-112` |
| ONLY-AFTER | `[]` (the ADDED sources' objects; 0 added) | `gates-e1b-fix3.txt:69` | `gates-e1b-fix3.txt:115` |
| library ADDED | none | `[]` — `gates-e1b-fix3.txt:93` | `[]` — `gates-e1b-fix3.txt:139` |
| class B cross-config symmetry | EQUAL (0 both configs) | asserted — `gates-e1b-fix3.txt:154` ("class B sets equal (0 object(s))") | same assert (cross-config) |
| class A cross-config identical | yes | asserted — `gates-e1b-fix3.txt:157-159` | same |
| class B by name (fixed comparison, carried from fix2) | matches the declared empty set | `gates-e1b-fix3.txt:163` — measured `[]` equals declared `[]` | `gates-e1b-fix3.txt:164` — measured `[]` equals declared `[]` |

NOISE-ONLY (informational, not gated beyond the library rule): Release 3
(`psym-classification-Release.txt:15-18`, all bench objects, 0 library),
Debug 5 (`psym-classification-Debug.txt:15-20`, the 3 bench objects +
`tests/drivers/.../test_print_tables.cpp.o` + the library's own
`src/drivers/sqp_solver.cpp.o`, 1 of them library) — matching the
derivation's carried §2 statement ("Release 3 ... 0 library; Debug 5: those
3 + tests/drivers test_print_tables.cpp.o + the library's
src/drivers/sqp_solver.cpp.o").

The mover lists are in `psym-classification-{Release,Debug}.txt`, with the
full byte-difference dumps in `psym-compare-{Release,Debug}.txt` and the
object inventories in `psym-libobjs-{Release,Debug}.txt` /
`psym-libobjs-both-{Release,Debug}.txt` (the two are byte-identical this
round — no library object moved between the `-Release`/`-Debug` and
`-both-Release`/`-both-Debug` inventories). Class A (1, identical in both
configurations) is `tests/sqp/CMakeFiles/hven_sqp_tests.dir/
test_equality_basis.cpp.o` — the only source inside the P-SYM universe that
was actually modified (`test_sqp_equality_basis_seams.cpp` sits outside the
universe, built under `hven_sqp_seam_tests`). Class B is empty in both
configurations — no production-library object is a P-SYM mover.

## G5b — sections (library objects present on both sides)

| | DECLARED (§4) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| non-debug-mover set | empty | empty — `gates-e1b-fix3.txt:98,101-102`; `psym-sections-Release.txt:9` | empty — `gates-e1b-fix3.txt:148,149`; `psym-sections-Debug.txt:10` |
| objects compared | 44 | 44 — `gates-e1b-fix3.txt:95`; `psym-sections-Release.txt:6` | 44 — `gates-e1b-fix3.txt:142`; `psym-sections-Debug.txt:7` |
| byte-identical | — | 44 — `psym-sections-Release.txt:7` | 43 — `psym-sections-Debug.txt:8` |
| DEBUG_ONLY | Release 0 / Debug 1 (the one comment-only production source, `sqp_solver.cpp.o`) | 0 — `gates-e1b-fix3.txt:97,105`; `psym-sections-Release.txt:8` | 1 (`CMakeFiles/hven.dir/src/drivers/sqp_solver.cpp.o`) — `gates-e1b-fix3.txt:141,153`; `psym-sections-Debug.txt:6,9` |
| REFUSED | 0 | 0 — `gates-e1b-fix3.txt:99`; `psym-sections-Release.txt:10` | 0 — `gates-e1b-fix3.txt:146`; `psym-sections-Debug.txt:11` |

The Debug-only mover, `src/drivers/sqp_solver.cpp.o`, is a comment-only
source's own object and is asserted NOISE-ONLY at `gates-e1b-fix3.txt:150-152`
("library Debug: DEBUG-ONLY ... is a comment-only source's object";
"comment-only source Debug: ... -> NOISE-ONLY").

## G6 — install smoke, export contract, format/diff cleanliness

| | DECLARED (§6) | MEASURED |
|---|---|---|
| install smoke | PASSED | PASSED, rc 0 — `gates-e1b-fix3.txt:168,170`; `install-smoke.txt:1191` |
| export contract | OK, align 32 | OK, align 32 — `gates-e1b-fix3.txt:176,178`; `export-contract.txt:3,6` |
| clang-format | rc 0 on every changed .h/.cpp | rc 0 — `gates-e1b-fix3.txt:180` |
| dirty files | 0 | 0 — `gates-e1b-fix3.txt:182` |

## Fold proofs

| | DECLARED | MEASURED |
|---|---|---|
| total / failed | 46 / 0 | 46 / 0 — `chain-e1b-fix3.txt:21`; `fold-proofs.txt:99` |

## GATES_SUMMARY

`GATES_SUMMARY G1=0 G2=NOT-RUN(declared) G3=0 G4=NOT-RUN(declared) G5=0 G6=0`
(`gates-e1b-fix3.txt:186`); `GATES_AGGREGATE rc=0 over [0 0 0 0]`
(`gates-e1b-fix3.txt:188`); `CHAIN_SUMMARY gates=0 fold_proofs=0`
(`chain-e1b-fix3.txt:24`).

## DECLARED FINDING for this round

The first fix3 chain (`6fbb43d6`, transcripts outside this directory at
`.scratch/m7w2e1bfix3gates/logs/`) is the declared finding: it refused on a
by-name rename-check artefact (a `grep -q`-under-`pipefail` SIGPIPE that made
the negated presence test true on a run that in fact carried the rename),
not on any P-SYM, section, or test-count disagreement — every other gate in
that chain, and every G5 measurement within it, matched the declaration, and
the by-name set-equality halves that the chain never reached were confirmed
offline against that same chain's raw suite logs (rc 0 both suites).
`20f97328`'s successor runner (`gates-e1bfix3-r2.sh`) fixes the rename check
to count matches instead of testing early-exit presence, changes no declared
value, and reruns the full reduced chain (G1, G3, G5, G6 + fold proofs; G2
and G4 held NOT RUN) green on its first leg with this directory's evidence.
Every declared value in `psym-derivation-e1bfix3-r2.txt` (carrying forward
`psym-derivation-e1bfix3.txt` §§1-8 unchanged) matched the measured r2 chain
exactly.
