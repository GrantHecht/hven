# M7 W2 A6 — the final lists (M7-1 … M7-42, plus the M8 registrations of the close)

Status: **RE-FILED per settler ruling C182 (2026-10-31); ratified by the close artifact `docs/notes/2026-10-m7-close.md` (C185).** Date: 2026-10-31. Base: m7 `d4b8e84b`.
Docs only; nothing was built or run. Supersedes the draft lists of
`docs/notes/2026-10-m7-w2-a6-register-triage.md` (`6259eb88`), which is left unedited.
Authority: brief §1 W2 A6 (`docs/notes/2026-09-m7-brief.md:287`); rulings C124 in
`.superpowers/m7-w2-c10-report.md:480`.

## 0. How to read this page

- `C<n>` are rulings in `.superpowers/m7-w2-c10-report.md`; `m7:NNN` are lines of
  `docs/notes/2026-09-m7-ledger.md`; `m6:NNN` of `docs/notes/2026-08-m6-ledger.md`. The register
  itself is `.superpowers/m7-planning-facts.md` §A.4.
- Lists are the brief's three (MEASURED, ARGUED, CARRIED) plus the triage's two bookkeeping
  class (CLOSED). Each id is on exactly one. Where C124 moved an item, it
  sits on its new list.
- This page is re-filed per C182 (`.superpowers/m7-w2-c10-report.md:601`), which accepted the
  read-only criterion-2 judgement round. The rule applied: MEASURED means a TRAIN read was taken in M7;
  a heuristic with no such read is CARRIED to M8 (C124 (7) amended accordingly), not MEASURED.
- ARGUED rows rest on the triage's argument (`6259eb88` §3). The close line ratifies them
  explicitly, except as re-filed here (C182).
- The prior draft's UNSETTLED marks are resolved by C182; the item C182 leaves to the close
  artifact (the H5/H6/H7 package dispositions) is answered in `docs/notes/2026-10-m7-close.md`
  section 6 (C187).

## 1. Counts

| list | n | ids |
|---|---|---|
| MEASURED on TRAIN | 4 | 20, 36, 37, 38 |
| ARGUED from the record | 13 | 5, 8, 10, 12, 17, 22, 24, 27, 28, 31, 32, 33, 34 |
| CARRIED to M8 | 21 | 1, 2, 4 (residual), 6, 7, 9, 11, 13, 14, 15, 16 (hven half), 18, 19, 23 (hven half), 29, 30, 35, 39, 40, 41, 42 |
| CLOSED | 4 | 3, 21, 25, 26 |
| total | 42 | |

M7-16 and M7-23 are split (C182): the harness half is CLOSED at H1, the hven half is CARRIED; each
id is counted once, under CARRIED. The OWNED ELSEWHERE class is empty after the re-filing (M7-2
to CARRIED; M7-16, M7-23 split).

Changes from the prior draft of this page, all by C182: M7-1, 7, 9, 11, 13, 14, 15 MEASURED to
CARRIED; M7-5 MEASURED to ARGUED; M7-6, M7-19 ARGUED to CARRIED; M7-2 OWNED ELSEWHERE to CARRIED;
M7-16, M7-23 OWNED ELSEWHERE to split. Changes from the triage's counts, by earlier ruling: M7-18,
30, 41 and M7-4's residual CARRIED (C124 (1), (3), (4), (8)); M7-25 stays CLOSED, both halves
(C124 (6)); M7-35 and M7-37 moved off OWNED ELSEWHERE when A1 closed negative (CARRIED, C128) and A2 closed (MEASURED, C132); M7-39 ARGUED to CARRIED (C138);
M7-29 ARGUED to CARRIED (C124 (5), §5 below); M7-40 and M7-42 to CARRIED (§5); M7-21 filed CLOSED.

Exit criterion 2 (two settings): `project_start_point` and `equality_basis` failed criterion 2 as
written (C182) and were measured in RETAKE21 (ruling C184; arms ON, OFF-PSP, OFF-EQB on CUTEst 23
+ HS TRAIN; hven-bench close `8a4627f`). **Outcome (C186): both STAY ON; criterion 2 is met for
both; no default event.** `project_start_point`: CUTEst 6W/1L/39T, HS 1W/1L/40T. `equality_basis`:
CUTEst 7W/0L/39T, HS 0W/0L/42T. The close artifact's section 4.2 carries the per-cell detail and
the OFF-PSP iteration-0 error note.

A6 close line (C185): MEASURED 4 / ARGUED 13 / CARRIED 21 / CLOSED 4 = 42. The close ratifies the
13 ARGUED rows and sends every CARRIED row to M8 (`docs/notes/2026-10-m7-close.md` section 7).

## 2. MEASURED on TRAIN

| id | disposition | evidence |
|---|---|---|
| M7-20 | **MEASURED, settled.** `near_active_rows` against F7 and TRAIN: F7 bound window `weak = near = 0`; F7 path N<=1000 `near_active_peak` 8-273, `weak_active_peak` 0-19; CUTEst TRAIN `weak` 0 on all 36 rows, `near` non-zero on ACOPR118 (15) and ACOPR57 (2) only; HS 0/770. No heuristic reads the census (A1 did not land), so the precondition lapses. The read was taken on F7 and the existing C1 cost census, not as a new TRAIN run. | A1 page §3.5 (`e721ee48`, `f7e5324c`), C125 Q7; `docs/notes/data/2026-10-m7-a1/`. |
| M7-36 | **MEASURED, settled: NO default move.** `ipqp_converge_slack` stays 1e2: slack 1 loses hs; slack 1e3 vetoed by fixture F-b. | A3, C138 (1); `docs/notes/data/2026-10-m7-a3/`; landed C141, gates `1ff37db2` (C142). |
| M7-37 | **MEASURED, settled: lands OFF.** `ipqp_mu0_rule` (complementarity) loses on cost on both families and never fires (fallbacks 29/29); `ipqp_mu_residual_floor` wins hs and cutest but loses fixtures E1-c and T-a, so a lost fixture vetoes the default (A2-F4). `init_mu` 1e-3 wins hs: configuration guidance only. Both fields at OFF. | A2, C129-C132; `9973a4fb`, gates `a869e7f3`, `docs/notes/data/2026-10-m7-a2/`, `-a2-gates/`. |
| M7-38 | **MEASURED, settled.** Sticky trial NOT ADOPTED in either form (P-U budget losses; P-C numerical_error on two_negative_eigenvalue_row). `ipqp_ladder_skip_after` stays 3; the field lands at the shipped value, skip 1 not acted on. | A3, C138 (2)-(4); C140-C142; A3-R2..R5 to M8. |

## 3. ARGUED from the record

| id | disposition | argument / evidence |
|---|---|---|
| M7-5 | ARGUED (C182): `ipqp_reseed_after_disproval` is inert on TRAIN and ships OFF, so criterion 2 is met by the default being unchanged. No OFF-vs-ON TRAIN read was taken. | W0 T4, `m7:574`, fix1 `dd4378d5`; constructed win OFF 6 majors/max_iter vs ON 3 majors/optimal (`m7:719-731`); C182. |
| M7-8 | `elastic_applied` is a label derived from `report.has_value()`; carry the label fix unless a read needs the distinction. | Triage row; A0 `docs/notes/2026-09-m7-w2-a0-signals-design.md`. |
| M7-10 | Retire as an instrument need: `symbolic_analyses` is already aggregated at R and `--budget-iters k` makes prefix runs routine. | `docs/notes/2026-09-m7-w2-a0-signals-design.md:155`; `m7:739-750`. |
| M7-12 | Known test-content gap; take only if a fix touches the dual-norm window. A new seam is not justified for a class that cannot fire in the shipped population. | Triage row; CLAUDE.md §6/§8. |
| M7-17 | Keep telemetry always-on; wall is informational, counters are the currency; no instruction-count read shows a cost. | Triage row; `docs/measurement.md`. |
| M7-22 | Carry unrequested; reopen on the first test not writable at the boundary. | CLAUDE.md §6/§8; triage row. |
| M7-24 | Carry to M9, do not foreclose; QN went through a decorator at the engine's call sites instead. | Triage row; QN stages (`m7:970`). |
| M7-27 | Argue against a new stall form until a TRAIN cell shows a frozen full-step trajectory at min-mu; none recorded. | A0 `:167`; triage row. |
| M7-28 | Notes only; `ipqp_face_uncertain` is already at R. Revisit equilibration only with the mu_0 work; A2 landed it OFF (C129-C132). | A0 `:131, :181`; C129. |
| M7-31 | Carry; moving the stopping rule is a declared break (`border_refine_steps` 159 to 166 on F2far/cold) and nothing in W2 bought a reason. | D1; triage row. |
| M7-32 | Carry with M7-31. | D2. |
| M7-33 | Carry; adopting refined duals moves every pinned dual. R2's `duals_carried` surface shows it is delicate. | D3; `.superpowers/m7-w2-c10-report.md:192-194`. |
| M7-34 | Carry; blocked, no backend reports singularity as a status. | D4; T7 fix round 3. |

## 4. CLOSED

| id | evidence |
|---|---|
| M7-3 | W0 T1 CLOSED 2026-09-15 (`m7:106`; `c1ecdfd4`, `00b11955`, `90fa8008`). |
| M7-21 | T5 disposition 2026-09-14: "not taken (no interface widened)" (`m7:101`). |
| M7-25 | Both halves, C124 (6): elastic-QP half shipped at W2; R6 third-producer coverage at M6 W1 T6 (`cc4ff97`, `m6:842-857`). |
| M7-26 | Per-lay copies <0.4%, NOT the lever (`m6:454-457`); nothing in M7 asks for the permutation. |

## 5. CARRIED to M8

Destination of every row below: **M8**.

| id | reason |
|---|---|
| M7-1 | C182: no TRAIN read of the `elastic_rung_engine` default (`kWalk`) vs OFF (`kTier`) was taken in M7; C124 (7) amended. Build/F7 measurement only (W0 T3, `m7:445`, `40ef105f`, `2200c880`). |
| M7-2 | C182: H5 is unbuilt (no many-solves leg in the record). C187: H5 NOT BUILT in M7, CARRIED to M8 with the published-comparison infrastructure (`docs/notes/2026-10-m7-close.md` section 6). Brief `:200-207`. |
| M7-4 (residual) | The task closed at W0 T2 (`m7:280`, `b68590a5` ... `231a279f`); the +0.6% unattributed residual after the mask is carried, C124 (8). |
| M7-6 | C182: filed CARRIED (was ARGUED under C124 (9)). Probe D has no TRAIN population and no TRAIN read was taken; column scaling has shipped default-on (SCW `59939b29`, C117, C120). `m7:773-775`. |
| M7-7 | C182: take-vs-decline of the elastic-site restoration offer (D6) not read on TRAIN in M7; RST (C109) did not name it. |
| M7-9 | C182: only the zero-suspicion A-SQP read exists (`m7:747`); the false-fire rate across the three engines' cells (D) was never taken. |
| M7-11 | C182: `rho_0` headroom cap (D12) not read on TRAIN in M7. |
| M7-13 | C182: `x_soc` as a restoration candidate (D14) not read on TRAIN in M7; RST/YKN/RD8 not diagnosed seed-limited (C124 (3)). |
| M7-14 | C182: cross-mode disagreement shape (D15) not read on TRAIN; the A4-0 sweep (`m7:739-745`) is a different statistic. |
| M7-15 | C182: the two `mu = 1e-6` pathologies (D16) not read; A2 measured `init_mu`/floor on other cells (C129). |
| M7-16 (hven half) | C182 split: the harness half is CLOSED at H1; the hven half (configure-time `git describe` stamp; H7, brief `:215`) is CARRIED. H7 hven half CARRIED to M8 (C187; close note section 6). |
| M7-18 | Row-kind census: on no list in the brief; no W2 heuristic reads it; T5 took no interface widening. C124 (1). |
| M7-19 | C182: filed CARRIED (was ARGUED). No M7 task moved an IPM counter that would have taken it; take with the next task that does. Triage row. |
| M7-23 (hven half) | C182 split: the harness half is CLOSED at H1; the hven half (emitter, H7, brief `:217`) is CARRIED. Read the A0 clause-4 note (`m7:806-811`) first. H7 hven half CARRIED to M8 (C187; close note section 6). |
| M7-29 | Warm costs more IPM iterations than cold (24 vs 5). The identity with acceptance cell A12 is not established: `m6:244-248` registers the symptom as A12, but the A12 result reported (`m6:1207`, 36 to 33 vs cold 48 to 47) is an IPQP-tier datum, not the IPM 24-vs-5. Carried per C124 (5); its reader is A5-R1's between-solve layer. |
| M7-30 | IPM re-entry mu reset: the owner's M6 condition lapsed (`m6:522-525`) and no M7 package owns it. C124 (4). |
| M7-35 | Stopping-mu vs bound geometry: A1 CLOSED NEGATIVE for M7. Mechanism confirmed (gate-8, `e1_f7_n20000_af30_m1e-6`), but the rule fails "no cell worse end-to-end" on 4 of 29 E1 cells; code held on `w2-a1-impl`; carried with A1-R4..R6. C125-C128; record `558ab224` (`docs/notes/data/2026-10-m7-a1-impl/`). |
| M7-39 | Paper-vs-driver IC constants: A3 did not need them; carried. C138. |
| M7-40 | Barrier endgame at the guard: needs a purpose-built curvature/bound population; none was built in W2. C182 does not address it; it stays CARRIED per the brief and triage. |
| M7-41 | Restoration seed generalization: no restoration cell in RST/YKN/RD8 was diagnosed seed-limited. C124 (3). |
| M7-42 | C182: certification fixture class stays CARRIED; A2's "M7" fixture is a mutant name, not this class. A1's code and fixtures were held, none landed on m7 (C128). |

## 6. Split rows (M7-16, M7-23)

| id | harness half | hven half |
|---|---|---|
| M7-16 | CLOSED at H1 (C182) | CARRIED to M8 (§5) |
| M7-23 | CLOSED at H1 (C182) | CARRIED to M8 (§5) |

## 7. Registered to M8 during the close

Each is CARRIED; none is a register id.

| item | what | reason carried |
|---|---|---|
| WX-R12 | Windows NC2 qp3 ends kNumericalError on a feasible convex LP. | A robustness gap, not a test matter. Windows is off-reference and the status is ruled honest (no false certificate, convex oracle holds). C163; reaffirmed C176. |
| CF-1-R1b | A config-aware alignment probe in Debug. | An installed Debug hven exports `EIGEN_MAX_ALIGN_BYTES=32` while compiling without AVX. The fix `7b5081f0` closed the ODR violation but not the probe. C171, C175. |
| CF-1-R1c | An Eigen recurrence guard. | Eigen only via `hven_dep_eigen`, or an aligned_malloc/realloc agreement test, so the mixed-flag failure cannot return. C171. |
| RT-R1 | RETAKE20 comparator's unmeasured list omits a cell that also moved. | Prints "(none)" when the cell moved; accepted as noted, no post-data edit. C177. |
| CEN-R1 | E2 fix1b `bf37c305` hits the minor cap on `f7_n1000_path_corrupted` QP1. | Factorizations 2976 to 28444; survives only through M1's `qp_failure_budget`. Census diagnosis, C179-C180. |
| CEN-R2 | Per-minor cost 1.35-1.92x on minor-heavy f7 path cells from E2, E4 and BPS. | Pushes `n10000_path_physics` from Optimal to dnf_budget (baseline used 2802 of 3600 s; the U0 baseline row, 2801.6 s, is authoritative; C179's 2759 s was a different measurement). The extra solves should be made conditional. C179-C180. |
| CEN-R3 | An intermediate 57-cell census for walk-touching changes. | A process item: the 20 undeclared f7 path-window movers were gated only on the 27-cell replay. C180. |
| A5-R1 | Cross-engine between-solve switching, with its own pre-registered rule. | A5 dropped from M7 (brief amendment 2026-10-28); the only evidence is an oracle union of 19 vs 17 on CUTEst TRAIN across two pins, not a measurement of switching. Needs both lanes at one pin and a rule scoring switching against the best static arm. C139. |
| SCL-R2 | Power-of-two row factors. | Moves the existing row layer for every `enable_scaling` user; kept out of the SCL round (`.superpowers/m7-w2-c10-report.md:409`). The trace field of the same id shipped in SCW `59939b29`. |
| BRITGAS main-loop cost (RST-B1) | The main-loop cost on BRITGAS. | Deferred by the owner, C113a-b; W3-R5 (tau 1000 on BRITGAS) is an input. |
| nx=1e5 envelope population (D36) | A real tier envelope population at `nx = 1e5`. | Retargeted to M8 as D36 (register note, `m7-planning-facts.md` §A.4 last row). |
| Small-dense-problem measurement round | A measurement round on small dense problems. | Owner addition, 2026-10-30. No scoping text in the C-report. |
| Solver API revisit | Revisit the public solver API. | Owner addition, 2026-10-30. No scoping text in the C-report. |
| `examples/` set | A set of examples. | Owner addition, 2026-10-30. No scoping text in the C-report. |

Also carried by earlier rulings, not part of this page's counts: A1-R4..R6 (C128), A2-F1..F4 and
A2-R1..R3 (C129), A3-R2..R5 (C138), A7-R1..R4 (C134), W3-R1..R5 (C146), WX-R8 (design §5).

## 8. Open items for the close artifact

All four items are taken up by `docs/notes/2026-10-m7-close.md`:

1. RETAKE21 outcome: section 4.2 (both settings STAY ON, C186); filled in at §1 above.
2. H5/H6/H7 package dispositions (M7-2, M7-16, M7-23 hven half): section 6, settled by C187.
3. ARGUED rows (§3, including M7-5) ratified: section 7 (C185).
4. Criterion-2 reading for untoggled QP-engine mechanisms, with E4's before/after record and
   CEN-R1/R2 disclosed: section 4.3 (C182).
