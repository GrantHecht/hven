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
#     --exceptions <file>   <object>::<symbol> [KIND] ## reason it may differ
#                           (the object qualifier is MANDATORY, `*::` is the
#                           explicit wildcard, and `[KIND]` is optional -- see
#                           the EXCEPTION LOOKUP block for the full key
#                           grammar and its precedence)
#     --allow-foreign-captures
#                           compare a pair of captures made by a DIFFERENT
#                           version of this script (refused by default)
#
#   Environment: PSYM_MAX_PAIRS -- how many UNCLASSIFIED instruction pairs the
#     classifier PRINTS per symbol before it collapses the rest into
#     "... and N more". Default 5; `0` means unbounded. OUTPUT ONLY, with one
#     stated qualification: for every VALID value it changes no verdict, no
#     count and no exit status, and the `UNCLASSIFIED n` figure on every COUNTS
#     line is the full count at any cap -- but an INVALID value (not a
#     non-negative integer) is refused with exit 2 before any comparison runs,
#     so the knob can affect the exit status only by declining to run at all,
#     never by changing a comparison's outcome. It exists because a
#     class of change whose whole transcript is one repeated instruction shape
#     (M6 W5 T1's member-displacement shift) cannot be audited from a listing
#     that hides all but five pairs per symbol -- a claim of "every pair is the
#     same −8 displacement" is checkable only against every pair. A
#     non-integer value is refused rather than silently defaulted.
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
#   (c) MERGED-PAD-class -- an intra-function alignment-nop run whose LENGTH
#       moved, with every non-pad opcode and every relocation record identical,
#       at most one shared control-transfer shift, and that shift accounted for
#       by the pad bytes plus any control transfers the pad change itself
#       re-encoded. Arrived at M6 W5 T6 commit 0 and widened to the re-encoding
#       term at commit 0 fix3; the rule, its SIX conditions, its limit and its
#       falsifiers are stated in full at PSYM_CLASSIFY_AWK.
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
#     always produced. Note what that fast path asserts, since it is the common
#     case and it is STRONGER than the per-symbol verdict below it: a
#     `NOISE-ONLY` line with NO `RELOCATIONS` line under it means the two arms'
#     relocation streams are LITERALLY equal, MANGLED, target for target and
#     addend for addend -- no demangling, no symbol map, no neutralisation.
#     Only an object whose relocations actually differ falls
#     through to the per-symbol layer, which renders each target through the
#     symbol map and then either matches it or reports the pair. If that layer
#     then compares ZERO relocation records, the transcript has contradicted
#     itself -- the streams differ, yet nothing was compared -- and the run
#     FAILS with a `RELOC-BLIND` line naming the cause (M6 W5 T0 fix3); the
#     likeliest cause is an objdump whose relocation-line format this script no
#     longer parses. This is a
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
#       - AN EXECUTABLE SECTION (`.text`, `.text.startup`, `.text._Z...`,
#         decided by objdump's own CODE flag and never by a name prefix) is
#         NOT a datum, and this is the one class where neutralising the addend
#         would mask code identity (M6 W5 T0 fix3). clang references a local
#         symbol in ANOTHER section as section+offset, so a direct call to an
#         INTERNAL-linkage function -- a `static` or anonymous-namespace helper
#         called from a COMDAT template instantiation, a `__cxx_global_var_init`
#         reached from its TLS wrapper -- arrives as `.text+0x1cdc` with the
#         callee's NAME nowhere in the record. For that class the addend IS the
#         callee's identity: neutralising it re-opens, for internal-linkage
#         callees, precisely the same-shape substitution hole the named class
#         above closes for external ones.
#
#         So the addend is RESOLVED instead. `<addend> + 4` for the 32-bit
#         PC-relative forms (whose displacement-field width is folded into the
#         addend) and `<addend>` for the absolute ones is looked up, SECTION-
#         AWARE, in the SAME arm's `objdump -t` table -- section-aware because
#         two of this tree's real targets sit at offset 0 of their section, and
#         an address alone cannot tell `.text+0` from `.text._Zfoo+0`. The
#         defined FUNCTION at that address is then rendered as the target,
#         demangled, symbol-mapped on the before arm, with NO variant tag, and
#         with the residual addend a direct reference to that symbol would have
#         carried (`-0x4` for a PC-relative form, `+0x0` for an absolute one).
#         An internal-linkage callee is therefore compared BY NAME, exactly
#         like an external one, and survives a TU split.
#
#         Where NO defined function sits at exactly that address -- or where two
#         objects of a split's after arm define different functions there, which
#         this script detects and refuses to guess between -- the target is
#         rendered `<section>+<LITERAL addend>` instead. That is R17's stated
#         fallback and the conservative direction: a layout shift then reports a
#         false DIFFERS, never a masked one.
#
#       - A NON-EXECUTABLE SECTION (`.rodata`, `.rodata._ZN3fmt...`,
#         `.data.rel.ro`, `.bss`) is
#         compared by section NAME, with an embedded mangled name demangled and
#         mapped like any other, and its addend NEUTRALISED to `+LOCAL`. The
#         addend of such a relocation is the byte offset of a datum
#         within a section this comparison does not read, and it moves whenever
#         anything ahead of it in that section changes size -- the same layout
#         property the `__FILE__` class (b) and the SELF rule already exclude.
#         The section NAME is stable under a split and is compared.
#
#       - A COMPILER-LOCAL LABEL (`.L.str.137`, `.LCPI8_0`, `.Lswitch.table._Z...`)
#         is compared by its FAMILY: the NUMERIC SUFFIX outside any embedded
#         mangled name is stripped (M6 W5 T1 fix1; it used to replace every
#         digit RUN with `N`, which left the un-suffixed `.L.str` with nothing
#         to replace, so it never compared equal to `.L.str.48` -- two members
#         of one family), and the addend is neutralised as above. An
#         assembler-local label is a name the compiler mints per TU in emission
#         order; splitting a TU or adding one string literal renumbers it
#         wholesale, and it names data in a non-executable section, so its
#         number carries no information this comparison is entitled to assert
#         on. What survives is the family, so a reference that moved from a
#         constant pool to a string table is still a finding.
#
#     THE STATED LIMIT of the two remaining neutralisations (they now apply to
#     NON-executable sections and to compiler-local labels only): a change that
#     makes a call site reference a DIFFERENT datum OF THE SAME KIND is not
#     visible. Three cases, named because a reader of a W5 gate will ask about
#     all three: a different string literal; a different NUMERIC CONSTANT-POOL
#     entry, so a tolerance change such as `1e-6` to `1e-4` is a NOISE-ONLY
#     PASS with 0 relocation records compared; and a different internal-linkage
#     DATA object (a local vtable or static table in `.bss`/`.data.rel.ro`).
#     None of it was visible before fix2 either -- the content lives in a
#     non-executable section, which is class (b) -- so this is the existing
#     coverage limit restated at a finer grain, not a new one. The EXECUTABLE
#     class is no longer part of it: since fix3 an internal-linkage CALLEE is
#     resolved to its symbol and compared by name.
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
#     An exception's SYMBOL may always be written in the PLAIN demangled form
#     this file's FILE FORMATS block documents, on EITHER side. A finding on a
#     tagged key (`[D1]`, `[C2]`, `[_ZThn8_]`, `[#n]`) is matched first against
#     the tagged key and then against the symbol's bare demangled name, and
#     that fallback is symmetric: `ONLY-BEFORE`, `ONLY-AFTER` and `DIFFERS` all
#     honour it. Writing the internal tagged form is never required, and the
#     tag is an implementation detail this script is free to change. The OBJECT
#     qualifier in front of it is not optional (M6 W5 T1 fix2); see FILE
#     FORMATS.
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
#   --exceptions    <object relpath>::<demangled name> [KIND] ## <reason>
#                   Split on the FIRST ` ## `; the KEY is then split on its
#                   FIRST `::` into an OBJECT and a SYMBOL. The symbol is the
#                   NEW-side name for a symbol that exists after, the OLD-side
#                   name for one that existed only before, and either for a
#                   mapped pair.
#
#                   THE OBJECT QUALIFIER IS MANDATORY (M6 W5 T1 fix2). It is
#                   the relpath as it appears in `.psym-manifest` -- the
#                   BEFORE-arm one, which for a split is the old object whose
#                   symbols are being accounted -- or the literal `*`, which is
#                   how an entry says explicitly that it applies in EVERY
#                   object. A key with no `::`, and one whose qualifier is
#                   neither `*` nor a relpath the BEFORE arm's manifest lists
#                   (so, a bare C++ name whose leading namespace would
#                   otherwise parse as an object, and equally a typo'd or
#                   stale object path), is REFUSED with exit 1 before any
#                   comparison runs; so is a key that appears twice, which used
#                   to be silently overwritten by whichever line came last. A
#                   `*::` entry consumed in more than one object prints a
#                   `WILDCARD-USED` line naming them.
#
#                   Why it is mandatory rather than optional: the exception
#                   namespace was GLOBAL while findings are per object. The
#                   same demangled name is one weak COMDAT body in twenty test
#                   objects, so an entry written about the object that lost its
#                   copy also excused a MUTATED body of that name in the object
#                   that gained the strong one. An optional qualifier leaves
#                   that hole open for anyone who does not use it.
#
#                   The KIND a key may also carry -- `[ONLY-BEFORE]`,
#                   `[ONLY-AFTER]`, `[DIFFERS]`, `[SECTION]`,
#                   `[LOCAL-FAMILY]` -- arrived with fix1 and is unchanged: the
#                   qualified form is tried first, a kindless entry still
#                   excuses any kind, and a kindless entry SPENT on a section
#                   move stops covering that symbol's body. Object specificity
#                   is tried before kind specificity. A COMPILER-LOCAL FAMILY
#                   name (`.L.str`, `GCC_except_table`) excuses a family whose
#                   count changed IN THAT OBJECT -- the family rule is per
#                   object, and since fix2 so is the exception that answers it.
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
# fix1 re-ran both calibrations against the wider object set, fix2 re-ran them
# again with relocations compared, and fix3 re-ran them once more with
# executable-section targets resolved: 129 objects and 125145 defined symbols.
# The fix3 transcripts are pasted in `.superpowers/w5-t0-fix3-report.md`, and
# they are byte-identical to fix2's -- the resolution moves no count on this
# tree, because none of the three objects calibration (b) touches carries a
# `.text`-relative relocation. What DID move is measured separately: a survey of
# all 128 disassembled objects finds 27 executable-section targets (25 `.text`,
# 2 `.text.startup`, 13 distinct callees), and fix3 resolves 27 of 27 to a
# symbol name, none falling back to the literal addend.
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
#   `--allow-foreign-captures`). fix3 added two more: a `.text`-relative call to
#   an INTERNAL-linkage callee whose identity two arms swap, with the literal
#   addends coinciding (fix2 reported PASS, 4 relocation records compared; fix3
#   reports DIFFERS naming both resolved callees, and PASSes with 2 records
#   through a symbol map that declares the swap), and an R-BLIND copy of the
#   tool -- one whose relocation-record pattern never matches -- on the
#   same-shape substitution pair (fix2 PASSed with a transcript that said both
#   "the relocation targets are not literally equal" and "0 relocation records
#   compared"; fix3 FAILs it with a `RELOC-BLIND` line).
#
#   M6 W5 T1 fix2 re-ran both calibrations at its own sha and added the two
#   fixtures its own declared changes are answerable to:
#
#     * ADJACENT-SIBLING CALLEE SWAP (the SQP lane's `s_a`/`s_b`): one TU whose
#       `caller` tail-jumps to an anonymous-namespace helper, with two helpers
#       swapped in the layout between the arms so that `caller` reaches a
#       DIFFERENT function at the SAME offset. fix1 reported `caller` IDENTICAL
#       -- the hole this round closes; fix2 reports it DIFFERS, naming
#       `(anonymous namespace)::g(int)` against `(anonymous namespace)::h(int)`.
#       In the same fixture, a second function that calls BOTH helpers in both
#       arms is IDENTICAL under fix2 and DIFFERS under fix1, because the two
#       calls now compare by callee NAME rather than by offset.
#
#     * SAME-NAME, SAME-KIND MUTATION IN A SECOND OBJECT: one weak COMDAT body
#       defined in two objects, mutated in both arms, with an exception written
#       about the FIRST object. fix1 PASSes (its plain key excused both); fix2
#       FAILs, reporting the second object's `DIFFERS`, and PASSes only when the
#       entry is rewritten as an explicit `*::` wildcard -- which then prints
#       `WILDCARD-USED ... 2 objects`. The same fixture shows the three refusals
#       the loader now makes: a bare (fix1-format) key, a key whose qualifier
#       is not an object of the before manifest, and a duplicate key.

# ---------------------------------------------------------------------------
# M6 W5 T1 fix1: SECTION MOVES, AND THE COMPILER-LOCAL FAMILY RULE ONE LAYER UP
# ---------------------------------------------------------------------------
#
# Two DECLARED changes to what a P-SYM verdict asserts, both found by T1's own
# transcripts and ruled on before T6 (which splits a TU and would hit both at a
# larger scale). Each is stated here because CLAUDE.md section 7 makes a change
# to a pinned instrument a declared event, not a discovery.
#
#   * A SECTION MOVE NO LONGER SKIPS THE BODY, AND IS EXCUSABLE BY NAME. Until
#     now `SECTION-MOVED` printed, incremented the difference count and
#     `continue`d -- ahead of the exception lookup every other finding class
#     goes through, and without comparing a single instruction. An `inline`
#     function that becomes a strong out-of-line definition necessarily leaves
#     its COMDAT `.text.<SYM>` for plain `.text`, so NO exception set could
#     make a "move a body out of line" commit pass, however well understood the
#     move was, and the transcript said nothing at all about whether the code
#     had changed. Now: the line is still printed, the body is compared anyway
#     on the same per-symbol path as every other symbol, and the move is
#     excusable by name. The exception key `<name> [SECTION]` excuses the MOVE
#     alone; a PLAIN-name entry excuses the move and is then SPENT, so a body
#     that also differs is still a finding. That is one half of ONE EXCEPTION,
#     ONE FINDING; the other half is the KIND-QUALIFIED exception key documented
#     under FILE FORMATS, without which an entry written about the object that
#     LOST a weak copy (`ONLY-BEFORE`) silently excuses the changed body in the
#     object that GAINED the strong one -- measured on this very commit. Both
#     halves are what keep the falsifier live: change one instruction inside a
#     moved body and the run still FAILS.
#
#     Companion sections move with the body. A relocation whose target is the
#     ENCLOSING symbol's own `.rodata.<SELF>` / `.gcc_except_table.<SELF>` /
#     `.text.<SELF>` (its jump table, its LSDA, its own text) is rendered as the
#     BASE section, because those COMDAT companions merge into the plain
#     sections exactly when the body does. Without that rule an excused
#     `SECTION-MOVED` is immediately followed by a `DIFFERS` on the jump-table
#     relocation, which is the same layout fact reported twice. A companion
#     section naming SOME OTHER symbol is untouched: that really is a different
#     datum.
#
#   * THE SYMBOL CENSUS NOW COMPARES `.L*` AND `GCC_except_table<n>` BY FAMILY
#     COUNT. The RELOCATION layer has always compared a compiler-local label by
#     its family, with the reason stated in the RELOCATION TARGETS block: the
#     number is minted per TU in emission order and carries no information this
#     comparison is entitled to assert on. The census compared the same names
#     LITERALLY, so any change that shifted a TU's emission order produced
#     hundreds of `ONLY-BEFORE`/`ONLY-AFTER` findings with no code difference
#     behind them (M6 W5 T1 measured 1744/1714 on a commit whose callers had no
#     source change) and needed a generated exception line per label to say
#     nothing. The census now counts them PER OBJECT PER FAMILY and compares the
#     counts, so a renumbering is silent and a family that GAINS or LOSES a
#     member is a `LOCAL-FAMILY` finding, excusable by the family name. The
#     family is the name with its NUMERIC SUFFIX removed -- a suffix, not every
#     digit run, so `.L.str` and `.L.str.48` are one family, which the older
#     digit-run rule could not express. Families covered: `.L*` and
#     `GCC_except_table<n>`, both DATA.
#
#     `__cxx_global_var_init.<n>` was deliberately OUTSIDE that rule at fix1:
#     it is a FUNCTION, and excusing a renumbering must never excuse a body. Its
#     number is dropped in demangle_table() instead, which routed the family
#     through the per-symbol layer's `[#n]` rank fallback so the bodies were
#     compared pairwise. MEASURED, and corrected in fix2 (the fix1 text here
#     said this tree defines none): the tree defines 40 of them in 20 objects,
#     and 38 carry a rank tag in every comparison -- the two objects with a
#     single initialiser need none. On a stable TU the rank is emission order
#     and the pairing is exact; across a TU SPLIT it is not, which is why the
#     tool prints a `P-SYM warning:` when it mints one while a symbol map is in
#     play, and why "T6 should key the family by the global each initialiser
#     touches instead."
#
#     SUPERSEDED AT M6 W5 T6 COMMIT 0, which does exactly that. The per-symbol
#     layer now keys each initialiser by the global it touches, and the family
#     therefore JOINED the census count rule -- see is_local_family() for why
#     that is now sound and what each layer asserts.
#
#   Both changes make a former FAIL able to pass -- the first only with a named
#   exception, the second only where the count is equal -- and neither can turn
#   a passing comparison into a failing one except by finding something: a
#   family whose count moved, or a moved body whose instructions differ.
#
# ---------------------------------------------------------------------------
# M6 W5 T1 fix2: THE SELF RANGE IS THE SYMBOL'S OWN SIZE, AND AN EXCEPTION
# NAMES THE OBJECT IT IS ABOUT
# ---------------------------------------------------------------------------
#
# Two more DECLARED changes, both closing a hole a REVIEWER found in fix1
# rather than one a transcript showed, and both required before the next P-SYM
# gate reads a comparison that can contain the case they cover.
#
#   * THE SELF RULE IS BOUNDED BY THE SYMBOL TABLE, AND AN OUT-OF-RANGE TARGET
#     IS RESOLVED BY NAME. fix1 bounded the address-based limb by "last
#     instruction + 16", argued that a sibling function was outside that range
#     by construction, and was WRONG: functions are 16-byte aligned, so the
#     next one commonly starts inside that envelope, and two arms whose caller
#     tail-jumped to two DIFFERENT adjacent siblings compared IDENTICAL. The
#     range is now `[start, start + size)` from `objdump -t`; a placeholder at
#     the end of a body is SELF only when the following relocation record
#     independently names its target; and a bare target outside the range is
#     resolved through the symbol table to the symbol CONTAINING it and
#     compared BY NAME. See flatten_symbols() for the four limbs in order. The
#     direction of this one is STRICTER on both counts: it removes an equation
#     the tool used to make, and it also compares by name a class of target
#     that even the PRE-fix1 tool compared by bare address -- a callee swap
#     that kept the address was masked then too.
#
#   * AN EXCEPTION KEY NAMES ITS OBJECT. `<object relpath>::<symbol> [KIND]`,
#     with `*::<symbol>` as the explicit wildcard, a bare symbol name REFUSED,
#     and a duplicate key REFUSED (it used to be silently overwritten, so a
#     file could carry two reasons for one key and print the wrong one). Kind
#     qualification alone left the namespace global across objects, and the
#     same demangled name is one weak COMDAT body in twenty test objects. See
#     the EXCEPTION LOOKUP block. This one can only make a comparison FAIL that
#     used to pass -- it never excuses more -- and every exception file written
#     for an earlier version of this script has to be rewritten, which is
#     deliberate: a file that still parses would be a file whose masking was
#     never re-examined.
#
# ---------------------------------------------------------------------------
# M6 W5 T2 commit 0: A SIZE-ZERO SYMBOL IS STILL A NAME, AND THE OBJECT WINS
# ---------------------------------------------------------------------------
#
# Four DECLARED changes, three of them from the reviewer re-check of fix2 and
# one from the SQP lane. None moved a verdict on the population fix2 gated --
# each is checked by its own falsifier, and both directions are stated.
#
#   * THE EXACT-START LOOKUP SEES A SIZE-ZERO FUNCTION. fix2 bounded the SELF
#     rule by the symbol size and resolved an out-of-range target through the
#     symbols that CONTAIN it, but containment skips a size-zero row -- there
#     is no range to be inside -- so two arms whose caller reached two
#     DIFFERENT size-zero siblings at one offset still rendered the same bare
#     address and compared IDENTICAL. containing() now tries an exact
#     (section, start) lookup FIRST, over every defined function row whatever
#     its size. Same start, same size, different names is an alias pair and the
#     lexicographically smallest MANGLED name is taken (deterministic on both
#     arms, so an alias never DIFFERS and a rename still does); same start,
#     different sizes is genuine ambiguity and the limb declines, leaving the
#     containment limb to decide exactly as it did before. STRICTER only: it
#     adds resolution and removes none.
#
#   * AN OBJECT-SPECIFIC EXCEPTION BEATS A WILDCARD, WHICH IS WHAT THE HEADER
#     ALWAYS SAID. A kind-qualified `*::` entry used to be probed BEFORE an
#     object-specific kindless one, so the reason attached to the wildcard
#     could be printed for a finding an object-specific entry was written
#     about, and that entry then reported STALE. The four kindless probes now
#     sit with their own specificity level. Inert on every exception file this
#     tree has written (all carry zero wildcards) -- closed before one does.
#
#   * TRAILING-PAD PREFIXES ARE ORDER-FREE, matching psym_bin_pairs.awk. See
#     is_pad().
#
#   * The usage synopsis states the object-qualified exception key fix2 made
#     mandatory.

# ---------------------------------------------------------------------------
# M6 W5 T6 commit 0: WHAT A SPLIT NEEDS -- AN EMPTY BUILD DIRECTORY, A
# PER-OBJECT RESOLUTION TABLE, INITIALISERS KEYED BY THEIR GLOBAL, AND A
# MERGED ALIGNMENT PAD THAT IS NOT A FINDING
# ---------------------------------------------------------------------------
#
# Five DECLARED changes, landed BEFORE the T6 driver restructure rather than
# discovered inside it. Four are registered items (M6 W5 T0 F-D, T1 fix1's own
# note, T2 fix1's class, the T3 ledger); one is the SQP lane's T5 M1, in
# psym_bin_pairs.awk. Each carries its own falsifier, and both directions are
# stated. The rules the fifth item states also apply to the audit script.
#
#   * A CAPTURE TAKEN AFTER A TU MOVE, RENAME OR DELETE MUST START FROM AN
#     EMPTY BUILD DIRECTORY. The PRECONDITION block above says both arms must
#     be built at the same absolute build-directory path, and that stands. This
#     is the second half of it, and M6 W5 T4 (D2) is where it was learned the
#     expensive way. `ninja -t clean` removes the outputs of the CURRENT build
#     graph, and a renamed, moved or deleted TU is not in it -- so its stale
#     `.o` survives, `capture` enumerates it from the target directory, and the
#     after arm carries an object the after source tree cannot produce. T4's
#     transcript is the record: one `ONLY-AFTER ... ipqp_trace.cpp.o`,
#     `1 after-arm objects unaccounted for`, `P-SYM: FAIL`. The tool caught it
#     THERE only because the object was gone from the before arm; a stale object
#     present in BOTH arms is byte-identical and passes, while asserting
#     something about a file that no longer exists.
#
#     MEASURED at M6 W5 T6 commit 0, on ninja 1.13.2, with a two-TU probe whose
#     second TU is renamed (`.scratch/w5t6-0/ninja`, reproduced in that task's
#     report). Objects present after the rename and a rebuild:
#
#       no clean at all                        alpha.o  beta.o(STALE)  gamma.o
#       ninja -t clean                         alpha.o  beta.o(STALE)  gamma.o
#       ninja -t cleandead                     alpha.o                 gamma.o
#       ninja -t cleandead && ninja -t clean   alpha.o                 gamma.o
#       rm -rf <build> && configure again      alpha.o                 gamma.o
#
#     So `cleandead` DOES remove it on this ninja -- which is a statement about
#     ninja 1.13.2 and about a `.ninja_log` that survived, not a guarantee: the
#     dead-output list is reconstructed from that log, and a log that was
#     truncated, deleted, or written by a ninja whose cleandead behaviour
#     differs takes the tool straight back to the first two rows.
#
#     THE RULE, which needs no version caveat: for any comparison whose change
#     ADDS, MOVES, RENAMES or DELETES a translation unit -- every T6 landing
#     from cut (d) on, and T4 and T8 besides -- `rm -rf` the build directory and
#     CONFIGURE AGAIN for BOTH arms, at the same absolute path. A comparison
#     whose change only edits existing TUs may rebuild in place as before. This
#     script cannot check the rule for you, for the same reason it cannot check
#     the path precondition: a snapshot records object bytes, not the history of
#     the directory they came from.
#
#   * THE EXECUTABLE-SECTION RESOLUTION TABLE IS KEYED PER OBJECT (T0 F-D). A
#     split arm is the UNION of several objects, and every relocatable object
#     starts each of its sections at offset 0, so `.text+0x1cdc` names a
#     different function in each half. Keyed by (arm, section, offset) alone the
#     table saw two names for one key, refused to guess -- correctly -- and fell
#     back to the LITERAL addend, which then differed from the resolved name the
#     before arm produced on EVERY internal-linkage call across the split. The
#     key now carries the object the relocation was read from, and the after arm
#     is disassembled, laid out and flattened ONE OBJECT AT A TIME, so each
#     target is resolved in the object that carries it. `@ambiguous@` survives
#     and now means what it says: two functions at one address WITHIN one
#     object. Direction: STRICTER -- it replaces a layout-dependent literal with
#     a comparison by NAME. Falsifier: a callee substituted across the split
#     must still DIFFER (see the negative fixtures below).
#
#   * `__cxx_global_var_init.<n>` IS KEYED BY THE GLOBAL IT INITIALISES. T1
#     fix1 recorded the need for this in its own text ("T6 should key the family
#     by the global each initialiser touches instead"): the number is emission
#     order, each half of a split renumbers from zero, and the `[#n]` rank
#     fallback then pairs two unrelated bodies. See the block above
#     gvi_identities() for the identity, and for the two cases in which it is
#     DECLINED rather than approximated. A declined initialiser is counted on
#     the PERSYM line, and is a FINDING when the after arm is a split. Direction:
#     both -- it can turn a false ONLY-BEFORE/ONLY-AFTER pair into a match, and
#     it can FAIL a split whose initialisers cannot be identified, which the
#     rank fallback used to pass on a coincidence.
#
#   * A MERGED INTRA-FUNCTION ALIGNMENT PAD IS ACCEPTED NOISE, CLASS (c). The
#     accepted noise class above gains a third member; the rule, its six
#     conditions, its stated limit and its falsifiers are written out in full at
#     PSYM_CLASSIFY_AWK, because that is the ONE classifier both paths call and
#     widening it must stay a single visible act. It answers the T3 ledger item:
#     `require_claimed_nonzeros`, an UNEDITED file-local neighbour of an edited
#     function, differed by one merged 16-byte pad with 307 non-pad opcodes and
#     12 relocation records identical, and had to be excused by hand and audited
#     from raw disassembly because the line COUNT moved and the classifier said
#     STRUCTURAL. This also carries T2 fix1's trailing-pad class INWARD, which
#     is the registered "intra-function alignment-nop normalisation on the
#     per-symbol path": the per-symbol layer already trimmed a TRAILING pad run
#     before comparing, and now a pad run inside the body reaches the classifier
#     instead of the STRUCTURAL exit. Direction: a former FAIL can pass. A
#     transcript names it -- a `PAD-MERGED` line with the pad counts and the
#     control-transfer shift, or a `PAD-ROUTE DECLINED` line saying the route
#     was tried and refused -- so it is never silent.
#
#   * `psym_bin_pairs.awk` GAINS AN RDATA BIN for a relocation record whose
#     type and target are identical and whose addend differs, excluding an
#     executable-section target. The SQP lane registered it at T5 (M1). Its
#     reasoning and its stated limit are in that file.
#
#   * COMMIT 0 fix3: EVERY INSTRUCTION CARRIES ITS BYTE LENGTH, class (c) gains
#     a SIXTH condition, and rule 5 gains a re-encoding term. flatten_symbols()
#     annotates every non-pad instruction line with ` ;LEN=<n>` beside the
#     ` ;PAD=<n>` it already put on pads, and the `S` record carries the symbol
#     start as a fifth field. Rule 5 now reads
#     `shift == pad delta + control-transfer widening`, and rule 6 forbids a
#     length change anywhere else, so the residual the widening term opens is
#     bounded rather than free. The PAD-MERGED / PAD-ROUTE DECLINED lines print
#     the widening count and both arms symbol start mod 32, which is the CAUSE
#     of the class on a `-falign-loops=32` toolchain. `psym_bin_pairs.awk` gains
#     a WIDTH bin for the pairs this makes visible. Direction: a former FAIL can
#     pass (the widening term), AND a former pass can fail (rule 6, and any
#     length change that used to be invisible). Registered by the SQP lane and
#     by Codex at the T6.b review, from the `push_history` symbol that had to be
#     excused by name on a hand audit.
#
# THE CLAIM TEMPLATE. A P-SYM gate is a CLAIM made BEFORE the run and checked
# against the transcript, not a transcript read afterwards for whatever it
# happens to say. The template a task's claim follows:
#
#     "<the named functions/objects> DIFFERS, by name, with <the reason> as the
#      cause; <the named class> is ONLY-AFTER / ONLY-BEFORE, excused by name;
#      everything else identical -- AND ITS FILE-LOCAL NEIGHBOURS MAY SHIFT
#      WITHIN `.text`."
#
# The last clause arrived with the T3 ledger and is part of the template, not a
# hedge. At `-O3`, without `-ffunction-sections`, every function of a TU shares
# one `.text`: editing one function moves the ones after it, changes what
# alignment padding the assembler inserts around them, and shifts every
# self-relative target inside them -- with no source change and no opcode
# change anywhere. A claim that says "only the edited functions move" is
# therefore FALSE as stated on this toolchain, and a reviewer holding the
# transcript to it will either reject a correct commit or accept a widened
# exception list to make it true. The classifier now classifies the common case
# (class (c) above) instead of leaving it to prose, but the claim still has to
# ADMIT the class, because a neighbour can still land outside it.
#
# ---------------------------------------------------------------------------

set -euo pipefail

usage() {
    echo "usage: $0 capture <snapshot-dir> [build-dir]" >&2
    echo "       $0 compare [--object-map <f>] [--symbol-map <f>] [--exceptions <f>]" >&2
    echo "                  [--allow-foreign-captures] <before-dir> <after-dir>" >&2
    exit 2
}

OBJDUMP="${OBJDUMP:-objdump}"
NM="${NM:-nm}"

# ---------------------------------------------------------------------------
# THE DISASSEMBLER IS PART OF THE INSTRUMENT, AND IT IS CHECKED (M6 W5 T6
# commit 0 fix2, Codex Important 3).
#
# `OBJDUMP` is configurable, and two of this script's parsers are written to
# GNU binutils' exact output shapes: exec_sections() reads the `CODE` flag from
# `objdump -h`, and flatten_symbols() requires an instruction line whose address
# is followed IMMEDIATELY by a colon and a tab. `llvm-objdump` prints `TEXT`
# rather than `CODE` and puts spaces before that tab. Pointed at it, this script
# would parse ZERO executable sections and ZERO instructions -- and a mapped or
# split comparison would then hold symbols with EMPTY bodies and compare them
# EQUAL. A silent all-pass is the worst failure an identity instrument can have,
# so the disassembler is verified before any comparison runs and a symbol that
# parses to zero instructions is a HARD ERROR, never "identical".
#
# The check is a version probe here plus a per-object self-check in do_compare()
# (see REFUSE-EMPTY below). `capture` does not disassemble and is not gated.
# ---------------------------------------------------------------------------
require_gnu_objdump() {
    local v
    if ! v="$("${OBJDUMP}" --version 2>/dev/null | head -1)"; then
        echo "psym_compare: cannot run '${OBJDUMP} --version'." >&2
        exit 2
    fi
    case "${v}" in
        *"GNU objdump"*) : ;;
        *)
            echo "psym_compare: REFUSING to run: '${OBJDUMP}' is not GNU objdump." >&2
            echo "              version line: ${v}" >&2
            echo "              This script parses GNU binutils' exact output shapes -- the" >&2
            echo "              CODE flag of 'objdump -h' and an address followed immediately" >&2
            echo "              by ':<TAB>' on an instruction line. Another disassembler" >&2
            echo "              (llvm-objdump prints TEXT and puts spaces before the tab)" >&2
            echo "              parses to ZERO sections and ZERO instructions here, and empty" >&2
            echo "              bodies compare EQUAL. Set OBJDUMP to GNU objdump." >&2
            exit 2 ;;
    esac
}

# Normalized and EXPORTED so the classifier's ENVIRON lookup sees it whether or
# not the caller exported it, and refused when it is not a count: a typo that
# silently reverted to 5 would leave an audit reading a truncated listing while
# believing it unbounded.
PSYM_MAX_PAIRS="${PSYM_MAX_PAIRS:-5}"
if ! [[ "${PSYM_MAX_PAIRS}" =~ ^[0-9]+$ ]]; then
    echo "psym_compare: PSYM_MAX_PAIRS must be a non-negative integer (0 = unbounded);" >&2
    echo "              got '${PSYM_MAX_PAIRS}'" >&2
    exit 2
fi
export PSYM_MAX_PAIRS

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

# ---------------------------------------------------------------------------
# OBJECT LAYOUT: the section headers and the symbol table, from ONE `objdump`
# run per object per arm, cached in a file that both derivations below read.
# They exist for the EXECUTABLE-SECTION relocation class documented in the
# RELOCATION TARGETS block above: a `.text+0x1cdc` target has to be resolved to
# the function defined at that address, and doing it needs the section flags
# (which sections are code) and the symbol table (what is where).
#
# Cost: this runs ONLY for an object that already reached the disassembly
# stage, i.e. one that is not byte-identical across the two arms. A no-op
# rebuild runs it zero times.
# ---------------------------------------------------------------------------
obj_layout() {
    local obj
    for obj in "$@"; do
        "${OBJDUMP}" -h -t "${obj}"
    done
}

# The names of the object's EXECUTABLE sections, read from `objdump -h`'s own
# CODE flag rather than from a `.text` name prefix -- `.text.startup` and
# `.text._Z...` are code, `.init_array` is DATA and must not be treated as
# code, and a section named `.textual` would not be code at all. Reads a cached
# `objdump -h -t` listing on stdin.
exec_sections() {
    awk -v objid="${1:-1}" '
        /^Sections:/ { inh = 1; next }
        /^SYMBOL TABLE:/ { inh = 0; next }
        !inh { next }
        /^[ \t]*[0-9]+[ \t]+[^ \t]+[ \t]/ { sec = $2; next }
        { if (sec != "" && $0 ~ /(^|[ ,])CODE([ ,]|$)/) print objid "\t" sec; sec = "" }'
}

# "<section>\t<offset in hex, no leading zeros>\t<SIZE in hex>\t<mangled name>"
# for every DEFINED FUNCTION of the object. Reads the same cached listing on
# stdin.
#
# FUNCTIONS ONLY, deliberately: objdump also lists a section symbol (`l d`) at
# offset 0 of every section, and that symbol's name IS the section name, so
# including it would resolve every `<section>+0` target to the useless string
# `.text` and shadow the real function that sits there -- which is exactly the
# case two of this tree's thirteen real targets are in.
#
# The key is (section, offset), never the offset alone: a COMDAT template
# instantiation and a `.text` helper both sit at offset 0 of their own
# sections.
#
# The SIZE column arrived with M6 W5 T1 fix2. It is what bounds the SELF rule
# in flatten_symbols() to `[start, start + size)`, and what lets a bare target
# OUTSIDE that range be resolved to the symbol that actually CONTAINS it
# instead of being compared as a bare address. Both are the same fact read from
# the one table that states it, rather than guessed from a disassembly listing.
sym_addrs() {
    awk -v objid="${1:-1}" '
        /^SYMBOL TABLE:/ { ins = 1; next }
        !ins { next }
        {
            p = index($0, "\t")
            if (p == 0) next
            s = index($0, " ")
            if (s < 2) next
            val = substr($0, 1, s - 1)
            if (val !~ /^[0-9a-fA-F]+$/) next
            # The flag field is seven fixed columns; column 7 is the
            # function/file/object letter.
            if (substr($0, s + 7, 1) != "F") next
            head = substr($0, 1, p - 1)
            n = split(head, hf, /[ \t]+/)
            sec = hf[n]
            if (substr(sec, 1, 1) != ".") next
            rest = substr($0, p + 1)
            q = index(rest, " ")
            if (q == 0) next
            sz = substr(rest, 1, q - 1)
            if (sz !~ /^[0-9a-fA-F]+$/) next
            nm = substr(rest, q + 1)
            if (nm == "") next
            printf "%s\t%x\t%x\t%s\t%s\n", sec, strtonum("0x" val), strtonum("0x" sz), nm, objid
        }'
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

# ---- accepted noise class (c): A MERGED INTRA-FUNCTION ALIGNMENT PAD -------
# M6 W5 T6 commit 0, from the T3 ledger. The assembler emits alignment padding
# INSIDE a function (a loop or branch-target alignment) as well as between
# functions, and the length of an intra-function run is decided by where the
# preceding instructions happened to land -- so an edit ELSEWHERE in the same
# `.text`, or a neighbour that grew, can merge two pad runs into one or split
# one into two without changing a single opcode. T3 measured exactly that on
# `require_claimed_nonzeros`, an UNEDITED file-local neighbour of an edited
# function: 307 non-pad opcodes and 12 relocation records identical, one
# 16-byte pad merged, 57 self-relative targets shifted by -0x10 and 4
# unchanged. The line COUNT moves, so the classifier called it STRUCTURAL and
# the pair had to be excused by hand and audited from raw disassembly.
#
# It is now classified. The rule, stated so that widening it stays a visible
# act -- a pair is PAD-MERGED iff, after DELETING every alignment-pad line from
# both sides:
#
#   1. the pad run actually CHANGED -- either its LINE COUNT moved, or its BYTE
#      TOTAL did at an unchanged line count (M6 W5 T6.a fix1, from the SQP lane
#      ruling and the Claude-substitute F-I2). The second half is a nop
#      RE-ENCODING: the assembler picked `nopl 0x0(%rax)` at 7 bytes where it
#      had picked it at 3, or the other way, and the line reads the same either
#      way because objdump renders both the disp8 and the disp32 forms
#      identically. T6.a met it in the `push_history` lambda of `solve_impl_body` --
#      201 instructions, ONE changed line, and that line a pad -- and had to
#      excuse the whole symbol BY NAME, which is symbol-granular and would
#      therefore swallow a real one-line change sitting beside it at cut (b).
#      Rule 5 is unchanged and still governs: with the count equal the byte
#      delta is the only thing that moved, and every target shift must equal it.
#      If NOTHING moved -- equal count AND equal byte total -- this is not the
#      class and the ordinary verdict stands;
#   2. the two non-pad sequences have the SAME length -- no non-pad line was
#      added or removed;
#   3. every non-pad pair is either literally equal, or differs ONLY in a
#      trailing CONTROL-TRANSFER TARGET (bare hex, or `SELF+0xN`) with the same
#      mnemonic, the same leading operands and the same RENDERING kind; and
#   4. the target deltas take at most TWO values, 0 and ONE shared nonzero
#      shift -- which is what a SINGLE merged or split pad produces, everything
#      before it unmoved and everything after it moved by the same amount.
#
# On the PER-SYMBOL path the relocation records are lines of the compared
# stream and are never pads, so rule 2 and rule 3 assert them identical, target
# for target and addend for addend. On the POSITIONAL path the relocation lines
# were deleted before comparison, and the `cmp -s` on the raw relocation
# streams that follows a passing verdict is what asserts them; that comparison
# strips the within-section OFFSET, which is the only thing a merged pad moves.
#
#   5. the shared nonzero shift EQUALS the BYTE-LENGTH CHANGE OF THE PAD RUN
#      PLUS the summed BYTE-LENGTH CHANGE OF THE CONTROL TRANSFERS that were
#      re-encoded (both sides AFTER minus BEFORE). Rule 5 is the tightening the
#      SQP lane required at the T6.0 review, and it is what makes this a layout
#      rule rather than a licence. Without it rules 3 and 4 accept ANY single
#      shared shift, and the lane exhibited the mask on its own listings: a
#      function whose trailing pad lost one 3-byte nop and whose SOLE branch was
#      retargeted by -32 passed as noise, and one whose 16-byte pad merged while
#      both targets moved by -8 passed too. With rule 5 both are rejected
#      (-32 != -3, -8 != -16) and the T3 shape is kept (-16 = -16).
#
#      THE RE-ENCODING TERM ARRIVED AT COMMIT 0 fix3, on the SQP lane §6
#      ruling and the Codex I2 at the T6.b review, and the reason it had to is the
#      MECHANISM the rule 5 text already named: retargeting -- or merely
#      MOVING -- a branch can change its encoding length (rel8 <-> rel32), and a
#      branch that widens then pushes everything after it further along, so the
#      shift a listing shows is the pad delta AND the widening, not the pad
#      delta alone. T6.b measured exactly that on `push_history`, an unedited
#      lambda of the edited function: pad 7 -> 19 bytes (+12), ONE `je` re-encoded
#      2 -> 6 bytes (+4), 24 self-targets shifted +16 and 5 unshifted, 216 non-pad
#      lines otherwise identical. 12 + 4 = 16 closes; 12 alone does not, so the
#      pre-fix3 rule declined and the symbol had to be excused BY NAME on a hand
#      audit of all 223 instructions. A by-name exception is symbol-granular and
#      does not scale to a cut that moves many lambdas at once.
#
#      "THE SAME LOGICAL TARGET", NOT "THE SAME RENDERED TARGET TEXT". The
#      registration in the T6.b transcript said the contributing branches are
#      those "whose rendered target text is unchanged"; taken literally that
#      would have excluded the very `je` it was written about, whose rendered
#      target moved `SELF+0x284` -> `SELF+0x294` with the rest of the body. What
#      is unchanged is the target INSTRUCTION, and rules 3 and 4 are already
#      exactly that assertion: same mnemonic, same leading operands, same
#      rendering kind, and a target delta of 0 or the ONE shared shift. So the
#      contributing set is the control transfers those rules accepted, and the
#      wording is corrected here rather than reproduced.
#
#   6. NO NON-CONTROL-TRANSFER INSTRUCTION CHANGED BYTE LENGTH. A pad run and a
#      relative branch are the two things whose encoding LAYOUT is entitled to
#      move; anything else that occupies a different number of bytes while
#      rendering the same text -- `add $0x1,%eax` as `83 c0 01` on one arm and
#      `05 01 00 00 00` on the other, which objdump prints identically -- is a
#      RE-ENCODING, and it would pay part of the shift rule 5 is checking
#      without appearing in either term. Rule 6 is what keeps the rule 5 residual
#      term from being a hole: the residual may be paid by pads and by branches,
#      by nothing else, and a length change anywhere else is a finding.
#
# WHERE THE BYTE LENGTHS COME FROM. The byte length of an instruction is NOT
# derivable from its rendered text: objdump prints both `0f 1f 40 00` (4 bytes)
# and `0f 1f 80 00 00 00 00` (7 bytes) as `nopl 0x0(%rax)`, and it prints a
# rel8 `je` and a rel32 `je` to one target identically too. It IS derivable in
# flatten_symbols(), which has the address of each instruction -- the length of
# a line is the address of the next minus its own, and for the last line of a
# block it is the symbol end, where the symbol table gives a size. Since
# M6 W5 T6 commit 0 fix1 it ANNOTATES every pad line with ` ;PAD=<n>`, and since
# commit 0 fix3 every OTHER instruction line with ` ;LEN=<n>` (`?` in place of
# the number where the block gives no successor and no size). The pad annotation
# is part of the compared text, which is a second, free strictness: two pads that
# render identically but occupy different numbers of bytes DIFFER. The `;LEN=`
# annotation is NOT part of any text comparison -- rules 3 and 4, the
# immediate-move class and the printed pair all read the line through
# strip_len() -- because a widened branch must still be RECOGNISED as the same
# branch; what it does instead is make rules 5 and 6 answerable, and make a
# length change visible to a reader and to the WIDTH bin of psym_bin_pairs.awk.
#
# PARTIAL INFORMATION IS NOT USED. Where a re-encoded control transfer carries
# no length (`;LEN=?`, or an unannotated hand-written listing), the residual
# term is dropped WHOLE and rule 5 falls back to the pad-only equality it
# asserted before fix3. A listing cannot be told from one in which a second,
# unseen re-encoding cancels the visible one, so half a sum is worth less than
# none; the fallback is never weaker than the pre-fix3 rule.
#
# SO THE POSITIONAL PATH CANNOT TAKE THIS ROUTE WHENEVER RULE 5 HAS ANYTHING TO
# SAY: its input is the listing normalize_raw() makes, which carries no
# annotation, so a pair with a shifted control-transfer target DECLINES with
# `PAD-ROUTE DECLINED (no pad byte lengths...)` and falls through to the
# per-symbol layer exactly as it does for every other positional failure -- and
# the per-symbol layer, which has the annotation, is the verdict. That is the
# conservative direction and it is what closes the mask the lane found, on the
# path they demonstrated it on.
#
# It is NOT true that the positional path can never take the route at all
# (corrected by the SQP lane at the T6.0 fix1 review, M6): where the pad count
# moved and NO target shifted, rule 5 has nothing to check -- there is no
# observable consequence for the byte change to have to explain -- and rules 1-4
# decide on their own, with or without the annotation. That case is a pad run
# with nothing after it in the symbol, which is exactly a trailing pad.
#
# THE RESIDUAL LIMIT, stated so that it is not re-discovered: after rule 5 a
# retargeting is masked ONLY if its delta coincidentally EQUALS the NET RETAINED
# INTERIOR-PAD DELTA PLUS THE NET CONTROL-TRANSFER WIDENING. The earlier gloss
# "lands at the merged-pad boundary" was wrong and is corrected here (Codex
# Minor, T6 commit 0 fix2): this code does not retain pad POSITIONS at all, and
# where a body has several interior pad runs it compares against their NET
# total, so the residual is an arithmetic coincidence and not a positional one.
# COMMIT 0 fix3 WIDENS THAT RESIDUAL BY EXACTLY ONE TERM, and says so rather
# than leaving it to be found: a retarget whose delta happens to equal
# `pad delta + widening delta` is now masked where before only
# `pad delta` masked it. The term is bounded by rule 6 (nothing but a pad or a
# relative branch may change length) and by rules 3 and 4 (the widened branch
# must be the SAME instruction to a target that moved by 0 or by the one shared
# shift), and the widening count and its byte total are PRINTED on the
# PAD-MERGED line, so a reader can see how much of the shift the branches paid.
# Note also the SIGN, since one review stated it
# backwards: both sides are AFTER minus BEFORE -- the target shift is
# `target_after - target_before` and the pad delta is
# `padbytes_after - padbytes_before` -- not "deleted minus inserted".
# Everything else is now a finding. The deltas and the pad byte change are PRINTED, exactly
# as class (a) prints its immediate deltas, and they are the half of the
# classification a regex cannot do for the reader. THE NEXT TIGHTENING, if a
# real case of that residual ever appears, is POSITIONAL: a merged pad shifts
# every target AFTER it and no target BEFORE it, so the two delta values are
# not merely {0, k} but are 0 for the lines above the pad and k for the lines
# below it. That is recorded here rather than built now, because it needs the
# position of the pad in the block carried alongside its length, and no case has
# demanded it.
#
# The falsifiers this rule is answerable to, all run at every landing: a pad
# merge that ALSO changes one non-pad opcode (rule 3), one whose relocation
# record names a different callee (rule 3), one with two distinct nonzero
# shifts (rule 4), one that adds a non-pad instruction (rule 2), the two masks
# the lane exhibited (rule 5), and -- since fix3 -- a widened branch whose
# target ALSO moved to somewhere the shared shift does not explain (rules 4/5),
# a NON-control-transfer length change (rule 6), and a widening sum that does
# not close the residual (rule 5).
#
# THE ALIGNMENT DIAGNOSTIC. The PAD-MERGED and PAD-ROUTE DECLINED lines carry
# the symbol s ABSOLUTE start on both arms and that start modulo 32, wherever
# the caller supplied them (the per-symbol path always does). That is the CAUSE
# of this whole class on this toolchain: it builds with `-falign-loops=32`, so a
# body whose absolute start moved off a 32-byte boundary needs a longer interior
# pad run to land its loop back on one -- and it was the fact that took a hand
# disassembly to establish at T6.b. It is a diagnostic and nothing is decided by
# it: no rule reads it, and a symbol whose starts are unknown classifies exactly
# as it did.
function is_pad_line(s) {
    return s ~ /^\t((data16|cs|rex[0-9a-z.]*)[ \t]+)*(nop[lwqb]?([ \t]|$)|xchg[ \t]+%ax,%ax([ \t]|$))/
}
# The byte length flatten_symbols() annotated onto a pad line, or -1 for an
# unannotated line (the positional path) and -2 for an explicitly unknown one
# (`;PAD=?`, a block whose last line has no successor and no size).
function pad_len(s) {
    if (match(s, /[ \t];PAD=[0-9]+$/)) return substr(s, RSTART + 6, RLENGTH - 6) + 0
    if (s ~ /[ \t];PAD=\?$/) return -2
    return -1
}
# The byte length flatten_symbols() annotated onto a NON-pad instruction line
# (M6 W5 T6 commit 0 fix3), or -1 for an unannotated line (the positional path,
# and every hand-written fixture written before fix3) and -2 for an explicitly
# unknown one.
function insn_len(s) {
    if (match(s, /[ \t];LEN=[0-9]+$/)) return substr(s, RSTART + 6, RLENGTH - 6) + 0
    if (s ~ /[ \t];LEN=\?$/) return -2
    return -1
}
# The line WITHOUT its length annotation. Everything that reads an instruction
# as TEXT -- rules 3 and 4, the immediate-move class, the printed pair -- reads
# it through this, so adding the annotation did not move any text comparison.
function strip_len(s) { sub(/[ \t];LEN=([0-9]+|\?)$/, "", s); return s }
# A control transfer whose ENCODING LENGTH depends on how far its target is:
# a relative jump, conditional jump, call or loop. An INDIRECT form (`call *%rax`,
# `jmp *0x8(%rip)`) is excluded -- its length is fixed by its operand, not by
# layout, so a change in it is a re-encoding this class does not explain.
function is_ct_line(s) {
    if (s !~ /^\t(bnd[ \t]+|notrack[ \t]+|cs[ \t]+|ds[ \t]+)*(j[a-z]+|call[a-z]?|loop[a-z]*|xbegin)[ \t]/) return 0
    return s !~ /[ \t]\*/
}
function ct_target(s) {
    if (match(s, /[ \t](SELF\+0x[0-9a-f]+|[0-9a-f]+)$/)) return substr(s, RSTART + 1, RLENGTH - 1)
    return ""
}
function ct_key(s) { sub(/[ \t](SELF\+0x[0-9a-f]+|[0-9a-f]+)$/, " @T@", s); return s }
function ct_val(t) { sub(/^SELF\+/, "", t); if (t !~ /^0x/) t = "0x" t; return strtonum(t) }
function pad_only(   i, npb, npa, d, tb, ta, L, xb, xa, lb, la) {
    npb = 0; npa = 0; padb = 0; pada = 0; padbytes_b = 0; padbytes_a = 0; padunlen = 0
    for (i = 1; i <= nb; i++) {
        if (is_pad_line(b[i])) { padb++; L = pad_len(b[i]); if (L < 0) padunlen++; else padbytes_b += L; continue }
        npb++; pb[npb] = b[i]
    }
    for (i = 1; i <= na; i++) {
        if (is_pad_line(a[i])) { pada++; L = pad_len(a[i]); if (L < 0) padunlen++; else padbytes_a += L; continue }
        npa++; pa[npa] = a[i]
    }
    padreason = ""
    padbytes = padbytes_a - padbytes_b
    if (padb == pada && padbytes == 0) return 0
    if (npb != npa) { padreason = "a non-pad line was added or removed (rule 2)"; return 0 }
    padnp = npb; padeq = 0; padshift = 0; padnz = 0
    ctdelta = 0; ctwide = 0; ctunlen = 0
    for (i = 1; i <= npb; i++) {
        xb = strip_len(pb[i]); xa = strip_len(pa[i])
        lb = insn_len(pb[i]);  la = insn_len(pa[i])
        # RULE 6 and the rule-5 residual term, decided together because both
        # turn on the same question: did this line change LENGTH, and is it a
        # control transfer whose length layout is entitled to move?
        if (lb != la) {
            if (!is_ct_line(xb) || !is_ct_line(xa)) {
                padreason = sprintf("a NON-control-transfer instruction changed byte length (%d -> %d bytes) (rule 6): %s", lb, la, xb)
                return 0
            }
            if (lb < 0 || la < 0) ctunlen++
            else { ctdelta += la - lb; ctwide++ }
        }
        if (xb == xa) { padeq++; continue }
        tb = ct_target(xb); ta = ct_target(xa)
        if (tb == "" || ta == "") { padreason = "a non-pad line differs somewhere other than a control-transfer target (rule 3)"; return 0 }
        if ((tb ~ /^SELF/) != (ta ~ /^SELF/)) { padreason = "a control-transfer target changed RENDERING kind (rule 3)"; return 0 }
        if (ct_key(xb) != ct_key(xa)) { padreason = "a non-pad opcode or its leading operands changed (rule 3)"; return 0 }
        d = ct_val(ta) - ct_val(tb)
        if (d == 0) { padeq++; continue }
        if (padnz == 0) { padshift = d; padnz = 1 }
        else if (d != padshift) { padreason = "two distinct nonzero control-transfer shifts (rule 4)"; return 0 }
        else padnz++
    }
    # RULE 5. Nothing observable shifted -> there is nothing for the byte
    # length to have to explain, and the route is taken on rules 1-4 alone.
    # Otherwise the shared shift must BE what the bytes AHEAD of it actually
    # moved: the byte change of the pad run PLUS the byte change of the control
    # transfers that were re-encoded on the way (see the header). A listing that
    # does not carry the lengths cannot make either half of that claim.
    if (padnz > 0) {
        if (padunlen > 0) {
            padreason = sprintf("no pad byte lengths in this listing (%d unannotated pad line(s)), so the shared shift of %+d cannot be checked against the byte change of the pad run (rule 5) -- the per-symbol layer, which has them, is the verdict", padunlen, padshift)
            return 0
        }
        # PARTIAL INFORMATION IS NOT USED. A listing in which SOME re-encoded
        # control transfer carries no length cannot be told from one in which
        # an unseen second re-encoding cancels the one that is visible, so the
        # residual term is dropped WHOLE and rule 5 falls back to the pad-only
        # equality it asserted before fix3 -- never weaker than that.
        padexp = (ctunlen > 0) ? padbytes : padbytes + ctdelta
        if (padshift != padexp) {
            if (ctunlen > 0)
                padreason = sprintf("the shared control-transfer shift is %+d but the pad run changed by %+d bytes (%d -> %d) and %d re-encoded control transfer(s) carry no byte length, so the widening term cannot be used (rule 5)", padshift, padbytes, padbytes_b, padbytes_a, ctunlen)
            else if (ctwide > 0)
                padreason = sprintf("the shared control-transfer shift is %+d but the pad run changed by %+d bytes (%d -> %d) and %d re-encoded control transfer(s) by %+d bytes, which sum to %+d (rule 5)", padshift, padbytes, padbytes_b, padbytes_a, ctwide, ctdelta, padexp)
            else
                padreason = sprintf("the shared control-transfer shift is %+d but the pad run changed by %+d bytes (%d -> %d) (rule 5)", padshift, padbytes, padbytes_b, padbytes_a)
            return 0
        }
    }
    return 1
}
# The symbol s ABSOLUTE placement on the two arms, and what it is modulo the
# loop-alignment boundary this toolchain uses (`-falign-loops=32`). It is the
# CAUSE of the whole merged-pad class -- a body that starts 16 bytes off the
# boundary needs 16 more pad bytes to land its interior loop on it -- and it is
# printed so a reader sees that cause without disassembling anything
# (M6 W5 T6 commit 0 fix3). Empty when the caller supplied no addresses, which
# is every hand-written fixture and the positional path.
function align_note(   vb, va) {
    if (sym_start_b == "" || sym_start_a == "") return ""
    vb = strtonum("0x" sym_start_b); va = strtonum("0x" sym_start_a)
    return sprintf("; start 0x%s (mod 32 = %d) -> 0x%s (mod 32 = %d)", sym_start_b, vb % 32, sym_start_a, va % 32)
}
function report_pad_merged(   w) {
    w = (ctwide > 0) ? sprintf("; %d control transfer(s) re-encoded %+d bytes", ctwide, ctdelta) : ""
    printf "COUNTS %d insns; CHANGED %d; UNCLASSIFIED 0; DELTAS (pad-merged)\n", padnp, padnp - padeq
    printf "PAD-MERGED %d alignment-pad lines vs %d (%d -> %d bytes, %+d); %d non-pad lines, %d identical, %d control-transfer targets shifted %+d%s%s\n", \
           padb, pada, padbytes_b, padbytes_a, padbytes, padnp, padeq, padnz, padshift, w, align_note()
}

NR == FNR { b[FNR] = $0; nb = FNR; next }
{ a[FNR] = $0; na = FNR }
END {
    if (nb != na) {
        if (pad_only()) { report_pad_merged(); exit 0 }
        # Say that the pad route was TRIED and why it declined, so a reader of
        # a STRUCTURAL line on a pair whose pad count moved is not left to
        # guess which of the six rules it failed.
        if (padb != pada || padbytes != 0)
            printf "PAD-ROUTE DECLINED: %d alignment-pad lines vs %d (%d -> %d bytes)%s -- %s\n", \
                   padb, pada, padbytes_b, padbytes_a, align_note(), \
                   (padreason == "" ? "the non-pad streams are not equal modulo ONE shared control-transfer shift" : padreason)
        printf "STRUCTURAL: normalized listing is %d lines vs %d -- instructions were added or removed\n", nb, na
        exit 2
    }
    # Output-only cap on the printed pairs (PSYM_MAX_PAIRS; 0 = unbounded).
    # Read from the environment rather than passed with -v because this program
    # is invoked both directly and from inside the per-symbol awk, and an
    # environment variable reaches both without either call site knowing.
    cap = ENVIRON["PSYM_MAX_PAIRS"]
    cap = (cap == "") ? 5 : cap + 0
    changed = 0; bad = 0; insn_b = 0
    for (i = 1; i <= nb; i++) {
        if (b[i] ~ /^\t/) insn_b++
        if (b[i] == a[i]) continue
        changed++
        # Class (a) is a TEXT class -- the same instruction to the same
        # destination with a different immediate -- so it reads both sides
        # without the fix3 length annotation. A pair whose annotation ALSO
        # moved is not equal after redaction on the length either, so it falls
        # through to the buffered pairs below and the pad route decides it.
        ilb = strip_len(b[i]); ila = strip_len(a[i])
        if (insn_len(b[i]) == insn_len(a[i]) && is_imm_move(ilb) && is_imm_move(ila) && redact(ilb) == redact(ila)) {
            d = strtonum("0x" imm(ila)) - strtonum("0x" imm(ilb))
            deltas[d]++
        } else {
            # BUFFERED, not printed here (M6 W5 T6.a fix1). Rule 1 now admits an
            # equal-LINE-COUNT pad run whose BYTE TOTAL moved, and that case has
            # nb == na, so the pad route has to be tried AFTER this loop -- which
            # means this loop must not have printed a verdict it may not reach.
            bad++
            ub[bad] = b[i]; ua[bad] = a[i]
        }
    }
    # The equal-line-count pad route: a nop re-encoded at an unchanged count (see
    # rule 1). Tried only where the ordinary rules already failed, so a pair they
    # accept is never re-described.
    if (bad > 0) {
        if (pad_only()) { report_pad_merged(); exit 0 }
        if (padreason != "")
            printf "PAD-ROUTE DECLINED: %d alignment-pad lines vs %d (%d -> %d bytes)%s -- %s\n", \
                   padb, pada, padbytes_b, padbytes_a, align_note(), padreason
    }
    for (i = 1; i <= bad; i++) {
        if (cap != 0 && i > cap) break
        printf "  UNCLASSIFIED  - %s\n                + %s\n", ub[i], ua[i]
    }
    if (cap != 0 && bad > cap) printf "  ... and %d more unclassified differences\n", bad - cap
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
    local obj i=0
    for obj in "$@"; do
        i=$((i + 1))
        "${NM}" --defined-only "${obj}" 2>/dev/null | sed "s/\$/ @@${i}@@/"
    done | sed -nE 's/^[0-9a-fA-F]+[[:space:]]+([A-Za-z?])[[:space:]]+(.*) @@([0-9]+)@@$/\1\t\2\t\3/p' \
         | awk -F'\t' '
             # The cross-object dedup keeps a weak COMDAT body emitted into
             # BOTH halves of a split from being listed twice; the census
             # compares SETS of names, so one entry is the right number.
             #
             # `__cxx_global_var_init` is the exception, and it is why this
             # carries the object ordinal at all (M6 W5 T6 commit 0): each half
             # of a split defines its OWN bare `__cxx_global_var_init`, and
             # those are two different functions initialising two different
             # globals, not one body emitted twice. Deduping them made the
             # after arm carry one where the before arm carried three, which
             # the family COUNT rule below then read as a lost initialiser.
             # Keyed per object they are counted correctly, and the per-symbol
             # layer tells them apart by the global each one touches.
             # The ordinal is KEPT as a third column on these rows, and on
             # no others: the caller pipes this through `sort -u`, which would
             # otherwise re-merge the two halves and their identically-named bare
             # initialisers right back into one. The census reads columns 1
             # and 2 and ignores the rest.
             $2 ~ /^__cxx_global_var_init(\.[0-9]+)?$/ { if (!gseen[$3, $2]++) print $1 "\t" $2 "\t" $3; next }
             !seen[$2]++ { print $1 "\t" $2 }'
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
            # `__cxx_global_var_init.<n>` is a per-TU static-initialiser
            # FUNCTION whose number is emission order, not identity. It is NOT
            # in the census family rule (that rule excuses a renumbering, and a
            # renumbering must never excuse a BODY); collapsing the number here
            # instead puts the whole family through the `[#n]` rank fallback, so
            # the per-symbol layer pairs them by rank and COMPARES their
            # instructions rather than reading every renumbering as one deleted
            # and one added symbol.
            function barename(d) {
                if (d ~ /^__cxx_global_var_init\.[0-9]+$/) return "__cxx_global_var_init"
                return d
            }
            { mg[NR] = $1; bare[NR] = barename($2); cnt[bare[NR]]++ }
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

# The names in one or more flattened listings that need demangling, as
# candidates for plain_demangle. Two sources, and both are rendered by the same
# `rdem` table because both name a CALLEE rather than a definition of this
# object:
#
#   * a RELOCATION target -- the whole target when it names a symbol, and the
#     embedded mangled name when it names a section or a compiler-local label
#     that carries one (`.rodata._ZN3fmt...`, `.Lswitch.table._ZN4hven...`);
#
#   * an `@SYM@<mangled>@` token, which is how flatten_symbols() renders a bare
#     control-transfer target it resolved to the symbol containing it (limb 3
#     of the SELF rule). The per-symbol layer rewrites the token to the
#     demangled, symbol-mapped name before any comparison.
reloc_names() {
    awk '{
        i = index($0, "\t"); if (i == 0) next
        rest = substr($0, i + 1)
        j = index(rest, "\t"); if (j == 0) next
        kind = substr($0, 1, i - 1)
        body = substr(rest, j + 1)
        if (kind == "R") {
            k = index(body, "\t"); if (k == 0) next
            t = substr(body, k + 1)
            k = index(t, "\t"); if (k > 0) t = substr(t, 1, k - 1)
            if (substr(t, 1, 1) != ".") { print t; next }
            if (match(t, /_Z[A-Za-z0-9_$]+/)) print substr(t, RSTART, RLENGTH)
            next
        }
        while (match(body, /@SYM@[^@]+@/)) {
            print substr(body, RSTART + 5, RLENGTH - 6)
            body = substr(body, RSTART + RLENGTH)
        }
    }' "$@"
}

# ---------------------------------------------------------------------------
# `__cxx_global_var_init.<n>` KEYED BY THE GLOBAL IT INITIALISES
# (M6 W5 T6 commit 0, registered by M6 W5 T1 fix1: "T6 should key the family by
# the global each initialiser touches instead").
#
# These are per-TU static-initialiser FUNCTIONS whose NUMBER is emission order,
# not identity. demangle_table() drops the number, which routes the family
# through the `[#n]` rank fallback so their bodies are compared pairwise rather
# than read as one deleted and one added symbol. On a stable TU the rank IS
# emission order and the pairing is exact. Across a TU SPLIT it is not: each
# half renumbers from zero, so rank 2 of the before arm is compared against
# whichever initialiser happens to be second in the union of the after arm.
# That pairs two unrelated bodies and reports the difference between two
# different globals as a finding -- or, worse, equates them.
#
# The identity used instead is what the body actually TOUCHES: the ordered list
# of NAMED relocation targets in the block, each with its addend
# (`_ZN4hven6thingE+0x0,__cxa_atexit-0x4,...`). It is derived from the same
# flattened listing the comparison reads, it is a property of the body rather
# than of emission order, and a split does not change which globals an
# initialiser constructs. Emission order within ONE body is used as read: the
# body is what is stable, and re-sorting would discard the one ordering that is.
#
# NO GUESSED ASSOCIATION. An identity is DECLINED, not approximated, in two
# cases: an initialiser with no named relocation target at all (a constant
# initialiser referencing only `.bss+off`), and two initialisers of ONE ARM
# whose identity strings are equal. A declined initialiser falls back to the
# rank tag exactly as before and is COUNTED on the PERSYM line, and where the
# after arm is a SPLIT -- the one situation in which rank is unsound -- it is a
# FINDING, because there the fallback is a guess this tool is not entitled to
# make.
# ---------------------------------------------------------------------------
gvi_identities() {
    awk -F'\t' '
        $1 == "S" {
            oid = ($4 == "") ? 1 : $4 + 0
            cur = ($2 ~ /^__cxx_global_var_init(\.[0-9]+)?$/) ? (oid SUBSEP $2) : ""
            curname = $2
            if (cur != "") { if (!(cur in seen)) { seen[cur] = 1; ord[++nord] = cur; onm[cur] = curname; ooid[cur] = oid } }
            next
        }
        $1 == "R" && cur != "" {
            if (substr($4, 1, 1) == ".") next
            k = $4 $5
            if ((cur SUBSEP k) in have) next
            have[cur, k] = 1
            parts[cur] = parts[cur] (parts[cur] == "" ? "" : ",") k
            next
        }
        END {
            # The collision test is ARM-WIDE, across every object of the arm:
            # two initialisers with one identity cannot be told apart wherever
            # they sit, so neither gets one.
            for (i = 1; i <= nord; i++) if (parts[ord[i]] != "") ndup[parts[ord[i]]]++
            for (i = 1; i <= nord; i++) {
                c = ord[i]
                if (parts[c] == "") continue
                if (ndup[parts[c]] > 1) continue
                printf "%s\t%s\t%s\n", ooid[c], onm[c], parts[c]
            }
        }' "$@"
}

# ---------------------------------------------------------------------------
# The keying both comparison layers share: a symbol is identified by its
# DEMANGLED name, with the symbol map applied on the before arm, plus the
# disambiguating tag demangle_table() computed. Prepended verbatim to both awk
# programs so the two layers cannot key differently.
# ---------------------------------------------------------------------------
PSYM_KEYING_AWK='
function keyof(mangled, is_before, oid,   d, gid) {
    d = (mangled in dem) ? dem[mangled] : mangled
    # `__cxx_global_var_init` keyed by the global it initialises, where an
    # identity was derivable; see the block above gvi_identities(). Only the
    # per-symbol layer loads these tables, so the symbol CENSUS -- which checks
    # presence and absence, not bodies, and whose per-family counts are equal
    # across a split by construction -- keeps the rank behaviour it had.
    if (d == "__cxx_global_var_init") {
        if (oid == "") oid = 1
        gid = ""
        if (is_before) { if ((oid, mangled) in gvib) gid = gvib[oid, mangled] }
        else           { if ((oid, mangled) in gvia) gid = gvia[oid, mangled] }
        if (gid != "") { gvikeyed[oid, mangled] = 1; return d "{" gid "}" }
        gviun[oid, mangled] = 1
        return d ((mangled in tag) ? tag[mangled] : "")
    }
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
# `<object ordinal>\t<mangled>\t<identity>` into arr[oid, mangled]. Keyed by
# OBJECT as well as name because a split defines the same
# `__cxx_global_var_init` in both halves; see gvi_identities().
function load_gvi(f, arr,   line, n1, n2) {
    if (f == "") return
    while ((getline line < f) > 0) {
        n1 = index(line, "\t"); if (n1 == 0) continue
        n2 = index(substr(line, n1 + 1), "\t") + n1; if (n2 == n1) continue
        arr[substr(line, 1, n1 - 1) + 0, substr(line, n1 + 1, n2 - n1 - 1)] = substr(line, n2 + 1)
    }
    close(f)
}
# ---- COMPILER-LOCAL LABEL FAMILIES, for the symbol CENSUS ------------------
# `.L*` (`.L.str.137`, `.LCPI8_0`, `.Lswitch.table._Z...`) and
# `GCC_except_table<n>` are names the compiler mints per TU in EMISSION ORDER.
# Adding one string literal, or moving anything ahead of them, renumbers the
# whole family wholesale; they name DATA in sections this comparison does not
# read; and their numbers therefore carry no information the census is entitled
# to assert on. That is the SAME argument the RELOCATION layer has always made
# for the same names ("A COMPILER-LOCAL LABEL ... is compared by its FAMILY"),
# and until M6 W5 T1 fix1 the census did not make it -- so a header change that
# renumbered 1700 labels needed 1700 generated exception lines to say nothing.
#
# What the census asserts instead is the per-object per-family COUNT (a
# multiset, not a set): a family that GAINS or LOSES a member is still a
# finding, so a genuinely added string literal or a removed LSDA is still seen.
#
# `__cxx_global_var_init.<n>` JOINED THIS SET AT M6 W5 T6 commit 0, and the
# reason it was excluded before is the reason it can join now. T1 fix1 kept it
# out because these are FUNCTIONS and "a renumbering must never excuse a body"
# -- true while the census was the only thing keeping the family accounted, and
# while the per-symbol layer could only pair them by a RANK that a split
# renumbers. The per-symbol layer now identifies each one by the GLOBAL it
# initialises and compares the bodies pairwise on that key (see
# gvi_identities()), so the census counting the family excuses nothing: an
# added or removed initialiser still moves the count and is still a
# `LOCAL-FAMILY` finding, and a CHANGED one is still a per-symbol `DIFFERS`.
# What it stops doing is reading a split -- three initialisers becoming two in
# one half and one in the other -- as a lost symbol.
function is_local_family(nm) {
    return nm ~ /^\.L/ || nm ~ /^GCC_except_table[0-9]+$/ ||
           nm ~ /^__cxx_global_var_init(\.[0-9]+)?$/
}
# The family of a compiler-local name: its NUMERIC SUFFIX removed. A suffix, not
# every digit run, because `.L.str` and `.L.str.48` are one family and a rule
# that replaced digit runs left the un-suffixed member with nothing to replace,
# so the two never compared equal. An embedded mangled name is kept verbatim
# (`.Lswitch.table._ZN...` is that function'"'"'s table and no other'"'"'s).
function famnum(s) {
    sub(/[0-9]+_[0-9]+$/, "", s)
    while (sub(/\.[0-9]+$/, "", s)) { }
    sub(/[0-9]+$/, "", s)
    return s
}
# ---- EXCEPTION LOOKUP -----------------------------------------------------
# EVERY exception key is OBJECT-QUALIFIED -- `<object relpath>::<symbol>` --
# and `obj` is the object being compared (the BEFORE-arm relpath from the
# manifest, which for a split is the one old object whose symbols are being
# accounted). An entry meant to apply everywhere says so explicitly, as
# `*::<symbol>`; there is no unqualified form, and load_exceptions() refuses
# one. Both halves of that arrived with M6 W5 T1 fix2 (Codex I2), because
# kind-qualification alone left the namespace GLOBAL across objects: the same
# demangled name is one weak COMDAT body in twenty test objects, so an entry
# written about the object that lost its copy also excused a mutated body of
# the same name in the object that gained the strong one. A same-name,
# same-kind mutation in a SECOND object is now a finding, and the falsifier
# that proves it is in the fix2 report.
#
# The key an entry may take, most specific first:
#
#   <obj>::<full key> [<KIND>]  excuses THIS finding kind on this key, HERE
#   <obj>::<bare name> [<KIND>] ... written in the plain demangled form
#   <obj>::<full key>           excuses ANY finding on this key, here
#   <obj>::<bare name>
#   *::<full key> [<KIND>]      ... in every object (an explicit wildcard)
#   *::<bare name> [<KIND>]
#   *::<full key>               ... any finding, in every object
#   *::<bare name>
#
# KIND is ONLY-BEFORE, ONLY-AFTER, DIFFERS, SECTION or LOCAL-FAMILY. Object
# specificity is tried before kind specificity, so the entry written about THIS
# object always wins over a wildcard, whichever kinds they carry -- the order
# above IS that rule, and M6 W5 T2 commit 0 (Codex, T1 fix2 re-check, Minor 3)
# is where the code was reordered to match it: a kind-qualified WILDCARD used
# to be probed before an object-specific KINDLESS key, so the reason attached to
# the WILDCARD could be printed for a finding an object-specific entry was
# written about, leaving that entry stale. No gate moved -- every exception file this
# tree has written carries zero wildcards -- which is precisely why the
# contradiction had to be closed before one is written.
# Returns the key to charge, or "" for none. `nofallback` suppresses the four
# KINDLESS forms, which is how an entry already SPENT on a section move stops
# covering the body as well.
function excfind(k, bare, kind, nofallback,   c) {
    c = obj "::" k " [" kind "]";                   if (c in exc) return c
    if (bare != "") { c = obj "::" bare " [" kind "]"; if (c in exc) return c }
    if (!nofallback) {
        c = obj "::" k;                             if (c in exc) return c
        if (bare != "") { c = obj "::" bare;          if (c in exc) return c }
    }
    c = "*::" k " [" kind "]";                      if (c in exc) return c
    if (bare != "") { c = "*::" bare " [" kind "]";   if (c in exc) return c }
    if (nofallback) return ""
    c = "*::" k;                                    if (c in exc) return c
    if (bare != "") { c = "*::" bare;                 if (c in exc) return c }
    return ""
}
# Whether the key that was charged carried a FINDING KIND. An entry that did
# not is SPENT once it has excused a section move (see the SECTION-MOVED path).
function exc_kindless(ek) { return ek !~ /\[(ONLY-BEFORE|ONLY-AFTER|DIFFERS|SECTION|LOCAL-FAMILY)\]$/ }

function famof(nm,   pre) {
    if (match(nm, /_Z[A-Za-z0-9_$]+/)) {
        pre = substr(nm, 1, RSTART - 1)
        return famnum(pre) substr(nm, RSTART)
    }
    return famnum(nm)
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
# ---- assembler-resolved control-transfer targets --------------------------
# flatten_symbols() renders a bare target it resolved to the symbol CONTAINING
# it as `@SYM@<mangled>@+0xN` (limb 3 of the SELF rule). The name is rendered
# here, on the same terms a relocation target naming that callee would be:
# demangled through the plain `rdem` table, symbol-mapped on the before arm,
# with no variant tag -- so an assembler-resolved call to an internal-linkage
# sibling is compared BY NAME, exactly like a relocated call to an external
# one, and survives a rename the map declares. A line with no token is returned
# untouched, which is all but a handful of them.
function render_insn(l, is_before,   p, q, mg, out) {
    if (index(l, "@SYM@") == 0) return l
    out = ""
    while ((p = index(l, "@SYM@")) > 0) {
        out = out substr(l, 1, p - 1)
        l = substr(l, p + 5)
        q = index(l, "@")
        if (q == 0) return out l
        mg = substr(l, 1, q - 1)
        l = substr(l, q + 1)
        out = out reldem_of(mg, is_before)
        if (relmapped) relmapused[relmapold] = 1
    }
    return out l
}
# ---- COMDAT companion sections that name their OWN function ---------------
# A section-relative relocation target may be the enclosing function'"'"'s own
# COMDAT companion section: `.rodata.<SELF>` (its jump table or constant pool),
# `.gcc_except_table.<SELF>` (its LSDA), `.text.<SELF>` (its own body). When an
# inline function becomes a strong out-of-line definition those companions
# merge into the plain `.rodata` / `.gcc_except_table` / `.text` with the body,
# so the two arms name the same datum through two section names that differ
# only by the SELF suffix. Rendering such a target as its BASE section is the
# same SELF rule flatten_symbols() already applies to the section HEADER
# (`.text._Zfoo` -> `.text.<SYM>`) and that the branch/call path applies to
# self-relative targets: the part that moved is the enclosing symbol'"'"'s own
# name, which is not a code difference. A companion section naming SOME OTHER
# symbol is left alone -- that really is a different datum.
function strip_self_section(t, selfmg,   n) {
    if (selfmg == "" || substr(t, 1, 1) != ".") return t
    n = length(selfmg)
    if (length(t) > n + 1 && substr(t, length(t) - n) == "." selfmg)
        return substr(t, 1, length(t) - n - 1)
    return t
}
# ---- section-relative targets that name CODE ------------------------------
# `xsec[arm, name]` is the executable-section set and `saddr[arm, sec, off]`
# the defined-function-by-address table, both per ARM (1 = before, 2 = after)
# and both built from the same arm'"'"'s own objects, since a target must be
# resolved in the object that carries the relocation. See the EXECUTABLE
# SECTION class in the RELOCATION TARGETS block at the head of this file.
function load_execsec(f, armn,   line, t) {
    if (f == "") return
    while ((getline line < f) > 0) {
        if (line == "") continue
        t = index(line, "\t")
        if (t == 0) continue
        xsec[armn, substr(line, 1, t - 1) + 0, substr(line, t + 1)] = 1
    }
    close(f)
}
# THE RESOLUTION TABLE IS KEYED PER OBJECT (M6 W5 T6 commit 0, from T0 F-D).
# A split arm is the UNION of several objects, and every relocatable object
# starts each of its sections at 0, so `.text+0x1cdc` names a DIFFERENT
# function in each half of a split. Keyed by (arm, section, offset) alone, the
# table saw two names for one key, correctly refused to guess between them, and
# fell back to the LITERAL addend -- which then differed from the resolved name
# the before arm produced, on EVERY internal-linkage call across the split. That is a
# false DIFFERS per call site, on exactly the comparison T6 exists to make.
#
# The key now carries the OBJECT the relocation was read from, so each target
# is resolved in the object that carries it, exactly as it would be if that
# object were compared alone. `@ambiguous@` survives for the case it was
# written for and now means what it says: two functions defined at one address
# WITHIN ONE OBJECT (an alias row whose sizes disagree). The direction is
# STRICTER -- it removes a fallback to a layout-dependent literal and replaces
# it with a comparison by NAME; it can only turn a false DIFFERS into a match
# or a real callee change into a named finding.
function load_symaddr(f, armn,   line, nf, fld, sec, off, nm, oid, k) {
    if (f == "") return
    while ((getline line < f) > 0) {
        nf = split(line, fld, "\t")
        if (nf < 4) continue
        sec = fld[1]; off = fld[2]; nm = fld[4]
        oid = (nf >= 5) ? fld[5] + 0 : 1
        k = armn SUBSEP oid SUBSEP sec SUBSEP off
        if (!(k in saddr)) saddr[k] = nm
        else if (saddr[k] != nm) saddr[k] = "@ambiguous@"
    }
    close(f)
}
# A hex addend with an explicit sign. gawk'"'"'s strtonum() is not documented to
# accept one, so the sign is taken off first.
function hexnum(s,   neg) {
    if (s == "") return 0
    neg = 0
    if (substr(s, 1, 1) == "-") { neg = 1; s = substr(s, 2) }
    else if (substr(s, 1, 1) == "+") s = substr(s, 2)
    return neg ? -strtonum(s) : strtonum(s)
}
# The 32-bit PC-relative forms fold the width of the displacement field into
# the addend, so the callee sits at addend + 4. The absolute forms do not.
function is_pcrel(type) { return type ~ /(PLT32|PC32|PCREL)/ }
function exec_target_symbol(sec, type, addend, armn, oid,   tgt, k, nm) {
    if (!((armn SUBSEP oid SUBSEP sec) in xsec)) return ""
    tgt = hexnum(addend) + (is_pcrel(type) ? 4 : 0)
    if (tgt < 0) return ""
    k = armn SUBSEP oid SUBSEP sec SUBSEP sprintf("%x", tgt)
    if (!(k in saddr)) return ""
    nm = saddr[k]
    return (nm == "@ambiguous@") ? "" : nm
}
# See the RELOCATION TARGETS block in the header of this file for the classes
# and the reason each is rendered the way it is.
function render_reloc_target(t, is_before,   pre, mg, suf) {
    if (substr(t, 1, 1) != ".") return reldem_of(t, is_before)
    if (match(t, /_Z[A-Za-z0-9_$]+/)) {
        pre = substr(t, 1, RSTART - 1)
        mg  = substr(t, RSTART, RLENGTH)
        suf = substr(t, RSTART + RLENGTH)
        if (substr(t, 1, 2) == ".L") pre = famnum(pre)
        return pre reldem_of(mg, is_before) suf
    }
    return (substr(t, 1, 2) == ".L") ? famnum(t) : t
}
# The comparable rendering of one relocation. Deliberately NOT prefixed with a
# tab: the classifier counts tab-led lines as instructions, and a relocation is
# an annotation ON an instruction, not one of its own.
function reloc_text(type, target, addend, is_before, selfmg, oid,   t, armn, rs) {
    relmapped = 0; relmapold = ""
    armn = is_before ? 1 : 2
    if (oid == "") oid = 1
    if (substr(target, 1, 1) == "." && ((armn SUBSEP oid SUBSEP target) in xsec)) {
        rs = exec_target_symbol(target, type, addend, armn, oid)
        # Resolved: rendered exactly as a NAMED target is -- demangled, mapped,
        # untagged -- with the residual addend a direct reference to that
        # symbol would have carried. The section and the byte offset, both pure
        # layout, drop out; the callee'"'"'s NAME is what gets compared.
        if (rs != "") return "RELOC " type " " reldem_of(rs, is_before) " " (is_pcrel(type) ? "-0x4" : "+0x0")
        # Code, but nothing defined at that address (or two objects of a split
        # arm disagree about it): compare the LITERAL addend. R17'"'"'s stated
        # fallback, in its stated direction -- a false DIFFERS, never a mask.
        return "RELOC " type " " render_reloc_target(strip_self_section(target, selfmg), is_before) " " (addend == "" ? "+0x0" : addend)
    }
    t = render_reloc_target(strip_self_section(target, selfmg), is_before)
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
    if (is_local_family($2)) { bfam[famof($2)]++; nlocb++; next }
    key = keyof($2, 1)
    if (key in b) { printf "  COLLISION     two before-arm symbols claim one name: %s\n", key; coll++ }
    b[key] = $1
    borig[key] = bareof($2)
    if (bareof($2) in m) { wasmapped[key] = 1; mapused[bareof($2)] = 1 }
    next
}
{ if (is_local_family($2)) { afam[famof($2)]++; nloca++; next }
  akey = keyof($2, 0); a[akey] = $1; aorig[akey] = bareof($2) }
END {
    for (k in b) {
        if (k in a) { matched++; if (k in wasmapped) mapped++; continue }
        ek = excfind(k, borig[k], "ONLY-BEFORE", 0)
        if (ek != "") { excused++; used[ek] = 1; printf "  EXCEPTION     ONLY-BEFORE %s ## %s\n", ek, exc[ek]; continue }
        onlyb++
        if (onlyb <= 20) printf "  ONLY-BEFORE   %s\n", k
    }
    if (onlyb > 20) printf "  ... and %d more symbols present only in the before arm\n", onlyb - 20
    for (k in a) {
        if (k in b) continue
        ek = excfind(k, aorig[k], "ONLY-AFTER", 0)
        if (ek != "") { excused++; used[ek] = 1; printf "  EXCEPTION     ONLY-AFTER  %s ## %s\n", ek, exc[ek]; continue }
        onlya++
        if (onlya <= 20) printf "  ONLY-AFTER    %s\n", k
    }
    if (onlya > 20) printf "  ... and %d more symbols present only in the after arm\n", onlya - 20
    # Compiler-local label families, by COUNT. An unequal count -- including a
    # family present on one arm only -- is a finding, excusable by the FAMILY
    # name in the exceptions file.
    for (f in bfam) {
        if ((f in afam) && bfam[f] == afam[f]) { famok++; continue }
        ek = excfind(f, "", "LOCAL-FAMILY", 0)
        if (ek != "") { excused++; used[ek] = 1; printf "  EXCEPTION     LOCAL-FAMILY %s: %d before, %d after ## %s\n", f, bfam[f], afam[f] + 0, exc[ek]; continue }
        famdiff++
        if (famdiff <= 20) printf "  LOCAL-FAMILY  %s: %d before, %d after\n", f, bfam[f], afam[f] + 0
    }
    for (f in afam) {
        if (f in bfam) continue
        ek = excfind(f, "", "LOCAL-FAMILY", 0)
        if (ek != "") { excused++; used[ek] = 1; printf "  EXCEPTION     LOCAL-FAMILY %s: 0 before, %d after ## %s\n", f, afam[f], exc[ek]; continue }
        famdiff++
        if (famdiff <= 20) printf "  LOCAL-FAMILY  %s: 0 before, %d after\n", f, afam[f]
    }
    if (famdiff > 20) printf "  ... and %d more compiler-local families whose count differs\n", famdiff - 20
    # The object is recorded beside the key: the shell reports a
    # STALE-EXCEPTION from the key column and a WILDCARD-USED (a `*::` entry
    # consumed in more than one object) from the pair.
    for (k in used) print k "\t" obj >> f_used
    close(f_used)
    for (k in mapused) print k >> f_mapused
    close(f_mapused)
    printf "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", matched + 0, mapped + 0, onlyb + 0, onlya + 0, excused + 0, coll + 0, nrank + 0, famok + 0, famdiff + 0, nlocb + 0, nloca + 0 > f_stat
    close(f_stat)
    exit (onlyb + onlya + coll + famdiff > 0) ? 1 : 0
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
#
# The prefix run is order-FREE (M6 W5 T2 commit 0, lane M1): the dominant
# multi-byte nop clang emits is `data16 data16 ... cs nopw 0x0(%rax,%rax,1)`,
# which the fix2 shape -- `cs` only BEFORE `data16` -- did not match, while the
# `ispad` in psym_bin_pairs.awk already accepted any order. The two now agree on
# the same prefix set (`data16`, `cs`, `rex*`) in any order. Direction: a line
# this recognises is TRAILING padding that gets trimmed, so recognising more of
# them can only remove a false DIFFERS between two arms whose last function
# gained or lost its inter-function padding; it can never hide a difference
# INSIDE a body, which the trailing-run trim never reaches.
function is_pad(s) {
    return s ~ /^\t((data16|cs|rex[0-9a-z.]*)[ \t]+)*(nop[lwqb]?([ \t]|$)|xchg[ \t]+%ax,%ax([ \t]|$))/
}
BEGIN {
    FS = "\t"
    load_pairs_into(f_map, m)
    load_pairs_into(f_exc, exc)
    load_pairs_into(f_reldem, rdem)
    load_gvi(f_gvib, gvib)
    load_gvi(f_gvia, gvia)
    load_execsec(f_xsec_b, 1); load_execsec(f_xsec_a, 2)
    load_symaddr(f_saddr_b, 1); load_symaddr(f_saddr_a, 2)
    load_demangle(f_dem)
    for (x in tag) if (index(tag[x], "[#")) nrank++
    fb = tmpd "/blk-b.txt"
    fa = tmpd "/blk-a.txt"
}
NR == FNR {
    if ($1 == "R") {
        if (!bskip && bcur != "") {
            bn[bcur]++
            bi[bcur, bn[bcur]] = reloc_text($3, $4, $5, 1, $2, ($6 == "" ? 1 : $6 + 0))
            brel[bcur]++
            if (relmapped) { brelmap[bcur]++; relmapused[relmapold] = 1 }
        }
        next
    }
    if ($1 == "S") {
        k = keyof($2, 1, ($4 == "" ? 1 : $4 + 0))
        bcur = k
        if (k in bsec) { printf "  COLLISION     two before-arm symbols claim one name: %s\n", k; coll++; bskip = 1 }
        else {
            bsec[k] = lit_replace($3, $2, "<SYM>")
            bn[k] = 0
            bskip = 0
            borig[k] = bareof($2)
            bstart[k] = $5
            if (bareof($2) in m) bmapped[k] = 1
        }
    } else if (!bskip && bcur != "") { bn[bcur]++; bi[bcur, bn[bcur]] = render_insn(field3($0), 1) }
    next
}
{
    if ($1 == "R") {
        if (acur != "") {
            rtxt = reloc_text($3, $4, $5, 0, $2, ($6 == "" ? 1 : $6 + 0))
            if (adupmode) { c = adup[acur]; adn[acur, c]++; adi[acur, c, adn[acur, c]] = rtxt }
            else { an[acur]++; ai[acur, an[acur]] = rtxt }
        }
        next
    }
    if ($1 == "S") {
        k = keyof($2, 0, ($4 == "" ? 1 : $4 + 0))
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
            astart[k] = $5
            adupmode = 0
        }
    } else if (acur != "") {
        itxt = render_insn(field3($0), 0)
        if (adupmode) { c = adup[acur]; adn[acur, c]++; adi[acur, c, adn[acur, c]] = itxt }
        else { an[acur]++; ai[acur, an[acur]] = itxt }
    }
}
END {
    for (k in bsec) {
        if (!(k in asec)) {
            ek = excfind(k, borig[k], "ONLY-BEFORE", 0)
            if (ek != "") { excused++; used[ek] = 1; printf "  EXCEPTION     ONLY-BEFORE %s ## %s\n", ek, exc[ek]; continue }
            onlyb++
            printf "  ONLY-BEFORE   %s\n", k
            continue
        }
        # A SECTION MOVE is reported on its own line, is excusable BY NAME on
        # the same terms as DIFFERS -- and does NOT stop the body from being
        # compared. Moving a function out of line necessarily moves it from its
        # COMDAT `.text.<SYM>` into plain `.text`, so before M6 W5 T1 fix1 no
        # exception set could make such a commit pass however well understood
        # the move was, and the transcript said nothing about the instructions.
        # It now carries the instruction and relocation verdict beside the
        # section note. The exception key `<name> [SECTION]` excuses the MOVE
        # alone; a plain-name entry excuses the move and is then SPENT, so a
        # body that also differs is still a finding -- one exception, one
        # finding, and the falsifier (change one instruction in a moved body)
        # stays live.
        if (bsec[k] != asec[k]) {
            ek = excfind(k, borig[k], "SECTION", 0)
            secexc = ""
            if (ek != "") {
                excused++; used[ek] = 1; secexc = exc[ek]
                # A KINDLESS entry is now SPENT: it excused the move, and it
                # no longer covers the body this comparison is about to make.
                if (exc_kindless(ek)) spent[k] = 1
            }
            if (secexc != "")
                printf "  EXCEPTION     SECTION-MOVED %s: %s -> %s ## %s\n", k, bsec[k], asec[k], secexc
            else {
                printf "  SECTION-MOVED %s: %s -> %s\n", k, bsec[k], asec[k]
                diff++
            }
            secmoved++
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
        # The two ABSOLUTE symbol starts ride into the classifier so its
        # merged-pad verdict can name the alignment fact that CAUSED the pad to
        # move (M6 W5 T6 commit 0 fix3). They are hex digits from flatten_symbols(),
        # so they need no quoting beyond this.
        cmd = "awk -v sym_start_b=" (bstart[k] == "" ? "\"\"" : bstart[k]) \
              " -v sym_start_a=" (astart[k] == "" ? "\"\"" : astart[k]) \
              " -f " f_cls " " fb " " fa " 2>&1"
        verdict = ""
        while ((cmd | getline l) > 0) verdict = verdict (verdict == "" ? "" : "\n") l
        status = close(cmd)
        if (status == 0) {
            noise++
            if (k in bmapped) mapped++
            printf "  NOISE-ONLY    %s\n                %s\n", k, verdict
        } else if ((ek = excfind(k, borig[k], "DIFFERS", (k in spent))) != "") {
            excused++; used[ek] = 1
            printf "  EXCEPTION     DIFFERS %s ## %s\n", ek, exc[ek]
        } else {
            diff++
            printf "  DIFFERS       %s\n", k
            n = split(verdict, vl, "\n")
            for (i = 1; i <= n; i++) printf "                %s\n", vl[i]
        }
    }
    for (k in asec) {
        if (k in bsec) continue
        ek = excfind(k, aorig[k], "ONLY-AFTER", 0)
        if (ek != "") { excused++; used[ek] = 1; printf "  EXCEPTION     ONLY-AFTER  %s ## %s\n", ek, exc[ek]; continue }
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
    ngvik = 0; for (x in gvikeyed) ngvik++
    ngviu = 0; for (x in gviun)    ngviu++
    # NO GUESSED ASSOCIATION. Across a SPLIT the `[#n]` rank is emission order
    # in each half independently, so an initialiser whose identity could not be
    # derived (or which collided with another in its own arm) would be paired
    # by a number that means nothing. Report it as a finding rather than
    # compare two bodies that may belong to two different globals. On a
    # non-split comparison the rank IS emission order for one TU, and the
    # fallback stays what it has always been.
    if (nsplit > 1 && ngviu > 0) {
        printf "  GVI-UNRESOLVED %d of %d __cxx_global_var_init symbols have no derivable global identity (no named relocation target, or an identity shared with another initialiser of the same arm) -- across a split the rank fallback is emission order in each half and pairs nothing in particular\n", ngviu, ngviu + ngvik
        gvifail = ngviu
    }
    for (k in used) print k "\t" obj >> f_used
    close(f_used)
    for (k in relmapused) print k >> f_mapused
    close(f_mapused)
    printf "%d\t%d\n", nreloc + 0, nrelmap + 0 > f_relstat
    close(f_relstat)
    printf "PERSYM %d identical, %d noise-only, %d differing, %d only-before, %d only-after, %d excepted, %d collisions, %d matched through the symbol map, %d with unequal trailing alignment padding, %d duplicate copies identical, %d rank-tagged, %d section moves, %d relocation records compared (%d through the symbol map), %d global-var-init keyed by identity (%d unresolved)\n",
           ident + 0, noise + 0, diff + 0, onlyb + 0, onlya + 0, excused + 0, coll + 0, mapped + 0, padtrim + 0, dupok + 0, nrank + 0, secmoved + 0, nreloc + 0, nrelmap + 0, ngvik + 0, ngviu + 0
    exit (diff + onlyb + onlya + coll + gvifail > 0) ? 1 : 0
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
#
#   THE LABEL IS NOT A RELIABLE WITNESS, AND SINCE M6 W5 T1 fix1 IT IS NOT THE
#   ONLY ONE. objdump names a target by the nearest symbol at or below its
#   ADDRESS, and in a relocatable object every section starts at 0, so the
#   nearest symbol by address can sit in a completely different section. A
#   function alone in its COMDAT `.text.<SYM>` therefore gets its own end-of-
#   body target -- the placeholder of a relocated `call`, which is just the
#   next instruction -- labelled with whatever `.rodata` symbol happens to be
#   at that offset, while the SAME instruction in plain `.text` is labelled
#   `<SELF+off>`. That is a labelling artifact, not a code difference, and it
#   is exactly what an inline-to-out-of-line move produces.
#
#   SO EVERY BARE TARGET IS DECIDED BY THE SYMBOL TABLE, NOT BY THE LABEL
#   (M6 W5 T1 fix2). fix1 answered the artifact with an address range
#   `[symbol start, last instruction + 16]`, and that range was WRONG in the
#   direction that matters: functions are 16-byte aligned, so the NEXT function
#   commonly begins inside that 16-byte envelope, and a resolved tail
#   `jmp`/`call` to it was rendered `SELF+off` -- equating two calls to two
#   DIFFERENT adjacent functions whenever they sat at the same offset from
#   their caller. Demonstrated on a fixture by the SQP lane, and fixed here by
#   reading the extent from the place that states it. Four limbs, in order:
#
#     1. IN RANGE. The range is `[start, start + size)` from this arm's own
#        `objdump -t` table (sym_addrs' SIZE column). A target inside the
#        enclosing symbol's own body is `SELF+off` whatever objdump called it:
#        an intra-function branch, and the placeholder of a relocated call in
#        mid-body.
#
#     2. A RELOCATED PLACEHOLDER. An instruction that carries a relocation --
#        the very next record in this stream is that relocation, one line of
#        lookahead -- has NO target of its own: its operand is a zero
#        placeholder objdump renders as the following address, and the callee's
#        identity lives in the relocation record, which reloc_text() compares
#        by NAME. Such a target is `SELF+off` too, which is what makes the
#        trailing relocated call of a moved-out-of-line body compare equal.
#        This limb equates nothing: the R record beside it is compared.
#
#     3. OUT OF RANGE, RESOLVED BY NAME. A bare target that is neither of those
#        is a call or jump the ASSEMBLER resolved, which it only does to a
#        symbol it can see -- an internal-linkage sibling in the same section.
#        The symbol table says which one: the target is looked up in the same
#        (section, address) table the executable-section relocation class uses,
#        and rendered `<demangled name>+off` through the symbol map, exactly as
#        a relocation naming that callee would be. That is what makes the
#        SQP lane's falsifier fail as it should -- two arms whose `caller`
#        tail-jumps to two different anonymous-namespace siblings now render
#        two different NAMES -- and it also closes the OLDER hole in the same
#        place: before fix1 such a target was compared as a bare ADDRESS, so a
#        callee swap that kept the address was equated too.
#
#     4. NOTHING RESOLVES. If the symbol table has no size for the enclosing
#        symbol, a label naming the enclosing symbol is still honoured (the
#        pre-fix1 rule, and the only one available without an extent).
#        Otherwise the bare address survives untouched, exactly as before: a
#        split that moves such a callee reports a DIFFERS. That is the
#        conservative direction -- a false finding, never a masked one.
#
#   Limbs 1 and 3 need the block's extent and the whole symbol table, so this
#   awk buffers one symbol at a time rather than streaming.
flatten_symbols() {
    awk -v symtab="${2:-}" -v objid="${3:-1}" '
        # objdump labels a target by the nearest symbol at or below it, and in a
        # COMDAT text section the candidates include the SECTION symbol, whose
        # name is `.text.<the enclosing function it holds>`. Such a label names
        # the enclosing symbol just as much as a bare `<sym+off>` does, since
        # the section holds exactly that one function at offset 0. It is only
        # ever consulted by limb 4, where no size is available to bound the
        # range with -- limbs 1 to 3 decide by the symbol table alone.
        function is_self_label(gname, cur) {
            if (gname == cur) return 1
            if (substr(gname, 1, 1) != "." || cur == "") return 0
            if (length(gname) <= length(cur) + 1) return 0
            return substr(gname, length(gname) - length(cur)) == "." cur
        }
        # THIS OBJECT s defined-function table: sizes for the SELF range, and
        # a per-section list for the containment lookup limb 3 makes. Since
        # M6 W5 T6 commit 0 the caller runs one flatten per OBJECT with that
        # object s own table, rather than once per ARM over the union of a
        # split s objects -- every relocatable object starts each section at 0,
        # so the union made two halves of a split disagree about what sits at
        # one (section, offset), and the lookup then refused to guess and left
        # the bare address in place. Ambiguity WITHIN one object is still
        # refused, and still means what it says.
        function loadsym(   line, nf, fld, k) {
            if (symtab == "") return
            while ((getline line < symtab) > 0) {
                nf = split(line, fld, "\t")
                if (nf < 4) continue
                k = fld[1] SUBSEP fld[2] SUBSEP fld[4]
                if (!(k in ssize)) ssize[k] = strtonum("0x" fld[3])
                nsec[fld[1]]++
                secst[fld[1], nsec[fld[1]]] = strtonum("0x" fld[2])
                secsz[fld[1], nsec[fld[1]]] = strtonum("0x" fld[3])
                secnm[fld[1], nsec[fld[1]]] = fld[4]
            }
            close(symtab)
        }
        # The symbol DEFINED EXACTLY AT `addr` in `section`, size irrelevant --
        # a SIZE-ZERO function is a symbol like any other here, and this limb
        # exists for it. Two arms whose caller reached two DIFFERENT size-zero
        # siblings at one offset used to render the same bare address and
        # compare identical, because the containment limb below skips a
        # zero-size row (there is no range to be inside). M6 W5 T2 commit 0
        # (Codex, T1 fix2 re-check, Important 1) closes that: this runs FIRST,
        # so an exact hit names the target whatever its size.
        #
        # Candidates all share `addr` by construction. Same name (an alias row
        # repeated in the table) is one candidate. DIFFERENT names sharing the
        # start AND the size are true aliases -- a C1/C2 pair, an
        # `.symver`-style second name -- and the lexicographically smallest
        # MANGLED name is taken, which is a deterministic choice made the same
        # way on both arms, so a rename still DIFFERS and an alias never does.
        # Different names with different SIZES are genuinely ambiguous -- a
        # size-zero marker sitting on the first byte of a real function is the
        # shape -- and this limb REFUSES to pick between them, returning "" so
        # that the containment limb below decides on its own (pre-existing)
        # terms, with its own ambiguity refusal. The tool never guesses which
        # of two disagreeing candidates a target meant, and this limb can
        # therefore only ADD resolution, never take away a name the fix2 tool
        # already resolved.
        function exact_at(section, addr,   i, hit, hsz, nm) {
            hit = ""; hsz = 0
            for (i = 1; i <= nsec[section]; i++) {
                if (secst[section, i] != addr) continue
                nm = secnm[section, i]
                if (hit == "") { hit = nm; hsz = secsz[section, i]; continue }
                if (nm == hit) continue
                if (secsz[section, i] != hsz) return ""
                if (nm < hit) hit = nm
            }
            return hit
        }
        # The symbol whose [start, start+size) contains `addr` in `section`, or
        # "" for none and for an ambiguous one -- preceded by the exact-start
        # lookup above, which sees size-zero symbols the containment limb
        # cannot. Sets chit_st to its start.
        # Memoized because a body branches to the same few addresses repeatedly.
        function containing(section, addr,   i, hit, hst, k) {
            k = section SUBSEP addr
            if (k in cmemo) { chit_st = cmemost[k]; return cmemo[k] }
            hit = exact_at(section, addr)
            if (hit != "") {
                cmemo[k] = hit; cmemost[k] = addr
                chit_st = addr
                return hit
            }
            hit = ""; hst = 0
            for (i = 1; i <= nsec[section]; i++) {
                if (secsz[section, i] <= 0) continue
                if (addr < secst[section, i] || addr >= secst[section, i] + secsz[section, i]) continue
                if (hit == "") { hit = secnm[section, i]; hst = secst[section, i] }
                else if (hit != secnm[section, i]) { hit = ""; hst = 0; break }
            }
            cmemo[k] = hit; cmemost[k] = hst
            chit_st = hst
            return hit
        }
        # ALIGNMENT PADDING, and its BYTE LENGTH (M6 W5 T6 commit 0 fix1).
        # The same predicate the classifier uses -- kept in step with
        # is_pad_line() there, and with ispad() in psym_bin_pairs.awk.
        function is_pad_f(s) {
            return s ~ /^\t((data16|cs|rex[0-9a-z.]*)[ \t]+)*(nop[lwqb]?([ \t]|$)|xchg[ \t]+%ax,%ax([ \t]|$))/
        }
        # The length of a pad is NOT derivable from the text objdump renders for
        # it: `0f 1f 40 00` (4 bytes) and `0f 1f 80 00 00 00 00` (7 bytes) both
        # print as `nopl 0x0(%rax)`. It IS derivable here, from the addresses
        # this function reads and then strips -- the length of a line is the
        # address of the next instruction minus its own, and for the last
        # instruction of a block it is the end of the symbol, where the symbol
        # table gives a size. Annotated onto the emitted pad line as ` ;PAD=<n>`
        # (`;PAD=?` when neither is available) so that the classifier can check
        # rule 5 of the merged-pad class, and so that two pads which render
        # identically but occupy different numbers of bytes DIFFER.
        # Replace the trailing bare-hex target of a control transfer with `t`.
        # A literal splice, never sub()"s replacement text, because a mangled
        # name may contain characters sub() would read as backreferences.
        function retarget(l, t) {
            if (match(l, /[0-9a-f]+$/)) return substr(l, 1, RSTART - 1) t
            return l
        }
        # Emits the buffered block, applying the four limbs above now that the
        # block s extent is known. btgt[i] is -1 for every line that carries no
        # bare control-transfer target.
        function flushblk(   i, j, l, nm, selfsz, nxt, ilen) {
            selfsz = ((cursec SUBSEP curstart SUBSEP curblk) in ssize) \
                     ? ssize[cursec SUBSEP curstart SUBSEP curblk] : 0
            for (i = 1; i <= nbuf; i++) {
                l = buf[i]
                # The byte length of EVERY instruction, not only of a pad
                # (M6 W5 T6 commit 0 fix3). Same derivation as before -- the
                # address of the next instruction of this block minus its own,
                # falling back to the symbol end where the table gives a size --
                # now computed once and annotated onto both kinds of line.
                ilen = -1
                if (btyp[i] == "I" && baddr[i] >= 0) {
                    nxt = -1
                    for (j = i + 1; j <= nbuf; j++) if (btyp[j] == "I") { nxt = baddr[j]; break }
                    if (nxt < 0 && selfsz > 0 && sstart + selfsz > baddr[i]) nxt = sstart + selfsz
                    if (nxt >= 0) ilen = nxt - baddr[i]
                }
                if (btyp[i] == "I" && is_pad_f(l)) {
                    l = (ilen < 0) ? l " ;PAD=?" : sprintf("%s ;PAD=%d", l, ilen)
                }
                else if (btyp[i] == "I") {
                    if (btgt[i] >= 0) {
                        if (selfsz > 0 && btgt[i] >= sstart && btgt[i] < sstart + selfsz)
                            l = retarget(l, sprintf("SELF+0x%x", btgt[i] - sstart))
                        else if (i < nbuf && btyp[i + 1] == "R")
                            l = retarget(l, sprintf("SELF+0x%x", btgt[i] - sstart))
                        else if ((nm = containing(cursec, btgt[i])) != "")
                            l = (nm == curblk) \
                                ? retarget(l, sprintf("SELF+0x%x", btgt[i] - sstart)) \
                                : retarget(l, sprintf("@SYM@%s@+0x%x", nm, btgt[i] - chit_st))
                        else if (selfsz <= 0 && bself[i])
                            l = retarget(l, sprintf("SELF+0x%x", btgt[i] - sstart))
                    }
                    # AFTER the retarget, because retarget() splices at the end
                    # of the line and the annotation would otherwise be the
                    # thing it replaced.
                    l = (ilen < 0) ? l " ;LEN=?" : sprintf("%s ;LEN=%d", l, ilen)
                }
                # An R record carries the OBJECT it was read from as a fifth
                # field, so the per-symbol layer resolves an executable-section
                # addend in the object that owns it. An S record carries it too
                # (fourth field, see the S rule below). An I record does NOT:
                # the per-symbol layer slices an instruction payload from the
                # second tab to END OF LINE, and a trailing field would land
                # inside the instruction text.
                if (btyp[i] == "R") printf "R\t%s\t%s\t%s\n", curblk, l, objid
                else printf "%s\t%s\t%s\n", btyp[i], curblk, l
            }
            nbuf = 0
        }
        BEGIN { loadsym() }
        /^Disassembly of section / {
            flushblk()
            cur = ""
            sec = $0
            sub(/^Disassembly of section /, "", sec)
            sub(/:$/, "", sec)
            next
        }
        /^[0-9a-f]+ <.*>:$/ {
            flushblk()
            name = $0
            sub(/^[0-9a-f]+ </, "", name)
            sub(/>:$/, "", name)
            cur = name
            curblk = name
            cursec = sec
            sstart = strtonum("0x" $1)
            curstart = sprintf("%x", sstart)
            # The object ordinal rides on S too, as a fourth field: the same
            # mangled name can be DEFINED in both halves of a split (two
            # `__cxx_global_var_init`s, one per half), so a per-arm table keyed
            # by the name alone merges them. The per-symbol layer reads the
            # SECTION from $3 and never slices an S record to end of line, so a
            # trailing field is invisible to it -- which is why the symbol s
            # ABSOLUTE START can ride along as a FIFTH field (M6 W5 T6 commit 0
            # fix3) for the merged-pad class s alignment diagnostic, without
            # disturbing anything that already read this record.
            printf "S\t%s\t%s\t%s\t%x\n", cur, sec, objid, sstart
            next
        }
        # A relocation line: three tabs, the within-section offset, ": ", the
        # type, a tab, then "<target>" with an optional "+0xN"/"-0xN" addend
        # glued to it. It annotates the instruction ABOVE it, so emitting it
        # here keeps it adjacent to that instruction in the flattened stream --
        # which is also what makes limb 2 above one line of lookahead.
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
            nbuf++
            btyp[nbuf] = "R"
            buf[nbuf] = rtype "\t" rtarget "\t" radd
            btgt[nbuf] = -1
            bself[nbuf] = 0
            next
        }
        /^ *[0-9a-f]+:\t/ {
            if (cur == "") next
            line = $0
            iaddr = -1
            if (match(line, /^ *[0-9a-f]+:/)) {
                iahex = substr(line, RSTART, RLENGTH - 1)
                sub(/^ +/, "", iahex)
                iaddr = strtonum("0x" iahex)
            }
            sub(/^ *[0-9a-f]*:/, "", line)
            sub(/[ \t]+#.*$/, "", line)
            # A bare control-transfer target: objdump prints the absolute
            # address and then its own `<symbol+off>` label for it. Record the
            # ADDRESS and whether the label named the enclosing symbol, and let
            # flushblk() decide -- both questions it answers need the block s
            # extent, which is known only once the block ends.
            if (line ~ /[ \t][0-9a-f]+[ \t]*<[^<>]*>[ \t]*$/) {
                grp = line
                match(grp, /[ \t]*<[^<>]*>[ \t]*$/)
                grp = substr(grp, RSTART, RLENGTH)
                sub(/^[ \t]*</, "", grp)
                sub(/>[ \t]*$/, "", grp)
                gname = grp
                if ((p = index(grp, "+")) > 0 || (p = index(grp, "-")) > 0) gname = substr(grp, 1, p - 1)
                tline = line
                sub(/[ \t]*<[^<>]*>[ \t]*$/, "", tline)
                if (match(tline, /[0-9a-f]+$/)) {
                    tgt = strtonum("0x" substr(tline, RSTART, RLENGTH))
                    tself = is_self_label(gname, cur)
                }
            }
            sub(/[ \t]*<[^<>]*>[ \t]*$/, "", line)
            nbuf++
            btyp[nbuf] = "I"
            buf[nbuf] = line
            baddr[nbuf] = iaddr
            btgt[nbuf] = (tgt == "") ? -1 : tgt
            bself[nbuf] = (tgt == "") ? 0 : tself
            tgt = ""; tself = 0
        }
        END { flushblk() }
    ' "$1"
}

do_compare() {
    require_gnu_objdump
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
    if [ -n "${exceptions}" ]; then
        load_pairs "${exceptions}" " ## " "exceptions" > "${tmp}/exc.tsv"
        # Every key must be OBJECT-QUALIFIED, and no key may appear twice. See
        # the EXCEPTION LOOKUP block: an unqualified key is global across
        # objects, which is the masking M6 W5 T1 fix2 closes, and a duplicate
        # key used to be silently overwritten by whichever line came last --
        # so a file could carry two different reasons for one key and print
        # the wrong one. Both are refused here, before any object is read.
        awk -F'\t' -v file="${exceptions}" '
            NR == FNR { mf[$0] = 1; next }
            {
                k = $1
                i = index(k, "::")
                if (i == 0) {
                    printf "psym_compare: %s: entry %d is not object-qualified: %s\n", file, FNR, k > "/dev/stderr"
                    printf "              Every exception key is <object relpath>::<symbol> [KIND], and an\n" > "/dev/stderr"
                    printf "              entry meant to apply in EVERY object says so explicitly as\n" > "/dev/stderr"
                    printf "              *::<symbol> [KIND]. A bare symbol name is refused because it is\n" > "/dev/stderr"
                    printf "              global across objects without saying so.\n" > "/dev/stderr"
                    bad = 1; next
                }
                q = substr(k, 1, i - 1)
                r = substr(k, i + 2)
                if (q == "" || r == "") {
                    printf "psym_compare: %s: entry %d has an empty object or symbol field: %s\n", file, FNR, k > "/dev/stderr"
                    bad = 1; next
                }
                if (q != "*" && !(q in mf)) {
                    printf "psym_compare: %s: entry %d names an object that is not in the before arm s\n", file, FNR > "/dev/stderr"
                    printf "              manifest: %s\n", q > "/dev/stderr"
                    printf "              A qualifier is a relpath exactly as it appears in .psym-manifest,\n" > "/dev/stderr"
                    printf "              or \"*\" for every object. An entry naming an object that is not\n" > "/dev/stderr"
                    printf "              compared can never be consumed, and a bare C++ symbol name whose\n" > "/dev/stderr"
                    printf "              leading namespace parsed as a qualifier lands here too.\n" > "/dev/stderr"
                    bad = 1; next
                }
                if (k in seen) {
                    printf "psym_compare: %s: entry %d repeats a key first used on entry %d: %s\n", file, FNR, seen[k], k > "/dev/stderr"
                    printf "              Two reasons for one key means one of them is never printed.\n" > "/dev/stderr"
                    bad = 1; next
                }
                seen[k] = FNR
            }
            END { exit bad ? 1 : 0 }' "${before}/.psym-manifest" "${tmp}/exc.tsv" || exit 1
    fi

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
    local fam_ok=0 fam_diff=0 loc_before=0 loc_after=0
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
                            -v obj="${rel}" \
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
            fam_ok=$((fam_ok + $(echo "${st}" | cut -f8)))
            fam_diff=$((fam_diff + $(echo "${st}" | cut -f9)))
            loc_before=$((loc_before + $(echo "${st}" | cut -f10)))
            loc_after=$((loc_after + $(echo "${st}" | cut -f11)))
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
        # normalized listing, the per-symbol path reads the raw one. The AFTER
        # arm is kept BOTH ways -- one file per object, for the per-object
        # resolution the per-symbol layer now does, and their concatenation,
        # which is the input the positional path has always read.
        raw_disasm "${path_b}" > "${tmp}/raw-b.txt"
        : > "${tmp}/raw-a.txt"
        local oi=0
        for t in "${path_a_list[@]}"; do
            oi=$((oi + 1))
            raw_disasm "${t}" > "${tmp}/raw-a-${oi}.txt"
            cat "${tmp}/raw-a-${oi}.txt" >> "${tmp}/raw-a.txt"
        done
        normalize_raw < "${tmp}/raw-b.txt" > "${tmp}/b.txt"
        normalize_raw < "${tmp}/raw-a.txt" > "${tmp}/a.txt"

        # ONE `objdump -h -t` per object per arm, cached here, read twice: once
        # for the executable-section set and once for the defined-function
        # address table. Both feed the EXECUTABLE SECTION relocation class, and
        # both are now stamped with the OBJECT ordinal they came from -- see
        # load_symaddr() for why a split arm cannot share one table.
        obj_layout "${path_b}" > "${tmp}/lay-b.txt"
        exec_sections 1 < "${tmp}/lay-b.txt" | LC_ALL=C sort -u > "${tmp}/xsec-b.txt"
        sym_addrs 1 < "${tmp}/lay-b.txt" > "${tmp}/saddr-b.tsv"
        # REFUSE-BLIND-SECTIONS, the other half of the shape self-check: this
        # object has a `.text` section in its header listing, so exec_sections()
        # must have found at least one CODE section. None means the `CODE` flag
        # was not there to read -- the llvm-objdump `TEXT` shape -- and every
        # executable-section relocation would then quietly fall back to a
        # layout-dependent literal addend instead of resolving to a callee name.
        if grep -qE '^[ ]*[0-9]+[ ]+\.text' "${tmp}/lay-b.txt" && [ ! -s "${tmp}/xsec-b.txt" ]; then
            echo "psym_compare: REFUSING: ${rel} has a .text section but ${OBJDUMP} reported no" >&2
            echo "              CODE section. This script reads GNU binutils' CODE flag from" >&2
            echo "              'objdump -h'; without it every executable-section relocation" >&2
            echo "              falls back to a literal addend and resolves no callee by name." >&2
            exit 2
        fi
        : > "${tmp}/xsec-a.txt"
        : > "${tmp}/saddr-a.tsv"
        oi=0
        for t in "${path_a_list[@]}"; do
            oi=$((oi + 1))
            obj_layout "${t}" > "${tmp}/lay-a-${oi}.txt"
            exec_sections "${oi}" < "${tmp}/lay-a-${oi}.txt" >> "${tmp}/xsec-a.txt"
            sym_addrs "${oi}" < "${tmp}/lay-a-${oi}.txt" > "${tmp}/saddr-a-${oi}.tsv"
            cat "${tmp}/saddr-a-${oi}.tsv" >> "${tmp}/saddr-a.tsv"
        done
        LC_ALL=C sort -u "${tmp}/xsec-a.txt" -o "${tmp}/xsec-a.txt"

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
        flatten_symbols "${tmp}/raw-b.txt" "${tmp}/saddr-b.tsv" 1 > "${tmp}/flat-b.tsv"
        : > "${tmp}/flat-a.tsv"
        oi=0
        for t in "${path_a_list[@]}"; do
            oi=$((oi + 1))
            flatten_symbols "${tmp}/raw-a-${oi}.txt" "${tmp}/saddr-a-${oi}.tsv" "${oi}" \
                >> "${tmp}/flat-a.tsv"
        done
        # REFUSE-EMPTY (M6 W5 T6 commit 0 fix2). flatten_symbols() only walks
        # `Disassembly of section` blocks, which for `objdump -d` are executable
        # sections, so EVERY `S` record it emits names a function in code and
        # must carry at least one `I` record. A symbol with none means the
        # instruction shape was not parsed -- the llvm-objdump failure mode --
        # and an empty body compares EQUAL to another empty body. That is the
        # one outcome this instrument must never produce quietly.
        local empty_syms
        empty_syms="$(awk -F'\t' '
            $1 == "S" { if (cur != "" && n == 0) print cur; cur = $2; n = 0; next }
            $1 == "I" { n++ }
            END { if (cur != "" && n == 0) print cur }' \
            "${tmp}/flat-b.tsv" "${tmp}/flat-a.tsv" | head -5)"
        if [ -n "${empty_syms}" ]; then
            echo "psym_compare: REFUSING: symbols in ${rel} parsed to ZERO instructions." >&2
            echo "${empty_syms}" | sed 's/^/              /' >&2
            echo "              Every symbol flatten_symbols() emits comes from an EXECUTABLE" >&2
            echo "              section, so a body with no instructions means the disassembly" >&2
            echo "              shape was not parsed -- and two empty bodies compare EQUAL." >&2
            echo "              This is the llvm-objdump failure mode; check OBJDUMP." >&2
            exit 2
        fi
        cut -f2 "${tmp}/flat-b.tsv" "${tmp}/flat-a.tsv" | LC_ALL=C sort -u > "${tmp}/mangled.txt"
        demangle_table < "${tmp}/mangled.txt" > "${tmp}/demangle.tsv"
        # Relocation targets get their OWN plain table -- see plain_demangle().
        # The defined-function names are added to it because an EXECUTABLE
        # section target resolves to one of them, and those names appear
        # nowhere in the relocation records themselves.
        { reloc_names "${tmp}/flat-b.tsv" "${tmp}/flat-a.tsv"
          cut -f4 "${tmp}/saddr-b.tsv" "${tmp}/saddr-a.tsv"; } | LC_ALL=C sort -u \
            | plain_demangle > "${tmp}/reldem.tsv"
        # Per ARM, because the same `__cxx_global_var_init.<n>` name can name
        # two different globals on the two sides of a split.
        gvi_identities "${tmp}/flat-b.tsv" > "${tmp}/gvi-b.tsv"
        gvi_identities "${tmp}/flat-a.tsv" > "${tmp}/gvi-a.tsv"

        local per_out per_rc
        rm -f "${tmp}/relstat"
        set +e
        per_out="$(awk -F'\t' \
                        -v obj="${rel}" \
                        -v f_map="${tmp}/symmap.tsv" -v f_exc="${tmp}/exc.tsv" \
                        -v f_dem="${tmp}/demangle.tsv" -v f_used="${tmp}/exc-used" \
                        -v f_reldem="${tmp}/reldem.tsv" -v f_mapused="${tmp}/symmap-used" \
                        -v f_relstat="${tmp}/relstat" \
                        -v f_xsec_b="${tmp}/xsec-b.txt" -v f_xsec_a="${tmp}/xsec-a.txt" \
                        -v f_saddr_b="${tmp}/saddr-b.tsv" -v f_saddr_a="${tmp}/saddr-a.tsv" \
                        -v f_gvib="${tmp}/gvi-b.tsv" -v f_gvia="${tmp}/gvi-a.tsv" \
                        -v nsplit="${ntargets}" \
                        -v f_cls="${tmp}/classify.awk" -v tmpd="${tmp}" \
                        -f "${tmp}/persym.awk" \
                        "${tmp}/flat-b.tsv" "${tmp}/flat-a.tsv")"
        per_rc=$?
        set -e
        local rel_this=0
        if [ -f "${tmp}/relstat" ]; then
            local rst
            rst="$(cat "${tmp}/relstat")"
            rel_this="$(echo "${rst}" | cut -f1)"
            reloc_compared=$((reloc_compared + rel_this))
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
            #
            # R23 (M6 W5 T0 fix3): unless NOTHING was compared. The
            # `RELOCATIONS` line above asserts the two arms' raw relocation
            # streams are not equal; a per-symbol layer that then compared ZERO
            # relocation records has contradicted it, and the PASS it produced
            # rests on a relocation comparison that never ran. Fail, naming the
            # cause: the likeliest one by far is an objdump whose relocation
            # line format is not the one flatten_symbols() parses.
            if [ "${rel_this}" -eq 0 ]; then
                echo "RELOC-BLIND ${rel}: the raw relocation streams differ, yet the per-symbol layer"
                echo "            compared 0 relocation records -- a contradiction, and a PASS here"
                echo "            would rest on a comparison that never ran. Either this objdump's"
                echo "            relocation-line format is not the one flatten_symbols() parses"
                echo "            (three tabs, offset, COLON-SPACE, type, TAB, target), or every"
                echo "            differing relocation belongs to a symbol matched on one arm only."
                rc=1
            fi
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
        cut -f1 "${tmp}/exc-used" | LC_ALL=C sort -u > "${tmp}/exc-used-sorted"
        while IFS= read -r rel; do
            [ -n "${rel}" ] || continue
            stale=$((stale + 1))
            echo "STALE-EXCEPTION  ${rel} (listed as expected to differ; nothing used it)"
        done < <(LC_ALL=C comm -23 "${tmp}/exc-all" "${tmp}/exc-used-sorted")
        # A `*::` entry consumed in more than one object is reported with the
        # objects that consumed it. Non-fatal, and not a widening: the wildcard
        # is what the file ASKED for. It is printed because the whole point of
        # the object qualifier is that a reader can see how far an exception
        # reached, and a wildcard is the one form whose reach is not on its
        # own line.
        LC_ALL=C sort -u "${tmp}/exc-used" | awk -F'\t' '
            $1 ~ /^\*::/ { n[$1]++; o[$1] = o[$1] (o[$1] == "" ? "" : ", ") $2 }
            END { for (k in n) if (n[k] > 1) printf "WILDCARD-USED  %s  %d objects (%s)\n", k, n[k], o[k] }' \
            | LC_ALL=C sort
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
        echo "               THIS COUNT IS DOMINATED BY \`__cxx_global_var_init\`, and for that family"
        echo "               the rank is NO LONGER what pairs the bodies: since M6 W5 T6 commit 0 the"
        echo "               per-symbol layer keys each initialiser by the GLOBAL it touches, and the"
        echo "               PERSYM line's \`N global-var-init keyed by identity (M unresolved)\` column"
        echo "               is the sharper statement -- M is the number this warning is really about."
        echo "               Read that column first; a rank-tagged initialiser with M = 0 was paired by"
        echo "               identity, not by rank."
    fi
    echo "P-SYM: ${total} objects — ${identical} byte-identical, ${noise} differing within the accepted noise class, ${unclassified} with unclassified differences, ${moved} matched by basename after a path move, ${missing} missing"
    echo "P-SYM coverage: ${mapped_objects} objects matched through the object map, ${persym_objects} compared per symbol, ${persym_pass} objects passed per symbol, ${unmatched_after} after-arm objects unaccounted for; symbols — ${sym_matched} matched (${sym_mapped} through the symbol map), ${sym_only_before} only-before, ${sym_only_after} only-after, ${sym_exception} excepted, ${sym_collision} collisions, ${sym_rank_tagged} rank-tagged, ${stale} stale exceptions, ${stale_map} stale map entries; compiler-local label families — ${fam_ok} matched by count, ${fam_diff} differing (${loc_before} before-arm and ${loc_after} after-arm symbols accounted by family); relocations — ${reloc_compared} records compared (${reloc_mapped} through the symbol map)"
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
