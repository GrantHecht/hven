# The gate transcripts, and what each one is

Every file here is BYTE-VERBATIM tool output from the leg that produced it. None has
been reformatted, trimmed or re-wrapped.

| file | leg | result |
|---|---|---|
| `release-hven_sqp_tests.log` | Release suite, inside the gate leg's lock hold | 1251 / 1251 |
| `release-hven_drivers_tests.log` | Release suite | 134 / 134 |
| `debug-hven_sqp_tests.log` | Debug suite, `-Continuation.ProbeBudgetBoundsAFailingProposal` | 1250 / 1250 |
| `debug-hven_drivers_tests.log` | Debug suite | 134 / 134 |
| `debug-continuation-alone.log` | that one >600 s Debug test, ALONE and DETACHED (`nohup`, pid retained) | 1 / 1, 691 299 ms |
| `replay-{walk,ssn,ipm}.log` | the U0 27-cell three-arm replay, gate-leg arm | see `replay-comparison.txt` |
| `replay-restamp.log` | the same replay RE-TAKEN with a commit-exact binary stamp | 27 / 75 / 0 ×3 |
| `replay-comparison.txt` | both arms' stamps, the comparator's sha256, and the verdicts | — |
| `install-smoke.log` | `scripts/check_install_smoke.sh` | PASSED, 19 standalone-include TUs |
| `export-contract.log` | `scripts/check_export_contract.sh`, on the export file the gate leg's `find` reached first — the BUILD tree's copy | OK, `align=32` |
| `export-contract-installed.log` | the same check RE-RUN against the INSTALLED copy, with `md5sum` showing the two are byte-identical | OK, `align=32` |
| `clang-format.log` | the same command over the same tracked set at HEAD **and** at the base `0be12ec3` | 36 both sides, **0 new** |
| `gate-leg-transcript.log` | the gate leg itself, with its `PGREP_EMPTY` / `FOREGROUND_START` / `FOREGROUND_END` markers and `WRAPPER_RC 0` | — |

## `git diff --check`, stated exactly

Clean over the two CODE commits — every authored source line. In the evidence commit it
reports hits, and all of them are inside four of the VERBATIM transcripts above:

* `release-hven_sqp_tests.log` and `debug-hven_sqp_tests.log` — gtest prints two summary
  tables whose columns are space-padded, so those rows end in spaces, and the suite's
  last line is followed by a blank one;
* `install-smoke.log` and `gate-leg-transcript.log` — CMake's `-- MKL_TBB_LIBRARY = `
  line, whose value is empty on this box.

Not one hit is on a line this task wrote. They are left alone deliberately: trimming a
retained transcript to satisfy a whitespace check would make it no longer the thing the
tool printed, which is the only reason to keep it.

## One thing the export check turned up again

The gate leg's `find` reached the BUILD tree's `hvenTargets.cmake` before the installed
one, so `export-contract.log` names that path. `export-contract-installed.log` re-runs the
same check against `…/hven-install/lib64/cmake/hven/hvenTargets.cmake` and shows by
`md5sum` that the two files are byte-identical, so the gate's verdict is unchanged — but
the installed copy is the one a consumer sees and it is now checked by name.

Note the directory: **`lib64`**, not the `lib` that `CLAUDE.md` §1 states. That is
GNUInstallDirs on 64-bit Fedora and `find_package(hven)` is unaffected (CMake searches
both), but the sentence in CLAUDE.md is wrong on any lib64 distribution. M7 W0 T3
registered this and did not fix it — CLAUDE.md is the owner's — and this leg reproduces it.
