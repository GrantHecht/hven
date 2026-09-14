# M6 W6 T3 — LTO exercised once

Produced 2026-09-14 on branch `m6`; **code head `dee112b3`** (this directory is
the second and last T3 commit, and it is docs-only, so `dee112b3` is the tree
every number below describes). **Read `PROVENANCE.txt` first** — it records the
commits, the toolchain, the hardware, the declared solo protocol, the
comparator's sha256, and it governs every number here.

Everything is **MKL on Linux**. Apple/Accelerate and Windows are **UNOBSERVED**
and nothing is inferred from this run for either. **No wall-clock number in this
directory is asserted** — this is not a performance artifact and says nothing
about whether LTO is faster.

```
docs/notes/data/2026-09-m6-w6-lto/
  PROVENANCE.txt            the stamp, the protocol, the comparator's pin, the result
  f5-configure-proofs.txt   M5's F5 reproduced at c34fd570 and fixed at dee112b3
  reach.txt                 proof the flag reached the compile and link lines
  suite-lto.txt             the Release suite under LTO, and the LTO-OFF control
  replay/off-{walk,ssn,ipm}.csv   U0 27 cells, the named LTO-OFF arm
  replay/lto-{walk,ssn,ipm}.csv   U0 27 cells, the LTO-ON arm
  replay/comparisons.txt    the five pairs, each PLAIN and GATED
  spread.txt                the calibration reading, and the STOP test
  install-smoke.txt         the pre-declared finding, four consumer configurations
```

The 27-cell list is **not duplicated here**: it is
`docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt`, unchanged since
W1, and both arms were driven from that file. Every CSV carries its own
`#`-prefixed hven-native provenance header; strip the `#` lines and what remains
is the artifact.

---

## 1. What this settles

The M6 roadmap's quality floor asks for four words — "LTO-on build exercised
once" — and names no suite, comparison, configuration or artifact. The W6 plan
(`docs/notes/2026-09-m6-w6-plan.md` §0 J.3, as amended) decided what that means,
and this is the evidence for it.

| claim | where | result |
|---|---|---|
| M5's F5 is real, and is fixed | `f5-configure-proofs.txt` | reproduced at `c34fd570`, fixed at `dee112b3` |
| the flag reaches the code | `reach.txt` | 43/43 `libhven.a` objects and both consumer link lines carry `-flto=thin` |
| the suite passes under it | `suite-lto.txt` | **2587 / 2587**, identical to the LTO-OFF control |
| the trajectory does not move | `replay/`, `spread.txt` | every counter and every status byte-identical, all three engines |
| an installed LTO build is consumable | `install-smoke.txt` | **three of four consumer configurations; one fails** |

## 2. F5, and why it had to come first

`hven_compile_options` is a **macro**, so its `set()`s execute in the root
`CMakeLists.txt`'s own directory scope and do reach the targets. The registered
defect was never "the set goes nowhere" — it was the `else()` force-setting
`CMAKE_INTERPROCEDURAL_OPTIMIZATION` **OFF over a caller's own cache entry**. A
project that turns IPO on for itself, includes hven, and never touches
`HVEN_LINK_TIME_OPT` had IPO silently switched off for every target in that
directory, its own included.

Reproduced by configure at `c34fd570`: with
`-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON` in the cache, **0 of 43** `libhven.a`
compile lines carried `-flto`. After the fix, **43 of 43** carry `-flto=thin`,
and a configure that sets nothing still carries none. It had to come first
because the same `else()` could have masked the exercise itself.

> **One defect in the fix commit, declared.** The comment blocks `dee112b3`
> adds cite "line 47" for the macro and "line 122" for `BUILD_HVEN_WHEEL`'s
> forcing. Both were right at `c34fd570` and are stale in the committed file by
> that same commit — its own header-docblock addition pushed them to `:49` and
> `:124`. No behaviour and no number here depends on them; `PROVENANCE.txt`
> records it, and the two-character correction is registered for a fix round.

## 3. The exercise

One Release configure with `HVEN_LINK_TIME_OPT=ON`, in an empty directory, under
the uniform flag regime. CMake's IPO emits **`-flto=thin`** for this clang. It
reaches every one of the 43 objects archived into `libhven.a` (the target's PCH
included) and the link lines of both named consumers, `hven_sqp_tests` and
`hven_sqp_corpus`; the LTO-OFF arm carries it on none of them. The archive's
members are **LLVM IR bitcode** where the LTO-OFF arm's are ELF relocatables —
which is the fact §5 turns on.

The full Release suite passes under LTO, **2587 / 2587 with an inventory of
2589**, the same numbers the LTO-OFF arm reads in the same sitting. (The T3
brief expected 2542/2544; those are the W5 close numbers and are stale at this
HEAD — W6 T0–T2 added tests, and the W6 T2 close already read 2587/2589 on a
non-LTO tree.)

## 4. The replay, and the calibration reading

Five comparisons, each run twice — plain, and through
`compare_replay.py --residual-gate 1e-5`. **This is the gate flag's first
declared use.** Counters and statuses are exact everywhere; residuals pass the
gate; the control (the committed t10b IPM baseline against the LTO-OFF arm) is
0 differences on all 75 columns, which is what makes the rest a statement about
LTO rather than about the run.

Two things are worth a reader's attention, and neither is a regression.

**(a) The floor, not the tolerance, is what admits the residual moves.** Of the
residual cell-columns that moved, **21 per engine sit outside the 1e-5 relative
tolerance** and pass only on the 1e-13 absolute floor — the worst a **3.166e-02**
relative move on `kkt_primal`, between `1.151644575e-16` and `1.189293199e-16`,
an absolute difference of 3.765e-18. So "within gate" here must not be read as
"LTO perturbs residuals by less than 1e-5 relative". It does not; at the 1e-16
scale these values live at, the floor is the gate.

**(b) Two floating-point columns sit outside the gate's set.** On the IPM arm,
`ipqp_alpha_p_min` and `ipqp_alpha_d_min` differ in one cell
(`f7_n1000_path_warm`) by 1.756e-10 and 2.112e-09 relative, and are reported as
gated differences because the gate's column set covers only the seven
residual-class columns. They are **not counters** — but
`scripts/compare_replay.py:43-45` says every column at index ≥ 21 *is* an integer
counter, and that sentence is wrong for this schema: five such columns hold
floats (42 `ipqp_rho_demanded_max`, 43 `ipqp_rho_demanded_last`, 55
`ipqp_restart_shift_max`, 72, 73). The tool's behaviour is fine; the sentence a
reader would use to classify these two is not. **Registered, not fixed here** —
T3 changed no comparator.

## 5. The install-smoke finding

Declared a finding before it was run (plan A2), and the premise held: a clang
thin-LTO `libhven.a` is bitcode, and a consumer that links without LTO cannot
read it.

| front end | consumer IPO | result |
|---|---|---|
| GCC 16.2.1 (the system default, what CI uses) | off | PASSED |
| GCC 16.2.1 | on | PASSED |
| clang 22.1.8 | off | **FAILED** — `ld.bfd: … libhven.a: error adding symbols: file format not recognized` |
| clang 22.1.8 | on | PASSED |

The export contract passes on **all four** install prefixes, the failing one
included — the library installs correctly; only the consumer's link fails. GCC's
arms pass because GCC's LTO objects are ELF files that BFD ld reads as ordinary
objects; clang's bitcode members it cannot read at all without a plugin the
non-LTO link never loads.

The remedy is registered, not built: it collides with the export contract's own
assertion that hven exports **no** `INTERFACE_COMPILE_OPTIONS`
(`scripts/check_export_contract.sh:21-24`), asserted on all three CI lanes.
`install-smoke.txt` states the three candidates for the settler. Nothing about
the default posture changes under any of them — `HVEN_LINK_TIME_OPT` is OFF, and
the CI install smoke passes either way.

## 6. The CI-arm question (M5's, answered here)

The M5 close registered, beside F5, that the "LTO-on build [was] never exercised
by any suite (reachable now — a candidate CI matrix arm if the knob ever
matters)" (`docs/notes/2026-08-m5-ledger.md:561-563`). Plan A14 routed the answer
to this task's ledger line. The recommendation — a recommendation, not a
decision — is in `.superpowers/w6-t3-report.md` and in the ledger line T3 closes
with.
