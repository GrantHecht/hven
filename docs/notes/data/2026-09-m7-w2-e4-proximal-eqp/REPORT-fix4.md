<!-- M7 W2 E4 (the proximal EQP) -- the implementer report for fix rounds 4 and 4b, copied
     VERBATIM from .superpowers/m7-w2-e4-fix4-report.md. Nothing below this comment is edited:
     the bytes after this preamble are that file byte for byte (cmp-checked). -->

# M7 W2 E4 FIX ROUND 4 (+ 4b) -- report

Commit (on branch `e4`, worktree `/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9`):

    a81849854f8a9da9c0d4f4d6e00bb6677ae40f14
    build(m7): W2 E4 fix round 4 -- the precompiled header is CLANG-ONLY, ...

Parent 5ed7bf16. Tree clean. No attribution trailer of any kind. Staged with
explicit pathspecs. Files changed (3, none of them C++):

    /home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9/src/CMakeLists.txt
    /home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9/docs/build.md
    /home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9/scripts/check_pch_neutrality.sh

---

## 1. THE MECHANISM, IN ONE PARAGRAPH

The G6 install smoke did not fail on the E4 sources. It failed on a stale GCC
precompiled header that ccache replayed. A GCC `.gch` records no input
identities for validation: the ONLY thing that keeps a consuming TU from
re-reading a `#pragma once` header the PCH already holds is a table of
(file size, content md5) pairs baked into the `.gch` when it was built, and GCC
never re-checks that table against the files on disk (there is no macro include
guard to fall back on -- hven's headers are `#pragma once` only). ccache's
preprocessor-mode key for the PCH-producing compile is the preprocessed text,
which a COMMENT-ONLY, LINE-COUNT-PRESERVING header edit leaves byte-identical --
so ccache serves a `.gch` built before such an edit while the header on disk no
longer matches the table inside it, and every consuming TU then re-parses that
header and dies on `multiple definition of 'enum class ...'`. The header this
happened to is `include/hven/core/solver_status.h`: commit 0275d16d (2026-09-25)
changed it from its cc60978e text by comments only, with the line count
preserved at 280, and the `.gch` the failing build consumed still recorded the
cc60978e bytes. Clang is unexposed (it records each input's size and mtime in
the AST file and refuses one whose inputs moved), which is why every clang build
of the same sources is green.

## 2. EVIDENCE (all OBSERVED unless marked)

- **L0 -- the sources are innocent.** `scripts/check_install_smoke.sh` at the
  worktree sources (5ed7bf16; `git diff 13013b4c 5ed7bf16 -- include src cmake
  scripts CMakeLists.txt tests bench` is empty, main tree clean), same GCC
  16.2.1, fresh workdir: `== hven install smoke: PASSED`, rc 0.
  `.scratch/e4/logs/l0-after.log`.
- **R1 -- deterministic replay of the gate failure.** The gate log's exact
  compile line, ccache DISABLED, against the gates' preserved
  `.scratch/m7w2e4gates/smoke-work/hven-build/CMakeFiles/hven.dir/cmake_pch.hxx.gch`:
  6 errors, identical to the gate log. `.scratch/e4/replay/replay1.log`.
  So the failure lives in that one artifact, not in the sources and not in
  ccache's handling of the consuming compile.
- **F1 -- the artifact's once-only table, parsed.** 840 entries of
  `{off_t size; unsigned char md5[16]; bool once_only;}` (32 B each, header at
  byte 291639803, `have_once_only = 1`). Against the PCH's true closure
  (821 files, derived with the failing command's own flags) exactly ONE closure
  file has no entry: `include/hven/core/solver_status.h`, 13828 B, md5
  `3bacc0480e61c74f38e5440ba5513273`. And exactly one `once_only` entry matches
  no closure file: 13874 B, md5 `07a2f51bac196f412de858bdda456afa`.
- **F2 -- what that orphan entry is.** `git cat-file`: blob `33c655f3` =
  `include/hven/core/solver_status.h` at `cc60978e` (2026-09-21) is 13874 B with
  md5 `07a2f51b...`. The `.gch` was built while that file had its cc60978e text.
- **F3 -- why ccache could not see the difference.** `git diff cc60978e
  0275d16d -- include/hven/core/solver_status.h` is COMMENT-ONLY (non-comment
  diff empty) and LINE-COUNT-PRESERVING (280 lines both sides), so the
  preprocessed text of the PCH TU is byte-identical across it. This also
  explains why the gate's diagnostic quoted the CURRENT file's line numbers
  (23/82/118/140/168/211) for the "previous definition".
- **F4 -- reproduced live.** Fresh GCC build dir, PCH built with ccache ON;
  then a comment-only, line-count-preserving edit to that header; then the
  `.gch` removed and the PCH edge rebuilt with ccache ON: ccache logs
  `Result: preprocessed_cache_hit`, and the served `.gch` is BYTE-IDENTICAL
  (sha256 `2fb16ae9350e8590...`) to the pre-edit one, its table still recording
  the PRE-edit md5 and not the post-edit one. `.scratch/e4/rep/`,
  `.scratch/e4/logs/rep.marker`.
- **C3 -- an alternative refuted, recorded so nobody re-tests it.** GCC does
  NOT reuse an existing `cmake_pch.hxx.gch` when producing a new one: with the
  old `.gch` in place (leg A) and with it removed first (leg B), both outputs
  record the post-edit md5. `.scratch/e4/ccprobe/`, `.scratch/e4/logs/c3.marker`.
- Environment: GCC 16.2.1 (Red Hat 16.2.1-2), ccache 4.12.3,
  `sloppiness = pch_defines, time_macros`, `pch_external_checksum = true`,
  `max_size = 40.0 GB` and the cache at 100 % of it (3362 cleanups, 26 errors).
  Each GCC `.gch` here is ~292 MB.
- INFERRED (the one link not directly re-run): that the gates' specific stale
  entry was stored by an earlier main-tree GCC PCH build in the 2026-09-21..25
  window. It is the only state of the box in which that content existed
  (no checkout on the box holds it today), and F4 shows the replay path that
  serves it.

## 3. THE FIX

`src/CMakeLists.txt`: `target_precompile_headers(hven PRIVATE hven_pch.h)` is
now guarded on `CMAKE_CXX_COMPILER_ID MATCHES "Clang"` (matches Clang,
AppleClang and clang-cl). Under GCC the target configures with no precompiled
header, so there is no `.gch` to go stale and nothing for a cache to replay.

Why at this point and not elsewhere:

- No ccache setting makes a GCC `.gch` validate its own inputs, so the cause
  cannot be fixed on the cache side.
- `docs/build.md` already disclosed that the PCH's membership bar 2 -- the
  object stays byte-identical to the non-PCH build -- does NOT hold under GCC
  (three opted-in TUs differ there). The guard makes that disclosure structural
  instead of conventional.
- It costs nothing: MEASURED, GCC Release library target, ccache disabled, -j6,
  box alone under `/tmp/box-build.lock`: **54.29 s wall / 901512 KiB peak RSS
  without the PCH vs 53.86 s / 901252 KiB with it**. The GCC PCH bought 0.4 s of
  a 54 s parallel build and cost a 292 MB artifact.
- Coverage is not lost: the PCH path stays exercised by the smoke's second
  `-DCMAKE_CXX_COMPILER=clang++` arm and by `scripts/check_pch_neutrality.sh`,
  which already defaults to clang.
- The membership machinery (opt-out loop + three postconditions) is deliberately
  left UNGUARDED -- it is what stops a new TU from quietly joining the PCH and is
  worth running on every configure.

`docs/build.md` records the guard, the failure mode, and closes the GCC
byte-identity limitation by construction. `scripts/check_pch_neutrality.sh`'s
header records that a caller passing `-DCMAKE_CXX_COMPILER=g++` now fails its
engagement assertion, correctly.

**Rejected alternative, with reasoning:** adding macro include guards alongside
`#pragma once` across the PCH closure. It would suppress the re-inclusion -- by
making the consuming TU silently use the PCH's stale declarations instead of
failing loudly. That trades a loud error for silent staleness, across 74+
headers, against the project's `#pragma once` convention. Rejected.

## 4. VERIFICATION

| check | result |
|---|---|
| GCC install smoke, fixed tree, fresh workdir | `INSTALL_SMOKE_RC=0`, `== hven install smoke: PASSED`, 25 s |
| `.gch` produced under GCC | **0** (`GCH_COUNT_IN_SMOKE=0`, `PCH_CONSUMERS_IN_SMOKE=0`) -- the hazard is gone by construction |
| installed `hvenTargets.cmake` | present (4314 B), `TARGETS_LS_RC=0` |
| consumer round-trip | "both solvers converged through one generic consumer; 19 standalone-include TUs compiled and linked" |
| `scripts/check_export_contract.sh <targets> linux 32` | `EXPORT_CONTRACT_RC=0`, "export contract OK for platform=linux, align=32" |
| clang Release configure + build | rc 0 |
| **libhven.a byte identity across the change (clang Release)** | `sha256 57c458e02bbbbddfd84efdd7ac012331e371b7c32d779e1ea253608d284b1393` BEFORE and AFTER, and again in an independent fresh build dir at the parent commit -- **identical** |
| `build.ninja` PCH lines, clang | identical; the only 36 differing lines are the git commit-stamp macros (`HVEN_RIG_COMMIT`, `HVEN_SQP_*_GIT_DESCRIBE`), which moved because the build dir had last been configured at a WIP commit |
| clang PCH still engaged | `cmake_pch.hxx.pch` present, 5 `-include cmake_pch.hxx` occurrences in `build.ninja` |
| full clang Release suite | 2869 / 2870 pass, **1 failure -- pre-existing, see §5** |
| Debug build + suite | NOT RUN, and the reason is that no header used by the tests changed: the commit touches one CMake file, one doc and one shell comment |
| corpus `rowcmp.py k3 k4` | NOT APPLICABLE -- no engine TU changed; the stronger statement is the byte-identical `libhven.a` above |
| clang-format | no C++ file changed, so nothing to format; `git diff --check` rc 0 |

Logs: `.scratch/e4/logs/{verify,verify2,exonerate}.marker`,
`.scratch/e4/{verify,verify2,exon}/`.

## 5. THE ONE SUITE FAILURE -- PRE-EXISTING, NOT THIS ROUND

`CorpusRunnerProcess.ScoreModelSurfaceWritesTheCensusArtifactAndLeavesTheMainCsvUnchanged`
(`tests/sqp/test_corpus_cells.cpp:1244`) fails in THIS WORKTREE'S clang Release
tree. It is not caused by fix round 4, and the proof is direct: a fresh clang
Release build dir in the same worktree at the PRE-PATCH commit 5ed7bf16, clean
tree, fails the same cell 3/3 SOLO (`.scratch/e4/exon/`), with the same
`libhven.a` sha256. Also 10/10 solo at the fix commit -- deterministic for these
binaries, so not a co-run flake either (CLAUDE.md §7's re-run-alone rule was
applied).

What it is: the assertion compares residual-class CSV columns from two
PROCESSES (score-model surface on vs off) through `residual_columns_agree`,
whose gate is `|a-b| <= 1e-5 * max(|a|,|b|)` -- relative only, with no absolute
floor. The observed cells are numerical noise near zero: at 5ed7bf16, column 15
`on=3.806354307e-19` vs `off=3.604125191e-19` (absolute delta 2.0e-20); at the
fix commit, columns 12/15/16 at ~2e-16 (absolute delta 5.6e-22). At those
magnitudes the gate is asserting ~4-digit agreement on exactly the
address-sensitive noise CLAUDE.md §7 exempts from equality and asks to be gated
by a CALIBRATED near-ulp gate. It passes in the main tree's gates build
(different source path -> different layout -> different last digits), which is
the same sensitivity seen from the other side.

Remedy (registered, not taken here -- it is a test-contract change and outside a
fix round scoped to the gate failure): give that gate an absolute floor.

## 6. WHAT THE GATE RE-DECLARATION NEEDS

- **No P-SYM-relevant file changed.** The commit touches `src/CMakeLists.txt`,
  `docs/build.md`, `scripts/check_pch_neutrality.sh`. No public header, no
  detail header, no TU. Expect no class A or class B movers from it and no suite
  count change.
- **But the head MOVED**, so per the standing rule a range-derived declaration
  (84ea0385..head) must be RE-DERIVED on the new range and MEASURED, not carried
  from the 13013b4c declaration. The expectation above is a prediction, not a
  substitute for the rehearsal.
- G6 is the sub-gate to re-run. Its other sub-checks are unaffected; its
  install-smoke leg will now build hven under GCC with no PCH.
- **Watch this in the G6 runner:** `g6.sh` passes `$CHANGED` (the round's changed
  C++ files, derived from git) to `clang-format --dry-run -Werror $CHANGED`.
  This commit changes NO C++ file, so that list can come out empty and
  `clang-format` with no file arguments reads stdin. Check the caller that
  derives `$CHANGED` before re-running, or the leg can hang rather than pass.

## 7. REGISTERED ITEMS

1. **There is no GCC build/test lane in CI.** `.github/workflows/ci.yml` has no
   g++ job; GCC is exercised only by `check_install_smoke.sh`'s DEFAULT arm
   (the Linux lane's "Run hven install smoke" step), which is why this class of
   failure could only ever surface there. Whether a real GCC lane is wanted is a
   separate decision; nothing in this round needs one.
2. **The box's ccache config disagrees with `docs/build.md`.**
   `~/.config/ccache/ccache.conf` carries `pch_external_checksum = true`, which
   that page says is inert here and "deliberately not in the config above".
   Harmless (nothing emits a `.sum`), but the documented and actual configs
   should agree.
3. **The ccache store is saturated**: 40.0 / 40.0 GB, 3362 cleanups, 26 errors,
   with ~292 MB per GCC `.gch`. With the GCC PCH gone, that artifact leaves the
   cache entirely.
4. **`docs/build.md` says the PCH is used by "six" TUs while `_hven_pch_sources`
   lists four.** Pre-existing drift, untouched by this round.
5. **The PCH set does NOT need to exclude core headers.** The mechanism was
   staleness, not membership: any `#pragma once` header in the closure is exposed
   the same way, and a membership change would not have prevented it.
6. **`tests/sqp/test_corpus_cells.cpp`'s residual gate needs an absolute floor**
   -- see §5.

---

# APPENDIX: FIX ROUND 4b -- the shared corpus residual gate gets its absolute floor

Commit `84fb520e6ef89867227b2048a7d92177574a25a0` on branch `e4`, parent
`a8184985`. Tree clean, no attribution trailer, explicit pathspecs. Files (3):

    tests/sqp/test_corpus_cells.cpp
    scripts/compare_replay.py
    scripts/census_compare.py

(paths relative to `/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9`)

## 4b.1 What was wrong

`runner_test::residual_columns_agree` implemented only the RELATIVE arm of the
gate this repository states for the residual class. The rule, per CLAUDE.md §7's
"calibrated near-ulp gate" and `docs/measurement.md`'s "the gate's two numbers",
is

    |a - b| <= max(1e-5 * max(|a|, |b|), 1e-13)

and **both comparators already implement exactly that** —
`scripts/compare_replay.py` and `scripts/census_compare.py`, as
`max(rel * scale, RESIDUAL_ABS_FLOOR)` with `RESIDUAL_ABS_FLOOR = 1e-13`. The
C++ side carried the relative half only; the floor lived at the pin sites. Both
scripts' docstrings said so in as many words ("the relative half ... together
with the floor at ...").

So this is **not a new tolerance**. The constant is the repo's own, derived in
this same file by
`CorpusTask6bRepair.TheKSsnArmMovesTheFourCrashingCellsAndNothingElse`:
residual norms formed by cancellation from O(1) data are accurate to a few ulps
of 1.0 however small the norm is, so a purely relative test below ~1e-15
measures noise against noise; worst observed drift 4.5e-15, 22x head-room, six
orders below the smallest genuine movement (`kkt_stationarity`, ~1e-7).
`CorpusTask6bPhaseB` applies the same constant at three further sites.

## 4b.2 The citation question, measured

The floor is cited repo-wide as `test_corpus_cells.cpp:3618-3623`, derivation at
`:3607-3617` (in both comparators and in `docs/measurement.md`). **Those cites
are already wrong in the current tree, before this commit**: OBSERVED, `:3607`
–`:3617` is the R6 column-set assertion and `:3618`–`:3623` is the `moved`
bookkeeping; the real derivation sits at ~`:3639-3648` with the constant at
`:3649`. They were last repaired at M6 W6 T6 fix1 and have drifted again.

This round therefore does **not** attempt a general renumbering sweep, and does
not introduce a new line-number dependency:

- the new comment cites the floor **by TEST NAME**, which does not drift;
- the only cite this commit *moves* is the comparator's own range,
  `:981-1012` → `:981-1037`, which WAS accurate; it is repaired in both scripts;
- the stale floor cite in those two sentences is replaced by the test name;
- the two anchors everything else depends on are **unmoved by construction** —
  `:935` is still `kResidualRelativeGate`, `:974` is still `is_residual_column`
  (verified after the edit), so `:908-944`, `:932-934`, `:935`, `:937-973`,
  `:938-951` and `:974` are exactly as accurate as before. The derivation block's
  own rewrite was made **line-count-preserving** for this reason.
- `scripts/compare_replay.py:43-45`, which a frozen PROVENANCE cites, is above
  the edits and unmoved.
- `docs/notes/**` is untouched (frozen provenance and ledgers).

## 4b.3 The gate's own contract test — updated, not relaxed

`CorpusResidualGate.AcceptsEveryRecordedSpreadAndRefusesARealMove` exists to make
the gate a checked claim, and it fired on the first build (its expectations
encoded the missing half). It now checks the whole rule:

| case | before | after | why |
|---|---|---|---|
| `3.806354307e-19` vs `3.604125191e-19` | absent | **ACCEPT** (new) | 2e-20 absolute; the measured pair the floor exists for |
| `1.957054230e-16` vs `1.957609898e-16` | absent | **ACCEPT** (new) | 5.6e-22 absolute; likewise |
| `1.000000000e-13` vs `3.000000000e-13` | absent | **REFUSE** (new) | the floor is a boundary, not an amnesty: 2e-13 is past it where the relative arm is dead |
| `1.0e-9` vs `1.0001e-9` (1e-4 rel) | REFUSE | replaced by `1.0e-6` vs `1.0001e-6` | its absolute move was exactly 1e-13, i.e. ON the boundary; at 1e-6 scale it is 1e-10, three orders above the floor, and the relative arm is the live one. Intent unchanged |
| `0` vs `1.0e-30` | REFUSE | **ACCEPT** | the old absolute ("never, however small") was an artifact of the missing half — both Python comparators have always accepted it, and a value that small is not distinguishable from zero at these columns' absolute accuracy |
| `0` vs `1.0e-12` | absent | **REFUSE** (new) | above the floor a nonzero against an exact zero is still refused |
| the four calibration readings, byte equality, the non-finite/NaN/unparseable cases, the 12-column set enumeration | unchanged | unchanged | the floor only ever relaxes; none of these move |

Every literal in the test was evaluated against the new rule **offline before the
build** (14 pairs, 0 mismatches), and the suite then confirmed it.

## 4b.4 MEASURED

| check | result |
|---|---|
| deviating cell SOLO, dir A = `.scratch/e4/exon/build` (fresh clang Release; **3/3 FAIL before**) | **3/3 PASS** |
| deviating cell SOLO, dir B = `.scratch/nc2-build-release` (pre-existing; **10/10 then 3/3 FAIL before**) | **3/3 PASS** |
| full clang Release suite, dir B, `ctest -j4` under `setsid`, box alone under `/tmp/box-build.lock` | **100% tests passed, 0 failed out of 2870** (129.39 s) |
| suite denominator | **2870, unchanged** — no test added or removed, so no suite-count declaration moves |
| `libhven.a` | **unchanged**, sha256 `57c458e02bbbbddfd84efdd7ac012331e371b7c32d779e1ea253608d284b1393`, in both build dirs — a test-only change touching nothing the library emits |
| `clang-format --dry-run -Werror tests/sqp/test_corpus_cells.cpp` | rc 0 |
| `python3 -m py_compile` on both scripts | OK |
| `git diff --check` | rc 0 |

Correction to the coordinator's framing, for the record: **both** of my build
dirs failed the cell before this round (dir B 10/10 solo at a8184985 and 3/3 at
5ed7bf16-equivalent state; dir A 3/3 at 5ed7bf16). The tree where it passed was
the MAIN tree's gates build — a third layout. All three are now covered by the
rule rather than by luck.

## 4b.5 For the gate re-declaration (fix4b's own contribution)

- **P-SYM: no library header or TU changed.** `tests/sqp/test_corpus_cells.cpp` is
  a test TU; the two scripts are not compiled. `libhven.a` is byte-identical, so
  no class A or class B mover comes from fix4b either.
- **Suite counts unchanged** (2870 Release), so the declared 2869/2870 arithmetic
  is untouched by fix4b.
- The head moves again (a8184985 → 84fb520e), so the P-SYM rehearsal must be
  re-derived on the final range and MEASURED, as flagged for fix4.
- G6's `clang-format --dry-run -Werror $CHANGED` now HAS a C++ file to check
  (fix4b changes one), which removes the empty-`$CHANGED` hazard flagged for
  fix4 if the round is declared over `84ea0385..84fb520e`.

## 4b.6 Registered by fix4b

7. **The `:NNN` cites into `test_corpus_cells.cpp` have drifted again** — the
   floor's cite (`:3607-3617` / `:3618-3623`, in `scripts/compare_replay.py`,
   `scripts/census_compare.py`, `docs/measurement.md`, `docs/concurrency.md` and
   `tests/drivers/test_concurrency.cpp`) points at unrelated code today. Measured,
   pre-existing, and left to a separate declared pass; fix4b repaired only the
   cite it moved and stopped citing that one by number. A cheaper standing remedy
   worth considering: cite by TEST or symbol name everywhere, as fix4b now does.
8. **`docs/measurement.md` lists the floor's sites as `:3618`, `:3809`.** After
   fix4b the shared comparator carries it too. Not edited here (its cites are part
   of item 7's drift); the page's substantive claim — that the gate has two
   numbers — is now more true, not less.
9. **`scripts/compare_replay.py` / `scripts/census_compare.py` were edited** for
   the sentence describing the C++ comparator, which fix4b falsified. Frozen
   provenance names them by PATH as the comparator; the byte-pinned copies are the
   ones under `docs/notes/data/**/probes/`, which are untouched. Precedent for
   editing the live scripts: M6 W6 T6b and T6 fix1 did exactly this.

SIGNOFF: M7-W2-E4-FIX4-REPORT
