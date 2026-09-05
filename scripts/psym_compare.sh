#!/usr/bin/env bash
# P-SYM: per-symbol disassembly identity between two builds, with the accepted
# noise class classified mechanically rather than eyeballed.
#
# WHAT P-SYM IS. The phase-C proof vocabulary's per-batch default for a
# relocation commit is "the code the compiler emits did not change." A
# relocation moves files, so object bytes are NOT expected to be identical --
# `__FILE__` moves with the file and `__LINE__` moves with every inserted line,
# and both reach the object. What must be identical is the INSTRUCTIONS, per
# symbol. This script draws that line in one place so every batch draws it the
# same way, instead of each agent re-inventing a comparison and then widening
# it by hand when the first legitimate difference appears.
#
# USAGE
#
#   scripts/psym_compare.sh capture <snapshot-dir> [build-dir]
#   scripts/psym_compare.sh compare [options] <before-dir> <after-dir>
#
#     --object-map <file>   old-object-relpath => new-object-relpath
#     --symbol-map <file>   old-demangled-symbol => new-demangled-symbol
#     --exceptions <file>   demangled-symbol ## reason it may differ
#     --allow-foreign-captures
#                           compare a pair of captures made by a DIFFERENT
#                           version of this script (refused by default)
#
# Typical session, from a clean configure, CCACHE_DISABLE=1, same host, same
# preset (a cache hit would replay a stored object instead of compiling the
# source in front of you, which is the thing under test):
#
#   CCACHE_DISABLE=1 cmake --build build -j6
#   scripts/psym_compare.sh capture /tmp/psym-before
#   ... make the change ...
#   CCACHE_DISABLE=1 cmake --build build -j6
#   scripts/psym_compare.sh capture /tmp/psym-after
#   scripts/psym_compare.sh compare /tmp/psym-before /tmp/psym-after
#
# PRECONDITION, AND IT IS THE ONE MOST EASILY BROKEN: BOTH ARMS MUST BE BUILT
# AT THE SAME ABSOLUTE BUILD-DIRECTORY PATH. Note that the session above uses
# the literal path `build` for both captures, and that is not incidental. The
# build directory's absolute path reaches an object through more than one route
# -- `__FILE__` expansions in anything compiled out of the build tree, the
# configure-time stamps CMake generates into sources, and assertion/throw
# strings built from a source path -- so two arms compiled at, say,
# `build-before/` and `build-after/` will differ in ways that have nothing to
# do with the change under test, and the differences land in `.rodata`, where
# this script's TEXT comparison cannot see them to classify them. What it CAN
# see is the knock-on: a `.rodata` size change shifts later sections, which
# moves symbol addresses, which changes the rip-relative annotations
# `normalize()` strips -- so the failure mode is not a clean signal but a
# quiet, partial one. Configure ONE build directory, capture the before arm,
# rebuild IN PLACE, capture the after arm. This script cannot check the
# precondition for you: a capture snapshot records object bytes, not the path
# they came from, and inferring the path from the bytes is exactly the
# `.rodata` archaeology the comparison is defined not to do.
#
# Exit status is the verdict: 0 iff every object is either byte-identical or
# differs ONLY within the accepted noise class defined below, AND every object
# and every symbol on BOTH sides is accounted for (matched, mapped, or named in
# the exceptions file). Any other difference exits 1 and is printed with the
# offending instruction pair.
#
# THE OBJECT SET is the one the phase-C plan names for P-SYM, plus the five
# targets M6 W5 T0 added (see below): `libhven.a`, every `hven_sqp_tests`,
# `hven_interior_tests`, `hven_model_tests` and `hven_tests` object, and the
# objects of the bench binaries the asserted artifacts come from
# (`hven_sqp_corpus`, `hven_sqp_bench`, `hven_sqp_ssn_safeguard_probe`,
# `hven_sqp_f7_cold`, `hven_sqp_tau_bar_sweep_probe`, `hven_sqp_crossover`,
# `hven_sqp_ipqp_e1_arm`). Debug objects are NOT part of P-SYM -- they differ
# trivially, and Debug correctness is P-SUITE's job.
#
# The set is enumerated GROUP by group (one group per CMake target), and
# `capture` fails naming the group if any group yields zero objects: a target
# directory that a CMake rename or a partial build left empty would otherwise
# shrink the instrument silently, and objects outside the instrument are
# objects nobody guards.
#
# The last two of the original bench set joined in phase-C T6 (2026-08-19).
# They were omitted originally because their artifacts are archived studies
# rather than gated baselines -- but three consecutive T-tasks (T4, T5, T6)
# each had to declare them as objects their blast radius covered and the script
# did not compare, and each had to reconcile that arithmetic by hand in its
# report. A probe the instrument cannot see is a probe whose regressions nobody
# is looking for; `bench/CMakeLists.txt` already makes the same argument for
# BUILDING them ("an uncompiled probe rots silently"), and this is that
# argument applied one step further. Both are ordinary
# `bench/CMakeFiles/<target>.dir` object sets, so nothing about the comparison
# changes -- only its coverage.
#
# `hven_interior_tests`, `hven_model_tests`, `hven_sqp_crossover`,
# `hven_tests` and `hven_sqp_ipqp_e1_arm` joined in M6 W5 T0 (2026-09-05) for
# the same reason one step further again: W5 changes the IPM callback surface,
# the model layer's type names and the driver's TU structure, and the suites
# that exercise those surfaces -- plus the crossover runner that links the
# bench-side adapter -- were outside the instrument. `hven_tests` (core and
# linear) joined with them: it is 11 cheap objects and it is the only cover
# over the core/linear headers a W5 task touches. `hven_sqp_ipqp_e1_arm` joined
# because the reason first given for excluding it was wrong -- it is the
# producer of the A4 gate CSV that is compared cell-for-cell against
# `docs/notes/data/2026-08-m6-w1-acceptance/a4-gate-mu1e-2.csv` at every task
# that touches the QP engine, which is a gated artifact by any reading, and its
# single object is the one thing a rename can move that `libhven.a` does not
# cover.
#
# Targets deliberately still OUTSIDE the set, so their absence is a stated
# choice rather than an oversight: `hven_fault_injection_tests` and
# `hven_ipqp_seam_tests` (both recompile library TUs a second time under
# `HVEN_TESTING`, so their objects are not the production library's code and
# would report a permanent, meaningless difference against it), the
# `hven_golden_rig*` targets (they compile against pinned OLD-SEAM checkouts,
# not this tree alone), the `hven_compile_fail_*` targets (they are expected
# NOT to compile), and `hven_sqp_snopt_f7` (optional, and gated behind a source
# firewall this repository does not cross).
#
# Adding a target here WIDENS what must be identical, so it can only make the
# instrument stricter, never laxer. A snapshot captured before such a change
# has fewer entries in its `.psym-manifest` than one captured after. That used
# to be a silent skip; since M6 W5 T0 it cannot be, on two counts: `compare`
# now walks BOTH manifests (an object present only in the after arm is a
# finding, not an omission), and every capture records the sha256 of the script
# that made it in `.psym-version`, which `compare` refuses to proceed past when
# the two arms disagree. Capture both arms with the same version of this
# script; the runner stamps its own sha into the compare header so a report
# says which version drew the line.
#
# Since M6 W5 T0 fix2 that refusal extends to the RUNNER: a pair of captures
# whose stamp differs from the running script's own sha256 is REFUSED, not
# merely noted. The rule it enforces is that EVERY ARM OF A W5 COMPARISON IS
# CAPTURED BY THE CURRENT TOOL, so the declared object set is the set that was
# actually compared -- an old pair otherwise passes on an old, narrower
# universe and this runner cannot say what that universe left out.
# `--allow-foreign-captures` proceeds anyway and prints a note saying the set
# compared is the OLDER script's. It is for forensics on an archived snapshot
# pair, never for a gate.
#
# THE ACCEPTED NOISE CLASS, stated so that widening it is a visible act:
#
#   (a) `__LINE__`-class -- an immediate-constant move whose two sides are the
#       SAME instruction with the SAME destination, differing only in the
#       immediate: `mov $0xNNN,%reg` or `movl $0xNNN,offset(%reg)`. This is how
#       `__LINE__` reaches an object: GoogleTest's EXPECT_/ASSERT_ macros, and
#       every assert/throw site, materialize the line number as an immediate.
#       Inserting or deleting a line above such a site shifts the constant and
#       nothing else.
#
#   (b) `__FILE__`-class -- string CONTENT. It is accepted by construction
#       rather than by rule: `objdump -d` disassembles executable sections, and
#       a file-name string lives in `.rodata`. A changed `__FILE__` therefore
#       does not appear in this comparison at all, and the rip-relative
#       reference to it disassembles identically (displacement 0 plus a
#       relocation) in both objects. This script proves TEXT identity and says
#       nothing about `.rodata`; that is exactly the licensed difference, not a
#       gap being papered over.
#
# NORMALIZATION NOTE: `objdump -d --no-show-raw-insn` appends a trailing
# annotation comment to rip-relative and branch/call instructions -- e.g.
# `lea 0x0(%rip),%rax        # 7 <sym+0x7>` -- carrying an absolute or
# section-relative target address. That address is a function of overall
# layout (e.g. an unrelated object's size shifting a later section), not of
# the instruction's own bytes, so two builds with byte-identical instructions
# can show different annotations and vice versa. `normalize()` strips
# everything from the first ` #` (space then comment marker) to end-of-line
# before comparison. This was checked against every disassembled line in this
# repo's build tree (149k+ `#`-bearing lines across all P-SYM objects): the
# marker is always a single ` #` per line, always preceded by whitespace, and
# always of the form `# <hex> <sym+off>` -- objdump never emits a bare `#`
# inside operand text under `--no-show-raw-insn`, so this strip cannot mangle
# a legitimate instruction.
#
# The same layout-derived argument applies to a SECOND annotation objdump
# emits inline on branch/call targets rather than as a trailing comment --
# e.g. `call   27f5 <.L.str.137+0x466>` -- where the bracketed
# `<symbol+off>` is objdump's nearest-symbol-by-address label for the target,
# not part of the instruction: a `.rodata` size change elsewhere in the
# object shifts what symbol is nearest, so the SAME call to the SAME address
# gets a different label across builds. `normalize()` strips a trailing
# `<...>` group the same way it strips the trailing `#` comment, leaving the
# target address and the mnemonic/operands untouched -- only the cosmetic
# symbolization goes.
#
# RELOCATION TARGETS (M6 W5 T0 fix2). Until fix2 this script disassembled with
# `objdump -d` and compared DEFINED symbols only, on both paths. That left one
# hole, and it was the hole that matters most to what W5 is about. In a
# relocatable object a direct call to a symbol outside the current section is
# `e8 00 00 00 00` plus a relocation; `objdump -d` prints the zero placeholder
# as an address labelled with the ENCLOSING symbol, and the callee's name
# appears NOWHERE in that listing. Two objects that call two DIFFERENT external
# functions of the same signature at the same offset therefore disassemble
# identically, and the census sees nothing either, because the callees are
# undefined here. P-SYM passed such a pair silently. `-r` closes it: the
# relocation line is the only place the callee's name lives, and the per-symbol
# path now folds each relocation into the instruction it annotates and compares
# `<type> <target> <addend>` alongside the instruction text.
#
#   * The POSITIONAL path's INPUT is unchanged. `normalize_raw()` deletes the
#     relocation lines before the classifier sees them, so the positional
#     listing is byte-for-byte the one `objdump -d` produced and the classifier
#     returns exactly the verdict it always did. This was verified across every
#     disassembled object in the set (128 of the 129; `libhven.a` is an archive
#     and is not disassembled): 0 differences.
#
#   * WHEN the relocation comparison runs. A byte-identical object skips it,
#     because identical bytes are identical relocations. Any other object is
#     first classified positionally, exactly as before; if that lands in the
#     accepted noise class, the raw relocation streams are compared literally,
#     and if THEY agree too the object is finished with the output it has
#     always produced. Only an object whose relocations actually differ falls
#     through to the per-symbol layer, which renders each target through the
#     symbol map and then either matches it or reports the pair. This is a
#     DECLARED widening of what reaches the per-symbol layer, and it is the
#     only arrangement in which the tool can see its own falsifier: the
#     same-shape callee substitution above has a noise-class instruction stream
#     BY CONSTRUCTION, so a relocation check that only ran behind a positional
#     failure would never once run on the case it exists for.
#
#   * HOW a target is rendered, by class, since not every relocation names a
#     symbol:
#
#       - A NAMED SYMBOL (the target does not begin with `.`) is demangled,
#         has the symbol map applied on the before arm, and keeps its addend
#         VERBATIM. No disambiguating tag is applied: a relocation names a
#         symbol, not a variant, and the referenced symbol is usually not
#         defined in this object at all. The demangling for targets uses its
#         own plain table, kept separate from the one that tags DEFINED
#         symbols, so a mere reference can never perturb those tags.
#
#       - A SECTION (`.rodata`, `.rodata._ZN3fmt...`, `.data.rel.ro`) is
#         compared by section NAME, with an embedded mangled name demangled and
#         mapped like any other, and its addend NEUTRALISED to `+LOCAL`. The
#         addend of a section-relative relocation is the byte offset of a datum
#         within a section this comparison does not read, and it moves whenever
#         anything ahead of it in that section changes size -- the same layout
#         property the `__FILE__` class (b) and the SELF rule already exclude.
#         The section NAME is stable under a split and is compared.
#
#       - A COMPILER-LOCAL LABEL (`.L.str.137`, `.LCPI8_0`, `.Lswitch.table._Z...`)
#         is compared by its FAMILY: digit runs outside any embedded mangled
#         name are replaced by `N`, and the addend is neutralised as above. An
#         assembler-local label is a name the compiler mints per TU in emission
#         order; splitting a TU or adding one string literal renumbers it
#         wholesale, and it names data in a non-executable section, so its
#         number carries no information this comparison is entitled to assert
#         on. What survives is the family, so a reference that moved from a
#         constant pool to a string table is still a finding.
#
#     THE STATED LIMIT of the two neutralisations: a change that makes a call
#     site reference a DIFFERENT string literal or a different constant of the
#     same kind is not visible. It was not visible before fix2 either -- the
#     content lives in `.rodata`, which is class (b) -- so this is the existing
#     coverage limit restated at a finer grain, not a new one.
#
#   * WHAT IT DOES FOR A SPLIT. A cross-half call whose callee already had
#     EXTERNAL linkage is a PLT32 relocation on BOTH arms (clang does not
#     resolve such a call intra-section even within one TU, measured), so the
#     SELF rule neutralises the placeholder, the relocation records are
#     literally equal, and the caller MATCHES across the split. The residue the
#     fix1 header warned of is narrower than it said: it is a callee that was
#     INTERNAL-linkage (`static`, or in an anonymous namespace) before the
#     split. There the assembler really did resolve the call with no relocation
#     at all, and the split had to give the callee external linkage to move it.
#     That pair is deliberately NOT equated. The linkage change is a real
#     difference -- an interposable call through a PLT is not the call the
#     assembler resolved -- and the two mangled names (`_ZN4demoL6delta2Ei`
#     and `_ZN4demo6delta2Ei`) share one demangled name, so the `[#n]` rank
#     fallback reports the pair as ONLY-BEFORE/ONLY-AFTER as well. Both
#     findings are false in the sense that the CODE is unchanged and true in
#     the sense that the LINKAGE is not; the honest handling is a named
#     exception carrying that reason, not a further widening.
#
# Everything else is a real difference. In particular an instruction-COUNT
# change is never noise: the script reports it as STRUCTURAL and refuses to
# classify that object further, because once the counts differ, pairing lines
# positionally stops meaning anything.
#
# THE ONE KNOWN LIMIT OF CLASS (a), stated rather than hidden: a genuine change
# to a numeric constant in an otherwise untouched instruction is
# indistinguishable from a line-number shift by shape alone. That is why the
# compare step PRINTS the set of observed immediate deltas per object. A line
# shift produces a small number of distinct deltas, each equal to a count of
# inserted/deleted lines; a changed constant shows up as a lone odd delta with
# no siblings. Read that summary -- it is the half of the classification a
# regex cannot do for you.
#
# COVERAGE LIMIT (inherited from P-SYM itself): the Accelerate `#ifdef` arms
# are not compiled on Linux at all, so nothing here says anything about them.
#
# ---------------------------------------------------------------------------
# M6 W5 T0: BIDIRECTIONAL COVERAGE, DEMANGLED-NAME MATCHING, MAPS, EXCEPTIONS
# ---------------------------------------------------------------------------
#
# WHY. Up to W5 this script compared normalized listings POSITIONALLY within
# each object and walked only the BEFORE manifest. That is exactly right for a
# relocation -- the same TU compiled from a moved file -- and exactly wrong for
# the two things M6 W5 does: T6 SPLITS a translation unit (one old object's
# symbols land in two new objects, and every symbol's start address moves), and
# T8 RENAMES types (every mangled name changes, and Itanium mangling encodes
# identifier LENGTHS -- `17AggregateEvalSeam` becomes `16AssemblyEvalSeam` --
# so no textual substitution on a mangled name can be trusted). Positional
# pairing under either change reports STRUCTURAL noise and proves nothing.
#
# WHAT WAS ADDED, and what deliberately did NOT change:
#
#   * The `capture` mode is unchanged except for the wider object set and the
#     new `.psym-version` stamp.
#
#   * The NO-MAP PASS path through `compare` is unchanged. For a group of one
#     old object and one new object with no map entry in play, this script
#     still runs the byte comparison first and then the SAME positional
#     classification, emitting the SAME `IDENTICAL` / `NOISE-ONLY` /
#     `DIFFERS` / `MOVED` / `MISSING` lines and the same trailing
#     `P-SYM: N objects - ...` summary line. A relocation batch that passed
#     before reads exactly as it did before. The per-symbol layer engages only
#     where that positional comparison FAILS, or where a map is in play -- so
#     it can never turn a former PASS into anything else.
#
#     DECLARED CHANGE TO WHAT A NO-MAP PASS ASSERTS (M6 W5 T0 fix1). What the
#     sentence above does NOT say, and what is stated here rather than left to
#     be inferred: a positional FAILURE now falls through to the per-symbol
#     verdict EVEN WITH NO MAP IN PLAY, and that verdict is deliberately
#     address-blind and trailing-padding-blind (see the two paragraphs on the
#     per-symbol path below). So an object whose only difference is symbol
#     REORDERING, an address shift, or an end-of-block alignment-nop run --
#     which the pre-W5 script reported as `DIFFERS` and exit 1, because
#     `normalize()` leaves the address on each `0000... <_Zsym>:` header line
#     -- is now `DIFFERS` followed by a PASSING `PER-SYMBOL` line and exit 0.
#     The same declaration covers the SELF-relative branch/call targets
#     documented at flatten_symbols(): on the per-symbol path a control
#     transfer whose objdump label names the enclosing symbol is compared by
#     its OFFSET, not by its absolute address in the object. Without that rule
#     no TU split can pass at all, because this project compiles without
#     `-ffunction-sections`: every function of a TU shares one `.text`, so
#     splitting the TU moves every function that was not first, and every
#     relocation-target address printed inside them moves with it.
#
#     That widening is intentional: P-SYM is defined as per-symbol INSTRUCTION
#     identity (docs/notes/2026-08-21-m4-plan-gate-review-hven.md:85), and the
#     positional path's address sensitivity was an implementation consequence
#     of comparing flat listings, not a term of the contract. It is recorded
#     here because a future relocation batch that would once have failed can
#     now pass, and CLAUDE.md section 7 requires such a change to be declared,
#     not discovered.
#
#     DECLARED CHANGE, THE OTHER DIRECTION (M6 W5 T0 fix2). fix2 makes a
#     no-map PASS STRICTER as well: relocation targets are compared, so an
#     object that used to pass -- byte-different, instruction stream in the
#     accepted noise class -- now FAILS if any call reaches a differently
#     NAMED callee. Calibration (b) below is exactly that case: the two caller
#     objects of the renamed function passed as NOISE-ONLY at fix1 and fail at
#     fix2 without a symbol map. This is the whole point of the round, and it
#     is declared for the same reason the widening above is: a batch that
#     passed before can now fail, and no one should have to discover that from
#     a transcript.
#
#   * SYMBOL ACCOUNTING is new and runs on EVERY object in the set, both
#     arms, always. It is `nm --defined-only` -- every DEFINED symbol, text and
#     data alike -- put through `c++filt` by demangle_table(), which is also
#     where the disambiguating tag of the next paragraph is computed; no
#     textual surgery is ever performed on a mangled name. Each old symbol name
#     then has the symbol map applied to it;
#     the result must appear on the new side, and every new-side symbol must be
#     claimed by exactly one old-side symbol. A symbol on one side only is a
#     FINDING (`ONLY-BEFORE` / `ONLY-AFTER`), not a silent skip. This is what
#     makes the coverage bidirectional at the symbol level; the object level is
#     bidirectional because `compare` now walks the AFTER manifest too and
#     reports any object no BEFORE object claimed.
#
#     Why symbol accounting is not redundant with the instruction comparison:
#     it runs even on byte-identical objects (where it is free -- `nm` is
#     milliseconds against `objdump -d`'s tens of seconds on the large test
#     objects) so the report can state a real matched-symbol count, and it
#     covers data symbols (vtables, typeinfo, string tables) that a text-only
#     disassembly never sees but a type rename certainly moves.
#
#   * INSTRUCTION IDENTITY BY DEMANGLED NAME. Where the positional path does
#     not apply or does not pass, the two sides' disassemblies are split into
#     per-symbol blocks and paired by DEMANGLED name (through the symbol map),
#     not by position. Within a matched pair the classifier is the same awk
#     program the positional path uses -- byte for byte, one copy, called
#     twice -- so the accepted noise class is identical in both paths and
#     widening it is still a single visible act.
#
#     A THIRD layout artifact the per-symbol path excludes, and the positional
#     path does not: TRAILING inter-function alignment padding. objdump
#     attributes the assembler's inter-function nops to the symbol they follow,
#     so a function that was followed by another in the same section and is now
#     last in its own object loses them. That is a consequence of the split, not
#     of the code; only a run at the END of a block is trimmed, padding inside a
#     function is compared, and the count of symbols whose trailing padding
#     differed is reported on the PERSYM line so the exclusion is visible.
#
#     Two things are deliberately NOT compared per symbol: the symbol's start
#     ADDRESS (it is layout -- under a TU split every address moves, and P-SYM
#     is defined as per-symbol INSTRUCTION identity, see
#     docs/notes/2026-08-21-m4-plan-gate-review-hven.md:85) and the symbol's
#     own name inside its section name (`.text._Zold` vs `.text._Znew` is the
#     rename, not a code change). The section name IS compared, with the
#     symbol's own mangled name replaced by a placeholder on both sides, so a
#     symbol that moved from `.text` to `.text.unlikely` is still a finding.
#
#   * COLLISION DETECTION. The symbol map is rejected if two lines share an
#     old name or share a new name. Per object, it is an error if two old
#     symbols end up claiming one new name -- whether because two map entries
#     point at it, or because a map target is a name that ALREADY exists on the
#     old side unmapped. A map that quietly merges two symbols would let a real
#     deletion pass as a rename, which is the one failure mode a rename proof
#     must not have.
#
#   * EXCEPTIONS. `--exceptions <file>` names symbols allowed to differ, or to
#     exist on one side only, each WITH a reason. The reason is printed
#     verbatim on the line that consumes it, so a report cannot contain an
#     unexplained exception. An exception nobody used is printed as
#     `STALE-EXCEPTION` (it does not fail the run -- it is a claim that is no
#     longer needed, not a difference).
#
#     An exception may always be written in the PLAIN demangled form this
#     file's FILE FORMATS block documents, on EITHER side. A finding on a
#     tagged key (`[D1]`, `[C2]`, `[_ZThn8_]`, `[#n]`) is matched first against
#     the tagged key and then against the symbol's bare demangled name, and
#     that fallback is symmetric: `ONLY-BEFORE`, `ONLY-AFTER` and `DIFFERS` all
#     honour it. Writing the internal tagged form is never required, and the
#     tag is an implementation detail this script is free to change.
#
#     Why that symmetry matters, concretely: the thunk tag carries the
#     ADJUSTMENT (`_ZThn8_` vs `_ZThn16_`), not just the fact of being a thunk.
#     A change that removes or reorders a base class -- M6 W5 T1 -- moves a
#     thunk's adjustment, so the same function's thunk is a different mangled
#     name and a different tag on the two arms, and it reports as an
#     `ONLY-BEFORE` plus an `ONLY-AFTER` pair rather than as one `DIFFERS`.
#     That is the honest report (the thunk really is a different symbol), and
#     ONE plain-name exception excuses both halves of the pair.
#
#   * STALE MAP ENTRIES. A `--symbol-map` line whose OLD name no before-arm
#     symbol carries, and a `--object-map` line whose OLD relpath is not in the
#     before manifest, are each printed as
#     `STALE-MAP <old> (mapped to <new>; nothing used it)` and counted on the
#     coverage line. Non-fatal, exactly like `STALE-EXCEPTION`: a dead map line
#     is a claim that is no longer needed, not a difference. It is reported
#     because a generated map (T8's will be) that drifts by one line against
#     its generator produces a wall of `ONLY-BEFORE`/`ONLY-AFTER` findings
#     whose actual cause is the dead line, and the tempting repair -- an
#     exceptions entry -- would bury a real rename.
#
#     The two map kinds have SEPARATE used-namespaces (M6 W5 T0 fix2): an old
#     symbol name and an old object relpath are different sorts of string, and
#     when they shared one namespace a coincidental equality between them let
#     one map's consumption suppress the other's `STALE-MAP`. A symbol-map
#     entry consumed only by a RELOCATION target counts as used, since a
#     relocation is a real consumer of the map.
#
#   * DUPLICATE SYMBOLS ACROSS A SPLIT. A split can legitimately place the SAME
#     weak/COMDAT body (an inline function, a template instantiation) into BOTH
#     new objects. Every copy is kept: copy 1 is the one compared against the
#     before arm, and copies 2..n are compared instruction-by-instruction
#     against copy 1 through the same classifier, reported as
#     `DUPLICATE-IDENTICAL <sym>` or `DUPLICATE-DIFFERS <sym>`. A DIFFERS
#     counts as a differing symbol and FAILS the run. The bar for a duplicate
#     is stricter than the accepted noise class deliberately: two copies in the
#     SAME build arm come from the same source through the same preprocessing,
#     so not even a `__LINE__` immediate can legitimately differ between them,
#     and an unequal pair is an ODR violation (typically a macro that changes
#     the inline body in one half) -- exactly the failure a TU split can
#     introduce and nothing else in this script would see. The
#     `DUPLICATE-IDENTICAL` lines are capped at 20 per object; the count is on
#     the `PERSYM` line, and no `DUPLICATE-DIFFERS` line is ever suppressed.
#
#   * RANK-TAGGED KEYS ARE COUNTED. The `[#n]` fallback described below is the
#     one keying construct that is NOT rename-stable, so the number of keys
#     that needed it is reported per object on the `PERSYM` line
#     (`N rank-tagged`) and in total on the coverage line, and a run that mints
#     one WHILE a symbol map is in play prints a `P-SYM warning:` saying so. A
#     report can then state the measured number instead of asserting that the
#     fallback did not occur.
#
#   * THE SUMMARY LINE'S BUCKETS. The original `P-SYM: N objects - ...` line
#     keeps its format and its meanings: `differing within the accepted noise
#     class` counts POSITIONAL noise-class passes only, and never absorbs an
#     object that passed per symbol. Per-symbol passes are their own bucket,
#     `K objects passed per symbol`, on the coverage line. Those five counts
#     plus that bucket account for every object in the set, with two stated
#     qualifications: `matched by basename after a path move` is a CROSS-CUTTING
#     tally (a moved object is also counted under whichever bucket its
#     comparison lands in), and `libhven.a`, when it differs, is reported as
#     `ARCHIVE` and counted in none of them.
#
#     One consequence of the relocation comparison, since fix2: an object whose
#     instruction stream is noise-class prints `NOISE-ONLY` and is counted in
#     that bucket BEFORE its relocations are looked at. If they then disagree,
#     a `RELOCATIONS` line says so, the `PER-SYMBOL` line below it is the
#     verdict exactly as it is under `DIFFERS`, and the object is moved out of
#     the noise bucket into `with unclassified differences`. The counts on the
#     summary line always describe the FINAL verdicts; a `NOISE-ONLY` line
#     followed by a `RELOCATIONS` line is a statement about the instruction
#     stream alone.
#
# FILE FORMATS (comments are whole lines beginning with optional spaces then
# `#`; blank lines ignored; leading/trailing spaces on each field trimmed):
#
#   --object-map    <old relpath> => <new relpath>
#                   Relative to the snapshot root, i.e. exactly as the paths
#                   appear in `.psym-manifest`. REPEAT the same old path on
#                   several lines to declare a SPLIT: the old object's symbols
#                   are then matched against the UNION of the listed new
#                   objects. Many-to-one is rejected (two old objects may not
#                   claim one new object).
#
#   --symbol-map    <old demangled> => <new demangled>
#                   Split on the FIRST ` => ` (space-arrow-space). Demangled
#                   C++ names do not contain that sequence; mangled names are
#                   never written in these files, precisely because their
#                   length prefixes make textual substitution unsound.
#
#   --exceptions    <demangled name> ## <reason>
#                   Split on the FIRST ` ## `. The name is the NEW-side name
#                   for a symbol that exists after, the OLD-side name for one
#                   that existed only before, and either for a mapped pair.
#
# CALIBRATION, so a later run has a baseline to recognize. Phase-C task C0.3
# (commit 6875712, a comments-and-docs-only change) was measured with exactly
# this method: 60 objects, 54 byte-identical -- including all of `libhven.a`
# and all of `bench/` -- and the 6 that differed were exactly the 6 directly
# edited TUs, with identical instruction counts and all 885 changed
# instructions in class (a). That is the floor: a comment-only edit perturbs
# only the edited TU, and only through `__LINE__`. A relocation batch should
# show that class and nothing else. The per-object DELTAS summaries are what
# make that readable: each of the six reported a single delta equal to the
# number of comment lines inserted above the affected assertions (+7, +8, +7,
# +5, +6, and +7/+9 for the one file edited in two places).
#
# The same calibration was run in the negative, because a classifier that
# cannot fail proves nothing: adding two real statements to one of those TUs
# and re-comparing reports it as STRUCTURAL (the normalized listing gains
# lines) and exits 1, with the other 59 objects still byte-identical.
#
# M6 W5 T0 re-calibrated the extended tool the same way, in both directions;
# fix1 re-ran both calibrations against the wider object set, and fix2 re-ran
# them again with relocations compared: 129 objects and 125145 defined symbols.
# The fix2 transcripts are pasted in `.superpowers/w5-t0-fix2-report.md`.
#
#   (a) a FULL rebuild of the same commit into the SAME absolute build path
#       with CCACHE_DISABLE=1 (`ninja -t clean` then rebuild, so every object
#       is genuinely recompiled rather than replayed): 129/129 byte-identical,
#       125145 symbols matched, 0 findings, 0 rank-tagged keys, PASS. Every
#       object being byte-identical, no relocation record needs comparing --
#       identical bytes are identical relocations -- so the relocation counters
#       read 0 and the set's 607085 relocation records (measured) are asserted
#       by byte identity, which is the stronger statement.
#
#   (b) a deliberate one-symbol rename -- `hven::solvers::census_variable_bounds`
#       to `..._probe`, its declaration, its definition and both call sites,
#       with the fmt string literals left alone so nothing but the name moves.
#       WITHOUT a map: `ipqp_trace.cpp.o` reports exactly one `ONLY-BEFORE` and
#       one `ONLY-AFTER`, and -- since fix2 -- the two CALLER objects, whose
#       instruction streams are still identical, FAIL on the relocation target
#       that names the renamed callee. WITH a one-line symbol map: all three
#       pass, the definition through the symbol map and the callers through
#       their relocations. The mutant was reverted; it is not committed.
#
#   The fix1 round additionally exercised, on purpose-built fixtures rather
#   than on this tree: a plain-name exception excusing a TAGGED `ONLY-AFTER`;
#   a stale symbol-map source and a stale object-map source (`STALE-MAP`, both
#   non-fatal); an inline body emitted into BOTH halves of a split
#   (`DUPLICATE-IDENTICAL`, PASS); and the ODR-violating variant of the same
#   split (`DUPLICATE-DIFFERS`, exit 1). fix2 added, on the same fixtures: a
#   same-shape substitution of one external callee for another (silently PASSed
#   before fix2, `DIFFERS` naming both relocation targets after it), the same
#   substitution declared in a symbol map (PASS, matched through the map on the
#   relocation), a split whose cross-half callee was already external (MATCHES)
#   and one whose callee was `static` before the split (stated limit, DIFFERS),
#   an object-map path and a symbol-map name that collide as strings (the stale
#   symbol-map entry was suppressed before fix2 and is reported after it), and
#   a capture pair stamped by a foreign tool version (REFUSED; PASS under
#   `--allow-foreign-captures`).

set -euo pipefail

usage() {
    echo "usage: $0 capture <snapshot-dir> [build-dir]" >&2
    echo "       $0 compare [--object-map <f>] [--symbol-map <f>] [--exceptions <f>]" >&2
    echo "                  [--allow-foreign-captures] <before-dir> <after-dir>" >&2
    exit 2
}

OBJDUMP="${OBJDUMP:-objdump}"
NM="${NM:-nm}"

# This script's own identity, stamped into every capture and printed by every
# compare. `compare` refuses two captures whose stamps disagree: the object SET
# is a property of the capturing script, so an old-vs-new pairing would compare
# different universes and call the missing half nothing at all.
PSYM_SELF="${BASH_SOURCE[0]}"
tool_sha() {
    sha256sum "${PSYM_SELF}" | cut -d' ' -f1
}

# The P-SYM object set, as GROUPS: one entry per CMake target, "<group name>:
# <object directory relative to the build dir>". Kept in one place so
# `psym_objects` and `do_capture`'s non-empty assertion cannot disagree about
# what P-SYM covers, and so a group that yields nothing is named in the error.
PSYM_GROUPS=(
    "hven:CMakeFiles/hven.dir"
    "hven_sqp_tests:tests/sqp/CMakeFiles/hven_sqp_tests.dir"
    "hven_interior_tests:tests/interior/CMakeFiles/hven_interior_tests.dir"
    "hven_model_tests:tests/model/CMakeFiles/hven_model_tests.dir"
    "hven_tests:tests/CMakeFiles/hven_tests.dir"
    "hven_sqp_corpus:bench/CMakeFiles/hven_sqp_corpus.dir"
    "hven_sqp_bench:bench/CMakeFiles/hven_sqp_bench.dir"
    "hven_sqp_ssn_safeguard_probe:bench/CMakeFiles/hven_sqp_ssn_safeguard_probe.dir"
    "hven_sqp_f7_cold:bench/CMakeFiles/hven_sqp_f7_cold.dir"
    "hven_sqp_tau_bar_sweep_probe:bench/CMakeFiles/hven_sqp_tau_bar_sweep_probe.dir"
    "hven_sqp_crossover:bench/CMakeFiles/hven_sqp_crossover.dir"
    "hven_sqp_ipqp_e1_arm:bench/CMakeFiles/hven_sqp_ipqp_e1_arm.dir"
)

# Emits "<group><TAB><object path>" for every object in the set.
psym_objects() {
    local build="$1" entry name dir
    for entry in "${PSYM_GROUPS[@]}"; do
        name="${entry%%:*}"
        dir="${entry#*:}"
        find "${build}/${dir}" -name '*.o' 2>/dev/null | LC_ALL=C sort \
            | sed "s|^|${name}\t|"
    done
}

do_capture() {
    local snapshot="$1"
    local build="${2:-build}"

    if [ ! -d "${build}" ]; then
        echo "psym_compare: no such build directory: ${build}" >&2
        exit 1
    fi

    mkdir -p "${snapshot}"
    : > "${snapshot}/.psym-manifest"

    local count=0 obj rel group entry name
    declare -A group_count=()
    while IFS=$'\t' read -r group obj; do
        rel="${obj#"${build}"/}"
        mkdir -p "${snapshot}/$(dirname "${rel}")"
        cp "${obj}" "${snapshot}/${rel}"
        echo "${rel}" >> "${snapshot}/.psym-manifest"
        count=$((count + 1))
        group_count["${group}"]=$(( ${group_count["${group}"]:-0} + 1 ))
    done < <(psym_objects "${build}")

    # Every group must be non-empty. A target directory a CMake rename or a
    # partial build left empty would otherwise shrink the object set silently,
    # and the `.psym-version` stamp only catches shrinkage BETWEEN versions --
    # it cannot catch a group going missing within one.
    local empty=""
    for entry in "${PSYM_GROUPS[@]}"; do
        name="${entry%%:*}"
        if [ "${group_count["${name}"]:-0}" -eq 0 ]; then
            empty="${empty}${empty:+ }${name}"
        fi
    done
    if [ -n "${empty}" ]; then
        echo "psym_compare: these P-SYM object groups are EMPTY in ${build}: ${empty}" >&2
        echo "              Every group in PSYM_GROUPS must contribute at least one object;" >&2
        echo "              an empty group is a silently narrowed instrument. Build the" >&2
        echo "              missing targets (or fix their directory in PSYM_GROUPS) and" >&2
        echo "              recapture." >&2
        exit 1
    fi

    if [ -f "${build}/libhven.a" ]; then
        cp "${build}/libhven.a" "${snapshot}/libhven.a"
        echo "libhven.a" >> "${snapshot}/.psym-manifest"
        count=$((count + 1))
    else
        echo "psym_compare: ${build}/libhven.a is missing -- the library itself is the" >&2
        echo "              one object P-SYM can never be without." >&2
        exit 1
    fi

    tool_sha > "${snapshot}/.psym-version"
    echo "captured ${count} objects from ${build} into ${snapshot}"
    for entry in "${PSYM_GROUPS[@]}"; do
        name="${entry%%:*}"
        printf '  %-32s %d\n' "${name}" "${group_count["${name}"]}"
    done
    printf '  %-32s %d\n' "libhven.a" 1
    echo "psym_compare: tool sha256 $(tool_sha)"
}

# Normalized disassembly: instruction text only, no addresses, no raw bytes,
# no file-name header, no trailing address-target annotation, no inline
# call/jump-target symbolization (see the NORMALIZATION NOTE above). Symbol
# header lines ("0000... <_Zfoo>:") survive deliberately -- they are what
# makes this a PER-SYMBOL comparison rather than one flat instruction stream,
# so a symbol that moved between sections shows up as a difference instead of
# cancelling out.
# `-r` interleaves each relocation as its own line, indented, immediately after
# the instruction it annotates:
#
#     31:	call   36 <..caller..+0x36>
#     		32: R_X86_64_PLT32	_ZN4hven7solvers7NlpEvalC2Ev-0x4
#
# That line is the ONLY place a direct external callee's NAME appears -- the
# instruction itself carries a zero placeholder that objdump labels with the
# ENCLOSING symbol -- so the per-symbol path needs it (see the RELOCATION
# TARGETS block in this file's header). Adding `-r` does not change the three
# header lines `tail -n +3` drops, and it does not change any instruction line.
raw_disasm() {
    "${OBJDUMP}" -dr --no-show-raw-insn "$1" | tail -n +3
}
# The POSITIONAL path's input, which must stay exactly what it was before `-r`
# was added: the relocation lines are deleted here, before any comparison, so
# the positional listing is byte-identical to the one `objdump -d` produced.
# The delete pattern cannot touch an instruction line: an instruction's address
# is followed by a COLON-TAB, a relocation's offset by a COLON-SPACE.
normalize_raw() {
    sed -e '/^[ \t][ \t]*[0-9a-f][0-9a-f]*: [^ \t]/d' \
        -e 's/^ *[0-9a-f]*://' -e 's/[ \t]\+#.*$//' -e 's/[ \t]*<[^<>]*>[ \t]*$//'
}
normalize() {
    raw_disasm "$1" | normalize_raw
}

# Scratch directory for the normalized listings. Deliberately NOT a `local` in
# do_compare: the EXIT trap runs after that function has returned, and a local
# would be out of scope by then -- which under `set -u` turns a clean pass into
# a spurious failure.
#
# cleanup_tmp's own exit status matters: bash adopts an EXIT trap's exit
# status as the whole script's exit status whenever that status is nonzero
# (this is how a genuine `set -e` abort survives the trap, since the trap
# itself then exits 0 and the prior nonzero status is left standing -- but it
# also means a trap that itself exits nonzero silently overwrites a real
# success). `[ -n "${PSYM_TMP}" ] && rm -rf ...` exits nonzero via its own
# short-circuit whenever PSYM_TMP is unset -- true throughout `capture` mode,
# which never touches PSYM_TMP -- turning every successful capture into a
# reported failure. The `if` form below exits 0 when there is nothing to
# clean up, so the trap only ever reports failure for a genuine `rm` failure.
PSYM_TMP=""
cleanup_tmp() {
    if [ -n "${PSYM_TMP}" ]; then
        rm -rf "${PSYM_TMP}"
    fi
}
trap cleanup_tmp EXIT

# ---------------------------------------------------------------------------
# The classifier. ONE copy, used by the positional path and the per-symbol
# path alike, so the accepted noise class cannot drift between them. Reads two
# normalized listings and writes its verdict to stdout; exit 0 = within the
# noise class, 1 = unclassified differences, 2 = structural.
# ---------------------------------------------------------------------------
PSYM_CLASSIFY_AWK='
# Accepted noise class (a): an immediate-constant move, same instruction and
# same destination on both sides, differing only in the immediate.
function is_imm_move(s) {
    return s ~ /^\tmov[lqbw]?[ \t]+\$0x[0-9a-f]+,(%[a-z0-9]+|-?0x[0-9a-f]+\(%[a-z0-9]+(,%[a-z0-9]+,[0-9])?\)|\(%[a-z0-9]+\))$/
}
function redact(s) { gsub(/\$0x[0-9a-f]+/, "$IMM", s); return s }
function imm(s) { if (match(s, /\$0x[0-9a-f]+/)) return substr(s, RSTART + 3, RLENGTH - 3); return "" }

NR == FNR { b[FNR] = $0; nb = FNR; next }
{ a[FNR] = $0; na = FNR }
END {
    if (nb != na) {
        printf "STRUCTURAL: normalized listing is %d lines vs %d -- instructions were added or removed\n", nb, na
        exit 2
    }
    changed = 0; bad = 0; insn_b = 0
    for (i = 1; i <= nb; i++) {
        if (b[i] ~ /^\t/) insn_b++
        if (b[i] == a[i]) continue
        changed++
        if (is_imm_move(b[i]) && is_imm_move(a[i]) && redact(b[i]) == redact(a[i])) {
            d = strtonum("0x" imm(a[i])) - strtonum("0x" imm(b[i]))
            deltas[d]++
        } else {
            bad++
            if (bad <= 5) printf "  UNCLASSIFIED  - %s\n                + %s\n", b[i], a[i]
        }
    }
    if (bad > 5) printf "  ... and %d more unclassified differences\n", bad - 5
    ds = ""
    for (d in deltas) ds = ds sprintf(" %+d(x%d)", d, deltas[d])
    printf "COUNTS %d insns; CHANGED %d; UNCLASSIFIED %d; DELTAS%s\n", insn_b, changed, bad, ds
    exit (bad > 0) ? 1 : 0
}
'

# ---------------------------------------------------------------------------
# Map / exception file loading. Each emits a TAB-separated two-column file.
# ---------------------------------------------------------------------------
load_pairs() {
    # $1 = file, $2 = separator (" => " or " ## "), $3 = human name for errors
    local file="$1" sep="$2" what="$3"
    awk -v sep="${sep}" -v what="${what}" -v file="${file}" '
        { line = $0 }
        line ~ /^[[:space:]]*#/ { next }
        line ~ /^[[:space:]]*$/ { next }
        {
            i = index(line, sep)
            if (i == 0) {
                printf "psym_compare: %s: line %d has no \"%s\" separator: %s\n", file, FNR, sep, line > "/dev/stderr"
                bad = 1
                next
            }
            l = substr(line, 1, i - 1)
            r = substr(line, i + length(sep))
            gsub(/^[[:space:]]+|[[:space:]]+$/, "", l)
            gsub(/^[[:space:]]+|[[:space:]]+$/, "", r)
            if (l == "" || r == "") {
                printf "psym_compare: %s: line %d has an empty %s field: %s\n", file, FNR, what, line > "/dev/stderr"
                bad = 1
                next
            }
            printf "%s\t%s\n", l, r
        }
        END { if (bad) exit 1 }
    ' "${file}"
}

# ---------------------------------------------------------------------------
# Symbol enumeration: every DEFINED symbol of an object, MANGLED. Demangling
# happens later, through demangle_table(), because the demangled form alone is
# not a key: a destructor emits D0/D1/D2 variants (and a constructor C1/C2/C3)
# that all demangle to one string, and `nm -C` would silently merge them.
# Emits "<type letter>\t<mangled name>".
# ---------------------------------------------------------------------------
obj_symbols() {
    local obj
    for obj in "$@"; do
        "${NM}" --defined-only "${obj}" 2>/dev/null
    done | sed -nE 's/^[0-9a-fA-F]+[[:space:]]+([A-Za-z?])[[:space:]]+(.*)$/\1\t\2/p' \
         | awk -F'\t' '!seen[$2]++'
}

# ---------------------------------------------------------------------------
# Mangled -> (bare demangled, disambiguating tag). Reads a sorted, unique list
# of mangled names on stdin; writes "<mangled>\t<demangled>\t<tag>".
#
# The tag is EMPTY unless several mangled names in this object share one
# demangled name -- the ctor/dtor variant case above, essentially. When they
# do, the tag is the Itanium variant marker (`[C1]`, `[D2]`, ...) taken from
# the mangled name, which a type RENAME does not touch, so a tagged key
# survives the rename the map describes. The `[#n]` fallback (deterministic
# rank in sorted-mangled order) exists so two genuinely indistinguishable
# names are still kept apart rather than merged: merging them would let a
# deleted symbol hide behind a surviving twin.
#
# The tag is appended AFTER the symbol map is applied, so map files are always
# written in plain demangled names and never mention a variant marker.
# ---------------------------------------------------------------------------
demangle_table() {
    local names
    names="$(mktemp)"
    cat > "${names}"
    paste -d'\t' "${names}" <(c++filt < "${names}") \
        | awk -F'\t' '
            # The structural, identifier-free parts of a mangled name: the
            # thunk prefix (`_ZThn8_`, `_ZTv0_n24_` -- the adjustment is NOT in
            # the demangled text) and the ctor/dtor variant marker. Both are
            # untouched by a type rename, which is what makes them usable as a
            # tag on both arms of a rename comparison.
            # Position of the LAST match of the dynamic regex STRING `re` in
            # `s`, or 0 (a string, not a /regex/ constant: awk evaluates a
            # regex constant passed as an argument as a boolean match against
            # $0). The ctor/dtor
            # variant marker is always in the trailing `<name>E<params>`
            # position, but `[CD][0-9]E` can also occur INSIDE an identifier (a
            # class named `MyC1Extra` mangles to `9MyC1Extra`), and taking the
            # first match would mis-tag -- asymmetrically, if a rename removes
            # the accidental substring on one arm only.
            function lastpos(s, re,   pos, off, rest) {
                pos = 0; off = 0; rest = s
                while (match(rest, re)) {
                    pos = off + RSTART
                    off = pos
                    rest = substr(rest, RSTART + 1)
                }
                return pos
            }
            function tagof(mg,   t, p) {
                t = ""
                if (match(mg, /^_ZT[a-zA-Z]+[0-9]*_(n[0-9]+_)?/)) t = t "[" substr(mg, 1, RLENGTH) "]"
                p = lastpos(mg, "[CD][0-9]E")
                if (p > 0) t = t "[" substr(mg, p, 2) "]"
                return t
            }
            { mg[NR] = $1; bare[NR] = $2; cnt[$2]++ }
            END {
                for (i = 1; i <= NR; i++) {
                    tg[i] = (cnt[bare[i]] > 1) ? tagof(mg[i]) : ""
                    cnt2[bare[i] tg[i]]++
                }
                for (i = 1; i <= NR; i++) {
                    t = tg[i]
                    # Still indistinguishable: fall back to the rank in
                    # sorted-mangled order rather than merging two symbols,
                    # because a merge would let a deleted symbol hide behind a
                    # surviving twin.
                    if (cnt2[bare[i] t] > 1) { rank[bare[i] t]++; t = t "[#" rank[bare[i] t] "]" }
                    printf "%s\t%s\t%s\n", mg[i], bare[i], t
                }
            }'
    rm -f "${names}"
}

# Mangled -> demangled, with NO variant tag and no cross-name disambiguation.
# Used for RELOCATION TARGETS only: a relocation names a symbol (often one this
# object does not define), so it has no variant to disambiguate, and feeding
# such a name into demangle_table() would let a mere REFERENCE change the tags
# that table assigns to the object's own DEFINED symbols.
plain_demangle() {
    local names
    names="$(mktemp)"
    cat > "${names}"
    if [ -s "${names}" ]; then
        paste -d'\t' "${names}" <(c++filt < "${names}")
    fi
    rm -f "${names}"
}

# The relocation-target names in one or more flattened listings, as candidates
# for plain_demangle: the whole target when it names a symbol, and the embedded
# mangled name when it names a section or a compiler-local label that carries
# one (`.rodata._ZN3fmt...`, `.Lswitch.table._ZN4hven...`).
reloc_names() {
    awk -F'\t' '$1 == "R" {
        t = $4
        if (substr(t, 1, 1) != ".") { print t; next }
        if (match(t, /_Z[A-Za-z0-9_$]+/)) print substr(t, RSTART, RLENGTH)
    }' "$@"
}

# ---------------------------------------------------------------------------
# The keying both comparison layers share: a symbol is identified by its
# DEMANGLED name, with the symbol map applied on the before arm, plus the
# disambiguating tag demangle_table() computed. Prepended verbatim to both awk
# programs so the two layers cannot key differently.
# ---------------------------------------------------------------------------
PSYM_KEYING_AWK='
function keyof(mangled, is_before,   d) {
    d = (mangled in dem) ? dem[mangled] : mangled
    if (is_before && (d in m)) d = m[d]
    return d ((mangled in tag) ? tag[mangled] : "")
}
function bareof(mangled) { return (mangled in dem) ? dem[mangled] : mangled }
function load_demangle(f,   line, n1, n2) {
    while ((getline line < f) > 0) {
        n1 = index(line, "\t")
        n2 = index(substr(line, n1 + 1), "\t") + n1
        dem[substr(line, 1, n1 - 1)] = substr(line, n1 + 1, n2 - n1 - 1)
        tag[substr(line, 1, n1 - 1)] = substr(line, n2 + 1)
    }
    close(f)
}
function load_pairs_into(f, arr,   line, n) {
    while ((getline line < f) > 0) { n = index(line, "\t"); arr[substr(line, 1, n - 1)] = substr(line, n + 1) }
    close(f)
}
# ---- relocation TARGETS ---------------------------------------------------
# A relocation names a SYMBOL, not a variant, so the disambiguating tag keyof()
# appends is deliberately NOT applied here: `[D1]` distinguishes two definitions
# in one object, and a relocation target is a reference that may not be defined
# in this object at all. `rdem` is a PLAIN mangled->demangled table built over
# the relocation targets only, kept separate from `dem` so that adding a
# referenced-but-not-defined name cannot perturb the variant tags `dem`/`tag`
# assign to the DEFINED symbols of the object.
#
# `relmapped` is set as a side effect when the symbol map rewrote the target,
# and `relmapold` carries the OLD name so the caller can mark the map entry
# consumed (a map line used only by a relocation is NOT stale).
function reldem_of(mangled, is_before,   d) {
    d = (mangled in rdem) ? rdem[mangled] : mangled
    if (is_before && (d in m)) { relmapped = 1; relmapold = d; d = m[d] }
    return d
}
function normnum(s) { gsub(/[0-9]+/, "N", s); return s }
# See the RELOCATION TARGETS block in the header of this file for the classes
# and the reason each is rendered the way it is.
function render_reloc_target(t, is_before,   pre, mg, suf) {
    if (substr(t, 1, 1) != ".") return reldem_of(t, is_before)
    if (match(t, /_Z[A-Za-z0-9_$]+/)) {
        pre = substr(t, 1, RSTART - 1)
        mg  = substr(t, RSTART, RLENGTH)
        suf = substr(t, RSTART + RLENGTH)
        if (substr(t, 1, 2) == ".L") pre = normnum(pre)
        return pre reldem_of(mg, is_before) suf
    }
    return (substr(t, 1, 2) == ".L") ? normnum(t) : t
}
# The comparable rendering of one relocation. Deliberately NOT prefixed with a
# tab: the classifier counts tab-led lines as instructions, and a relocation is
# an annotation ON an instruction, not one of its own.
function reloc_text(type, target, addend, is_before,   t) {
    relmapped = 0; relmapold = ""
    t = render_reloc_target(target, is_before)
    if (substr(target, 1, 1) == ".") addend = "+LOCAL"
    else if (addend == "") addend = "+0x0"
    return "RELOC " type " " t " " addend
}
'


# ---------------------------------------------------------------------------
# Symbol COVERAGE, both arms, every object. Reads two "<type>\t<mangled>"
# listings; applies the symbol map to the before arm; reports any name that
# exists on one side only. Writes a TAB-separated stat line to `f_stat` so the
# caller never has to parse prose.
# ---------------------------------------------------------------------------
PSYM_SYMCOVER_AWK='
BEGIN {
    FS = "\t"
    load_pairs_into(f_map, m)
    load_pairs_into(f_exc, exc)
    load_demangle(f_dem)
    # The `[#n]` rank fallback is the one key that is not rename-stable, so
    # count how many of this object'"'"'s names needed it. `tag` is keyed by
    # mangled name over the UNION of both arms, which is the natural per-object
    # number.
    for (x in tag) if (index(tag[x], "[#")) nrank++
}
NR == FNR {
    key = keyof($2, 1)
    if (key in b) { printf "  COLLISION     two before-arm symbols claim one name: %s\n", key; coll++ }
    b[key] = $1
    borig[key] = bareof($2)
    if (bareof($2) in m) { wasmapped[key] = 1; mapused[bareof($2)] = 1 }
    next
}
{ akey = keyof($2, 0); a[akey] = $1; aorig[akey] = bareof($2) }
END {
    for (k in b) {
        if (k in a) { matched++; if (k in wasmapped) mapped++; continue }
        if (k in exc) { excused++; used[k] = 1; printf "  EXCEPTION     ONLY-BEFORE %s ## %s\n", k, exc[k]; continue }
        if (borig[k] in exc) { excused++; used[borig[k]] = 1; printf "  EXCEPTION     ONLY-BEFORE %s ## %s\n", borig[k], exc[borig[k]]; continue }
        onlyb++
        if (onlyb <= 20) printf "  ONLY-BEFORE   %s\n", k
    }
    if (onlyb > 20) printf "  ... and %d more symbols present only in the before arm\n", onlyb - 20
    for (k in a) {
        if (k in b) continue
        if (k in exc) { excused++; used[k] = 1; printf "  EXCEPTION     ONLY-AFTER  %s ## %s\n", k, exc[k]; continue }
        if (aorig[k] in exc) { excused++; used[aorig[k]] = 1; printf "  EXCEPTION     ONLY-AFTER  %s ## %s\n", aorig[k], exc[aorig[k]]; continue }
        onlya++
        if (onlya <= 20) printf "  ONLY-AFTER    %s\n", k
    }
    if (onlya > 20) printf "  ... and %d more symbols present only in the after arm\n", onlya - 20
    for (k in used) print k >> f_used
    close(f_used)
    for (k in mapused) print k >> f_mapused
    close(f_mapused)
    printf "%d\t%d\t%d\t%d\t%d\t%d\t%d\n", matched + 0, mapped + 0, onlyb + 0, onlya + 0, excused + 0, coll + 0, nrank + 0 > f_stat
    close(f_stat)
    exit (onlyb + onlya + coll > 0) ? 1 : 0
}
'

# ---------------------------------------------------------------------------
# INSTRUCTION identity per symbol, paired by DEMANGLED name through the symbol
# map. Reads the two flattened listings produced by flatten_symbols(). The
# noise classification is not re-implemented here: each differing block is
# handed to the ONE classifier in `f_cls`, so both paths accept exactly the
# same class.
# ---------------------------------------------------------------------------
PSYM_PERSYM_AWK='
function field3(s,   i, j) { i = index(s, "\t"); j = index(substr(s, i + 1), "\t") + i; return substr(s, j + 1) }
# Literal (never regex) replacement -- a mangled name can contain "." and "$",
# and treating it as a pattern would corrupt the section name it edits.
function lit_replace(hay, needle, repl,   p, out) {
    out = ""
    while ((p = index(hay, needle)) > 0) { out = out substr(hay, 1, p - 1) repl; hay = substr(hay, p + length(needle)) }
    return out hay
}
# TRAILING inter-function alignment padding. objdump attributes the nops the
# assembler inserts between two functions to the symbol they follow, so a
# function that used to be followed by another in the same section and is now
# last in its own object loses them -- a pure layout consequence of a TU split,
# not a code change. Only a run at the very END of a block is trimmed, and only
# on the per-symbol path; padding INSIDE a function (loop alignment) is code and
# is compared. The positional path is untouched by this.
function is_pad(s) {
    return s ~ /^\t(cs[ \t]+)?(data16[ \t]+)*(nop[lwqb]?([ \t]|$)|xchg[ \t]+%ax,%ax$)/
}
BEGIN {
    FS = "\t"
    load_pairs_into(f_map, m)
    load_pairs_into(f_exc, exc)
    load_pairs_into(f_reldem, rdem)
    load_demangle(f_dem)
    for (x in tag) if (index(tag[x], "[#")) nrank++
    fb = tmpd "/blk-b.txt"
    fa = tmpd "/blk-a.txt"
}
NR == FNR {
    if ($1 == "R") {
        if (!bskip && bcur != "") {
            bn[bcur]++
            bi[bcur, bn[bcur]] = reloc_text($3, $4, $5, 1)
            brel[bcur]++
            if (relmapped) { brelmap[bcur]++; relmapused[relmapold] = 1 }
        }
        next
    }
    if ($1 == "S") {
        k = keyof($2, 1)
        bcur = k
        if (k in bsec) { printf "  COLLISION     two before-arm symbols claim one name: %s\n", k; coll++; bskip = 1 }
        else {
            bsec[k] = lit_replace($3, $2, "<SYM>")
            bn[k] = 0
            bskip = 0
            borig[k] = bareof($2)
            if (bareof($2) in m) bmapped[k] = 1
        }
    } else if (!bskip && bcur != "") { bn[bcur]++; bi[bcur, bn[bcur]] = field3($0) }
    next
}
{
    if ($1 == "R") {
        if (acur != "") {
            rtxt = reloc_text($3, $4, $5, 0)
            if (adupmode) { c = adup[acur]; adn[acur, c]++; adi[acur, c, adn[acur, c]] = rtxt }
            else { an[acur]++; ai[acur, an[acur]] = rtxt }
        }
        next
    }
    if ($1 == "S") {
        k = keyof($2, 0)
        acur = k
        # A split can legitimately place the SAME weak/COMDAT body into both
        # new objects. Copy 1 is the one compared against the before arm;
        # copies 2..n are KEPT and compared against copy 1 in END, because two
        # copies in one build arm share their preprocessing and so cannot
        # legitimately differ at all.
        if (k in asec) {
            adup[k]++
            adsec[k, adup[k]] = lit_replace($3, $2, "<SYM>")
            adn[k, adup[k]] = 0
            adupmode = 1
        } else {
            asec[k] = lit_replace($3, $2, "<SYM>")
            an[k] = 0
            aorig[k] = bareof($2)
            adupmode = 0
        }
    } else if (acur != "") {
        if (adupmode) { c = adup[acur]; adn[acur, c]++; adi[acur, c, adn[acur, c]] = field3($0) }
        else { an[acur]++; ai[acur, an[acur]] = field3($0) }
    }
}
END {
    for (k in bsec) {
        if (!(k in asec)) {
            if (k in exc) { excused++; used[k] = 1; printf "  EXCEPTION     ONLY-BEFORE %s ## %s\n", k, exc[k]; continue }
            if (borig[k] in exc) { excused++; used[borig[k]] = 1; printf "  EXCEPTION     ONLY-BEFORE %s ## %s\n", borig[k], exc[borig[k]]; continue }
            onlyb++
            printf "  ONLY-BEFORE   %s\n", k
            continue
        }
        if (bsec[k] != asec[k]) {
            printf "  SECTION-MOVED %s: %s -> %s\n", k, bsec[k], asec[k]
            diff++
            continue
        }
        nreloc += brel[k] + 0
        nrelmap += brelmap[k] + 0
        nb = bn[k]; while (nb > 0 && is_pad(bi[k, nb])) nb--
        na = an[k]; while (na > 0 && is_pad(ai[k, na])) na--
        if (bn[k] - nb != an[k] - na) padtrim++
        same = (nb == na)
        if (same) { for (i = 1; i <= nb; i++) if (bi[k, i] != ai[k, i]) { same = 0; break } }
        if (same) { ident++; if (k in bmapped) mapped++; continue }
        printf "" > fb
        printf "" > fa
        for (i = 1; i <= nb; i++) print bi[k, i] > fb
        for (i = 1; i <= na; i++) print ai[k, i] > fa
        close(fb)
        close(fa)
        cmd = "awk -f " f_cls " " fb " " fa " 2>&1"
        verdict = ""
        while ((cmd | getline l) > 0) verdict = verdict (verdict == "" ? "" : "\n") l
        status = close(cmd)
        if (status == 0) {
            noise++
            if (k in bmapped) mapped++
            printf "  NOISE-ONLY    %s\n                %s\n", k, verdict
        } else if (k in exc) {
            excused++; used[k] = 1
            printf "  EXCEPTION     DIFFERS %s ## %s\n", k, exc[k]
        } else if (borig[k] in exc) {
            excused++; used[borig[k]] = 1
            printf "  EXCEPTION     DIFFERS %s ## %s\n", borig[k], exc[borig[k]]
        } else {
            diff++
            printf "  DIFFERS       %s\n", k
            n = split(verdict, vl, "\n")
            for (i = 1; i <= n; i++) printf "                %s\n", vl[i]
        }
    }
    for (k in asec) {
        if (k in bsec) continue
        if (k in exc) { excused++; used[k] = 1; printf "  EXCEPTION     ONLY-AFTER  %s ## %s\n", k, exc[k]; continue }
        if (aorig[k] in exc) { excused++; used[aorig[k]] = 1; printf "  EXCEPTION     ONLY-AFTER  %s ## %s\n", aorig[k], exc[aorig[k]]; continue }
        onlya++
        printf "  ONLY-AFTER    %s\n", k
    }
    # Copies 2..n of a weak/COMDAT body, against copy 1. The classifier is the
    # same one every other comparison uses, but here it only supplies the
    # printed instruction pair: ANY difference between two copies in one build
    # is a finding, since both were preprocessed identically.
    for (k in adup) {
        na1 = an[k]; while (na1 > 0 && is_pad(ai[k, na1])) na1--
        for (c = 1; c <= adup[k]; c++) {
            nc = adn[k, c]; while (nc > 0 && is_pad(adi[k, c, nc])) nc--
            dupsame = (nc == na1) && (adsec[k, c] == asec[k])
            if (dupsame) { for (i = 1; i <= nc; i++) if (adi[k, c, i] != ai[k, i]) { dupsame = 0; break } }
            if (dupsame) {
                dupok++
                if (dupok <= 20) printf "  DUPLICATE-IDENTICAL %s (copy %d of %d)\n", k, c + 1, adup[k] + 1
                continue
            }
            printf "" > fb
            printf "" > fa
            for (i = 1; i <= na1; i++) print ai[k, i] > fb
            for (i = 1; i <= nc; i++) print adi[k, c, i] > fa
            close(fb)
            close(fa)
            cmd = "awk -f " f_cls " " fb " " fa " 2>&1"
            verdict = ""
            while ((cmd | getline l) > 0) verdict = verdict (verdict == "" ? "" : "\n") l
            close(cmd)
            diff++
            printf "  DUPLICATE-DIFFERS   %s (copy %d of %d differs from copy 1 -- two COMDAT copies in one build share their preprocessing, so this is an ODR finding, not noise)\n", k, c + 1, adup[k] + 1
            if (adsec[k, c] != asec[k]) printf "                section %s -> %s\n", asec[k], adsec[k, c]
            n = split(verdict, vl, "\n")
            for (i = 1; i <= n; i++) printf "                %s\n", vl[i]
        }
    }
    if (dupok > 20) printf "  ... and %d more identical duplicate copies\n", dupok - 20
    for (k in used) print k >> f_used
    close(f_used)
    for (k in relmapused) print k >> f_mapused
    close(f_mapused)
    printf "%d\t%d\n", nreloc + 0, nrelmap + 0 > f_relstat
    close(f_relstat)
    printf "PERSYM %d identical, %d noise-only, %d differing, %d only-before, %d only-after, %d excepted, %d collisions, %d matched through the symbol map, %d with unequal trailing alignment padding, %d duplicate copies identical, %d rank-tagged, %d relocation records compared (%d through the symbol map)\n",
           ident + 0, noise + 0, diff + 0, onlyb + 0, onlya + 0, excused + 0, coll + 0, mapped + 0, padtrim + 0, dupok + 0, nrank + 0, nreloc + 0, nrelmap + 0
    exit (diff + onlyb + onlya + coll > 0) ? 1 : 0
}
'

# ---------------------------------------------------------------------------
# Per-symbol flattening of the RAW `objdump -d` listing (so the expensive
# objdump runs once per object per arm, and both paths consume the same bytes).
# Emits, for each symbol:
#   S<TAB><mangled><TAB><section>
#   I<TAB><mangled><TAB><instruction line, leading tab preserved>
#
# It applies the same normalization normalize_raw() does -- strip the leading
# address, strip the trailing ` #` annotation, strip the trailing `<...>`
# symbolization -- plus ONE rule that only the per-symbol path can apply,
# because only it knows which symbol a line belongs to:
#
#   SELF-RELATIVE BRANCH AND CALL TARGETS. `objdump -d` prints a control
#   transfer's target as an ABSOLUTE address in the object, followed by its
#   `<symbol+offset>` label: `call 16 <_ZN4demo4betaEi+0x6>`. For a call
#   through a relocation the target is simply the next instruction, and for an
#   intra-function jump it is a point inside the same function -- in both cases
#   the label names the ENCLOSING symbol, and the OFFSET is the stable part
#   while the absolute address is pure layout. A TU split moves every function
#   that was not first in its section, so the same instruction prints
#   `call 16` in the whole TU and `call 6` in the split one. Where the label
#   names the enclosing symbol, this rewrites the pair as `call SELF+0x6`,
#   which is identical on both sides. Where it names some OTHER symbol (a
#   direct call the assembler resolved to a sibling function in the same
#   section, which needs no relocation), nothing is rewritten: the absolute
#   target survives, and a split that moves the callee reports a DIFFERS. That
#   is the conservative direction -- a false finding, never a masked one.
# ---------------------------------------------------------------------------
flatten_symbols() {
    awk '
        /^Disassembly of section / {
            sec = $0
            sub(/^Disassembly of section /, "", sec)
            sub(/:$/, "", sec)
            next
        }
        /^[0-9a-f]+ <.*>:$/ {
            name = $0
            sub(/^[0-9a-f]+ </, "", name)
            sub(/>:$/, "", name)
            cur = name
            printf "S\t%s\t%s\n", cur, sec
            next
        }
        # A relocation line: three tabs, the within-section offset, ": ", the
        # type, a tab, then "<target>" with an optional "+0xN"/"-0xN" addend
        # glued to it. It annotates the instruction ABOVE it, so emitting it
        # here keeps it adjacent to that instruction in the flattened stream.
        # It cannot be confused with an instruction line: an instruction address
        # is followed by a COLON-TAB, a relocation offset by a COLON-SPACE.
        /^[ \t]+[0-9a-f]+: [^ \t]+\t/ {
            if (cur == "") next
            line = $0
            sub(/^[ \t]+[0-9a-f]+: /, "", line)
            p = index(line, "\t")
            rtype = substr(line, 1, p - 1)
            rtarget = substr(line, p + 1)
            radd = ""
            if (match(rtarget, /[+-]0x[0-9a-f]+$/)) {
                radd = substr(rtarget, RSTART)
                rtarget = substr(rtarget, 1, RSTART - 1)
            }
            printf "R\t%s\t%s\t%s\t%s\n", cur, rtype, rtarget, radd
            next
        }
        /^ *[0-9a-f]+:\t/ {
            if (cur == "") next
            line = $0
            sub(/^ *[0-9a-f]*:/, "", line)
            sub(/[ \t]+#.*$/, "", line)
            if (match(line, /[ \t]*<[^<>]*>[ \t]*$/)) {
                grp = substr(line, RSTART, RLENGTH)
                sub(/^[ \t]*</, "", grp)
                sub(/>[ \t]*$/, "", grp)
                gname = grp
                goff = "+0x0"
                if ((p = index(grp, "+")) > 0 || (p = index(grp, "-")) > 0) {
                    gname = substr(grp, 1, p - 1)
                    goff = substr(grp, p)
                }
                if (gname == cur && line ~ /[ \t][0-9a-f]+[ \t]*<[^<>]*>[ \t]*$/) {
                    sub(/[ \t]*<[^<>]*>[ \t]*$/, "", line)
                    sub(/[0-9a-f]+$/, "SELF" goff, line)
                }
            }
            sub(/[ \t]*<[^<>]*>[ \t]*$/, "", line)
            printf "I\t%s\t%s\n", cur, line
        }
    ' "$1"
}

do_compare() {
    local before="" after=""
    local object_map="" symbol_map="" exceptions="" allow_foreign=0

    while [ $# -gt 0 ]; do
        case "$1" in
            --object-map) [ $# -ge 2 ] || usage; object_map="$2"; shift 2 ;;
            --symbol-map) [ $# -ge 2 ] || usage; symbol_map="$2"; shift 2 ;;
            --exceptions) [ $# -ge 2 ] || usage; exceptions="$2"; shift 2 ;;
            --allow-foreign-captures) allow_foreign=1; shift ;;
            --) shift ;;
            -*) echo "psym_compare: unknown option $1" >&2; usage ;;
            *)  if [ -z "${before}" ]; then before="$1"
                elif [ -z "${after}" ]; then after="$1"
                else echo "psym_compare: too many positional arguments" >&2; usage
                fi
                shift ;;
        esac
    done
    [ -n "${before}" ] && [ -n "${after}" ] || usage

    local f
    for f in "${object_map}" "${symbol_map}" "${exceptions}"; do
        if [ -n "${f}" ] && [ ! -f "${f}" ]; then
            echo "psym_compare: no such file: ${f}" >&2
            exit 1
        fi
    done

    PSYM_TMP="$(mktemp -d)"
    local tmp="${PSYM_TMP}"
    printf '%s' "${PSYM_CLASSIFY_AWK}" > "${tmp}/classify.awk"
    printf '%s%s' "${PSYM_KEYING_AWK}" "${PSYM_SYMCOVER_AWK}" > "${tmp}/symcover.awk"
    printf '%s%s' "${PSYM_KEYING_AWK}" "${PSYM_PERSYM_AWK}" > "${tmp}/persym.awk"

    # --- tool-version gate -------------------------------------------------
    local self_sha ver_b ver_a
    self_sha="$(tool_sha)"
    if [ ! -f "${before}/.psym-version" ] || [ ! -f "${after}/.psym-version" ]; then
        echo "psym_compare: a capture without a .psym-version stamp cannot be compared -- it was" >&2
        echo "              made by a script version whose object SET is unknown. Recapture both arms." >&2
        exit 1
    fi
    ver_b="$(cat "${before}/.psym-version")"
    ver_a="$(cat "${after}/.psym-version")"
    if [ "${ver_b}" != "${ver_a}" ]; then
        echo "psym_compare: the two captures were made by DIFFERENT versions of this script" >&2
        echo "              before: ${ver_b}" >&2
        echo "              after:  ${ver_a}" >&2
        echo "              The object set is a property of the capturing script; comparing across" >&2
        echo "              versions would silently omit whatever the older one did not capture." >&2
        exit 1
    fi

    echo "P-SYM tool sha256 (runner):   ${self_sha}"
    echo "P-SYM tool sha256 (captures): ${ver_b}"
    if [ "${ver_b}" != "${self_sha}" ]; then
        if [ "${allow_foreign}" -eq 0 ]; then
            echo "psym_compare: both captures were made by a DIFFERENT version of this script" >&2
            echo "              captures: ${ver_b}" >&2
            echo "              runner:   ${self_sha}" >&2
            echo "              The object SET is a property of the CAPTURING script, so an old pair" >&2
            echo "              compares an old, narrower universe and this runner cannot say what it" >&2
            echo "              left out. Every arm of a W5 comparison is captured by the current tool," >&2
            echo "              so the declared object set is what is compared. Recapture both arms," >&2
            echo "              or pass --allow-foreign-captures if you deliberately want the older" >&2
            echo "              universe classified by this runner." >&2
            exit 1
        fi
        echo "P-SYM note: both captures were made by a DIFFERENT version of this script than the"
        echo "            one now running (the classification below is this runner's), and"
        echo "            --allow-foreign-captures was passed. The object set compared is the OLDER"
        echo "            script's, which may be narrower than this one's."
    fi
    echo "P-SYM maps: object-map=${object_map:-none} symbol-map=${symbol_map:-none} exceptions=${exceptions:-none}"
    echo

    # --- load the maps -----------------------------------------------------
    : > "${tmp}/objmap.tsv"
    : > "${tmp}/symmap.tsv"
    : > "${tmp}/exc.tsv"
    if [ -n "${object_map}" ]; then load_pairs "${object_map}" " => " "object-map" > "${tmp}/objmap.tsv"; fi
    if [ -n "${symbol_map}" ]; then load_pairs "${symbol_map}" " => " "symbol-map" > "${tmp}/symmap.tsv"; fi
    if [ -n "${exceptions}" ]; then load_pairs "${exceptions}" " ## " "exceptions" > "${tmp}/exc.tsv"; fi

    # Map-file level collision checks: a repeated old name, or a repeated new
    # name, is rejected before any object is looked at.
    local dup
    if [ -s "${tmp}/symmap.tsv" ]; then
        dup="$(cut -f1 "${tmp}/symmap.tsv" | LC_ALL=C sort | uniq -d)"
        if [ -n "${dup}" ]; then
            echo "psym_compare: symbol map maps these OLD names more than once:" >&2
            echo "${dup}" | sed 's/^/  /' >&2
            exit 1
        fi
        dup="$(cut -f2 "${tmp}/symmap.tsv" | LC_ALL=C sort | uniq -d)"
        if [ -n "${dup}" ]; then
            echo "psym_compare: COLLISION -- symbol map points several old names at one new name:" >&2
            echo "${dup}" | sed 's/^/  /' >&2
            exit 1
        fi
    fi
    if [ -s "${tmp}/objmap.tsv" ]; then
        dup="$(cut -f2 "${tmp}/objmap.tsv" | LC_ALL=C sort | uniq -d)"
        if [ -n "${dup}" ]; then
            echo "psym_compare: COLLISION -- object map points several old objects at one new object:" >&2
            echo "${dup}" | sed 's/^/  /' >&2
            exit 1
        fi
    fi

    : > "${tmp}/exc-used"
    # R19: the two map kinds get SEPARATE used-namespaces. An old symbol name
    # and an old object relpath are different sorts of string, and sharing one
    # namespace let a coincidental equality between them suppress one map's
    # STALE-MAP report.
    : > "${tmp}/symmap-used"
    : > "${tmp}/objmap-used"
    : > "${tmp}/claimed"
    LC_ALL=C sort "${after}/.psym-manifest" > "${tmp}/after-manifest"

    local identical=0 noise=0 unclassified=0 moved=0 missing=0 total=0
    local mapped_objects=0 unmatched_after=0 persym_objects=0 persym_pass=0
    local sym_matched=0 sym_mapped=0 sym_only_before=0 sym_only_after=0
    local sym_exception=0 sym_collision=0 sym_rank_tagged=0
    local reloc_compared=0 reloc_mapped=0
    local rc=0

    local rel path_b kind ntargets t pos_noise=0
    local -a targets=()
    while IFS= read -r rel; do
        [ -n "${rel}" ] || continue
        total=$((total + 1))
        path_b="${before}/${rel}"
        targets=()
        kind="direct"
        pos_noise=0

        if awk -F'\t' -v k="${rel}" '$1 == k { found = 1 } END { exit found ? 0 : 1 }' "${tmp}/objmap.tsv"; then
            kind="mapped"
            while IFS= read -r t; do
                if [ ! -f "${after}/${t}" ]; then
                    echo "MAP-ERROR   ${rel} => ${t} (no such object in ${after})"
                    rc=1
                    continue
                fi
                targets+=("${t}")
            done < <(awk -F'\t' -v k="${rel}" '$1 == k { print $2 }' "${tmp}/objmap.tsv")
            echo "${rel}" >> "${tmp}/objmap-used"
            if [ "${#targets[@]}" -eq 0 ]; then
                missing=$((missing + 1))
                echo "MISSING     ${rel} (object map named no reachable counterpart)"
                rc=1
                continue
            fi
            mapped_objects=$((mapped_objects + 1))
            ntargets="${#targets[@]}"
            if [ "${ntargets}" -gt 1 ]; then
                local joined
                joined="$(printf '%s, ' "${targets[@]}")"
                echo "SPLIT       ${rel} -> ${joined%, }"
            else
                echo "RENAMED     ${rel} -> ${targets[0]}"
            fi
        elif [ -f "${after}/${rel}" ]; then
            targets=("${rel}")
        else
            local candidates
            candidates="$(find "${after}" -name "$(basename "${rel}")" -type f)"
            if [ "$(echo "${candidates}" | grep -c .)" -eq 1 ] && [ -n "${candidates}" ]; then
                targets=("${candidates#"${after}"/}")
                moved=$((moved + 1))
                kind="moved"
                echo "MOVED       ${rel} -> ${targets[0]}"
            else
                missing=$((missing + 1))
                echo "MISSING     ${rel} (no counterpart in ${after})"
                rc=1
                continue
            fi
        fi

        ntargets="${#targets[@]}"
        local path_a_list=()
        for t in "${targets[@]}"; do
            if grep -qxF "${t}" "${tmp}/claimed"; then
                echo "COLLISION   ${rel} and an earlier object both claim ${t}"
                rc=1
            fi
            echo "${t}" >> "${tmp}/claimed"
            path_a_list+=("${after}/${t}")
        done

        # ---- symbol accounting (every object, both arms, always) ----------
        #
        # `libhven.a` is excluded here and only here: its members are captured
        # and accounted for individually, so counting the archive's symbols
        # again would double every one of them.
        if [ "${rel}" != "libhven.a" ]; then
            obj_symbols "${path_b}" | LC_ALL=C sort -u > "${tmp}/syms-b.tsv"
            obj_symbols "${path_a_list[@]}" | LC_ALL=C sort -u > "${tmp}/syms-a.tsv"
            cut -f2 "${tmp}/syms-b.tsv" "${tmp}/syms-a.tsv" | LC_ALL=C sort -u \
                | demangle_table > "${tmp}/syms-demangle.tsv"
            local sym_out sym_rc
            rm -f "${tmp}/symstat"
            set +e
            sym_out="$(awk -F'\t' \
                            -v f_map="${tmp}/symmap.tsv" -v f_exc="${tmp}/exc.tsv" \
                            -v f_dem="${tmp}/syms-demangle.tsv" \
                            -v f_used="${tmp}/exc-used" -v f_stat="${tmp}/symstat" \
                            -v f_mapused="${tmp}/symmap-used" \
                            -f "${tmp}/symcover.awk" \
                            "${tmp}/syms-b.tsv" "${tmp}/syms-a.tsv")"
            sym_rc=$?
            set -e
            local st
            st="$(cat "${tmp}/symstat")"
            sym_matched=$((sym_matched + $(echo "${st}" | cut -f1)))
            sym_mapped=$((sym_mapped + $(echo "${st}" | cut -f2)))
            sym_only_before=$((sym_only_before + $(echo "${st}" | cut -f3)))
            sym_only_after=$((sym_only_after + $(echo "${st}" | cut -f4)))
            sym_exception=$((sym_exception + $(echo "${st}" | cut -f5)))
            sym_collision=$((sym_collision + $(echo "${st}" | cut -f6)))
            sym_rank_tagged=$((sym_rank_tagged + $(echo "${st}" | cut -f7)))
            if [ "${sym_rc}" -ne 0 ]; then
                rc=1
                echo "SYMBOLS     ${rel}: COVERAGE FINDINGS"
                echo "${sym_out}" | sed 's/^/            /'
            elif [ -n "${sym_out}" ]; then
                echo "SYMBOLS     ${rel}: exceptions consumed"
                echo "${sym_out}" | sed 's/^/            /'
            fi
        fi

        # ---- instruction identity ------------------------------------------
        if [ "${ntargets}" -eq 1 ] && cmp -s "${path_b}" "${path_a_list[0]}"; then
            identical=$((identical + 1))
            continue
        fi

        # `libhven.a` is an archive: its member objects are compared through
        # their own entries in the object set, and the archive itself carries
        # timestamps and an index that are not instructions. Report and move on.
        if [ "${rel}" = "libhven.a" ]; then
            echo "ARCHIVE     libhven.a differs -- see its member objects above; archive"
            echo "            metadata (member timestamps, symbol index) is not instructions"
            continue
        fi

        # ONE objdump per object per arm: the positional path reads the
        # normalized listing, the per-symbol path reads the raw one.
        raw_disasm "${path_b}" > "${tmp}/raw-b.txt"
        : > "${tmp}/raw-a.txt"
        for t in "${path_a_list[@]}"; do raw_disasm "${t}" >> "${tmp}/raw-a.txt"; done
        normalize_raw < "${tmp}/raw-b.txt" > "${tmp}/b.txt"
        normalize_raw < "${tmp}/raw-a.txt" > "${tmp}/a.txt"

        # The POSITIONAL comparison, unchanged, first. It is strictly stronger
        # than the per-symbol comparison -- it requires the symbol header lines
        # (name AND address) to line up too -- so a pass here is a pass there,
        # and reporting it exactly as before keeps a relocation batch's output
        # byte-for-byte what it has always been. Only a failure, or an object
        # the map splits, reaches the per-symbol layer.
        if [ "${ntargets}" -eq 1 ] && [ "${kind}" != "mapped" ]; then
            local out awk_rc
            set +e
            out="$(awk -f "${tmp}/classify.awk" "${tmp}/b.txt" "${tmp}/a.txt")"
            awk_rc=$?
            set -e
            if [ "${awk_rc}" -eq 0 ]; then
                # The INSTRUCTION stream is within the accepted noise class.
                # That verdict is now printed exactly as it always was, but it
                # is no longer the whole story: `-d` shows a direct external
                # call as a placeholder, so two objects that call DIFFERENT
                # functions of the same signature at the same offset have
                # identical instruction streams. The relocations are what tell
                # them apart. Compare them raw and mangled first -- if they
                # agree, nothing a symbol map could do would change that, and
                # this object is finished exactly as before. Only when they
                # DISAGREE does the object fall through to the per-symbol
                # layer, which renders each target through the map and either
                # matches it or reports the pair.
                noise=$((noise + 1))
                echo "NOISE-ONLY  ${rel}"
                echo "            ${out##*$'\n'}"
                grep '^[[:space:]][[:space:]]*[0-9a-f][0-9a-f]*: [^[:space:]]' "${tmp}/raw-b.txt" \
                    | sed 's/^[^:]*: //' > "${tmp}/rel-b.txt" || true
                grep '^[[:space:]][[:space:]]*[0-9a-f][0-9a-f]*: [^[:space:]]' "${tmp}/raw-a.txt" \
                    | sed 's/^[^:]*: //' > "${tmp}/rel-a.txt" || true
                if cmp -s "${tmp}/rel-b.txt" "${tmp}/rel-a.txt"; then
                    continue
                fi
                echo "RELOCATIONS ${rel} (the instruction stream is within the accepted noise class,"
                echo "            but the relocation targets are not literally equal; the PER-SYMBOL"
                echo "            line below is the verdict)"
                pos_noise=1
            else
                echo "DIFFERS     ${rel} (positional pairing did not hold; the PER-SYMBOL line below is the verdict)"
                echo "${out}" | sed 's/^/            /'
            fi
        fi

        # ---- per-symbol, by DEMANGLED name, through the maps ---------------
        persym_objects=$((persym_objects + 1))
        flatten_symbols "${tmp}/raw-b.txt" > "${tmp}/flat-b.tsv"
        flatten_symbols "${tmp}/raw-a.txt" > "${tmp}/flat-a.tsv"
        cut -f2 "${tmp}/flat-b.tsv" "${tmp}/flat-a.tsv" | LC_ALL=C sort -u > "${tmp}/mangled.txt"
        demangle_table < "${tmp}/mangled.txt" > "${tmp}/demangle.tsv"
        # Relocation targets get their OWN plain table -- see plain_demangle().
        reloc_names "${tmp}/flat-b.tsv" "${tmp}/flat-a.tsv" | LC_ALL=C sort -u \
            | plain_demangle > "${tmp}/reldem.tsv"

        local per_out per_rc
        rm -f "${tmp}/relstat"
        set +e
        per_out="$(awk -F'\t' \
                        -v f_map="${tmp}/symmap.tsv" -v f_exc="${tmp}/exc.tsv" \
                        -v f_dem="${tmp}/demangle.tsv" -v f_used="${tmp}/exc-used" \
                        -v f_reldem="${tmp}/reldem.tsv" -v f_mapused="${tmp}/symmap-used" \
                        -v f_relstat="${tmp}/relstat" \
                        -v f_cls="${tmp}/classify.awk" -v tmpd="${tmp}" \
                        -f "${tmp}/persym.awk" \
                        "${tmp}/flat-b.tsv" "${tmp}/flat-a.tsv")"
        per_rc=$?
        set -e
        if [ -f "${tmp}/relstat" ]; then
            local rst
            rst="$(cat "${tmp}/relstat")"
            reloc_compared=$((reloc_compared + $(echo "${rst}" | cut -f1)))
            reloc_mapped=$((reloc_mapped + $(echo "${rst}" | cut -f2)))
        fi
        echo "PER-SYMBOL  ${rel}: ${per_out##*$'\n'}"
        if [ -n "$(echo "${per_out}" | sed '$d')" ]; then
            echo "${per_out}" | sed '$d' | sed 's/^/            /'
        fi
        if [ "${per_rc}" -ne 0 ]; then
            # An object whose instruction stream was noise-class but whose
            # relocations disagree is NOT "differing within the accepted noise
            # class": take it back out of that bucket, so the summary line keeps
            # meaning what it says.
            if [ "${pos_noise}" -eq 1 ]; then noise=$((noise - 1)); fi
            unclassified=$((unclassified + 1))
            rc=1
        elif [ "${pos_noise}" -eq 1 ]; then
            # The instruction stream was noise-class and the relocations matched
            # through the map. The object stays in the positional noise bucket
            # it was already counted in; it is not a per-symbol pass.
            :
        else
            # NOT `noise`: the original summary line's "differing within the
            # accepted noise class" means POSITIONAL noise-class passes, and an
            # object that passed per symbol -- through the maps, or by being
            # address- and padding-blind -- is a different claim. It gets its
            # own bucket on the coverage line.
            persym_pass=$((persym_pass + 1))
        fi
    done < "${before}/.psym-manifest"

    # --- the other direction: after-arm objects nothing claimed ------------
    LC_ALL=C sort -u "${tmp}/claimed" > "${tmp}/claimed-sorted"
    while IFS= read -r rel; do
        [ -n "${rel}" ] || continue
        unmatched_after=$((unmatched_after + 1))
        echo "ONLY-AFTER  ${rel} (no object in the before arm accounts for it)"
        rc=1
    done < <(LC_ALL=C comm -13 "${tmp}/claimed-sorted" "${tmp}/after-manifest")

    # --- exceptions nobody needed ------------------------------------------
    local stale=0
    if [ -s "${tmp}/exc.tsv" ]; then
        cut -f1 "${tmp}/exc.tsv" | LC_ALL=C sort -u > "${tmp}/exc-all"
        LC_ALL=C sort -u "${tmp}/exc-used" > "${tmp}/exc-used-sorted"
        while IFS= read -r rel; do
            [ -n "${rel}" ] || continue
            stale=$((stale + 1))
            echo "STALE-EXCEPTION  ${rel} (listed as expected to differ; nothing used it)"
        done < <(LC_ALL=C comm -23 "${tmp}/exc-all" "${tmp}/exc-used-sorted")
    fi

    # --- map entries nobody needed -----------------------------------------
    #
    # Non-fatal, exactly like STALE-EXCEPTION. The symbol map's OLD names are
    # marked used by the symbol CENSUS, which runs on every object and sees
    # every defined symbol, so an unused one really is used by nothing; the
    # object map's OLD relpaths are marked used by the manifest walk.
    local stale_map=0 old new
    LC_ALL=C sort -u "${tmp}/symmap-used" > "${tmp}/symmap-used-sorted"
    LC_ALL=C sort -u "${tmp}/objmap-used" > "${tmp}/objmap-used-sorted"
    if [ -s "${tmp}/symmap.tsv" ]; then
        while IFS= read -r old; do
            [ -n "${old}" ] || continue
            new="$(awk -F'\t' -v k="${old}" '$1 == k { print $2; exit }' "${tmp}/symmap.tsv")"
            stale_map=$((stale_map + 1))
            echo "STALE-MAP   ${old} (mapped to ${new}; nothing used it)"
        done < <(LC_ALL=C comm -23 <(cut -f1 "${tmp}/symmap.tsv" | LC_ALL=C sort -u) "${tmp}/symmap-used-sorted")
    fi
    if [ -s "${tmp}/objmap.tsv" ]; then
        while IFS= read -r old; do
            [ -n "${old}" ] || continue
            new="$(awk -F'\t' -v k="${old}" '$1 == k { print $2; exit }' "${tmp}/objmap.tsv")"
            stale_map=$((stale_map + 1))
            echo "STALE-MAP   ${old} (mapped to ${new}; nothing used it)"
        done < <(LC_ALL=C comm -23 <(cut -f1 "${tmp}/objmap.tsv" | LC_ALL=C sort -u) "${tmp}/objmap-used-sorted")
    fi

    echo
    if [ "${sym_rank_tagged}" -gt 0 ] && [ -n "${symbol_map}" ]; then
        echo "P-SYM warning: ${sym_rank_tagged} key(s) needed the [#n] rank fallback, and a symbol map is"
        echo "               in play. The rank is assigned over the union of both arms' MANGLED names,"
        echo "               so it is stable for a name a rename does not touch but NOT across a rename:"
        echo "               two indistinguishable twins can swap ranks between the arms. A DIFFERS or"
        echo "               an ONLY-BEFORE/ONLY-AFTER pair on a [#n]-tagged key may be a limit of this"
        echo "               tool rather than a code difference. The failure direction is conservative"
        echo "               (a false finding, never a masked one), but read such a finding by hand."
    fi
    echo "P-SYM: ${total} objects — ${identical} byte-identical, ${noise} differing within the accepted noise class, ${unclassified} with unclassified differences, ${moved} matched by basename after a path move, ${missing} missing"
    echo "P-SYM coverage: ${mapped_objects} objects matched through the object map, ${persym_objects} compared per symbol, ${persym_pass} objects passed per symbol, ${unmatched_after} after-arm objects unaccounted for; symbols — ${sym_matched} matched (${sym_mapped} through the symbol map), ${sym_only_before} only-before, ${sym_only_after} only-after, ${sym_exception} excepted, ${sym_collision} collisions, ${sym_rank_tagged} rank-tagged, ${stale} stale exceptions, ${stale_map} stale map entries; relocations — ${reloc_compared} records compared (${reloc_mapped} through the symbol map)"
    if [ "${rc}" -eq 0 ]; then
        echo "P-SYM: PASS — read the DELTAS summaries above before accepting (see this script's header)"
    else
        echo "P-SYM: FAIL — differences outside the accepted noise class"
    fi
    return "${rc}"
}

[ $# -ge 2 ] || usage
case "$1" in
    capture) [ $# -le 3 ] || usage; do_capture "$2" "${3:-build}" ;;
    compare) shift; do_compare "$@" ;;
    *) usage ;;
esac
