# RS-4 (includes) -- DECLARATION
Committed before any RS-4 edit; never edited afterwards (a failed gate is DECLARATION-RS4-r2.md, etc.).
Base: origin/m8 4064ff97a (the RS-3 head plus its CI record). The "before" arm is this commit, built and measured by legs/baseline2.sh into the ignored .scratch/.
Design: RS-design-page-v2.md, RS-4 group; section 2.3 (exception 4), section 5.2 (gate items 1-5, 7, 8).
Carries DECLARATION.md + -r2 + -r3, DECLARATION-RS2*.md (instrument rules) and DECLARATION-RS3*.md (neutralised arms, one capture-tool stamp).

## What changes (everything else must not change)
1. Every `#include "hven/..."` in include/, src/, tests/, bench/, examples/ becomes `#include <hven/...>`. Test-local includes (spelled from the tests root, RS-2) stay quoted; bare same-directory
   siblings stay quoted; src-private headers (src/trace/trace_names.h) stay quoted; `"../../bench/..."` relative includes stay quoted (a path, not a library header).
2. `.clang-format` gains include sorting: SortIncludes (CaseSensitive), IncludeBlocks Regroup, IncludeCategories (related header, then C++ standard, then third-party/system, then `<hven/...>`, then quoted local).
   The categories are documented in `.clang-format` comments. Then clang-format (the repo config, clang-format 22.1.8) is run over EVERY C++ file in the five trees (the .h/.hpp/.cpp/.cc/.inl/.ipp
   files that git tracks there), one pure-format commit. ReflowComments will rewrap comments that run past 100 columns; that is expected.
3. Order-dependent headers (a file that no longer compiles, or compiles differently, once sorted) get an explicit fix at the dependent header (a missing include), not a clang-format off block;
   a clang-format off block is used only where unavoidable and is justified in RESULT.txt. Each fix is listed in RESULT.txt.
4. Exception 4 is removed: include/hven/detail/model/nlp_model.h includes `hven/core/types.h` instead of `hven/qp/qp_types.h`; every file that relied on getting qp_types.h through nlp_model.h gains an
   explicit `#include <hven/qp/qp_types.h>`. The design's exception list and the core-layering test's allow-list are updated if either names the pair.
Nothing else is edited: no identifier, no logic, no CMake file (except a CMake file's include-path spelling if one exists), no line-number citation.
Commit structure: (a) instruments, (b) re-spelling + exception 4 + order fixes (semantic), (c) .clang-format + pure-format commit (clang-format output ONLY), (d) blame-ignore, (e) evidence.
The pure-format commit is exactly `clang-format -i` over the file list of the parent: re-running it on that parent reproduces the commit byte-for-byte (checked and recorded).

## Pass criteria (Linux)
G1  clean configure, CCACHE_DISABLE=1, linux-clang-release preset flags + bench + examples; library + tests + bench + examples build, rc 0.
G2  P-SYM, both arms with `__FILE__` neutralised (tools/neutral_flags.py), same build path, no object map, no symbol map, EMPTY exceptions: 0 UNCLASSIFIED, 0 missing, 0 only-before/after.
    A reformat shifts `__LINE__` (every gtest TEST registration, asserts) and may reorder inline-function or template-instantiation emission; any object that is not byte-identical is
    reported per object, and every non-identical object must be explained per symbol (a line-number immediate class, or an emission-order class), never waved through. A changed symbol SET
    is a finding: its cause (a transitive include relied upon) is found and fixed, and the gate re-run on a successor.
G3  compile_commands.json flag diff (flags_diff2.py, empty map): identical, 0 TUs differ.
G4  scripts/check_pch_neutrality.sh passes (the PCH include set's spelling and order may change).
G5  ctest Release and Debug 100% (3869 tests); per-executable sorted gtest lists RAW-identical to the committed RS-0 lists and to the base measurement; ctest -N identical modulo the byte-dump class.
G6  check_install_smoke.sh on both front ends + check_export_contract.sh (installed headers compile standalone); installed listing equals the RS-3 listing (957 files; RS-4 adds/removes no file).
G7  27-cell replay x (walk, ssn, ipm): compare_replay.py 0 differences against the replay measured on the base by baseline2.sh; serial, solo, under the box lock.
G8  exception 4 gone: grep of nlp_model.h shows no qp_types include; the core-layering test (which encodes the allow-list) passes; the design page lists 12 pairs minus this one, as RS-5 will encode.
G9  CI on origin rs-4 (workflow_dispatch): all four lanes green (a Windows STL1000 configure failure is reported, not fixed here).
Wall-clock is informational and never asserted. Box rules (CLAUDE.md 7-8, box-sharing protocol) apply: one solver process at a time, under flock /tmp/box-build.lock.
