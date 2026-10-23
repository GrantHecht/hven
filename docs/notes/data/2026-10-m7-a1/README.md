# M7 W2 A1 — design-round diagnostics

Evidence for `docs/notes/2026-10-m7-w2-a1-stopping-mu-design.md`. Light diagnostic
runs only: no TRAIN cell, no HOLDOUT cell, no sweep.

## Provenance

* hven: branch `w2-a1` at `3d27c29f` (m7 head), unmodified. Release build of
  `libhven.a` from that tree (`scripts/build.sh`), the repository's uniform flag
  regime (`-O3 -march=native -ffast-math -fno-finite-math-only`, as the bench
  targets compile).
* Toolchain: clang 22.1.8 (Fedora 22.1.8-4.fc44); Intel oneAPI MKL (`latest`).
* Hardware: AMD Ryzen 7 5800X3D.
* Date: 2026-10-23.
* Terms: every leg under `flock /tmp/box-build.lock`, one process at a time,
  `MKL_NUM_THREADS=1`, `OMP_NUM_THREADS=1`, `pgrep -a hven_bench_cell` empty before
  each leg. **No wall-clock figure is recorded or quoted**; the transcripts carry
  counters, iterates and residuals only.
* Accelerate: UNOBSERVED (MKL only).

## The instrument

`probe/a1_probe.cpp` links the built `libhven.a` and includes the bench headers
`bench/ipqp_e1_arm.h` and `bench/corpus_cells.h`. It sets public options only and
attaches a public `TraceSink` for the per-iteration `ipqp.iter` events. Modes:

* `e1 <cell> <slack> <cap>` — one E1 cell, tier-direct (`IpqpEngine::solve`, the
  arm's own `tr_radius = inf`), the per-iteration trace, the face scored against
  the constructed truth (over- / under-committed, uncertain), `z·s*/μ` over the
  over-committed rows, and the arm's own END-TO-END row (`e1arm::solve`).
* `e1demote <cell> <slack> <cap>` — the same solve; the returned face handed to
  `QpEngine::refine_on_face` exactly as the driver's route 2 hands it, under three
  post-hoc demotion variants: `none` (today), `band` (committed-active rows with
  `z ≤ kIpqpTightBandFactor · kIpqpWeakActiveFactor · √μ` reported uncertain),
  `oracle` (exactly the truly inactive committed rows; an upper bound). Rule A
  (`bi − Ai x ≤ 1e-8 · row_scale`) scored on the polished point when the polish
  accepts, on the tier's point otherwise.
* `gate8 <slack>` — the gate-8 family of `tests/sqp/test_ipqp_certification.cpp:529-559`
  rebuilt verbatim, μ_stop measured as `gate8_mu_stop()` does, swept in Σ for both
  curvature signs.
* `f7 <walk|ipm>` — F7 corpus neutral + physics-informed cells (bound window;
  path window with `A1_F7_PATH=1`): the SQP census `weak_active_peak` /
  `near_active_peak` and per-major counts, plus the tier's band / noise /
  uncertain counters.

`slack` is `ipqp_converge_slack` (shipped 100); `cap` sets `ipqp_hard_iter_cap`
and `ipqp_max_iter` to the value and `ipqp_max_factorizations` to 4× (the arm's own
`--hard-cap` convention); `0` keeps the shipped budgets.

## Reproduction of the record

The slack-100 / cap-60 rows reproduce the committed
`docs/notes/data/2026-08-m6-w1-acceptance/a4-gate-mu1e-2.csv` face counts, and the
RED cell's row on every scored column (status `max_iter`, 60 iterations, 11 649
misclassified, 2 351 uncertain, Rule A 10 946 / 178 / 5 124, μ 3.331e-8).

## Files

`out/` — one transcript per leg, named `<mode>-<cell>-s<slack>-c<cap>.txt`;
`gate8-slack{100,1}.txt`; `f7-{walk,ipm}.txt` (bound window, 10 cells each);
`f7-path-n1000-{walk,ipm}.txt` (path window, N ≤ 1000, 6 cells each, with
`A1_F7_MAXN=1000`); `f7-path-walk-stopped-at-n2000.txt` (the path-window walk
leg, stopped by the lane at the N = 2000 neutral cell after 14 minutes — its own
process, by exact pid; the ipm leg behind it never ran). The first bound-window
attempt (`scripts/run_probe.sh`, last two legs) included the path window and was
stopped the same way at the same cell; `scripts/run_probe2.sh` re-ran the bound
window and those transcripts are the ones here. The probe binaries `a1_probe` …
`a1_probe4` are successive builds of the one source; the source here is the final
form (the earlier builds lacked the `e1demote` mode and the two `A1_F7_*` filters,
which change nothing in the modes they share). `scripts/` — the build and run legs
as executed.
