# M7 W2 E4 fix round 1 — the certificate seam's gates, DECLARED vs MEASURED

Review: Codex E4 review, FIX-ROUND (1): Important 1 (no fixture for a positive
bound violation inside the structural margin), Minor 1 (exact-zero asserted only
as `<=`), Minor 2 (the E4 README's "every other fixture" wording).

Base `53676599` (E4's head). Code `5c2a0aae`. Declaration `cdef9f5e`
(`psym-derivation-e4fix1.txt`, committed before the chain started). Runner and
chain: `../wrappers-fix1/`. The round ran in a git worktree
(`worktree-agent-a7724c6ad1ce4c857`), so every build, the P-SYM checkout of the
base and the scratch tree belong to that worktree. The chain started
2026-09-22T18:58:44-04:00 at HEAD `cdef9f5e`, clean
([chain.txt:1-3](chain.txt)). It finished 23:23:41 with `gates=0 fold_proofs=0`
([chain.txt:29](chain.txt)). It shared the box with other lanes, so it held the
lock per suite chunk, per replay engine and per P-SYM configuration. Wall-clock
times are informational only.

## What the round changed

- `include/hven/detail/qp/qp_certificate_seam.h` (new): an injector declared
  entirely under `HVEN_TESTING`. It is included only from inside
  `qp_walk_trace.h`'s own `HVEN_TESTING` block.
- `src/qp/qp_engine.cpp`: adds one `#ifdef HVEN_TESTING` hook just before the
  certificate's bound half. The hook sits below the TU's only `assert()`.
- `tests/qp/test_qp_degeneracy_arm.cpp` adds three fixtures, all passing in both
  suites:
  - `CertificateSeamIsInertUnarmedOnTheFixtureQp`
  - `CertificateToleratesABoundViolationInsideAndOnTheStructuralMargin`
  - `CertificateRefusesABoundViolationOutsideTheStructuralMargin`

  The same file also carries M1: `TheCertificatesBoundHalfPassesASolvedQpWithActiveBounds`
  now asserts the violation is `+0.0` by bit pattern, and keeps a separate
  tolerance assertion.
- The fixtures were checked with two mutants in the dev tree before commit, and
  both mutants were reverted. Refusing any `v > 0` fails the tolerance fixture.
  Never refusing fails the refusal fixture, and M5's C7 fixture too.

## Gates

| gate | DECLARED | MEASURED |
|---|---|---|
| G1 Debug suite, 7 chunks | 2736, 0 failed | 2736, 0 failed ([gates-e4fix1.txt:97](gates-e4fix1.txt)) |
| G2 long test alone | 1, 0 failed | 1, 0 failed ([:110](gates-e4fix1.txt)) |
| G3 Release suite, 7 chunks | 2737, 0 failed | 2737, 0 failed ([:180](gates-e4fix1.txt)) |
| G4 U0 replay | 27 × 75 × 3, 0 differences | 27/75/0 on walk, ssn, ipm ([:202](gates-e4fix1.txt), `replay/comparison.txt`) |
| G5 P-SYM Release | DIFFERING 0 = A 0 + B 0, compare rc 0, control ≥ 1 | 0 = 0 + 0, rc 0, control 74 ([:211-225](gates-e4fix1.txt)) |
| G5 P-SYM Debug | DIFFERING 0 = A 0 + B 0, compare rc 0, control ≥ 1 | 0 = 0 + 0, rc 0, control 73 ([:254-268](gates-e4fix1.txt)) |
| G5b sections Release | 42/42 byte-identical, DEBUG_ONLY 0, non-debug 0 | 42/42, 0, 0 ([:236-238](gates-e4fix1.txt)) |
| G5b sections Debug | DEBUG_ONLY 1 = derived `qp_engine.cpp.o`, non-debug 0 | 41/42 byte-identical, DEBUG_ONLY 1 = `qp_engine.cpp.o`, non-debug 0 ([:280-292](gates-e4fix1.txt)) |
| G6 | smoke, export (align 32), clang-format, diff --check, dirty 0 | all rc 0, align 32, dirty 0 ([:303-317](gates-e4fix1.txt)) |
| fold proofs | 46 / 0 failed | 46 / 0 failed ([fold-proofs.txt:99](fold-proofs.txt)) |

Every measured cell matches its declaration. `nm -C` finds 0 `QpCertificate` or
`qp_certificate` symbols in `libhven.a` in either configuration
(`nm-seam-symbols.txt`).

**What this establishes.** In Release, the production library is byte-identical
to E4's. In Debug, `qp_engine.cpp.o` is the only object that differs, and only in
its debug sections. Its own source lines below the hook moved by 4.

G1 and G3 are pass/fail checks against each fixture's own assertions. They do not
compare every fixture's full state bit for bit. The bitwise behavioural
comparison is G4.
