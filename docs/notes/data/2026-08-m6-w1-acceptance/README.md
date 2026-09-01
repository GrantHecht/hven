# M6 W1 T10 — IPQP acceptance evidence (A4, Q4, A8, A13, A14, A10)

Produced 2026-09-01 on branch `m6`. Provenance and the declared serial
protocol: `PROVENANCE.txt` (read it first — it governs every number here).

The arm is `bench/ipqp_e1_arm.h` (implementation) + `bench/ipqp_e1_arm.cpp`
(CLI), built as `hven_sqp_ipqp_e1_arm`. Its in-tree gate is
`tests/sqp/test_ipqp_acceptance.cpp`, `IpqpAcceptanceA4.*`.

---

## 1. Regeneration identity — the cells really are E1's cells

E1's generator (`docs/notes/data/2026-08-m6-e1-acquisition/generator/
e1_generate.cpp`) cannot be rebuilt in this repository: it compiles against
the origin project's headers, and `run_sweep.sh` deleted every `.qp`/`.sol`
dump it wrote. So identity is established against what the artifact *did*
keep — the per-cell quantities its own verifier printed at `%.6e`, and the
contiguous variant's recorded LICQ offsets.

`compare_regen_to_e1.py` performs the comparison mechanically:

```
$ python3 compare_regen_to_e1.py regen-certificate-fpcontract-off.csv
cells compared: 27  reference cells found: 27  values compared: 225  differences: 0
```

**225 of 225 values, 0 differences**, across all 27 constructed cells
(18 scattered + 9 contiguous): stationarity residual and its scale, minimum
inactive relative slack, minimum active multiplier, the 10th-percentile
margin, minimum box slack, the LICQ `min|D|`/`max|D|`, and — for the nine
contiguous cells — the drawn-and-LICQ-shifted `active_offset`
(6571, 4393, 2196, 7088, 8010, 12414, 9176, 1974, 12102, exactly
`variant_contiguous_layout.csv`). The offsets alone certify that the
`mt19937_64` stream is aligned draw for draw.

### The one qualification, stated rather than buried

`regen-certificate-fpcontract-off.csv` was produced from an arm TU compiled
with `-ffp-contract=off`; `regen-certificate.csv` is the same arm under the
**shipped uniform flag regime** (CLAUDE.md §7: `-march=native -ffast-math
-fno-finite-math-only`, i.e. FMA contraction on). Against the artifact the
shipped-regime run differs in **43 of 225 values, and only in two columns**:

| column | differs | why |
|---|---|---|
| `stat_inf` | 27/27 | the verifier's own cancellation residual (~4e-16 on a scale of ~2); an FMA changes its last bits by construction |
| `licq_d_min` | 16/27 | smallest `\|D\|` of an `LDL^T` of `J J'`, 1e-8..1e-10; last-digit on 13 of them, ~4x on the three `af30` contiguous cells |
| everything else | 0/225 | exact |

Nothing that *describes a cell* moves: `stat_scale`, the slack and multiplier
extrema, the margin decile, the box slack, `licq_d_max` and every offset are
exact under **both** builds. The generator was compiled at plain `-O3`; hven
compiles under one uniform regime and this arm is not exempted from it. So
the honest claim is: **the recipe is bit-exact — reproduced digit for digit
at matched arithmetic — and the shipped regime perturbs the constructed `g`
at the last ulp.** That is a §7 flag-regime consequence, not a recipe drift.

---

## 2. A4 — the gate, at the shipped defaults

`a4-gate-shipped-defaults.csv`. All 29 cells, both sizes (`nx = 2e4` and
`1e5`), `IpqpOptions{}` verbatim, asserted rather than spot-checked.

**VERDICT: RED on all four of E1's pre-registered criteria.**

| criterion | result |
|---|---|
| every cell converges | **14 / 29** (14 `kOptimal`; 14 `kMaxIter` at the 60-iteration hard cap; 1 `kNumericalError`) |
| `< 40` iterations | **5 / 29** |
| exact active-set recovery (E1's own Rule A) | **3 / 29** |
| no monotone blow-up across the active fraction | **holds** — see below |

Iterations run 10–60; the two equality-only anchors converge in 10 and 11.
The blow-up criterion is the one that passes: iteration counts do not grow
with the active fraction at either size — at `nx = 1e5` scattered they are
`{1%: 47,56,60}`, `{10%: 35,51,60}`, `{30%: 49,58,60}`, i.e. flat-to-falling.
The cap, not the fraction, is what binds.

**The brief's proxy caveat therefore does NOT retire.** PIQP solved these
same cells in 9–18 iterations with exact recovery; the shipped tier does not.

### Where the failure lives — two shipped defaults, measured separately

| configuration | converges | `< 40` iters | Rule-A exact | total iters |
|---|---|---|---|---|
| **shipped** (`ipqp_init_mu = 0.1`, `ipqp_converge_slack = 1e2`) | 14/29 | 5/29 | 3/29 | 1471 |
| `ipqp_init_mu = 1e-2`, slack `1e2` | **28/29** | **28/29** | 11/29 | 690 |
| `ipqp_init_mu = 1e-2`, slack `1` | **28/29** | **28/29** | **25/29** | 737 |

(`a4-counterfactual-mu1e-2.csv`, `a4-counterfactual-mu1e-2-slack1.csv`.)

The decomposition is clean and each half is a *different* default:

- **`ipqp_init_mu` owns the iteration/convergence half.** At 1e-2 the gate
  goes from 5/29 to 28/29 under 40 iterations, on the same cells, same cap.
- **`ipqp_converge_slack` owns the recovery half.** The tier stops at
  `slack × QP tolerance = 1e-7` relative, which leaves active-row slacks
  above Rule A's `1e-8 × row_scale` threshold — so rows that ARE active are
  scored missed. At slack `1` (target the QP tolerance itself) Rule-A
  recovery goes 11/29 → 25/29, at a cost of 47 iterations across 29 cells.

At (`1e-2`, `1`) exactly **one** cell fails anything:
`e1_f7_n20000_af30_m1e-6` — `nx = 1e5`, 30 % active, the tightest margin
class — which reaches the 60-iteration cap. Two further cells miss exactly
one row each, which is the same isolated single miss E1's own artifact
recorded for PIQP on `e1_f7_n20000_af01_m1e-2`.

Only the `ipqp_init_mu` half is sanctioned by this task (spec §10 Q4). The
`ipqp_converge_slack` finding is reported, not acted on — see §7 below.

---

## 3. Q4 — the measured `ipqp_init_mu` sweep

`q4-mu-sweep.csv`. Four levels `{1e-3, 1e-2, 1e-1, 1}` over **37 cells in
four families**: E1's 29, HS10/24/33 under `SqpDriver` at `QpMode::kIpm`,
three indefinite QP fixtures tier-direct, and the two `path_warm` corpus
cells. Run at `ipqp_hard_iter_cap = 200` deliberately: at the shipped cap of
60 the slow levels truncate and the comparison would measure the cap rather
than the level. Decision on **counters**; wall informational.

| `ipqp_init_mu` | E1 iters | HS iters | indef iters | corpus iters | **total iters** | **total facts** | optimal |
|---|---|---|---|---|---|---|---|
| 1e-3 | 795 | 158 | 96 | 110 | 1159 | 1283 | 37/37 |
| **1e-2** | **715** | 214 | **55** | **68** | **1052** | **1191** | **37/37** |
| 1e-1 *(shipped placeholder)* | 1914 | 246 | 96 | 106 | 2362 | 2511 | 36/37 |
| 1 | 1757 | 213 | 56 | 151 | 2177 | 2277 | 35/37 |

**WINNER: `ipqp_init_mu = 1e-2`.** It is best on total iterations, best on
total factorizations, and one of the two levels that solves every cell. It
wins three of the four families outright; HS prefers 1e-3 (158 vs 214), the
only family that does, and 1e-3 is second overall. The shipped 0.1 is the
**worst** level on iterations — 2.2× the winner — and fails one cell.

The three non-optimal rows in the whole sweep are all at the two high levels:
`e1_f7_n4000_af01_m1e-6` (`kNumericalError`, mu = 1),
`e1_f7_n20000_af01_m1e-2` (`kNumericalError` at mu = 0.1 **and** at mu = 1).

**The 0.1 placeholder does not survive its own sweep.** Adopting the measured
default is a §7 DECLARED break; it is priced but NOT taken in this task —
see §7.

---

## 4. A8 — determinism

Two independent full A4 sweeps at the shipped defaults
(`a4-gate-shipped-defaults.csv` vs `...-repeat.csv`), separate processes,
same protocol:

```
A8: cells compared: 29  columns compared: 59  values: 1711  differences: 0
```

Every non-timing column — status, both active-set rules, the ratio-rule face
verdict, all four residual columns and all 39 `IpqpCounters` fields — is
bit-identical across the two runs. **A8 GREEN.**

---

## 5. A13 / A14 — the envelope legs

**A13 as written is not runnable in this repository, and this is a
pre-existing finding, not a new one.** Spec §8.3 asks for a *PSIOPT-envelope*
re-read; the psiopt engine is not in hven (CLAUDE.md §1: neither engine has
migrated yet — the SQP driver has, the interior-point engine has not), and
`docs/notes/2026-08-15-m3-phase-c-plan.md` §12 **Q6a** already ruled on
exactly this: "Plan §6 clause 3 names a psiopt-envelope sweep vehicle that
does not exist here … substitute the SQP vehicles that exist and say so in
the gate package." The 30.15× figure it would be re-read against is a
tycho_sqp P7 number and has no in-tree source.

**Substituted, under the Q6a precedent and declared as a substitution**: the
hven-side walk-vs-tier wall envelope at `nx = 1e5` on F7's own bound-arc
window, one occasion, both engines, `envelope-*.csv`. This is a *contemporary
in-tree envelope*, not a PSIOPT re-read, and no number here may be compared
against the 30.15× figure. If the settler wants the literal A13, it needs the
tycho lane and a built psiopt at the pinned commit.

**A14** (LTO-on / threads-on context leg) is **INFORMATIONAL and is not a
gate** — spec §8.4 says so and nothing in this artifact depends on it. See
`envelope-*.csv` for what was taken.

---

## 6. A10 — Accelerate, UNOBSERVED

No Apple hardware ran any leg of this task. Recorded as **UNOBSERVED**, never
zero-filled (CLAUDE.md §6, absolute):

| leg | Linux (MKL) | Apple (Accelerate) |
|---|---|---|
| A4 gate, 29 cells, both sizes | measured, this artifact | **UNOBSERVED** |
| A8 two-run determinism | 1711 values, 0 diffs | **UNOBSERVED** |
| Q4 `ipqp_init_mu` sweep, 37 cells | measured, this artifact | **UNOBSERVED** |
| A9 symbolic-analysis pin | T9 | **UNOBSERVED** |
| A2 real-hop subproblem | T9 | **UNOBSERVED** |
| A13 envelope | substituted (§5) | **UNOBSERVED** |

The standing mechanism for clearing these is the O12 Mac session
(`docs/notes/2026-08-22-o12-mac-session-runsheet.md`).

---

## 7. What this artifact does NOT do, and why

1. **It does not move `ipqp_init_mu`.** The sweep is decisive (§3) and the
   task text authorises adopting the winner, but the change was applied,
   measured and reverted: it turns **17 tests red**, and at least one of them
   is not a numeric pin. `IpqpCertificationTest.TheFinalReadVerifiesDirections
   OffAWeaklyActiveBoundInsteadOfMasking` — T4b's gate-8 fixture — stops
   producing a weakly active bound at all (`sigma` 0.01 vs the required
   `> 1`; `x(1) - lower(1) = 5e-4` vs `weak_scale = 3.54e-4`), because the
   fixture's mechanism is a function of where the barrier stops. Re-deriving
   it is re-DESIGNING it, in T4b/T5's territory, on a gate that already
   carries an owner-pending residual (plan §7 note (r) item 1, "RESIDUAL C1").
   `IpqpTrace.EscapeEventCarriesTheFullEvidenceBlockVerbatim` likewise changes
   which escape reason fires (1 → 2), which is a census question. The full
   list of 17 is in `.superpowers/w1-t10-report.md`.
2. **It does not move `ipqp_converge_slack`.** Q4 authorises one sweep, over
   `ipqp_init_mu`. The slack finding (§2) is measured and reported so the
   settler can decide whether A4's recovery arm is a second declared break or
   a statement about what the tier's stopping rule is *for*.
3. **It does not retire the proxy caveat.** A4 is red at the shipped
   defaults (§2). Under (`1e-2`, `1`) it is one cell short of green, and that
   cell is the hardest corner of the taxonomy.
