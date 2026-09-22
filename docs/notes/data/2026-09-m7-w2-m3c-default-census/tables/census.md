# M7 W2 M3c — the DEFAULT-PATH failure census

**What kills every failing CUTEst TRAIN cell of the `hven-sqp` lane at DEFAULT
options.** Twenty cells: the eleven `error` cells, the one `infeasible` cell,
and the eight `dnf_wall` cells. Governed by
[`RULE.md`](../RULE.md), which was committed before any leg ran and is applied
here AS WRITTEN.

**This file is GENERATED** by `wrappers/census_table.py` from
`logs/classify-census.txt` (the two axes, machine-applied by
`wrappers/classify_m3c.py`, whose 60 fold proofs are in `logs/fold-classify.txt`)
and from the two committed identity transcripts. The prose sections are the
implementer's; every figure in them is printed by a generated section.

## What this is not

`RULE.md` §0 and §5.3 bind every line below. **A class name says WHICH PREDICATE
FIRED, never why a solve died.** `QP_CAP_BUDGET` says a walk exhausted its minor
cap and the driver's consecutive-failure budget refused another shrink-and-retry;
it says nothing about whether that walk was cycling, saddle-stuck,
ill-conditioned or merely large. That is pass 2's question, and M3b's two cells
landed in DIFFERENT causes with the same outward counters. Every label is printed
with the fields that produced it. **No outcome here licenses a behaviour change**
(`RULE.md` §10); §"Recommendation" is a recommendation to the settler and binds
nothing.

## Provenance

| | |
|---|---|
| tree | `/home/ghecht/Projects/hven`, branch `m7` |
| hven HEAD the legs ran at | `8e4c21a7` (`RULE.md` was written against `b2bbaed8`; `8e4c21a7` is this task's own three data commits on top of it, no `src/` or `include/` change) |
| harness pin the rows are compared against | `aeebdccc` |
| pass-1 chain | detached pid 2702409, 2026-09-21T17:56:43 → 19:42:07 |
| options | hven's own defaults + `common.threads = 1`, `common.print_level = 10` — the lane's `default` label, constructed by the probe and printed per leg in its `OPTIONS` block |
| terms | one solver process at a time, `MKL_NUM_THREADS=1`, `taskset -c 2`, the box lock per leg |
| wall-clock | INFORMATIONAL ONLY (CLAUDE.md §7). The two budgets are STOPS, not measurements |

## The identity gate, first

Twelve cells are identity-comparable against the harness's committed rows at the
pin — the eleven `error` cells and `YORKNET` — and **all twelve reproduce them on
all 24 compared fields**, `IDENTITY_DIFFERS=0`. None of M2, M2b, M3a or M3b moved
default behaviour on any of them, which `RULE.md` §4 existed to CHECK rather than
to assume.

Three group-W cells carry TRUNCATED rows and the gate reports them NOT COMPARABLE:
a `dnf_wall` row at the pin carries `status = null` and no counters, so there is
nothing to reproduce (§4.1). Five group-W cells have NO ROW at all.

**The all-cells gate returns `IDENTITY_VERDICT=STRUCTURAL_REFUSAL`, rc 2, and the
rule says what that is.** It is §4.3 (c)/(d) refusing the five absent rows — "a
missing row is a REFUSAL, never a skip" — and NOT §3.2's structural abort, whose
bar is "EVERY identity-comparable cell of groups E and I differs on the five core
fields". That bar did not fire: the groups-E+I gate over exactly those cells
returns `IDENTITY_VERDICT=ALL_COMPARABLE_CELLS_IDENTICAL`. The twelve PASSes
stand; the census has a five-cell hole in it and says so on every line.


## The census — one row per cell

`TERMINATOR` is `RULE.md` §5.2's axis-1 class, in the rule's own precedence.
`FLAGS FIRED` lists the axis-2 flags that returned `true`; every flag's own
numbers, including the ones that returned `false` or `N/A`, are in that cell's
block below. `CLOSENESS` is the objective read-out of §7.1 — the gap to §1.3's
consensus, machine-derived from the committed lane rows, `ABSENT` where §1.3
produces no consensus.

| cell | grp | identity | TERMINATOR | FLAGS FIRED | CLOSENESS (objective) |
|---|---|---|---|---|---|
| `ACOPP118` | E | PASS | `QP_CAP_BUDGET` | F5, F7 | rel `9.506132628038157e-05` |
| `ACOPP57` | E | PASS | `QP_CAP_BUDGET` | F5, F7 | rel `6.154914464999857e-06` |
| `ACOPR57` | E | PASS | `QP_CAP_BUDGET` | F5, F7 | rel `3.613006988323532e-07` |
| `BRITGAS` | E | PASS | `QP_CAP_BUDGET` | F5, F6, F7 | rel `0.07549433611895162` |
| `DALLASM` | E | PASS | `QP_CAP_BUDGET` | F5, F6, F7 | rel `1286.8339415768012` |
| `NET3` | E | PASS | `START_POINT_NONFINITE` | (none) | n/a (f is nan) |
| `STEENBRB` | E | PASS | `QP_CAP_BUDGET` | F1, F5, F6, F7 | rel `450740830.88175243` |
| `STEENBRC` | E | PASS | `QP_CAP_BUDGET` | F1, F5, F6, F7 | ABSENT (no consensus) |
| `STEENBRD` | E | PASS | `START_POINT_NONFINITE` | (none) | n/a (f is nan) |
| `STEENBRE` | E | PASS | `START_POINT_NONFINITE` | (none) | n/a (f is nan) |
| `STEENBRG` | E | PASS | `QP_CAP_BUDGET` | F1, F5, F6, F7 | ABSENT (no consensus) |
| `YORKNET` | I | PASS | `RESTORATION_EXIT(infeasible)` | F2, F6, F7 | rel `6.771790695759687` |
| `ACOPR118` | W | NOT_COMPARABLE | `WALL_TRUNCATED` | F5, F7 | rel `0.0005930523384513883` |
| `BIGBANK` | W | NOT_COMPARABLE | `WALL_TRUNCATED` | (none) | ABSENT (no consensus) |
| `MSS2` | W | REFUSED | **CENSORED** | — | — (no row: RULE.md §3.1) |
| `MSS3` | W | REFUSED | **CENSORED** | — | — (no row: RULE.md §3.1) |
| `READING8` | W | REFUSED | **CENSORED** | — | — (no row: RULE.md §3.1) |
| `SAWPATH` | W | NOT_COMPARABLE | `WALL_TRUNCATED` | F2, F5, F6, F7 | rel `0.9763821095558931` |
| `TAX213322` | W | REFUSED | **CENSORED** | — | — (no row: RULE.md §3.1) |
| `TAX53322` | W | REFUSED | **CENSORED** | — | — (no row: RULE.md §3.1) |

**The five CENSORED cells are never filled.** `MSS2`, `MSS3`, `READING8`,
`TAX213322` and `TAX53322` were killed by the outer `timeout 900`; `RULE.md`
§3.1 and §5.2 class 1 forbid filling a row for them later or reconstructing one.
This is the expected outcome and not a surprise: the harness's own SQP cost
census (C1) already showed these five completing no major inside 1800 s. What
the census can say about them is exactly this sentence.


## Per cell — the evidence behind each line

### `ACOPP118`  (group E, identity PASS)

* **TERMINATOR** `QP_CAP_BUDGET` — numerical_error; the last history row's QP ended max_iter; qp_failures_in_a_row_peak == qp_failure_budget
* **EVIDENCE** `status=numerical_error deadline_fired=0 major_iters=7 factorizations=20501 f=129648.67425337316 qp_failures_in_a_row_peak=3 qp_failure_budget=3 restoration_entries=0 restoration_sub_status=not_run max_iter_option=100 last_qp_solved=1 last_qp_status=max_iter last_elastic_applied=0 last_verdict=Reject last_tr_radius=0.25`
* **CHAIN OF DEATH** `7 major row(s), last 6 shown: #1 tr=2 f=1.284e+05 h=4.758 kkt=4441 qp=optimal/107m -> AcceptH  #2 tr=2 f=1.287e+05 h=1.542 kkt=675.3 qp=optimal/20m -> AcceptH  #3 tr=2 f=1.296e+05 h=0.03076 kkt=25.15 qp=max_iter/4710m -> Reject  #4 tr=1 f=1.296e+05 h=0.03076 kkt=25.15 qp=max_iter/4710m -> Reject  #5 tr=0.5 f=1.296e+05 h=0.03076 kkt=25.15 qp=max_iter/4710m -> Reject  #6 tr=0.25 f=1.296e+05 h=0.03076 kkt=25.15 qp=max_iter/4710m -> Reject`
* **F1_RESTORATION_EXCURSION = N/A   [N/A: restoration_entries == 0]**
* **F2_RESTORATION_STUCK = N/A   [N/A: restoration_entries == 0]**
* **F3_TR_AT_FLOOR = false   [last row's tr_radius=0.25 against tr_min/0.5=2e-10]**
* **F4_REJECTION_STALL = false   [steps_accepted=3 judged_rejections=0 (= rejected_steps 3 - qp_failures_routed 3)]**
* **F5_CAP_CHURN = true   [qp_maxiter_exits=4 qp_cap_retries=0 qp_minor_iters=19917]**
* **F6_ELASTIC_ENGAGED = false   [elastic_activations=0 elastic_escalations=0 elastic_rho0_ceiling_hits=0 elastic_floor_retries=0]**
* **F7_WARM_FAILURES = true   [failing ledger records: warm=4 cold=0 of 7 records; first=3 last=6 (MAIN WALK ENGINE ONLY)]**
* **CONSENSUS** consensus 1.29661e+05 (6 significant digits): lanes hven-ipm, ipopt, snopt
* **LANE OBJECTIVES** `hven-ipm=129660.68611633655; ipopt=129660.68612024593; snopt=129660.6861968658`
* **CLOSENESS (objective)** `f=129648.67425337316 f*=129661.0 |f-f*|=12.325746626840555 rel=9.506132628038157e-05`
* **CLOSENESS (feasibility)** `max(feasibility_e, feasibility_i)=0.006540317377440985 feas_tol=1e-06 ratio=6540.317377440985`
* **COST vs SNOPT** hven majors=7 minors=19917 | SNOPT majors=67 minors=823 | hven factorizations=20501 has NO SNOPT counterpart (SNOPT publishes no factorization count)
* **LEDGER `QpStatus` SEQUENCE** (MAIN WALK ENGINE ONLY — `RULE.md` §2.2; the restoration sub-driver and the lazy engines are never handed the ledger, so this is a LOWER BOUND on the solve's QP solves)

      0:optimal(cold)[m=950,deg=15,dt=0,rt=14,+240,-355] 1:optimal(warm)[m=107,deg=0,dt=0,rt=0,+17,-44] 2:optimal(warm)[m=20,deg=0,dt=0,rt=0,+4,-7] 3:max_iter(warm)[m=4710,deg=2305,dt=0,rt=0,+2402,-2305] 4:max_iter(warm)[m=4710,deg=2303,dt=0,rt=0,+2399,-2306] 5:max_iter(warm)[m=4710,deg=2303,dt=0,rt=0,+2399,-2306] 6:max_iter(warm)[m=4710,deg=2303,dt=0,rt=0,+2399,-2306]

* **ledger print cap** `cap=5000 printed=7 truncated=0`

### `ACOPP57`  (group E, identity PASS)

* **TERMINATOR** `QP_CAP_BUDGET` — numerical_error; the last history row's QP ended max_iter; qp_failures_in_a_row_peak == qp_failure_budget
* **EVIDENCE** `status=numerical_error deadline_fired=0 major_iters=7 factorizations=128 f=41737.543107411046 qp_failures_in_a_row_peak=3 qp_failure_budget=3 restoration_entries=0 restoration_sub_status=not_run max_iter_option=100 last_qp_solved=1 last_qp_status=max_iter last_elastic_applied=0 last_verdict=Reject last_tr_radius=0.125`
* **CHAIN OF DEATH** `7 major row(s), last 6 shown: #1 tr=1 f=4.106e+04 h=1.345 kkt=4820 qp=optimal/15m -> AcceptH  #2 tr=1 f=4.166e+04 h=0.1199 kkt=138.9 qp=optimal/2m -> AcceptH  #3 tr=1 f=4.174e+04 h=0.0004171 kkt=0.6386 qp=max_iter/1795m -> Reject  #4 tr=0.5 f=4.174e+04 h=0.0004171 kkt=0.6386 qp=max_iter/1795m -> Reject  #5 tr=0.25 f=4.174e+04 h=0.0004171 kkt=0.6386 qp=max_iter/1795m -> Reject  #6 tr=0.125 f=4.174e+04 h=0.0004171 kkt=0.6386 qp=max_iter/1795m -> Reject`
* **F1_RESTORATION_EXCURSION = N/A   [N/A: restoration_entries == 0]**
* **F2_RESTORATION_STUCK = N/A   [N/A: restoration_entries == 0]**
* **F3_TR_AT_FLOOR = false   [last row's tr_radius=0.125 against tr_min/0.5=2e-10]**
* **F4_REJECTION_STALL = false   [steps_accepted=3 judged_rejections=0 (= rejected_steps 3 - qp_failures_routed 3)]**
* **F5_CAP_CHURN = true   [qp_maxiter_exits=4 qp_cap_retries=0 qp_minor_iters=7248]**
* **F6_ELASTIC_ENGAGED = false   [elastic_activations=0 elastic_escalations=0 elastic_rho0_ceiling_hits=0 elastic_floor_retries=0]**
* **F7_WARM_FAILURES = true   [failing ledger records: warm=4 cold=0 of 7 records; first=3 last=6 (MAIN WALK ENGINE ONLY)]**
* **CONSENSUS** consensus 4.17378e+04 (6 significant digits): lanes hven-ipm, ipopt, snopt
* **LANE OBJECTIVES** `hven-ipm=41737.786835841674; ipopt=41737.78683618633; snopt=41737.78684971186`
* **CLOSENESS (objective)** `f=41737.543107411046 f*=41737.8 |f-f*|=0.2568925889572711 rel=6.154914464999857e-06`
* **CLOSENESS (feasibility)** `max(feasibility_e, feasibility_i)=0.0001138587167886368 feas_tol=1e-06 ratio=113.8587167886368`
* **COST vs SNOPT** hven majors=7 minors=7248 | SNOPT majors=21 minors=128 | hven factorizations=128 has NO SNOPT counterpart (SNOPT publishes no factorization count)
* **LEDGER `QpStatus` SEQUENCE** (MAIN WALK ENGINE ONLY — `RULE.md` §2.2; the restoration sub-driver and the lazy engines are never handed the ledger, so this is a LOWER BOUND on the solve's QP solves)

      0:optimal(cold)[m=51,deg=3,dt=0,rt=2,+20,-13] 1:optimal(warm)[m=15,deg=0,dt=0,rt=0,+1,-6] 2:optimal(warm)[m=2,deg=0,dt=0,rt=0,+0,-0] 3:max_iter(warm)[m=1795,deg=890,dt=0,rt=0,+901,-890] 4:max_iter(warm)[m=1795,deg=891,dt=0,rt=0,+899,-892] 5:max_iter(warm)[m=1795,deg=887,dt=0,rt=0,+900,-888] 6:max_iter(warm)[m=1795,deg=884,dt=0,rt=0,+903,-885]

* **ledger print cap** `cap=5000 printed=7 truncated=0`

### `ACOPR57`  (group E, identity PASS)

* **TERMINATOR** `QP_CAP_BUDGET` — numerical_error; the last history row's QP ended max_iter; qp_failures_in_a_row_peak == qp_failure_budget
* **EVIDENCE** `status=numerical_error deadline_fired=0 major_iters=15 factorizations=4848 f=41737.784920103695 qp_failures_in_a_row_peak=3 qp_failure_budget=3 restoration_entries=0 restoration_sub_status=not_run max_iter_option=100 last_qp_solved=1 last_qp_status=max_iter last_elastic_applied=0 last_verdict=Reject last_tr_radius=0.015625`
* **CHAIN OF DEATH** `12 major row(s), last 6 shown: #9 tr=0.25 f=4.174e+04 h=1.014 kkt=18.05 qp=max_iter/2080m -> Reject  #10 tr=0.125 f=4.174e+04 h=1.014 kkt=18.05 qp=optimal/102m -> AcceptH  #11 tr=0.125 f=4.174e+04 h=0.2574 kkt=1.017 qp=max_iter/2080m -> Reject  #12 tr=0.0625 f=4.174e+04 h=0.2574 kkt=1.017 qp=max_iter/2080m -> Reject  #13 tr=0.03125 f=4.174e+04 h=0.2574 kkt=1.017 qp=max_iter/2080m -> Reject  #14 tr=0.01562 f=4.174e+04 h=0.2574 kkt=1.017 qp=max_iter/2080m -> Reject`
* **F1_RESTORATION_EXCURSION = N/A   [N/A: restoration_entries == 0]**
* **F2_RESTORATION_STUCK = N/A   [N/A: restoration_entries == 0]**
* **F3_TR_AT_FLOOR = false   [last row's tr_radius=0.015625 against tr_min/0.5=2e-10]**
* **F4_REJECTION_STALL = false   [steps_accepted=8 judged_rejections=2 (= rejected_steps 6 - qp_failures_routed 4)]**
* **F5_CAP_CHURN = true   [qp_maxiter_exits=5 qp_cap_retries=0 qp_minor_iters=13874]**
* **F6_ELASTIC_ENGAGED = false   [elastic_activations=0 elastic_escalations=0 elastic_rho0_ceiling_hits=0 elastic_floor_retries=0]**
* **F7_WARM_FAILURES = true   [failing ledger records: warm=6 cold=0 of 17 records; first=4 last=16 (MAIN WALK ENGINE ONLY)]**
* **CONSENSUS** consensus 4.17378e+04 (6 significant digits): lanes ipopt, snopt
* **LANE OBJECTIVES** `hven-ipm=NOT COUNTED; ipopt=41737.786029012874; snopt=41737.78654591297`
* **CLOSENESS (objective)** `f=41737.784920103695 f*=41737.8 |f-f*|=0.015079896307724994 rel=3.613006988323532e-07`
* **CLOSENESS (feasibility)** `max(feasibility_e, feasibility_i)=0.05305494283147438 feas_tol=1e-06 ratio=53054.942831474385`
* **COST vs SNOPT** hven majors=15 minors=13874 | SNOPT majors=22 minors=141 | hven factorizations=4848 has NO SNOPT counterpart (SNOPT publishes no factorization count)
* **LEDGER `QpStatus` SEQUENCE** (MAIN WALK ENGINE ONLY — `RULE.md` §2.2; the restoration sub-driver and the lazy engines are never handed the ledger, so this is a LOWER BOUND on the solve's QP solves)

      0:optimal(cold)[m=44,deg=3,dt=0,rt=2,+20,-11] 1:optimal(warm)[m=139,deg=0,dt=0,rt=0,+9,-64] 2:optimal(warm)[m=156,deg=0,dt=0,rt=0,+14,-70] 3:optimal(warm)[m=155,deg=3,dt=0,rt=2,+25,-64] 4:max_iter(warm)[m=2080,deg=957,dt=0,rt=4,+1012,-1008] 5:optimal(warm)[m=139,deg=5,dt=0,rt=3,+20,-58] 6:optimal(warm)[m=198,deg=5,dt=0,rt=2,+27,-84] 7:optimal(warm)[m=172,deg=0,dt=0,rt=0,+20,-75] 8:optimal(warm)[m=96,deg=8,dt=0,rt=6,+26,-34] 9:infeasible(warm)[m=111,deg=4,dt=0,rt=4,+46,-31] 10:optimal(warm)[m=82,deg=8,dt=0,rt=6,+21,-29] 11:max_iter(warm)[m=2080,deg=47,dt=0,rt=6,+714,-682] 12:optimal(warm)[m=102,deg=10,dt=0,rt=7,+21,-39] 13:max_iter(warm)[m=2080,deg=548,dt=0,rt=6,+709,-685] 14:max_iter(warm)[m=2080,deg=1025,dt=0,rt=5,+1051,-1022] 15:max_iter(warm)[m=2080,deg=1019,dt=0,rt=4,+1041,-1023] 16:max_iter(warm)[m=2080,deg=680,dt=0,rt=7,+728,-675]

* **ledger print cap** `cap=5000 printed=17 truncated=0`

### `BRITGAS`  (group E, identity PASS)

* **TERMINATOR** `QP_CAP_BUDGET` — numerical_error; the last history row's QP ended max_iter; qp_failures_in_a_row_peak == qp_failure_budget
* **EVIDENCE** `status=numerical_error deadline_fired=0 major_iters=4 factorizations=93749 f=0.075494336118951622 qp_failures_in_a_row_peak=3 qp_failure_budget=3 restoration_entries=1 restoration_sub_status=numerical_error max_iter_option=100 last_qp_solved=1 last_qp_status=max_iter last_elastic_applied=1 last_verdict=Restore last_tr_radius=0.125`
* **CHAIN OF DEATH** `4 major row(s), last 4 shown: #0 tr=1 f=0 h=115 kkt=2 qp=max_iter/4500m -> Reject  #1 tr=0.5 f=0 h=115 kkt=2 qp=max_iter/4500m -> Reject  #2 tr=0.25 f=0 h=115 kkt=2 qp=max_iter/4500m -> Reject  #3 tr=0.125 f=0 h=115 kkt=2 qp=max_iter/6980m/elastic -> Restore`
* **F1_RESTORATION_EXCURSION = false   [sub_status=numerical_error h_in=115.03999999999996 h_out=110.10740647336283 feas_tol=1e-06 f_in=0.0 f_out=0.07549433611895162 (f_out-f_in)/max(1,|f_in|)=0.07549433611895162]**
* **F2_RESTORATION_STUCK = false   [sub_status=numerical_error h_in=115.03999999999996 h_out=110.10740647336283 h_out/h_in=0.9571227961870903]**
* **F3_TR_AT_FLOOR = false   [last row's tr_radius=0.125 against tr_min/0.5=2e-10]**
* **F4_REJECTION_STALL = false   [steps_accepted=0 judged_rejections=0 (= rejected_steps 3 - qp_failures_routed 3)]**
* **F5_CAP_CHURN = true   [qp_maxiter_exits=9 qp_cap_retries=0 qp_minor_iters=92309]**
* **F6_ELASTIC_ENGAGED = true   [elastic_activations=1 elastic_escalations=0 elastic_rho0_ceiling_hits=0 elastic_floor_retries=0]**
* **F7_WARM_FAILURES = true   [failing ledger records: warm=3 cold=1 of 5 records; first=0 last=4 (MAIN WALK ENGINE ONLY)]**
* **CONSENSUS** consensus 0 (the |f| <= 1e-06 arm): lanes hven-ipm, snopt
* **LANE OBJECTIVES** `hven-ipm=-4.101893161782277e-07; ipopt=NOT COUNTED; snopt=0.0`
* **CLOSENESS (objective)** `f=0.07549433611895162 f*=0.0 |f-f*|=0.07549433611895162 rel=0.07549433611895162`
* **CLOSENESS (feasibility)** `max(feasibility_e, feasibility_i)=1.998283573986877 feas_tol=1e-06 ratio=1998283.573986877`
* **COST vs SNOPT** hven majors=4 minors=92309 | SNOPT majors=12 minors=793 | hven factorizations=93749 has NO SNOPT counterpart (SNOPT publishes no factorization count)
* **LEDGER `QpStatus` SEQUENCE** (MAIN WALK ENGINE ONLY — `RULE.md` §2.2; the restoration sub-driver and the lazy engines are never handed the ledger, so this is a LOWER BOUND on the solve's QP solves)

      0:max_iter(cold)[m=4500,deg=2154,dt=0,rt=11,+2305,-2168] 1:max_iter(warm)[m=4500,deg=2146,dt=0,rt=0,+2316,-2165] 2:max_iter(warm)[m=4500,deg=1,dt=0,rt=469,+1624,-1438] 3:infeasible(warm)[m=414,deg=0,dt=2,rt=0,+246,-84] 4:max_iter(warm)[m=6980,deg=3132,dt=42,rt=265,+3508,-3171]

* **ledger print cap** `cap=5000 printed=5 truncated=0`

### `DALLASM`  (group E, identity PASS)

* **TERMINATOR** `QP_CAP_BUDGET` — numerical_error; the last history row's QP ended max_iter; qp_failures_in_a_row_peak == qp_failure_budget
* **EVIDENCE** `status=numerical_error deadline_fired=0 major_iters=5 factorizations=4919 f=61974881.482906975 qp_failures_in_a_row_peak=3 qp_failure_budget=3 restoration_entries=1 restoration_sub_status=optimal max_iter_option=100 last_qp_solved=1 last_qp_status=max_iter last_elastic_applied=0 last_verdict=Reject last_tr_radius=0.0125`
* **CHAIN OF DEATH** `5 major row(s), last 5 shown: #0 tr=1 f=6.197e+07 h=12.46 kkt=8.947e+04 qp=max_iter/2150m/elastic -> Restore  #1 tr=0.1 f=6.197e+07 h=3.254e-13 kkt=8.947e+04 qp=max_iter/1960m -> Reject  #2 tr=0.05 f=6.197e+07 h=3.254e-13 kkt=8.947e+04 qp=max_iter/1960m -> Reject  #3 tr=0.025 f=6.197e+07 h=3.254e-13 kkt=8.947e+04 qp=max_iter/1960m -> Reject  #4 tr=0.0125 f=6.197e+07 h=3.254e-13 kkt=8.947e+04 qp=max_iter/1960m -> Reject`
* **F1_RESTORATION_EXCURSION = false   [sub_status=optimal h_in=12.456000000000556 h_out=3.2536720903635513e-13 feas_tol=1e-06 f_in=61971710.58868844 f_out=61974881.482906975 (f_out-f_in)/max(1,|f_in|)=5.1166801568214376e-05]**
* **F2_RESTORATION_STUCK = false   [sub_status=optimal h_in=12.456000000000556 h_out=3.2536720903635513e-13 h_out/h_in=2.6121323782622077e-14]**
* **F3_TR_AT_FLOOR = false   [last row's tr_radius=0.0125 against tr_min/0.5=2e-10]**
* **F4_REJECTION_STALL = false   [steps_accepted=0 judged_rejections=0 (= rejected_steps 3 - qp_failures_routed 3)]**
* **F5_CAP_CHURN = true   [qp_maxiter_exits=4 qp_cap_retries=0 qp_minor_iters=10583]**
* **F6_ELASTIC_ENGAGED = true   [elastic_activations=1 elastic_escalations=0 elastic_rho0_ceiling_hits=0 elastic_floor_retries=0]**
* **F7_WARM_FAILURES = true   [failing ledger records: warm=4 cold=1 of 6 records; first=1 last=5 (MAIN WALK ENGINE ONLY)]**
* **CONSENSUS** consensus -4.81982e+04 (6 significant digits): lanes hven-ipm, ipopt, snopt
* **LANE OBJECTIVES** `hven-ipm=-48198.18823051242; ipopt=-48198.188225705584; snopt=-48198.18817685391`
* **CLOSENESS (objective)** `f=61974881.482906975 f*=-48198.2 |f-f*|=62023079.68290698 rel=1286.8339415768012`
* **CLOSENESS (feasibility)** `max(feasibility_e, feasibility_i)=4.496403249731884e-14 feas_tol=1e-06 ratio=4.496403249731884e-08`
* **COST vs SNOPT** hven majors=5 minors=10583 | SNOPT majors=104 minors=235 | hven factorizations=4919 has NO SNOPT counterpart (SNOPT publishes no factorization count)
* **LEDGER `QpStatus` SEQUENCE** (MAIN WALK ENGINE ONLY — `RULE.md` §2.2; the restoration sub-driver and the lazy engines are never handed the ledger, so this is a LOWER BOUND on the solve's QP solves)

      0:infeasible(cold)[m=232,deg=2,dt=0,rt=1,+108,-61] 1:max_iter(warm)[m=2150,deg=1020,dt=0,rt=19,+1118,-1019] 2:max_iter(cold)[m=1960,deg=936,dt=0,rt=1,+1021,-936] 3:max_iter(warm)[m=1960,deg=958,dt=0,rt=464,+1023,-932] 4:max_iter(warm)[m=1960,deg=961,dt=0,rt=0,+1023,-932] 5:max_iter(warm)[m=1960,deg=959,dt=0,rt=0,+1023,-932]

* **ledger print cap** `cap=5000 printed=6 truncated=0`

### `NET3`  (group E, identity PASS)

* **TERMINATOR** `START_POINT_NONFINITE` — numerical_error with 0 majors, 0 factorizations and a nan objective
* **EVIDENCE** `status=numerical_error deadline_fired=0 major_iters=0 factorizations=0 f=-nan qp_failures_in_a_row_peak=0 qp_failure_budget=3 restoration_entries=0 restoration_sub_status=not_run max_iter_option=100 last_qp_solved=0 last_qp_status=n/a(qp_solved=0) last_elastic_applied=0 last_verdict=Reject last_tr_radius=1.0`
* **CHAIN OF DEATH** `1 major row(s), last 1 shown: #0 tr=1 f=-nan h=-nan kkt=nan qp=no-qp/0m -> Reject`
* **F1_RESTORATION_EXCURSION = N/A   [N/A: restoration_entries == 0]**
* **F2_RESTORATION_STUCK = N/A   [N/A: restoration_entries == 0]**
* **F3_TR_AT_FLOOR = false   [last row's tr_radius=1.0 against tr_min/0.5=2e-10]**
* **F4_REJECTION_STALL = false   [steps_accepted=0 judged_rejections=0 (= rejected_steps 0 - qp_failures_routed 0)]**
* **F5_CAP_CHURN = false   [qp_maxiter_exits=0 qp_cap_retries=0 qp_minor_iters=0]**
* **F6_ELASTIC_ENGAGED = false   [elastic_activations=0 elastic_escalations=0 elastic_rho0_ceiling_hits=0 elastic_floor_retries=0]**
* **F7_WARM_FAILURES = N/A   [N/A: the ledger holds no record]**
* **CONSENSUS** ABSENT (no consensus): 0 counted lane(s)
* **LANE OBJECTIVES** `hven-ipm=NOT COUNTED; ipopt=NOT COUNTED; snopt=NOT COUNTED`
* **CLOSENESS (objective)** `n/a (f is nan)`
* **CLOSENESS (feasibility)** `max(feasibility_e, feasibility_i)=nan feas_tol=1e-06 ratio=nan`
* **COST vs SNOPT** n/a (SNOPT did not solve this cell, or has no counter row)
* **LEDGER `QpStatus` SEQUENCE** (MAIN WALK ENGINE ONLY — `RULE.md` §2.2; the restoration sub-driver and the lazy engines are never handed the ledger, so this is a LOWER BOUND on the solve's QP solves)

      (the ledger holds no record)

* **ledger print cap** `cap=5000 printed=0 truncated=0`

### `STEENBRB`  (group E, identity PASS)

* **TERMINATOR** `QP_CAP_BUDGET` — numerical_error; the last history row's QP ended max_iter; qp_failures_in_a_row_peak == qp_failure_budget
* **EVIDENCE** `status=numerical_error deadline_fired=0 major_iters=5 factorizations=25973 f=4090860686442.3218 qp_failures_in_a_row_peak=3 qp_failure_budget=3 restoration_entries=1 restoration_sub_status=optimal max_iter_option=100 last_qp_solved=1 last_qp_status=max_iter last_elastic_applied=0 last_verdict=Reject last_tr_radius=0.0125`
* **CHAIN OF DEATH** `5 major row(s), last 5 shown: #0 tr=1 f=2859 h=1.98e+04 kkt=3454 qp=numerical_error/414m/elastic -> Restore  #1 tr=0.1 f=4.091e+12 h=1.63e-12 kkt=2.504e+13 qp=max_iter/4680m -> Reject  #2 tr=0.05 f=4.091e+12 h=1.63e-12 kkt=2.504e+13 qp=max_iter/4680m -> Reject  #3 tr=0.025 f=4.091e+12 h=1.63e-12 kkt=2.504e+13 qp=max_iter/4680m -> Reject  #4 tr=0.0125 f=4.091e+12 h=1.63e-12 kkt=2.504e+13 qp=max_iter/4680m -> Reject`
* **F1_RESTORATION_EXCURSION = true   [sub_status=optimal h_in=19800.0 h_out=1.6303669053986523e-12 feas_tol=1e-06 f_in=2858.7861353626763 f_out=4090860686442.322 (f_out-f_in)/max(1,|f_in|)=1430978215.8869166]**
* **F2_RESTORATION_STUCK = false   [sub_status=optimal h_in=19800.0 h_out=1.6303669053986523e-12 h_out/h_in=8.234176289892183e-17]**
* **F3_TR_AT_FLOOR = false   [last row's tr_radius=0.0125 against tr_min/0.5=2e-10]**
* **F4_REJECTION_STALL = false   [steps_accepted=0 judged_rejections=0 (= rejected_steps 3 - qp_failures_routed 3)]**
* **F5_CAP_CHURN = true   [qp_maxiter_exits=4 qp_cap_retries=0 qp_minor_iters=20637]**
* **F6_ELASTIC_ENGAGED = true   [elastic_activations=1 elastic_escalations=0 elastic_rho0_ceiling_hits=0 elastic_floor_retries=0]**
* **F7_WARM_FAILURES = true   [failing ledger records: warm=4 cold=1 of 6 records; first=1 last=5 (MAIN WALK ENGINE ONLY)]**
* **CONSENSUS** consensus 9.07586e+03 (6 significant digits): lanes hven-ipm, ipopt, snopt
* **LANE OBJECTIVES** `hven-ipm=9075.855373463924; ipopt=9075.855386577745; snopt=9075.855378231188`
* **CLOSENESS (objective)** `f=4090860686442.322 f*=9075.86 |f-f*|=4090860677366.462 rel=450740830.88175243`
* **CLOSENESS (feasibility)** `max(feasibility_e, feasibility_i)=2.2737367542337972e-13 feas_tol=1e-06 ratio=2.2737367542337972e-07`
* **COST vs SNOPT** hven majors=5 minors=20637 | SNOPT majors=177 minors=3706 | hven factorizations=25973 has NO SNOPT counterpart (SNOPT publishes no factorization count)
* **LEDGER `QpStatus` SEQUENCE** (MAIN WALK ENGINE ONLY — `RULE.md` §2.2; the restoration sub-driver and the lazy engines are never handed the ledger, so this is a LOWER BOUND on the solve's QP solves)

      0:infeasible(cold)[m=827,deg=18,dt=19,rt=10,+587,-225] 1:numerical_error(warm)[m=414,deg=32,dt=14,rt=30,+244,-146] 2:max_iter(cold)[m=4680,deg=1399,dt=0,rt=2471,+2528,-2151] 3:max_iter(warm)[m=4680,deg=1168,dt=7,rt=1166,+2370,-2309] 4:max_iter(warm)[m=4680,deg=2270,dt=0,rt=4,+2391,-2272] 5:max_iter(warm)[m=4680,deg=2274,dt=0,rt=5,+2392,-2277]

* **ledger print cap** `cap=5000 printed=6 truncated=0`

### `STEENBRC`  (group E, identity PASS)

* **TERMINATOR** `QP_CAP_BUDGET` — numerical_error; the last history row's QP ended max_iter; qp_failures_in_a_row_peak == qp_failure_budget
* **EVIDENCE** `status=numerical_error deadline_fired=0 major_iters=6 factorizations=42946 f=183453796271334.12 qp_failures_in_a_row_peak=3 qp_failure_budget=3 restoration_entries=1 restoration_sub_status=optimal max_iter_option=100 last_qp_solved=1 last_qp_status=max_iter last_elastic_applied=0 last_verdict=Reject last_tr_radius=0.0125`
* **CHAIN OF DEATH** `6 major row(s), last 6 shown: #0 tr=1 f=4528 h=4.18e+04 kkt=1e+04 qp=max_iter/5400m -> Reject  #1 tr=0.5 f=4528 h=4.18e+04 kkt=1e+04 qp=max_iter/5680m/elastic -> Restore  #2 tr=0.1 f=1.835e+14 h=4.89e-12 kkt=1.568e+15 qp=max_iter/5400m -> Reject  #3 tr=0.05 f=1.835e+14 h=4.89e-12 kkt=1.568e+15 qp=max_iter/5400m -> Reject  #4 tr=0.025 f=1.835e+14 h=4.89e-12 kkt=1.568e+15 qp=max_iter/5400m -> Reject  #5 tr=0.0125 f=1.835e+14 h=4.89e-12 kkt=1.568e+15 qp=max_iter/5400m -> Reject`
* **F1_RESTORATION_EXCURSION = true   [sub_status=optimal h_in=41800.0 h_out=4.890230408867829e-12 feas_tol=1e-06 f_in=4528.306135362676 f_out=183453796271334.12 (f_out-f_in)/max(1,|f_in|)=40512675332.210686]**
* **F2_RESTORATION_STUCK = false   [sub_status=optimal h_in=41800.0 h_out=4.890230408867829e-12 h_out/h_in=1.1699115810688585e-16]**
* **F3_TR_AT_FLOOR = false   [last row's tr_radius=0.0125 against tr_min/0.5=2e-10]**
* **F4_REJECTION_STALL = false   [steps_accepted=0 judged_rejections=0 (= rejected_steps 4 - qp_failures_routed 4)]**
* **F5_CAP_CHURN = true   [qp_maxiter_exits=5 qp_cap_retries=0 qp_minor_iters=35324]**
* **F6_ELASTIC_ENGAGED = true   [elastic_activations=1 elastic_escalations=5 elastic_rho0_ceiling_hits=0 elastic_floor_retries=0]**
* **F7_WARM_FAILURES = true   [failing ledger records: warm=4 cold=2 of 12 records; first=0 last=11 (MAIN WALK ENGINE ONLY)]**
* **CONSENSUS** ABSENT (no consensus): counted lanes hven-ipm=2.84286e+04, ipopt=2.85195e+04 agree nowhere to 6 significant digits
* **LANE OBJECTIVES** `hven-ipm=28428.572887358107; ipopt=28519.516906799814; snopt=NOT COUNTED`
* **CLOSENESS (objective)** `ABSENT (no consensus)`
* **CLOSENESS (feasibility)** `max(feasibility_e, feasibility_i)=9.094947017729282e-13 feas_tol=1e-06 ratio=9.094947017729282e-07`
* **COST vs SNOPT** n/a (SNOPT did not solve this cell, or has no counter row)
* **LEDGER `QpStatus` SEQUENCE** (MAIN WALK ENGINE ONLY — `RULE.md` §2.2; the restoration sub-driver and the lazy engines are never handed the ledger, so this is a LOWER BOUND on the solve's QP solves)

      0:max_iter(cold)[m=5400,deg=1176,dt=4,rt=1166,+2909,-2467] 1:infeasible(warm)[m=515,deg=55,dt=12,rt=15,+321,-169] 2:optimal(warm)[m=438,deg=50,dt=25,rt=30,+261,-153] 3:optimal(warm)[m=833,deg=156,dt=50,rt=233,+412,-420] 4:optimal(warm)[m=3,deg=0,dt=0,rt=0,+1,-1] 5:optimal(warm)[m=3,deg=0,dt=0,rt=0,+1,-1] 6:optimal(warm)[m=23,deg=9,dt=2,rt=0,+11,-11] 7:max_iter(warm)[m=5680,deg=2576,dt=8,rt=2570,+2589,-2579] 8:max_iter(cold)[m=5400,deg=2834,dt=0,rt=411,+2924,-2475] 9:max_iter(warm)[m=5400,deg=2651,dt=0,rt=2645,+2738,-2661] 10:max_iter(warm)[m=5400,deg=2651,dt=0,rt=14,+2761,-2629] 11:max_iter(warm)[m=5400,deg=2633,dt=0,rt=3,+2763,-2629]

* **ledger print cap** `cap=5000 printed=12 truncated=0`

### `STEENBRD`  (group E, identity PASS)

* **TERMINATOR** `START_POINT_NONFINITE` — numerical_error with 0 majors, 0 factorizations and a nan objective
* **EVIDENCE** `status=numerical_error deadline_fired=0 major_iters=0 factorizations=0 f=-nan qp_failures_in_a_row_peak=0 qp_failure_budget=3 restoration_entries=0 restoration_sub_status=not_run max_iter_option=100 last_qp_solved=0 last_qp_status=n/a(qp_solved=0) last_elastic_applied=0 last_verdict=Reject last_tr_radius=1.0`
* **CHAIN OF DEATH** `1 major row(s), last 1 shown: #0 tr=1 f=-nan h=1.98e+04 kkt=nan qp=no-qp/0m -> Reject`
* **F1_RESTORATION_EXCURSION = N/A   [N/A: restoration_entries == 0]**
* **F2_RESTORATION_STUCK = N/A   [N/A: restoration_entries == 0]**
* **F3_TR_AT_FLOOR = false   [last row's tr_radius=1.0 against tr_min/0.5=2e-10]**
* **F4_REJECTION_STALL = false   [steps_accepted=0 judged_rejections=0 (= rejected_steps 0 - qp_failures_routed 0)]**
* **F5_CAP_CHURN = false   [qp_maxiter_exits=0 qp_cap_retries=0 qp_minor_iters=0]**
* **F6_ELASTIC_ENGAGED = false   [elastic_activations=0 elastic_escalations=0 elastic_rho0_ceiling_hits=0 elastic_floor_retries=0]**
* **F7_WARM_FAILURES = N/A   [N/A: the ledger holds no record]**
* **CONSENSUS** consensus 9.03008e+03 (6 significant digits): lanes hven-ipm, snopt
* **LANE OBJECTIVES** `hven-ipm=9030.081764375458; ipopt=NOT COUNTED; snopt=9030.081867605188`
* **CLOSENESS (objective)** `n/a (f is nan)`
* **CLOSENESS (feasibility)** `max(feasibility_e, feasibility_i)=nan feas_tol=1e-06 ratio=nan`
* **COST vs SNOPT** hven majors=0 minors=0 | SNOPT majors=155 minors=2899 | hven factorizations=0 has NO SNOPT counterpart (SNOPT publishes no factorization count)
* **LEDGER `QpStatus` SEQUENCE** (MAIN WALK ENGINE ONLY — `RULE.md` §2.2; the restoration sub-driver and the lazy engines are never handed the ledger, so this is a LOWER BOUND on the solve's QP solves)

      (the ledger holds no record)

* **ledger print cap** `cap=5000 printed=0 truncated=0`

### `STEENBRE`  (group E, identity PASS)

* **TERMINATOR** `START_POINT_NONFINITE` — numerical_error with 0 majors, 0 factorizations and a nan objective
* **EVIDENCE** `status=numerical_error deadline_fired=0 major_iters=0 factorizations=0 f=-nan qp_failures_in_a_row_peak=0 qp_failure_budget=3 restoration_entries=0 restoration_sub_status=not_run max_iter_option=100 last_qp_solved=0 last_qp_status=n/a(qp_solved=0) last_elastic_applied=0 last_verdict=Reject last_tr_radius=1.0`
* **CHAIN OF DEATH** `1 major row(s), last 1 shown: #0 tr=1 f=-nan h=4.18e+04 kkt=nan qp=no-qp/0m -> Reject`
* **F1_RESTORATION_EXCURSION = N/A   [N/A: restoration_entries == 0]**
* **F2_RESTORATION_STUCK = N/A   [N/A: restoration_entries == 0]**
* **F3_TR_AT_FLOOR = false   [last row's tr_radius=1.0 against tr_min/0.5=2e-10]**
* **F4_REJECTION_STALL = false   [steps_accepted=0 judged_rejections=0 (= rejected_steps 0 - qp_failures_routed 0)]**
* **F5_CAP_CHURN = false   [qp_maxiter_exits=0 qp_cap_retries=0 qp_minor_iters=0]**
* **F6_ELASTIC_ENGAGED = false   [elastic_activations=0 elastic_escalations=0 elastic_rho0_ceiling_hits=0 elastic_floor_retries=0]**
* **F7_WARM_FAILURES = N/A   [N/A: the ledger holds no record]**
* **CONSENSUS** ABSENT (no consensus): counted lanes hven-ipm=2.83964e+04, snopt=2.74588e+04 agree nowhere to 6 significant digits
* **LANE OBJECTIVES** `hven-ipm=28396.373260963694; ipopt=NOT COUNTED; snopt=27458.821606407844`
* **CLOSENESS (objective)** `n/a (f is nan)`
* **CLOSENESS (feasibility)** `max(feasibility_e, feasibility_i)=nan feas_tol=1e-06 ratio=nan`
* **COST vs SNOPT** hven majors=0 minors=0 | SNOPT majors=175 minors=3431 | hven factorizations=0 has NO SNOPT counterpart (SNOPT publishes no factorization count)
* **LEDGER `QpStatus` SEQUENCE** (MAIN WALK ENGINE ONLY — `RULE.md` §2.2; the restoration sub-driver and the lazy engines are never handed the ledger, so this is a LOWER BOUND on the solve's QP solves)

      (the ledger holds no record)

* **ledger print cap** `cap=5000 printed=0 truncated=0`

### `STEENBRG`  (group E, identity PASS)

* **TERMINATOR** `QP_CAP_BUDGET` — numerical_error; the last history row's QP ended max_iter; qp_failures_in_a_row_peak == qp_failure_budget
* **EVIDENCE** `status=numerical_error deadline_fired=0 major_iters=6 factorizations=43096 f=183453796271333.81 qp_failures_in_a_row_peak=3 qp_failure_budget=3 restoration_entries=1 restoration_sub_status=optimal max_iter_option=100 last_qp_solved=1 last_qp_status=max_iter last_elastic_applied=0 last_verdict=Reject last_tr_radius=0.0125`
* **CHAIN OF DEATH** `6 major row(s), last 6 shown: #0 tr=1 f=4528 h=4.18e+04 kkt=1e+04 qp=max_iter/5400m -> Reject  #1 tr=0.5 f=4528 h=4.18e+04 kkt=1e+04 qp=max_iter/5680m/elastic -> Restore  #2 tr=0.1 f=1.835e+14 h=4.89e-12 kkt=1.568e+15 qp=max_iter/5400m -> Reject  #3 tr=0.05 f=1.835e+14 h=4.89e-12 kkt=1.568e+15 qp=max_iter/5400m -> Reject  #4 tr=0.025 f=1.835e+14 h=4.89e-12 kkt=1.568e+15 qp=max_iter/5400m -> Reject  #5 tr=0.0125 f=1.835e+14 h=4.89e-12 kkt=1.568e+15 qp=max_iter/5400m -> Reject`
* **F1_RESTORATION_EXCURSION = true   [sub_status=optimal h_in=41800.0 h_out=4.890230408867829e-12 feas_tol=1e-06 f_in=4527.98990759666 f_out=183453796271333.8 (f_out-f_in)/max(1,|f_in|)=40515504674.38616]**
* **F2_RESTORATION_STUCK = false   [sub_status=optimal h_in=41800.0 h_out=4.890230408867829e-12 h_out/h_in=1.1699115810688585e-16]**
* **F3_TR_AT_FLOOR = false   [last row's tr_radius=0.0125 against tr_min/0.5=2e-10]**
* **F4_REJECTION_STALL = false   [steps_accepted=0 judged_rejections=0 (= rejected_steps 4 - qp_failures_routed 4)]**
* **F5_CAP_CHURN = true   [qp_maxiter_exits=5 qp_cap_retries=0 qp_minor_iters=35429]**
* **F6_ELASTIC_ENGAGED = true   [elastic_activations=1 elastic_escalations=5 elastic_rho0_ceiling_hits=0 elastic_floor_retries=0]**
* **F7_WARM_FAILURES = true   [failing ledger records: warm=4 cold=2 of 12 records; first=0 last=11 (MAIN WALK ENGINE ONLY)]**
* **CONSENSUS** ABSENT (no consensus): counted lanes hven-ipm=2.82230e+04, ipopt=7.44656e+04 agree nowhere to 6 significant digits
* **LANE OBJECTIVES** `hven-ipm=28223.010673446184; ipopt=74465.57790209554; snopt=NOT COUNTED`
* **CLOSENESS (objective)** `ABSENT (no consensus)`
* **CLOSENESS (feasibility)** `max(feasibility_e, feasibility_i)=9.094947017729282e-13 feas_tol=1e-06 ratio=9.094947017729282e-07`
* **COST vs SNOPT** n/a (SNOPT did not solve this cell, or has no counter row)
* **LEDGER `QpStatus` SEQUENCE** (MAIN WALK ENGINE ONLY — `RULE.md` §2.2; the restoration sub-driver and the lazy engines are never handed the ledger, so this is a LOWER BOUND on the solve's QP solves)

      0:max_iter(cold)[m=5400,deg=1182,dt=8,rt=1165,+2906,-2470] 1:infeasible(warm)[m=515,deg=66,dt=13,rt=12,+326,-169] 2:optimal(warm)[m=466,deg=56,dt=25,rt=29,+269,-167] 3:optimal(warm)[m=914,deg=179,dt=69,rt=283,+454,-459] 4:optimal(warm)[m=3,deg=0,dt=0,rt=0,+1,-1] 5:optimal(warm)[m=3,deg=0,dt=0,rt=0,+1,-1] 6:optimal(warm)[m=19,deg=8,dt=4,rt=0,+9,-9] 7:max_iter(warm)[m=5680,deg=2835,dt=0,rt=2265,+2845,-2835] 8:max_iter(cold)[m=5400,deg=2211,dt=0,rt=2817,+2923,-2477] 9:max_iter(warm)[m=5400,deg=2132,dt=1,rt=2139,+2741,-2658] 10:max_iter(warm)[m=5400,deg=2624,dt=0,rt=15,+2753,-2621] 11:max_iter(warm)[m=5400,deg=2643,dt=1,rt=14,+2764,-2630]

* **ledger print cap** `cap=5000 printed=12 truncated=0`

### `YORKNET`  (group I, identity PASS)

* **TERMINATOR** `RESTORATION_EXIT(infeasible)` — restoration ran and classes 4-6 did not fire (the terminal status is not numerical_error); see RULE_AMBIGUITY
* **EVIDENCE** `status=infeasible deadline_fired=0 major_iters=1 factorizations=52180 f=110670.29950761795 qp_failures_in_a_row_peak=0 qp_failure_budget=3 restoration_entries=1 restoration_sub_status=infeasible max_iter_option=100 last_qp_solved=1 last_qp_status=max_iter last_elastic_applied=1 last_verdict=Restore last_tr_radius=1.0 RULE_AMBIGUITY=RULE.md section 5.2 #7's two readings DISAGREE on this cell: under the rule's own gloss (`classes 4-6 did not fire`) it is RESTORATION_EXIT(infeasible); under the literal phrase (`the last history row is not a QP-failure row`) classes 4-7 are all silent and the cell falls to OTHER. The row's failing qp_status is the ELASTIC LADDER's, stamped on by the restoration exit -- RULE.md section 5.1's own warning. The GLOSS reading is returned`
* **CHAIN OF DEATH** `1 major row(s), last 1 shown: #0 tr=1 f=1.107e+05 h=4.849e+04 kkt=4099 qp=max_iter/5080m/elastic -> Restore`
* **F1_RESTORATION_EXCURSION = false   [sub_status=infeasible h_in=48491.383947016526 h_out=48491.383947016526 feas_tol=1e-06 f_in=110670.29950761795 f_out=110670.29950761795 (f_out-f_in)/max(1,|f_in|)=0.0]**
* **F2_RESTORATION_STUCK = true   [sub_status=infeasible h_in=48491.383947016526 h_out=48491.383947016526 h_out/h_in=1.0]**
* **F3_TR_AT_FLOOR = false   [last row's tr_radius=1.0 against tr_min/0.5=2e-10]**
* **F4_REJECTION_STALL = false   [steps_accepted=0 judged_rejections=0 (= rejected_steps 0 - qp_failures_routed 0)]**
* **F5_CAP_CHURN = false   [qp_maxiter_exits=0 qp_cap_retries=0 qp_minor_iters=50241]**
* **F6_ELASTIC_ENGAGED = true   [elastic_activations=19 elastic_escalations=108 elastic_rho0_ceiling_hits=0 elastic_floor_retries=0]**
* **F7_WARM_FAILURES = true   [failing ledger records: warm=1 cold=0 of 8 records; first=7 last=7 (MAIN WALK ENGINE ONLY)]**
* **CONSENSUS** consensus 1.42400e+04 (6 significant digits): lanes hven-ipm, ipopt
* **LANE OBJECTIVES** `hven-ipm=14239.973404951827; ipopt=14239.97340588467; snopt=14617.538377902685`
* **CLOSENESS (objective)** `f=110670.29950761795 f*=14240.0 |f-f*|=96430.29950761795 rel=6.771790695759687`
* **CLOSENESS (feasibility)** `max(feasibility_e, feasibility_i)=4099.28292 feas_tol=1e-06 ratio=4099282920.0`
* **COST vs SNOPT** hven majors=1 minors=50241 | SNOPT majors=110 minors=2050 | hven factorizations=52180 has NO SNOPT counterpart (SNOPT publishes no factorization count)
* **LEDGER `QpStatus` SEQUENCE** (MAIN WALK ENGINE ONLY — `RULE.md` §2.2; the restoration sub-driver and the lazy engines are never handed the ledger, so this is a LOWER BOUND on the solve's QP solves)

      0:infeasible(cold)[m=397,deg=161,dt=27,rt=101,+346,-45] 1:optimal(warm)[m=1004,deg=356,dt=191,rt=304,+499,-288] 2:optimal(warm)[m=36,deg=3,dt=4,rt=5,+17,-17] 3:optimal(warm)[m=4,deg=0,dt=0,rt=1,+2,-1] 4:optimal(warm)[m=2,deg=0,dt=0,rt=1,+1,-0] 5:optimal(warm)[m=2,deg=0,dt=0,rt=0,+1,-0] 6:optimal(warm)[m=18,deg=0,dt=7,rt=0,+9,-8] 7:max_iter(warm)[m=5080,deg=2539,dt=2539,rt=1,+2540,-2540]

* **ledger print cap** `cap=5000 printed=8 truncated=0`

### `ACOPR118`  (group W, identity NOT_COMPARABLE)

* **TERMINATOR** `WALL_TRUNCATED` — deadline_fired == 1
* **EVIDENCE** `status=interrupted deadline_fired=1 major_iters=6 factorizations=12262 f=129584.10424074405 qp_failures_in_a_row_peak=1 qp_failure_budget=3 restoration_entries=0 restoration_sub_status=not_run max_iter_option=100 last_qp_solved=0 last_qp_status=n/a(qp_solved=0) last_elastic_applied=0 last_verdict=Reject last_tr_radius=0.25`
* **CHAIN OF DEATH** `7 major row(s), last 6 shown: #1 tr=0.5 f=1.313e+05 h=18.74 kkt=4000 qp=optimal/565m -> AcceptF  #2 tr=1 f=1.289e+05 h=52.72 kkt=3429 qp=optimal/426m -> AcceptH  #3 tr=1 f=1.291e+05 h=60.9 kkt=1268 qp=optimal/473m -> AcceptH  #4 tr=1 f=1.296e+05 h=7.065 kkt=256 qp=optimal/595m -> Reject  #5 tr=0.5 f=1.296e+05 h=7.065 kkt=256 qp=optimal/655m -> Reject  #6 tr=0.25 f=1.296e+05 h=7.065 kkt=256 qp=no-qp/0m -> Reject`
* **F1_RESTORATION_EXCURSION = N/A   [N/A: restoration_entries == 0]**
* **F2_RESTORATION_STUCK = N/A   [N/A: restoration_entries == 0]**
* **F3_TR_AT_FLOOR = false   [last row's tr_radius=0.25 against tr_min/0.5=2e-10]**
* **F4_REJECTION_STALL = false   [steps_accepted=3 judged_rejections=2 (= rejected_steps 3 - qp_failures_routed 1)]**
* **F5_CAP_CHURN = true   [qp_maxiter_exits=1 qp_cap_retries=0 qp_minor_iters=18614]**
* **F6_ELASTIC_ENGAGED = false   [elastic_activations=0 elastic_escalations=0 elastic_rho0_ceiling_hits=0 elastic_floor_retries=0]**
* **F7_WARM_FAILURES = true   [failing ledger records: warm=2 cold=1 of 8 records; first=0 last=7 (MAIN WALK ENGINE ONLY)]**
* **CONSENSUS** consensus 1.29661e+05 (6 significant digits): lanes hven-ipm, ipopt, snopt
* **LANE OBJECTIVES** `hven-ipm=129660.68731230218; ipopt=129660.68366077568; snopt=129660.68780399175`
* **CLOSENESS (objective)** `f=129584.10424074405 f*=129661.0 |f-f*|=76.89575925594545 rel=0.0005930523384513883`
* **CLOSENESS (feasibility)** `max(feasibility_e, feasibility_i)=0.3215712976089282 feas_tol=1e-06 ratio=321571.29760892823`
* **COST vs SNOPT** hven majors=6 minors=18614 | SNOPT majors=71 minors=912 | hven factorizations=12262 has NO SNOPT counterpart (SNOPT publishes no factorization count)
* **LEDGER `QpStatus` SEQUENCE** (MAIN WALK ENGINE ONLY — `RULE.md` §2.2; the restoration sub-driver and the lazy engines are never handed the ledger, so this is a LOWER BOUND on the solve's QP solves)

      0:max_iter(cold)[m=5300,deg=6,dt=0,rt=5,+1710,-1795] 1:optimal(warm)[m=565,deg=4,dt=0,rt=3,+146,-210] 2:optimal(warm)[m=426,deg=0,dt=0,rt=0,+70,-177] 3:optimal(warm)[m=473,deg=0,dt=0,rt=0,+81,-195] 4:optimal(warm)[m=595,deg=26,dt=0,rt=25,+148,-222] 5:max_iter(warm)[m=5300,deg=2416,dt=0,rt=56,+2613,-2521] 6:optimal(warm)[m=655,deg=36,dt=0,rt=35,+170,-241] 7:max_iter(warm)[m=5300,deg=2387,dt=0,rt=47,+2563,-2536]

* **ledger print cap** `cap=5000 printed=8 truncated=0`

### `BIGBANK`  (group W, identity NOT_COMPARABLE)

* **TERMINATOR** `WALL_TRUNCATED` — deadline_fired == 1
* **EVIDENCE** `status=interrupted deadline_fired=1 major_iters=3 factorizations=1501 f=-13070.503587333713 qp_failures_in_a_row_peak=0 qp_failure_budget=3 restoration_entries=0 restoration_sub_status=not_run max_iter_option=100 last_qp_solved=0 last_qp_status=n/a(qp_solved=0) last_elastic_applied=0 last_verdict=Reject last_tr_radius=8.0`
* **CHAIN OF DEATH** `4 major row(s), last 4 shown: #0 tr=1 f=-5194 h=1.195e-13 kkt=13.18 qp=optimal/1038m -> AcceptF  #1 tr=2 f=-6560 h=8.872e-11 kkt=69.53 qp=optimal/256m -> AcceptF  #2 tr=4 f=-8910 h=4.035e-11 kkt=68.44 qp=optimal/204m -> AcceptF  #3 tr=8 f=-1.307e+04 h=1.927e-11 kkt=64.68 qp=no-qp/0m -> Reject`
* **F1_RESTORATION_EXCURSION = N/A   [N/A: restoration_entries == 0]**
* **F2_RESTORATION_STUCK = N/A   [N/A: restoration_entries == 0]**
* **F3_TR_AT_FLOOR = false   [last row's tr_radius=8.0 against tr_min/0.5=2e-10]**
* **F4_REJECTION_STALL = false   [steps_accepted=3 judged_rejections=0 (= rejected_steps 0 - qp_failures_routed 0)]**
* **F5_CAP_CHURN = false   [qp_maxiter_exits=0 qp_cap_retries=0 qp_minor_iters=1498]**
* **F6_ELASTIC_ENGAGED = false   [elastic_activations=0 elastic_escalations=0 elastic_rho0_ceiling_hits=0 elastic_floor_retries=0]**
* **F7_WARM_FAILURES = false   [failing ledger records: warm=0 cold=0 of 3 records; first=none last=none (MAIN WALK ENGINE ONLY)]**
* **CONSENSUS** ABSENT (no consensus): 1 counted lane(s)
* **LANE OBJECTIVES** `hven-ipm=-4205693.299537316; ipopt=NOT COUNTED; snopt=NOT COUNTED`
* **CLOSENESS (objective)** `ABSENT (no consensus)`
* **CLOSENESS (feasibility)** `max(feasibility_e, feasibility_i)=3.522043767745231e-13 feas_tol=1e-06 ratio=3.522043767745231e-07`
* **COST vs SNOPT** n/a (SNOPT did not solve this cell, or has no counter row)
* **LEDGER `QpStatus` SEQUENCE** (MAIN WALK ENGINE ONLY — `RULE.md` §2.2; the restoration sub-driver and the lazy engines are never handed the ledger, so this is a LOWER BOUND on the solve's QP solves)

      0:optimal(cold)[m=1038,deg=13,dt=0,rt=8,+819,-113] 1:optimal(warm)[m=256,deg=8,dt=0,rt=1,+122,-70] 2:optimal(warm)[m=204,deg=3,dt=0,rt=0,+91,-57]

* **ledger print cap** `cap=5000 printed=3 truncated=0`

### `MSS2`  (group W, identity REFUSED)

**CENSORED.** The outer `timeout 900` killed the leg. There is NO ROW, and `RULE.md` §3.1 forbids filling one later or reconstructing it. No counter, no flag, no chain of death.

### `MSS3`  (group W, identity REFUSED)

**CENSORED.** The outer `timeout 900` killed the leg. There is NO ROW, and `RULE.md` §3.1 forbids filling one later or reconstructing it. No counter, no flag, no chain of death.

### `READING8`  (group W, identity REFUSED)

**CENSORED.** The outer `timeout 900` killed the leg. There is NO ROW, and `RULE.md` §3.1 forbids filling one later or reconstructing it. No counter, no flag, no chain of death.

### `SAWPATH`  (group W, identity NOT_COMPARABLE)

* **TERMINATOR** `WALL_TRUNCATED` — deadline_fired == 1
* **EVIDENCE** `status=interrupted deadline_fired=1 major_iters=1 factorizations=83624 f=1483.1937415552059 qp_failures_in_a_row_peak=0 qp_failure_budget=3 restoration_entries=1 restoration_sub_status=numerical_error max_iter_option=100 last_qp_solved=0 last_qp_status=n/a(qp_solved=0) last_elastic_applied=0 last_verdict=Reject last_tr_radius=0.1`
* **CHAIN OF DEATH** `2 major row(s), last 2 shown: #0 tr=1 f=1483 h=0 kkt=10.82 qp=max_iter/6830m/elastic -> Restore  #1 tr=0.1 f=1483 h=0 kkt=10.82 qp=no-qp/0m -> Reject`
* **F1_RESTORATION_EXCURSION = false   [sub_status=numerical_error h_in=0.0 h_out=0.0 feas_tol=1e-06 f_in=1483.1937415552059 f_out=1483.1937415552059 (f_out-f_in)/max(1,|f_in|)=0.0]**
* **F2_RESTORATION_STUCK = true   [sub_status=numerical_error h_in=0.0 h_out=0.0 h_out/h_in=n/a (h_in == 0)]**
* **F3_TR_AT_FLOOR = false   [last row's tr_radius=0.1 against tr_min/0.5=2e-10]**
* **F4_REJECTION_STALL = false   [steps_accepted=0 judged_rejections=0 (= rejected_steps 0 - qp_failures_routed 0)]**
* **F5_CAP_CHURN = true   [qp_maxiter_exits=4 qp_cap_retries=0 qp_minor_iters=77037]**
* **F6_ELASTIC_ENGAGED = true   [elastic_activations=1 elastic_escalations=5 elastic_rho0_ceiling_hits=0 elastic_floor_retries=0]**
* **F7_WARM_FAILURES = true   [failing ledger records: warm=1 cold=0 of 7 records; first=6 last=6 (MAIN WALK ENGINE ONLY)]**
* **CONSENSUS** consensus 7.50459e+02 (6 significant digits): lanes hven-ipm, snopt
* **LANE OBJECTIVES** `hven-ipm=750.4589467597547; ipopt=723.1680381883211; snopt=750.4589409027745`
* **CLOSENESS (objective)** `f=1483.1937415552059 f*=750.459 |f-f*|=732.7347415552059 rel=0.9763821095558931`
* **CLOSENESS (feasibility)** `max(feasibility_e, feasibility_i)=3.556 feas_tol=1e-06 ratio=3556000.0`
* **COST vs SNOPT** hven majors=1 minors=77037 | SNOPT majors=8 minors=21 | hven factorizations=83624 has NO SNOPT counterpart (SNOPT publishes no factorization count)
* **LEDGER `QpStatus` SEQUENCE** (MAIN WALK ENGINE ONLY — `RULE.md` §2.2; the restoration sub-driver and the lazy engines are never handed the ledger, so this is a LOWER BOUND on the solve's QP solves)

      0:infeasible(cold)[m=321,deg=0,dt=0,rt=0,+119,-100] 1:optimal(warm)[m=45,deg=2,dt=0,rt=0,+17,-13] 2:optimal(warm)[m=112,deg=24,dt=0,rt=20,+43,-43] 3:optimal(warm)[m=5,deg=0,dt=0,rt=0,+1,-1] 4:optimal(warm)[m=5,deg=0,dt=0,rt=0,+1,-1] 5:optimal(warm)[m=19,deg=0,dt=0,rt=0,+5,-6] 6:max_iter(warm)[m=6830,deg=3328,dt=0,rt=0,+3387,-3385]

* **ledger print cap** `cap=5000 printed=7 truncated=0`

### `TAX213322`  (group W, identity REFUSED)

**CENSORED.** The outer `timeout 900` killed the leg. There is NO ROW, and `RULE.md` §3.1 forbids filling one later or reconstructing it. No counter, no flag, no chain of death.

### `TAX53322`  (group W, identity REFUSED)

**CENSORED.** The outer `timeout 900` killed the leg. There is NO ROW, and `RULE.md` §3.1 forbids filling one later or reconstructing it. No counter, no flag, no chain of death.


## Findings about the RULE itself

`RULE.md` is **not edited** — a pre-registered rule's bytes are the pin, and a
defect in one is recorded, never repaired after the fact (M3b Corrections C6, C9).
Four defects were found by applying it.

### R1 — §5.2 #7 is AMBIGUOUS, and the ambiguity decides `YORKNET`'s class

Class 7's conjunct reads: *"the last history row is not a QP-failure row (i.e.
classes 4–6 did not fire)"*. Those are two different predicates. They disagree on
exactly one shape — a cell whose terminal status is NOT `numerical_error` (so
classes 4–6 cannot fire whatever the row says) while its last history row DOES
carry a failing `qp_status`. On the shipped path that shape is produced by the
elastic tier's own restoration exit, which stamps the ELASTIC LADDER's status
onto the row and sets the verdict to `Restore`
(`src/drivers/sqp_solver.cpp`, the `if (!report.usable)` arm) — the very overwrite
`RULE.md` §5.1's second semantic note warns about.

`YORKNET` is that cell, and it is the only one. Under the rule's own
parenthetical gloss it is `RESTORATION_EXIT(infeasible)`, which is what §5.2 #7's
"*Expected: YORKNET*" says the rule meant. Under the literal phrase, classes 4–7
are all silent and the cell falls to `OTHER`. **The gloss reading is what the
classifier returns**, because the parenthetical is the rule's own definition of
its own phrase in the same clause; the literal reading is printed beside it on
that cell's line so no reader has to take the script's word for it.

### R2 — §5.2 #4 fires on `BRITGAS`, whose exit is NOT the failure budget

`BRITGAS` satisfies class 4's predicate exactly: terminal `numerical_error`, the
last history row's QP ended `max_iter`, and `qp_failures_in_a_row_peak == 3 ==
qp_failure_budget`. It is labelled `QP_CAP_BUDGET` and that label stands as the
rule's output.

**Its mechanism is something else.** The budget was reached over majors 0–2, all
three `max_iter` and all three routed. Major 3's own subproblem then returned
`infeasible`, which the routed-failure block never sees — "`kInfeasible` reaches
neither field: the elastic tier consumed it one block above" — so the elastic
ladder ran, returned unusable at `max_iter`, and the RESTORATION exit followed.
The restoration sub-solve ended `numerical_error`, and that is the solve's
terminal status. The budget was never the gate that ended this cell. The census
prints `last_elastic_applied=1` and `last_verdict=Restore` on `BRITGAS`'s
evidence line, which is where a reader sees it; §5.2 #4's predicate simply does
not condition on either. This is the same class of defect as R1 and has the same
root: the terminal history row's `qp_*` group is not always the main engine's.

### R3a — F2's h-band degenerates when `h_in == 0`

`SAWPATH` enters restoration with `restoration_h_in = 0` and leaves with
`restoration_h_out = 0`, so `h_out >= 0.99 * h_in` reads `0 >= 0` and
`F2_RESTORATION_STUCK` fires. That is the rule's EXACT null — "a sub-solve that
returns the point it was handed" — so the flag is not wrong; but on a cell whose
violation was already zero it is not the statement the flag's name suggests
either. The pair `(0, 0)` is printed beside it, which is how a reader sees it,
and the ratio column prints `n/a (h_in == 0)` rather than dividing. `YORKNET` is
the flag's real case: `h_in == h_out == 48491.383947016526`, bit for bit, after
18 restoration iterations.

### R3 — the cause-shaped flags F3 and F4 are DEAD on this population

`F3_TR_AT_FLOOR` is `false` on **every** cell with a row (the smallest terminal
radius anywhere is `0.0125`, against a floor test at `2e-10`), and
`F4_REJECTION_STALL` is `false` on every cell too. F4's reason is worth stating
because it is a finding rather than a null: on these cells
`judged_rejections = rejected_steps − qp_failures_routed` is **0** on nine of the
twelve and 2 on the other three. **Essentially every `Reject` verdict in these
histories is a ROUTED QP FAILURE, not a merit-function rejection.** The
globalization strategy is barely being consulted; the subproblem is where these
solves die. That is an OBSERVED fact about the population and it is the single
most load-bearing line in this census.


## Recommendation to the settler — engine change ⇒ cells it plausibly converts

**A RECOMMENDATION, and nothing more** (`RULE.md` §0 and §10). Each candidate is
its own task with its own pre-registered rule and its own gates; none is selected
or designed here. OBSERVED and INFERRED are marked separately on every line.

### Candidate A — the QP walk's minor cap on a degenerate working set
*Would plausibly convert: `ACOPP118`, `ACOPP57`, `ACOPR57`, `DALLASM`,
`STEENBRB`, `STEENBRC`, `STEENBRG` (7 cells), and is implicated in `BRITGAS`,
`SAWPATH` and `YORKNET` (3 more).*

**OBSERVED.** Eight of the twelve identity-passing cells carry
`qp_failures_in_a_row_peak = 3 = qp_failure_budget` with 4–9 `qp_maxiter_exits`
and 0–1 `qp_numerical_exits`: the budget is spent on subproblems hitting their
ITERATION CAP, not on arithmetic. Every capped ledger record in this census has
the same shape — `degenerate_steps ≈ minors/2`, `ws_adds ≈ ws_drops ≈ minors/2`.
`ACOPP118`'s four capped records are `4710` minors with `2303–2305` degenerate
steps and `2399/2306` adds/drops; `YORKNET`'s single capped record is `5080`
minors with `2539` degenerate steps, `2539` drop ties and `2540/2540` adds/drops.
Three of the seven (`ACOPP118`, `ACOPP57`, `ACOPR57`) never restore at all and
accept 3–8 steps first, then stop moving.

**INFERRED, and pass 2 exists to check it.** That counter shape is what a
zero-motion period-2 add/drop cycle produces, which is the tail M3b traced at
`tr_init = +inf`. It is NOT established here: the ledger sees per-solve totals,
not per-minor motion, and M3b's own warning is that two cells with the same
outward counters had different causes.

### Candidate B — the start point (`project_start_point`, M7 W2 M4, OFF by default)
*Would plausibly convert: `STEENBRD`, `STEENBRE`; would not convert `NET3`
(3 cells in the class, 2 candidates).*

See the section below. **Likely a cheap, separate engine item**, and independent
of every QP question in this census.

### Candidate C — the restoration excursion
*Would plausibly convert: `STEENBRB`, `STEENBRC`, `STEENBRG` (3 cells, all of
them also in candidate A).*

**OBSERVED.** `F1_RESTORATION_EXCURSION` fires on exactly these three. Each
restores from a large violation to `h_out ≤ 5e-12` and lands the objective **nine
to eleven orders of magnitude above where it started**: `STEENBRB` `2.86e3 →
4.09e12`, `STEENBRC` `4.53e3 → 1.83e14`, `STEENBRG` `4.53e3 → 1.83e14`. Against
the three-lane consensus `9075.86` for `STEENBRB`, the restored point is `4.5e8`
relative away. The subsequent QPs at that point are the capped ones. `DALLASM`
restores too and does NOT fire the flag: its ratio is `5.1e-05`, three orders
under the bar — and yet `DALLASM`'s terminal objective `6.20e7` is `1.29e3`
relative from the three-lane consensus `-4.81982e4`, so it is far from the answer
for a reason this flag does not name.

**INFERRED.** Whether a feasibility restoration that ignores the objective
entirely (the shipped `restoration_proximity = 0`, proven per leg by the OPTIONS
block) is what puts these cells somewhere unrecoverable is not established by
these counters. M2b built the proximity term and landed it OFF; this census is
evidence FOR measuring it on these three cells, not evidence that it works.

### Candidate D — the wall
*Would plausibly convert: nothing directly; 5 CENSORED + 3 TRUNCATED cells are
invisible until something else moves.*

**OBSERVED.** `ACOPR118` at the stop is `5.9e-4` relative from the three-lane
consensus after 6 majors, with one capped `5300`-minor record already spent and a
second in flight. `BIGBANK` is still ACCEPTING steps (`AcceptF` on all three of
its majors, radius growing `1 → 2 → 4 → 8`) and has no failure of any kind — it is
simply slow. `SAWPATH` spent `77 037` minors inside ONE major, `6 830` of them in
a single capped elastic solve, against SNOPT's 8 majors and 21 minors.

**INFERRED.** `BIGBANK` looks budget-bound rather than broken; `ACOPR118` and
`SAWPATH` look like candidate A under a clock. None of that is a measurement of
what they would do with more time, and this census does not make one.

### What NOT to build from this census
**`TR_COLLAPSE` and `REJECTION_STALL` are not live on this population** (finding
R3): no cell reached the trust-region floor, and judged rejections are 0 or 2
everywhere. Trust-region floor work and merit/filter acceptance work would be
spent on conditions these twenty cells do not have.


## OUTSIDE THE RULE — the three `START_POINT_NONFINITE` cells

**`RULE.md` pre-registers no read-out for this class beyond §5.2 #3's predicate,
so everything in this section is OUTSIDE THE RULE and is labelled as such.** It
is written from the PUBLIC probe surface and the committed harness rows only. No
leg was run for it, no source was changed, and nothing below is a verdict.

`NET3`, `STEENBRD` and `STEENBRE` die at major 0 having built no subproblem:
0 majors, 0 factorizations, 0 minors, `restoration_sub_status = not_run`, an
empty ledger, and one history row whose QP never ran.

**OBSERVED — the objective itself is NaN at the start point.** Each row carries
`c_evals_full = 1`: exactly one full model evaluation happened, at `x0`, and the
probe's `f` on this exit is the model's own objective there
(`src/drivers/sqp_solver.cpp`: the non-finite exit sets `out.f = ev.f`, and
scaling is off by default). All three print `f = -nan`. So `f(x0)` is NaN, which
is on its own sufficient for the exit.

**ABSENT — which OTHER evaluation is also non-finite.** The driver's gate is
`kkt.finite`, which conjoins `isfinite(f)`, `grad.allFinite()`,
`ce.allFinite()` and `ci.allFinite()` into one bool, and on failure it sets every
residual to NaN. The public surface cannot separate them, and this section does
not guess. `c_start_point_clamps = 0` on all three is likewise NOT an observation
that `x0` is inside its box: `solver_counters.h` states the field is written only
under `project_start_point`, and at the toggle's OFF state it is identically 0.

**OBSERVED — what the other lanes did on the same three cells**, from the
committed rows:

| cell | hven-sqp @HEAD | hven-ipm @`aeebdccc` | IPOPT | SNOPT |
|---|---|---|---|---|
| `NET3` | `numerical_error`, 0 majors | `max_iter` (dnf_budget), 1000 iters, f = 5859616.03 | `Restoration_Failed`, 194 iters | `PointCannotBeImproved`, 10 iters |
| `STEENBRD` | `numerical_error`, 0 majors | **`optimal`**, 247 iters, f = 9030.0817643754581 | `Error_In_Step_Computation`, 192 iters | **`Optimal`**, 155 iters, f = 9030.0818676051877 |
| `STEENBRE` | `numerical_error`, 0 majors | **`optimal`**, 223 iters, f = 28396.373260963694 | `Error_In_Step_Computation`, 309 iters | **`Optimal`**, 175 iters, f = 27458.821606407844 |

`NET3` defeats all four lanes and is not a start-point story. **`STEENBRD` and
`STEENBRE` are solved by hven's OWN IPM engine, on the same model through the
same harness bridge** — so the model is evaluable somewhere near that start
point, and the SQP lane is the only lane that stops at it.

**OBSERVED — the option exists and is OFF.** Every leg's `OPTIONS` block prints
`opt_project_start_point=0`. `SqpOptions::project_start_point` (M7 W2 M4,
`include/hven/drivers/sqp_solver_types.h:715`) defaults to `false`; at `true` it
projects the `x0` ARGUMENT componentwise into the seam's declared box,
`min(max(x, l), u)`, above the `begin` line, so the whole solve describes one
point. It is byte-identical to OFF when every component is already in its box.
Its scope note says it projects the `x0` argument and not a warm solve's resolved
start point. `solver_counters.h` records that the IPM's twin is
`IpmSolver::push_initial_point_interior` — the IPM does not start at the raw `x0`
at all.

**INFERRED, and explicitly not established.** The obvious reading — that CUTEst's
declared start point for these cells lies outside the declared box, that the
model is not evaluable there, and that the IPM's interior push is why its lane
does not see this — is CONSISTENT with every observation above and is NOT
measured by any of them.

**What would have to be measured to confirm it**, as its own small task:

1. Evaluate the model's `f`, `cE`, `cI` and `grad` at the CUTEst start point for
   `NET3`, `STEENBRD` and `STEENBRE` through the harness bridge, and report WHICH
   of the four is non-finite and in which components.
2. Report, for each, the count of start-point components strictly outside
   `[l, u]` — i.e. run one leg per cell with `project_start_point = true` and read
   `start_point_clamps`, which is exactly the number that answers it.
3. Re-run those three cells at `project_start_point = true` at default options
   otherwise, and report the terminal status. Two cells converting would make this
   a one-flag change; `NET3` converting would be a surprise, since no lane solves it.

All three are cheap (seconds per cell: these solves stop at major 0 today) and
none of them touches the QP walk. **This is a separate engine item from everything
else in this census**, and it is the only class here whose remedy candidate is
already written and shipped-but-off.


---

*Generated by `wrappers/census_table.py`. `RULE.md` governs; it is not edited. No outcome in this file licenses a behaviour change.*


---

# PASS 2 — the per-minor walk at DEFAULT options

Governed by [`RULE-P2.md`](../RULE-P2.md), committed before any pass-2 leg ran, and by [`RULE-P2A.md`](../RULE-P2A.md), which decided whether the instrument was sound. Neither is edited. **Nothing in this section licenses a behaviour change, a default change, or a line of code** (RULE-P2 §0, §8).

## The question pass 1 left open

Pass 1's Candidate A said, in so many words, that the capped QPs *look like* a zero-motion period-2 drop/re-add cycle and that the ledger cannot tell. **RULE-P2 §0 asked it as a question with three possible answers** — `CYCLE`, `LONG WALK`, `SOMETHING ELSE` — fixed in advance so the rule could return something other than its own hypothesis.

## The instrument, and the two runs it took to trust it

Pass 2's first run was STOPPED on all ten cells by RULE-P2 §5.1's identity gate. `RULE-P2A.md`'s three-arm experiment located the cause entirely outside hven — the probe had been built by a DIFFERENT COMPILER from the library pass 1 linked — and returned its OUTCOME 1: the walk seam is neutral both compiled in and ARMED. The ten legs were then RE-TAKEN under the repaired build on RULE-P2 §3/§4's unchanged terms. The five process findings are in `README.md`; what matters here is the result of the gate on the re-take.

| | |
|---|---|
| cells traced | 10 |
| §5.1 identity gate on the re-take | **REPRODUCES 10 / 10**, DIFFERS 0, CENSORED 0; near-ulp acceptances **0** — every compared field, residuals included, is BYTE-equal to the committed pass-1 row |
| §2 observer-solve ↔ ledger-record mapping | **UNIQUE on all 10 cells**; not one AMBIGUOUS, not one NONE |
| §5.3 conservation sums | difference **0** on both sums on all 10 cells (92309 minors and 93749 factorizations on the largest, `BRITGAS`) |
| §2 ABORT flags (M3b C7) | `summary_truncated=0`, `summary_allocation_failed=0`, `minor_contiguous=1` everywhere |
| §3 stage 2 selection | 21 solves, mechanical, no judgement at selection time |

## The answer, per traced solve

`PERIOD` is the length of the exactly-periodic suffix on the trace's own F0 state (every recorded field but the inertia group, compared BIT for BIT); `|T|` and `SHARE` are that suffix's length and its share of the solve's passes; `Σ|Δx|∞` is RULE-P2 §6 #4's zero-motion quantity summed over it.

| cell | solve | selected as | §6.1 ANSWER | period | \|T\| | share | Σ\|Δx\|∞ over the tail |
|---|---|---|---|---|---|---|---|
| `ACOPP57` | 3 | first capped | **CYCLE** | 2 | 1776 | 0.989 | `0` |
| `ACOPP57` | 6 | last capped | **CYCLE** | 2 | 1760 | 0.981 | `0` |
| `DALLASM` | 1 | first capped | **CYCLE** | 2 | 1990 | 0.926 | `0` |
| `DALLASM` | 7 | last capped | **CYCLE** | 2 | 1853 | 0.945 | `0` |
| `STEENBRB` | 1 | first capped | **LONG WALK** | None | 414 | 1.000 | `74.5296` |
| `STEENBRB` | 16 | last capped | **CYCLE** | 2 | 4531 | 0.968 | `0` |
| `ACOPP118` | 3 | first capped | **CYCLE** | 2 | 4607 | 0.978 | `0` |
| `ACOPP118` | 6 | last capped | **CYCLE** | 2 | 4606 | 0.978 | `0` |
| `ACOPR57` | 4 | first capped | **CYCLE** | 2 | 1900 | 0.913 | `0` |
| `ACOPR57` | 16 | last capped | **SOMETHING ELSE** | 6 | 2010 | 0.966 | `0.600675` |
| `STEENBRC` | 0 | first capped | **CYCLE** | 4 | 4592 | 0.850 | `0` |
| `STEENBRC` | 27 | last capped | **CYCLE** | 2 | 5233 | 0.969 | `0` |
| `STEENBRG` | 0 | first capped | **CYCLE** | 4 | 4577 | 0.848 | `0` |
| `STEENBRG` | 27 | last capped | **CYCLE** | 2 | 5242 | 0.971 | `0` |
| `YORKNET` | 7 | first capped | **CYCLE** | 2 | 5077 | 0.999 | `0` |
| `YORKNET` | 145 | first not ledgered capped | **CYCLE** | 4 | 7990 | 0.888 | `0` |
| `SAWPATH` | 6 | first capped | **CYCLE** | 2 | 6655 | 0.974 | `0` |
| `SAWPATH` | 10 | first not ledgered capped | **CYCLE** | 2 | 15855 | 0.910 | `0` |
| `BRITGAS` | 0 | first capped | **CYCLE** | 2 | 4283 | 0.952 | `0` |
| `BRITGAS` | 4 | last capped | **CYCLE** | 2 | 5727 | 0.820 | `0` |
| `BRITGAS` | 11 | first not ledgered capped | **CYCLE** | 2 | 11074 | 0.946 | `0` |

**19 of 21 traced solves are `CYCLE`. 1 is `LONG WALK`. 1 is `SOMETHING ELSE`.** 8 of the 10 traced cells answer `CYCLE` on every solve traced on them: `ACOPP57`, `DALLASM`, `ACOPP118`, `STEENBRC`, `STEENBRG`, `YORKNET`, `SAWPATH`, `BRITGAS`.

### What a `CYCLE` row means here, precisely

On a `CYCLE` row the walk's WHOLE recorded state repeats exactly: on `ACOPP57` solve 3, 1776 of the 1795 passes are a period-2 suffix in which the same bound is dropped by the Dantzig rule and immediately re-admitted, `dx_inf` is `-0` on every add pass and absent on every drop pass, and Σ|Δx|∞ over the tail is **exactly 0**. Its re-admission share is 1.0 — all of the tail's add passes re-admit the SAME constraint, `('bound', 8, 'lower')` — and its candidate set is literally one set, seen 888 times.

The cycle is not always period 2. Four solves cycle at period 4 or 6 over TWO alternating constraint identities: `ACOPR57` solve 16 (period 6), `STEENBRC` solve 0 (period 4), `STEENBRG` solve 0 (period 4), `YORKNET` solve 145 (period 4). RULE-G's own period-2 tail finder, the inherited instrument, sees no tail at all on those (defect D1 below).

## The two rows that are not `CYCLE`

**`STEENBRB` solve 1 — `LONG WALK`.** It is the cell's FIRST capped record and the only ledger record on any of the ten TRACED cells whose status is `numerical_error` rather than `max_iter`; its trace ends on a `terminal` action. There is no exactly-periodic suffix at any period ≤ 64, the working-set signature sequence has no period, 389 of its 414 passes are distinct signatures, and the tail MOVES: Σ|Δx|∞ = `74.52962140787511` over the whole solve with 182 passes moving by M3b's own `step_tol` test. Its M3b walk class is `NEGATIVE_CURVATURE_BLOCKERS` — stepped 83, nullspace_veto 31 — and its inertia at minor 0 is `(209, 143, 0)` observed against `(244, 108)` expected, the +35 excess negatives M3b itself traced on this cell. **The same cell's LAST capped solve (solve 16) is a textbook period-2 `CYCLE`**, so `STEENBRB` is reported as two classes and not summarised into one, exactly as RULE-P2 §6.1 requires.

**`ACOPR57` solve 16 — `SOMETHING ELSE`, and it is a finding ABOUT THE RULE.** Its tail is an EXACTLY periodic suffix of period 6 covering 2010 of its 2080 passes — a drop, an add, and a `move_only` pass, twice over, on two alternating bounds — and the `move_only` pass records `dx_inf = 8.9653013325729537e-4` EVERY time. The iterate therefore moves on a sixth of the tail's passes and **returns exactly**: `x_l1` alternates between two values, bit for bit, for 2 010 passes. Σ|Δx|∞ over the tail is `0.600675189282388`, and RULE-P2 §6.1's `CYCLE` arm demands EXACT zero motion, so the rule cannot call it a cycle; its `LONG WALK` arm demands that the working set not repeat, and it does. **A non-zero-motion EXACT cycle is a shape §6.1 did not anticipate**, and §6.1 says in its own words that this arm is a finding about the rule. Its practical content is the same as a `CYCLE` row's: the solve is stalled and will spend its whole cap.

## What the tails are made of — RULE-P2 §6's read-outs

The full per-solve read-outs are machine-written in [`readouts-p2b.md`](readouts-p2b.md) and the answers in [`verdict-p2.txt`](../verdict-p2.txt). What the population shows:

* **§6 #3, the re-admitted constraint.** On 16 of the 21 traced solves the tail admits exactly ONE constraint identity, over and over, with a re-admission share of `1.0`; on 4 it alternates between exactly two. It is a BOUND in every case — the number of traced solves whose tail re-admits an INEQUALITY ROW is **0**.
* **§6 #8, candidate-set recurrence.** On the same solves the Dantzig drop's candidate SET is one or two sets, seen thousands of times: `tail_distinct_candidate_sets` is 1 or 2 on every `CYCLE` row, with a modal share of `1.0` or `0.5`. `drop_tied` is `1` on almost all of them — one tie, resolved the same way every pass. The exception is `YORKNET` solve 7, whose tail drop passes all carry `drop_tied = 8` (`tail_drop_tied_hist` = `{8: 2538}`): that many candidates tie on the drop rule and the same one is taken each time.
* **§6 #5, inertia.** M3b C11's qualification is carried on every line: the printed OBSERVED triple is the retained sparse `K0` factor's, with the dense Schur complement's Haynsworth contribution BACKED OUT of the EXPECTED pair instead, so a non-zero excess away from minor 0 is an ALGEBRAIC verdict and not a fresh per-working-set factorization — and on these solves minor 0 is usually NOT COLD either (`border_dim > 0` at minor 0 on most of them), so even the minor-0 comparison is algebraic here, unlike M3b's cold first QP. The TAIL excess is **exactly 0 on every pass of 14 of the 21 solves**: on most of this population the engine's own inertia test reports the working set as correct while the walk cycles on it.
* **§6 #6 and #7, curvature and the ride.** On 14 of the 21 solves the ride is NEVER ATTEMPTED anywhere in the tail and the cycle is pure Dantzig drop/re-add. On the other 7 the curvature quotient `p_hp/p_sq` sits at or below its tolerance and the ride fires: `SAWPATH` solve 10 vetoes 7 927 rides on the null space, `BRITGAS` solve 0 vetoes 2 141, `YORKNET` solve 7 vetoes 2 539 on a flat slope — and on `STEENBRC` solve 0, `STEENBRG` solve 0 and `YORKNET` solve 145 the ride reports `stepped` 1 148, 1 144 and 1 998 times **and the step is exactly zero**. A ride that steps nowhere is the cycle continuing, not an escape from it.
* **§6 #9, warm starts, and the question the dispatch named.** **Does a later capped solve START in the previous capped solve's cycle state? NO — on every one of the eight cells where the comparison is available, the answer is NOT EQUAL**, compared against ALL of the earlier solve's distinct tail signatures and not a sample. The later solve comes in with its own working set (`ws_size_in` 1–40 on 5 of the 8, and 333/382/386 on the other 3) and walks back into a cycle of its own. The cycle is RE-ENTERED each major, not INHERITED across majors. `YORKNET` and `SAWPATH` have one capped main-engine record each, so the comparison DECLINES on them rather than guessing. M3b C7's qualification is carried: an EQUALITY of SplitMix64 signatures would have been evidence and not proof; a NON-EQUALITY is conclusive.

## Findings about `RULE-P2.md` itself

`RULE-P2.md` is **not edited** — a pre-registered rule's bytes are the pin. Applying it found three defects, each of which had to be given a reading before §6.1 could return anything. Every reading is declared in `wrappers/readouts_p2b.py`'s docstring, and BOTH readings' answers are printed on every solve in `verdict-p2.txt`.

### D1 — §6.1 says "a tail" and never defines one

The only pre-registered tail in the inherited instruments is `RULE-G.md` §3's, a PERIOD-2 repeat detector. §6.1's own word is "PERIODIC", with no period named. A period-2-only detector cannot answer the question §6.1 asks, and on this population it demonstrably does not: on `STEENBRC` solve 0, `STEENBRG` solve 0, `YORKNET` solve 145 and `ACOPR57` solve 16 it finds a two-record tail where a period-4 or period-6 detector finds one covering 85–97% of the solve. **The reading applied** is RULE-G's own construction with its `2` replaced by a search over p ∈ [1, 64], taking the p that maximises the tail, ties to the smallest p — so it degenerates EXACTLY to RULE-G's tail whenever the winner is 2, which it is on 17 of the 21 solves. RULE-G's own period-2 answer is printed beside it on every solve.

### D2 — §6.1's `dx_inf == 0` conjunct is unreachable read strictly

A DROP pass takes no step and the probe writes `nan` into `dx_inf` and `alpha` alike; on a drop/add cycle that is HALF the passes. Read literally — every tail pass carries a PRESENT `dx_inf` equal to 0 — the `CYCLE` arm can never fire on any real trace, which is a defect and not a measurement. **The reading applied**: "exact zero motion on every tail pass" means NO tail pass moved the iterate — no present `dx_inf` is non-zero and their sum is exactly 0 — with the absent ones being passes that record no step at all, and the read-out naming which actions those are. Both numbers, `zero_motion_operative` and `zero_motion_strict`, are printed on every solve; under the strict reading all 19 `CYCLE` rows would read `SOMETHING ELSE` and the rule would have measured nothing. (The recorded value is `-0`; `-0.0 == 0.0` is exact, not a tolerance.)

### D3 — §6.1's `LONG WALK` clause is ambiguous

"no periodic tail, or a tail with non-zero motion: Σ‖Δx‖∞ > 0 over the tail and the working set does not repeat" parses either disjunctively or conjunctively, and "does not repeat" is undefined. **The reading applied** is the CONJUNCTIVE one, with "does not repeat" = the tail's signature sequence has NO minimal period — the exact complement of `CYCLE`'s "periodic", which is what makes the two arms disjoint. On this population the two parses differ on exactly ZERO solves, so the ambiguity changed no answer here; it is recorded because it could have.

**A fourth, smaller defect, of §5.1 rather than §6.1:** two of the fourteen byte fields §5.1 names — `elastic_activations` and `qp_failures_in_a_row_peak` — are **not printed by the pass-2 probe at all**. They are reported ABSENT on every cell by `map_p2.py` and are never counted as agreement. The whole-run gate that actually decided the re-take, `gate_p2a.py`, does not choose its fields at all: it compares the INTERSECTION of the two rows' own key sets, 58 fields, so the defect cost the gate nothing.

## What pass 2 settles, and what it does not

**SETTLED, OBSERVED.** The capped QPs at DEFAULT options ARE the zero-motion cycle M3b traced at `tr_init = +inf`. It is not an artefact of that configuration, it is not confined to the first QP, and it is not confined to one cell: it is 19 of 21 traced solves across 8 of the 10 traced cells, including both a main-walk solve and a restoration-sub-driver solve on the same cell. The counter shape pass 1 called INFERRED — `degenerate_steps ≈ minors/2`, `ws_adds ≈ ws_drops ≈ minors/2` — is now OBSERVED to be that cycle, on traces that reproduce the census rows byte for byte.

**NOT SETTLED.** Why the walk re-admits the constraint it just dropped is not measured here: the drop rule's multiplier sign and the ratio test's choice are recorded per pass, but no experiment in this census varies either. Nor is it measured what an anti-cycling rule would do — no remedy was built, run or selected, and RULE-P2 §8 forbids this section from licensing one.

---

*PASS 2's section above is generated by `wrappers/census_pass2_section.py --section census` from `tables/readouts-p2b.md`, `verdict-p2.txt`, `tables/mapping-p2b.txt` and `logs/gate-p2b.txt`; no figure in it is hand-transcribed. `RULE-P2.md` and `RULE-P2A.md` govern it and neither is edited. No outcome in this file licenses a behaviour change.*
