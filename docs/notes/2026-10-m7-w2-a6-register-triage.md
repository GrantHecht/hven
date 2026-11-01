# M7 W2 A6 — the register read, triaged (M7-1 … M7-42)

Status: **DRAFT for settler ruling.** Date: 2026-10-23. Base: m7 `3d27c29f`.
Authority: `docs/notes/2026-09-m7-brief.md` §1 W2 A6 (`:266–276`). Docs only; nothing
was built or run, and every claim below is a reading of the record.

## 0. Sources and how to read this page

- **The register itself** is not in the repo. Its ids and one-line titles are in
  `.superpowers/m7-planning-facts.md` §A.4 (`:141–182`), and each id's origin text
  (D1–D34, the roadmap bullets at `:385–415`) is in §A.2 / §A.3 of the same file.
  Ledger line numbers cited as `m6:NNNN` are `docs/notes/2026-08-m6-ledger.md`; `m7:NNN`
  are `docs/notes/2026-09-m7-ledger.md`.
- **The M7 ledger in the tree ends at 2026-09-17** (W2 M4 close, `m7:827`). The later W2 work
  (the walk / restoration / QN rounds, C1–C123) is recorded only in
  `.superpowers/m7-w2-c10-report.md`, which I read read-only. No entry in it closes, or
  names, any register id: it is a SNOPT-gap census on the walk, NC3/NC4/R2/CERT-1/P1E/C13,
  BPS, QC-1/QC-8, RDV, QN stages 1–3 and RST. C113 (`:456`) is the approved plan for what
  remains: A1, A2, A3, A6, A7, A4 → A5, W3, WX, W4. So "covered by work since the brief"
  below means the ledger and the C-report, and where neither names the item I say "not
  found in the record".
- **Lists:** MEASURED (needs a TRAIN read), ARGUED (disposition argued from the record,
  settled in the close line), CARRIED (to M8, with the reason), plus two bookkeeping
  classes the brief itself uses: ALREADY CLOSED and OWNED ELSEWHERE (a named work package).
  Each item is on exactly one.

## 1. Counts

| list | n | ids |
|---|---|---|
| MEASURED on TRAIN | 11 | 6, 7, 9, 11, 13, 14, 15, 20, 36, 38, 40 |
| ARGUED from the record | 18 | 8, 10, 12, 17, 19, 22, 24, 27, 28, 29, 30, 31, 32, 33, 34, 39, 41, 42 |
| CARRIED to M8 | 1 | 18 (proposed; the brief lists it nowhere) |
| ALREADY CLOSED (wholly or in the part that matters) | 6 | 1, 3, 4, 5, 25, 26 |
| OWNED ELSEWHERE | 6 | 2 (H5), 16 (H7), 21 (T5, closed), 23 (H7), 35 (A1), 37 (A2) |
| total | 42 | |

The brief's own lists name 40 of the 42 ids across MEASURED/ARGUED/closed/owned. **M7-18 is on
no list; I propose CARRIED.** The brief's MEASURED list is 11 ids once M7-36 and M7-38
(via A3) and M7-40 (conditional) are counted.

## 2. Flags (read these first)

1. **M7-18 is on no list in the brief** (row-kind census, kink measure at driver level,
   bound-side weak/near; D19). Proposed CARRIED; reason in the table.
2. **M7-35 and M7-37 are not on A6's lists** because A1 and A2 own them as work packages
   (brief `:237`, `:244`). That is consistent, but the close line should say so, or A6's
   "each item on exactly one" is not literally met by the brief's list.
3. **M7-41 is listed ARGUED, but the register says "No ledger argument exists"** (the
   word "generalization" appears nowhere in the M6 ledger; `.superpowers/m7-planning-facts.md:181`,
   §A.3). There is nothing to argue FROM; the argument below is built from the W2 M2 design
   and the RST record, and is thin.
4. **M7-30 is in the brief's ARGUED range (27…34) but is a task-candidate with an owner
   ruling on its timing.** The M6 ledger rules the IPM re-entry mu-carry "its own small task
   AFTER W1/W2, inside M6 if the window allows, M7 only if M6 runs long" (`m6:522–525`). M6 closed
   without it and no M7 work package owns it. It needs a decision, not an argument.
5. **M7-29 is in the ARGUED range but has its own acceptance cell** (A12, `m6:248`). Whether
   the A12 cell M6 reported (`m6:1207`, an IPQP-tier warm datum, 36 → 33 vs cold 48 → 47) is
   the same datum as tycho's IPM 24-vs-5 re-solve is not established in the record I read.
6. **M7-25's second half is also shipped.** The brief closes the elastic-QP half only
   ("shipped at W2"). The "R6 third-producer coverage" half is described in the M6 ledger as
   done at `m6:842–857` (W1 T6 CLOSED, `cc4ff97`: "R6 — the tier is the third `QpSolution`
   producer through the one export sweep"). The register keeps it open as "coverage";
   whether the existing T6 pins are that coverage is for the settler. I list M7-25 as closed.
7. **M7-1's default is still W2's to re-read on TRAIN** (brief T3 text, `:108–116`), so
   "closed" below means built, measured on F7 and gated; the TRAIN re-read of
   `elastic_rung_engine` is an open W2 obligation. Same shape for M7-5's toggle
   `ipqp_reseed_after_disproval` (default OFF, "a constructed win deferred to W2's TRAIN
   re-read", `m7:719–731`).
8. **M7-6 may have no TRAIN population.** Probe D (a constructed QP-NLP) is the fixture
   (D5); the A-SQP read already excluded NLP scaling as the mechanism for the 23 CUTEst
   cells (`m7:773–775`). A TRAIN measurement of M7-6 only reads whether any TRAIN cell
   has the shape.
9. **M7-4's residual is still registered:** after the mask removed 57 % of the attached
   cost, a +0.6 % unattributed residual remains and "D24 still registered" (`m7:719–731`
   W0 close). Closed as the task; the residual is an ARGUED-class tail, not a task.
10. **M7-16 is partly remedied.** The W0 close list item 6 names "the configure-time
    `# binary:` stamp's one-line remedy" as still owed (`m7:719–738`); `docs/measurement.md`
    (T6, `m7:631`) and the harness's re-configure-per-pin rule (`m7:101`) work around it.
    H7 owns the fix.

## 3. The register, one row per item

Columns: id · what · status in the record · list · evidence / argument.

| id | what | status | list | evidence / argument |
|---|---|---|---|---|
| M7-1 | Tier-solved elastic rung with its cost named (D11) | **W0 T3 CLOSED 2026-09-16** (`m7:445`; a0a73617, 74a4b4b0, fix1 40ef105f, fix2 2200c880): the rung behind `elastic_rung_engine`, `kWalk` default | ALREADY CLOSED | Measured at 40ef105f: walk rung factorizations linear in n (184 → 2281) against the tier's flat 73–75; the tier's 3-of-5 false `kInfeasibleSuspect` declines found (`m7:445–456`). Flag 7: default re-read on TRAIN still owed. |
| M7-2 | Many-solves-per-process leg, both engines, plus cross-row carrier (D22) | Task **H5**; no H5 closure in the ledger at this commit | OWNED ELSEWHERE | Brief `:200–207`. Not found closed in the record I read. |
| M7-3 | Shared-pool fan-out pin (D23) | **W0 T1 CLOSED 2026-09-15** (`m7:106`; c1ecdfd4, 00b11955, 90fa8008) | ALREADY CLOSED | `Concurrency.TwoPartitionedProgramsOnTwoThreadsMatchSerial` pins it, 3 premises asserted, finding 10/10 runs. D49 rode along. |
| M7-4 | Interior attached-callback cost + the interior leg's missing sink lever (D24) | **W0 T2 CLOSED 2026-09-15** (`m7:280`; b68590a5 … fix1 75200175, 231a279f) | ALREADY CLOSED | `IterationPayload` mask: 57 % of the attached cost removed, counters byte-identical, SQP corpus flat. Residual +0.6 % unattributed, "D24 still registered" (`m7:719–731`). Flag 9. |
| M7-5 | §0 gap 5 recoverable state after a disproval + the un-pinned disproved-entry charge (D10) | **W0 T4 CLOSED 2026-09-17** (`m7:574`; 5eafcb78, 39746705, 935ccfc2, fix1 dd4378d5) | ALREADY CLOSED | Reseed behind `ipqp_reseed_after_disproval` (default OFF); charge pinned three ways with a constructed vehicle, no test seam. Constructed win measured (OFF 6 majors/max_iter vs ON 3 majors/optimal); default re-read deferred to W2 TRAIN (flag 7). |
| M7-6 | Ill-scaled feasible row at DRIVER level, every mode (probe D; D5) | Not found worked in the record | MEASURED | TRAIN read: does any TRAIN cell carry a feasible row scaled 1e-4…1e-2 against the rest, and does `enable_scaling` change its status? Decided by: status/`elastic_activations`/restoration count per cell, scaling on vs off. Partial prior evidence: NLP scaling "not the mechanism" for the 23 CUTEst cells (`m7:773–775`), IPM lane 15/23. Flag 8. |
| M7-7 | Elastic-site restoration offer's TAKE question against the comparison set (D6) | Not found worked. Blocker (suite v1) lifted by H1/H2 closure | MEASURED | D6: no config-stable take across 2520 driver cells; the offer is 0 or rounding (2.22e-16 vs `h` 4e8); `t7_exhaustion_attached_report` records the seed on 1 Release row, 0 Debug. TRAIN read: take vs decline per cell on the counters. Overlaps the restoration round (C109 RST, `.superpowers/m7-w2-c10-report.md:428–436`), which did not name this question. |
| M7-8 | `elastic_applied` re-scope (D7) | Not found worked; the A-SQP read used `elastic_applied` at trace depth 1 to separate restoration request sources (`m7:770–773`) | ARGUED | The field is derived from `report.has_value()` (D7), so a refusal answered by rung B is mis-marked an elastic row. Argument: it is a reporting label; A0 promoted the elastic-related signals for MEASUREMENT only (`m7:795–825`). Supports: keep registered as a label fix, carry unless a W2 read needs the distinction. |
| M7-9 | `ipqp_suspicion_disproved` false-fire rate at scale (D8) | Partly informed since the brief | MEASURED | A-SQP read: the detector raised ZERO suspicions on the counted TRAIN cells (`m7:739–745`); W0 T3/T4 found it false-fires on two CONSTRUCTED consistent vehicles (`m7:719–731` item 1, "narrow the growth conjunct, design first, BEFORE T4's default is re-read on TRAIN"). TRAIN read: suspicions/disproved per cell across all three engines' cells; the specificity estimate D8 says is missing. Not yet taken. |
| M7-10 | Per-row `symbolic_analyses` on `SqpIterate` (contract change; D9) | A0 found `symbolic_analyses` already aggregated and "it is what separated the two triggers in the read's §M3" (`2026-09-m7-w2-a0-signals-design.md:155`) | ARGUED | The aggregate is already at R; the per-row field is a contract change for a prefix-truncation workaround that `--budget-iters k` now makes routine (`m7:739–750`: a capped run is a prefix, so one final row becomes a trajectory with no new code). Supports: retire as an instrument need; keep only if a consumer wants the row. |
| M7-11 | `rho_0` headroom cap (D12) | Not found worked | MEASURED | D12 gives the shape: `rho_0 <= kElasticRhoMax / kElasticRhoFactor^k`, k >= 1. TRAIN read: elastic one-rung exhaustions with `rho_0` at the ceiling, with vs without the cap; decided by exhaustion/major counts. Argument against urgency: when the ceiling binds the duals were already past the ladder's cap. |
| M7-12 | T2's two residual gaps: `sol *= dscale` trajectory; no `start == end` fixture (D13) | Not found worked | ARGUED | (i) is a test-content note, not a defect; (iii) needs an engine-surface change (retain reference multipliers, or a test seam). Under the §6/§8 seam convention a new seam is expensive for a class that cannot fire in the shipped population. Supports: carry as a known gap; take it only if a W2 fix touches the dual-norm window. |
| M7-13 | Rejected-SOC point `x_soc` as a restoration candidate (D14) | Not found worked; no hit for `x_soc` in any M7 page | MEASURED | TRAIN read: on rejection sites (b)/(c) where SOC did not run, how often is `h(x_soc) < h(entry)`; decided by restoration-seed adoption and the post-restoration objective. Restoration was re-worked since (M2 design, RST C109); the question was not asked there. |
| M7-14 | Eleven residual cross-mode disagreements (T6b disclosure (b); D15) | Not found worked | MEASURED | Byte-stable-against-BASE pathology: above the shipped `mu` the border trajectory ends at a box corner with a row off by 15 and the slack shut. TRAIN read: whether any TRAIN cell shows cross-mode (walk/SSN/IPM) status disagreement of that shape; the A4-0 sweep (2/4/2 of 23 across `kWalk`/`kSsn`/`kIpm`, `m7:739–745`) is the nearest data and is not the same statistic. |
| M7-15 | Two pre-existing `mu = 1e-6` pathologies (D16) | Not found worked | MEASURED | Ill-scaled feasible family reads `kInfeasible`; `w2_box_blocked_qp(0.5)` elastic at rho 1e7. Same instrument as M7-14; also the natural companion read of A2's mu default (A2 changes mu_0, brief `:244`). |
| M7-16 | Build-time-stamp trap (D17) | H7; partly worked around (flag 10) | OWNED ELSEWHERE | Brief `:215`. |
| M7-17 | Option gate for the M6 T3 telemetry cost (+0.5–1.1 % wall, always-on; D18) | Not found worked | ARGUED | The cost is wall, which `docs/measurement.md` / CLAUDE.md §7 call informational; counters are the asserted currency. The M7 precedent (T2) was to cut an attached cost with a mask, not a gate. Cheapest reclaim named by the register: fuse the previous-set carry into the census's own passes. Supports: keep always-on and carry the gate to M8 unless an instruction-count read (not wall) shows it; I did not find such a read. |
| M7-18 | Row-kind census (model contract); kink measure at driver level; bound-side weak/near (D19) | Not found worked; A0's inventory carries `near_active`/`weak_active` peaks at R (`2026-09-m7-w2-a0-signals-design.md:130, 343`) but not the census | CARRIED (proposed) | **On no list in the brief.** The census is a model-contract item, and T5 took no interface widening (`m7:101`). Bound-side weak/near is the only part with a W2 reader, and A0 already placed the existing peaks at R. Reason to carry: no W2 heuristic reads it. |
| M7-19 | IPM count-identity restoration-break hole; unity-batch latent-include; `enter_restoration` decide/run split; `h_facs == 0` (D20) | Not found worked | ARGUED | Count-identity and classification items on the IPM side. W2 work has been on the SQP walk/QP engine, none touching the IPM counters. Supports: keep registered; the identity matters for any replay claim, so take it with whichever task next moves an IPM counter. |
| M7-20 | `near_active_rows` measured against F7 before any heuristic reads it (0/770 on HS; D21) | A0 names it a PRECONDITION for A1 (`2026-09-m7-w2-a0-signals-design.md:130, 343`); not found measured | MEASURED | Precondition. TRAIN (and F7) read: the distribution of `near_active_rows` and `weak_active_peak` / `near_active_peak` over cells at `restoration_mode == off` vs on, against the F7 corpus. Decides whether any A1 rule that reads it has a signal at all. Must precede A1. |
| M7-21 | Defaulted-virtual rider for the dimension accessor (D25) | T5's. **Dispositioned 2026-09-14: "not taken (no interface widened)"** (`m7:101`) | OWNED ELSEWHERE | Closed by T5's own disposition. |
| M7-22 | `KktFactor` construction observer (third `HVEN_TESTING` target; D26) | Not found worked; `m7:77` and A0 `:492` only note counter aliasing | ARGUED | CLAUDE.md §6/§8: instrument at the boundary by preference; a new `HVEN_TESTING` target needs a stated need. No W2 task has stated one. Supports: carry unrequested; reopen on the first test that cannot be written at the boundary. |
| M7-23 | Table-driven corpus emitter (76 positional fmt slots; D27) | H7 | OWNED ELSEWHERE | Brief `:217`. Note: A0's clause 4 (fired-counter at R) is declared mutually exclusive with frozen golden trace bytes under a table-driven writer, and routes via the additive trailing key (`m7:806–811`). H7 should read that before building the emitter. |
| M7-24 | Per-piece convexification needs a second seam view (`AggregateEvalSeam` flattens to one CSR arena; D28) | Not found; no hit for `AggregateEvalSeam` in M7 docs | ARGUED | Register classes it also as an M9 touchpoint (`.superpowers/m7-planning-facts.md` §A.2). No W2 work convexifies per piece; the QN work (C30–C45) went through a carrier/decorator at the engine's call sites instead. Supports: carry to M9, "do not foreclose". |
| M7-25 | R6 third-producer coverage; the elastic-QP formulation half is subsumed by W2's feasibility mode (D29) | Elastic-QP half **shipped** (brief); R6 half **done at M6 W1 T6** (`m6:842–857`, `cc4ff97`) | ALREADY CLOSED | Flag 6. |
| M7-26 | raw↔claim permutation for a direct-scatter path; per-lay copies measured NOT the lever (D30) | Per-lay copies `1 µs / 271 µs = <0.4 %` — NOT the lever (`m6:454–457`) | ALREADY CLOSED (as a lever) | The permutation item stands only as part of the deterministic-fold / no-copy-scatter item; nothing in M7 asks for it. |
| M7-27 | "alpha = 1 but nothing moves" as a fourth stall form; no "an evidence failure happened" counter (D31) | Not found worked | ARGUED | A0 covered the counter side: `ipqp_stall_window` "already the per-toggle shape" (`2026-09-m7-w2-a0-signals-design.md:167`); `IpqpResult::inertia_evidence_failed` is the existing evidence flag. The new stall form is a taxonomy change at §6.2. Supports: argue against a new form until a TRAIN cell shows a frozen full-step trajectory at min-mu; none is recorded. |
| M7-28 | Equilibration fixed across ladder rungs; z ~ 1e-9 at free variables; `face_uncertain` accumulated on every exit; `SqpOptions` revalidated per subproblem (D32) | Partly informed | ARGUED | `ipqp_face_uncertain` is at R already ("none" to promote, A0 `:131, :181`). The others are observations with no recorded defect. Supports: keep as notes; revisit equilibration only with A2's mu_0 work (it changes the scale the rungs see). |
| M7-29 | Warm costs MORE IPM iterations than cold on a perturbed re-solve (24 vs 5; D33) | Registered as acceptance cell A12 (`m6:248`); M6 reported A12 warm advantage widening (`m6:1207`) | ARGUED | Flag 5: not established that the A12 cell is the same datum. The mechanism is stale pre-perturbation primal plus line search pinned near alpha 0.25, with no barrier state; it is an IPM warm-start issue whose reader would be A5's between-solve layer. Supports: link it to A5 if A5 is built, else carry. |
| M7-30 | IPM re-entry mu reset: a phase-sequence entry carrying mu + projected point (D34) | Owner ruling `m6:522–525`; not done in M6; no M7 owner | ARGUED | Flag 4. The record shows tycho's Brach_16/32seg main stage +2 iterations (+11 % / +18 % on 2.5 ms solves), PolarLT −32 %: "not a blanket loss". Supports: a decision for the settler, task or carry; tycho is the only reader of the symptom. |
| M7-31 | Whole-solve bordered stopping rule in PENALTY vs row units (D1) | Not found worked | ARGUED | D1: moving it changes `border_refine_steps` 159 → 166 on F2far/cold and needs the golden rig, bench baselines and U0 re-derived: a declared break. Nothing in W2 bought a reason. Supports: carry; cost per the record is a re-derivation event. |
| M7-32 | Alternative (ii)'s full form: unpenalized stable-face refine, whole-solve (D2) | Not found worked | ARGUED | D2 is the successor "if (1) or the residual disagreements are taken up". It rides M7-31 / M7-14. Supports: carry with M7-31. |
| M7-33 | Adopt the REFINED step's duals (off by O(H·Δx); D3) | Not found worked | ARGUED | A one-line candidate that moves returned duals, so it is an identity-breaking change for every pinned dual. R2's `duals_carried` work (C12–C13, `.superpowers/m7-w2-c10-report.md:192–194`) shows the dual-currency surface is delicate here. Supports: carry unless a W2 fix needs it. |
| M7-34 | Discriminated decline on a SINGULAR factorization status (D4) | Blocked in the record | ARGUED | `FactorizeOutcome::Status` has no singular status today; the MISS-branch catch was REMOVED at T7 fix round 3 for that reason. Supports: carry; unblocks only if a backend reports singularity as a status. |
| M7-35 | Stopping-mu vs bound geometry (the design anchor) | A1; not landed at this commit; C113 sequences it next | OWNED ELSEWHERE | Brief `:237–243`. Precondition M7-20. |
| M7-36 | `ipqp_converge_slack` as an adaptivity lever | A3; not landed | MEASURED | Via A3 (brief). TRAIN read per family against the shipped value: slack 1 = +47 iterations over 29 cells; 1e2 hand-off to tier 3 (`m6:1129`, `:1153`). |
| M7-37 | Complementarity-derived `mu_0`, fixed default as fallback; per-family defaults | A2; not landed | OWNED ELSEWHERE | Brief `:244–247`; papers only for the Mehrotra/PIQP rule (CLAUDE.md §6). |
| M7-38 | Sticky trial (NOT ADOPTED) and `kIpqpLadderSkipAfter` | A3; not landed | MEASURED | Via A3. The sticky trial stays NOT ADOPTED unless TRAIN reverses it (`m6:1090–1092`); `kIpqpLadderSkipAfter` is the lever to try. |
| M7-39 | Paper-vs-driver IC constants measurement | Not found worked (`m6:966`) | ARGUED | A measurement-instrument item with no consumer among the W2 packages; the A3 levers are the ones that read IPQP constants. Supports: fold into A3's read if it needs the constants, otherwise carry. |
| M7-40 | Barrier endgame at the guard (`ipqp_escape_numerical` is 0 on every population this project runs) | Not found worked | MEASURED (conditional) | Needs a purpose-built curvature/bound sweep; measured only if a population is built, as the brief says. No such population is recorded. If none is built by W2 close, it falls to CARRIED. |
| M7-41 | Restoration seed generalization beyond `clamp(x + p_elastic, box)` | Not found; the word "generalization" is in no ledger | ARGUED | Flag 3. What the record shows: the seed was reworked indirectly since (M2 design, resume rule; RST C109 diagnosis that BRITGAS's l1 model keeps ±sigma selector multipliers so every restoration QP is nonconvex). That changes the QP, not the seed. Supports: re-read after RST gates; carry if no restoration cell is seed-limited. Thin. |
| M7-42 | A certification fixture class that survives barrier-default moves | Brief puts the construction in A1 (`Sigma = 2 mu_stop / s^2`, brief `:242–243`) | ARGUED | The argument is in the brief: the gate-8 fixture stopped exercising its mechanism when a barrier default moved (`m6:1158–1159`), and the parametric rebuild from T4b's `Sigma = 2·mu_stop/s²` is the remedy (`m6:1186`). Supports: discharged by A1's fixture; carry only if A1 ships no fixture. |

## 4. What I did not find

- The register's own text (only ids and titles, in `.superpowers/`, outside the repo's
  tracked docs). The ids' origin text I could read is the D1–D34 quotation in
  `.superpowers/m7-planning-facts.md` §A.2.
- Any M7 ledger entry after 2026-09-17 in the tree, and any reference to a register id in
  `.superpowers/m7-w2-c10-report.md`.
- A closure record for H5 (M7-2), A1–A3 (M7-35…M7-38), or H7 (M7-16, M7-23).
- Any measurement of M7-6, -7, -9 (beyond the A-SQP zero-suspicion read), -11, -13, -14,
  -15, -20, -40 on TRAIN.

## 5. Rulings requested

1. M7-18: CARRIED, or ARGUED (the brief's silence).
2. M7-30: a task (and for which window), or CARRIED; it is not an argument item.
3. M7-25: close both halves, or keep R6 coverage open (flag 6).
4. M7-41: accept the thin argument or move it to CARRIED.
5. Whether A6's close line names M7-35 / M7-37 as owned (flag 2).
6. Order: M7-20's TRAIN read before A1, as the brief and A0 both require.
