# M7 W2 E1b fix round 2 — gate evidence: DECLARED vs MEASURED

Declaration (written and committed **before** the r2 chain ran, nothing in it
adjusted after): `../wrappers-e1b-fix2/psym-derivation-e1bfix2-r2.txt`, runner
`../wrappers-e1b-fix2/{gates-e1bfix2-r2.sh,chain_gates_e1bfix2_r2.sh}`. Base
`7ab88b81`, code head `92e6a12c`, range `7ab88b81..92e6a12c` for every
source/header derivation; the commits above the code head
(`68501337`, `d37c2dfb`, this file's commit) are docs/notes/data only. Chain:
detached, started `CHAIN_START 2026-09-24T05:19:29-04:00` at HEAD `d37c2dfb`
with 0 dirty files (`chain-e1b-fix2.txt:1-2`; `gates-e1b-fix2.txt:1-2`),
ended `CHAIN_END 2026-09-24T05:53:58-04:00` (`chain-e1b-fix2.txt:23`). Runner
transcript: `gates-e1b-fix2.txt` (copied verbatim from
`.scratch/m7w2e1bfix2gates-r2/logs/gates-e1bfix2-r2.log`). Chain wrapper
transcript: `chain-e1b-fix2.txt` (copied verbatim from
`.scratch/m7w2e1bfix2gates-r2/logs/chain-e1bfix2-r2.log`). Full test and
build transcripts, copied verbatim from `.scratch/m7w2e1bfix2gates-r2/gates/`:
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

Pre-flight and rehearsal are not separate transcript files this round: the
pre-flight is embedded inline in
`../wrappers-e1b-fix2/psym-derivation-e1bfix2-r2.txt` (the "PRE-FLIGHT" paragraph,
`2026-09-24T05:18:33-04:00`, `PREFLIGHT_OK`), and the P-SYM rehearsal it
cites is the first declaration's (`../wrappers-e1b-fix2/rehearsal/`,
`04:27:45-04:38:01`, already committed at `68501337`) — carried forward, not
re-run, because the code range and the 28 declared-value assignments are
unchanged between the first runner (`gates-e1bfix2.sh`) and the r2 runner
(`gates-e1bfix2-r2.sh`).

Every MEASURED cell below cites the file and line it was read from, in the
raw transcript copied alongside this README.

## The first chain (declaration `68501337`) — refused, then superseded

Before this r2 declaration, the first fix2 chain ran against the same
`68501337` declaration and range. Its transcripts live outside this
directory, at `.scratch/m7w2e1bfix2gates/logs/{chain,gates}-e1bfix2.log` (not
copied here — that chain did not close green and its tree carries no
"gates-e1b-fix2" evidence dir of its own). It ended with exactly two
`ASSERT FAIL` lines, both the post-G5 class-B-by-name check, and this
GATES_SUMMARY line, quoted verbatim from
`.scratch/m7w2e1bfix2gates/logs/gates-e1bfix2.log:186`:

```
GATES_SUMMARY G1=0 G2=NOT-RUN(declared) G3=0 G4=NOT-RUN(declared) G5=1 G6=0
```

(`GATES_AGGREGATE rc=1 over [0 0 1 0]`, same file line 188.) The two
failures, same file lines 163-164:

```
ASSERT FAIL class B by name Release: measured [] != declared [ ]
ASSERT FAIL class B by name Debug: measured [] != declared [ ]
```

G5 itself — the P-SYM capture, classify, and section comparison — measured
everything the declaration expected (differing 1 = A 1 + B 0, unexplained 0,
no library mover, sections as declared); the only refusal was a string
comparison. `printf '%s\n' $names` over an EMPTY declared list prints one
empty line, which `tr '\n' ' '` turns into a single space (`" "`, spelled
`[ ]` in the log), while the empty MEASURED set prints as the true empty
string (`[]`). Every other gate (G1, G3, G6) and fold proofs held; nothing
else in the first chain's run is a finding.

**The successor runner.** `d37c2dfb`'s runner (`gates-e1bfix2-r2.sh`,
E1BFR2 (i)) replaces the raw-string comparison with `class_b_names_check`:
whitespace-split both sides, drop blanks, `LC_ALL=C sort -u`, join with one
space, then compare — so an empty declared list and an empty measured set
compare equal regardless of how each was spelled going in. The fix is
exercised offline before the r2 chain's first leg (six cases: both empty
under either spelling → rc 0 pass; a non-empty measured set against a
declared-empty list, or the reverse, or a missing classification file → rc 2
refusal), recorded in `../wrappers-e1b-fix2/psym-derivation-e1bfix2-r2.txt`
(the "THE FIX" paragraph). The 28 declared-value assignments themselves are
unchanged (mechanical diff of the assignment lines between `gates-e1bfix2.sh`
and `gates-e1bfix2-r2.sh` is empty) — this is a comparison-function fix, not
a re-declaration of what the gates expect.

## G1 — Debug suite (long test excluded)

| | DECLARED (`psym-derivation-e1bfix2-r2.txt` §3) | MEASURED |
|---|---|---|
| tests | 2817 | 2817 — `gates-e1b-fix2.txt:37`; `suite-debug.txt:5641` |
| failed | 0 | 0 — `gates-e1b-fix2.txt:36`; `suite-debug.txt:5641` |
| by-name started count | 2819 (== the previous round's baseline) | 2819 — `gates-e1b-fix2.txt:38` |

## G2 — the long test, alone

Declared and measured **NOT RUN**: no library code, no header code, and
`tests/sqp/test_continuation.cpp` unchanged in the range — checked by value
in the pre-flight, re-asserted at `gates-e1b-fix2.txt:20` and restated at
`gates-e1b-fix2.txt:39` ("G2 NOT RUN (declared, E1BFR2 (f)): the long test's
source and every library line it runs are unchanged but for comments").
There is no `suite-debug-long.txt` in this evidence set.

## G3 — Release suite

| | DECLARED (§3) | MEASURED |
|---|---|---|
| tests | 2818 | 2818 — `gates-e1b-fix2.txt:51`; `suite-release.txt:5643` |
| failed | 0 | 0 — `gates-e1b-fix2.txt:50`; `suite-release.txt:5643` |
| by-name started count | 2820 (== the previous round's baseline) | 2820 — `gates-e1b-fix2.txt:52` |

## G4 — the U0 replay (behavioural gate)

Declared and measured **NOT RUN**: no numeric code changed in the range —
the comment-only proof at `gates-e1b-fix2.txt:10` ("every changed line of
`src/*.cpp` and `include/` ... is a `//` comment") is the pre-flight value
check, re-asserted at `gates-e1b-fix2.txt:53` ("G4 NOT RUN (declared,
E1BFR2 (f)): no numeric code changed (the pre-flight's comment-only proof)").
The replay's inputs are the previous round's (E2 fix1+1b, `7ab88b81`) and are
not re-run here. There is no `replay/` directory in this evidence set.

## G5 — P-SYM (both configurations)

| | DECLARED (§2) | MEASURED Release | MEASURED Debug |
|---|---|---|---|
| DIFFERING objects | 1 = 1 A + 0 B | 1 — `gates-e1b-fix2.txt:75`, asserted `:82` | 1 — `gates-e1b-fix2.txt:122`, asserted `:129` |
| class A (source changed) | 1 (`test_equality_basis.cpp.o`) | 1 — `gates-e1b-fix2.txt:76`, asserted `:83`; named in `psym-classification-Release.txt:10-11` | 1 — `gates-e1b-fix2.txt:123`, asserted `:130`; named in `psym-classification-Debug.txt:10-11` |
| class B (includer, source untouched) | 0 | 0 — `gates-e1b-fix2.txt:77`, asserted `:88` | 0 — `gates-e1b-fix2.txt:124`, asserted `:135` |
| UNEXPLAINED | 0 | 0 — `gates-e1b-fix2.txt:78`, asserted `:84` | 0 — `gates-e1b-fix2.txt:125`, asserted `:131` |
| negative control | ≥ 1 | 6 — `gates-e1b-fix2.txt:79,85`; `psym-classification-Release.txt:20` | 3 — `gates-e1b-fix2.txt:126,132`; `psym-classification-Debug.txt:23` |
| compare rc | 1 (expected; the gate is the classification) | 1 — `gates-e1b-fix2.txt:64,80` | 1 — `gates-e1b-fix2.txt:111,127` |
| capture pair | after = before = 151 | 151 — `gates-e1b-fix2.txt:58-60` | 151 — `gates-e1b-fix2.txt:105-107` |
| outside-universe seam object | built, 0 in the manifest | `gates-e1b-fix2.txt:61-62` | `gates-e1b-fix2.txt:108-109` |
| ONLY-AFTER | `[]` (the ADDED sources' objects; 0 added) | `gates-e1b-fix2.txt:65` | `gates-e1b-fix2.txt:112` |
| library ADDED | none | `[]` — `gates-e1b-fix2.txt:89` | `[]` — `gates-e1b-fix2.txt:136` |
| class B cross-config symmetry | EQUAL (0 both configs) | asserted — `gates-e1b-fix2.txt:154` ("class B sets equal (0 object(s))") | same assert (cross-config) |
| class A cross-config identical | yes | asserted — `gates-e1b-fix2.txt:157-159` | same |
| class B by name (fixed comparison) | matches the declared empty set | `gates-e1b-fix2.txt:163` — measured `[]` equals declared `[]` | `gates-e1b-fix2.txt:164` — measured `[]` equals declared `[]` |

The NOISE-ONLY class differs by configuration and is not itself asserted
against a declared count in this round's gate log line-range, but is
recorded in the classification files: Release 3 (`psym-classification-Release.txt:15-18`,
none in the production library, none explained by the changed source/header),
Debug 6 (`psym-classification-Debug.txt:15-21`, 2 of them library — the two
comment-only sources — 3 explained by the changed source/header). This
matches the derivation's carried-forward statement (`psym-derivation-e1bfix2-r2.txt`
§4, "DEBUG_ONLY 0 Rel / 2 Dbg (both comment-only sources' own objects)") once
DEBUG_ONLY is read from the sections gate below, which is the value that is
actually asserted.

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
| non-debug-mover set | empty | empty — `gates-e1b-fix2.txt:94,97-98`; `psym-sections-Release.txt:9` | empty — `gates-e1b-fix2.txt:143,146-147`; `psym-sections-Debug.txt:11` |
| objects compared | 44 | 44 — `gates-e1b-fix2.txt:91`; `psym-sections-Release.txt:6` | 44 — `gates-e1b-fix2.txt:140`; `psym-sections-Debug.txt:8` |
| byte-identical | — | 44 — `psym-sections-Release.txt:7` | 42 — `psym-sections-Debug.txt:9` |
| DEBUG_ONLY | Release 0 / Debug 2 (the two comment-only sources) | 0 — `gates-e1b-fix2.txt:93,102`; `psym-sections-Release.txt:8` | 2 (`equality_basis.cpp.o`, `sqp_solver.cpp.o`) — `gates-e1b-fix2.txt:138-139,153`; `psym-sections-Debug.txt:6-7,10` |
| REFUSED | 0 | 0 — `gates-e1b-fix2.txt:95`; `psym-sections-Release.txt:10` | 0 — `gates-e1b-fix2.txt:144`; `psym-sections-Debug.txt:12` |

No section count for any object is reported as moved this round (contrast
fix1, where `sqp_solver.cpp.o`'s section count moved between arms) — this
round's library-side change is comment-only, so the two DEBUG_ONLY objects
are debug-info-only movers, asserted NOISE-ONLY at `gates-e1b-fix2.txt:151-152`.

## G6 — install smoke, export contract, format/diff cleanliness

| | DECLARED (§6) | MEASURED |
|---|---|---|
| install smoke | PASSED | PASSED, rc 0 — `gates-e1b-fix2.txt:168,170`; `install-smoke.txt:1191` |
| export contract | OK, align 32 | OK, align 32 — `gates-e1b-fix2.txt:175,178`; `export-contract.txt:3,6` |
| clang-format | rc 0 on every changed .h/.cpp | rc 0 — `gates-e1b-fix2.txt:180` |
| dirty files | 0 | 0 — `gates-e1b-fix2.txt:182` |

## Fold proofs

| | DECLARED | MEASURED |
|---|---|---|
| total / failed | 46 / 0 | 46 / 0 — `chain-e1b-fix2.txt:21`; `fold-proofs.txt:99` |

## GATES_SUMMARY

`GATES_SUMMARY G1=0 G2=NOT-RUN(declared) G3=0 G4=NOT-RUN(declared) G5=0 G6=0`
(`gates-e1b-fix2.txt:186`); `GATES_AGGREGATE rc=0 over [0 0 0 0]`
(`gates-e1b-fix2.txt:188`); `CHAIN_SUMMARY gates=0 fold_proofs=0`
(`chain-e1b-fix2.txt:24`).

## DECLARED FINDING for this round

The first fix2 chain (`68501337`, transcripts outside this directory at
`.scratch/m7w2e1bfix2gates/logs/`) is the declared finding: it refused on a
by-name empty-list spelling mismatch (`[]` vs `[ ]`) in the post-G5 class-B
check, not on any P-SYM, section, or test-count disagreement — every other
gate in that chain, and every G5 measurement within it, matched the
declaration. `d37c2dfb`'s successor runner (`gates-e1bfix2-r2.sh`) fixes the
comparison to normalize both sides before comparing, changes no declared
value, and reruns the full reduced chain (G1, G3, G5, G6 + fold proofs; G2
and G4 held NOT RUN) green on its first leg with this directory's evidence.
Every declared value in `psym-derivation-e1bfix2-r2.txt` (§§1-8) matched the
measured r2 chain exactly.
