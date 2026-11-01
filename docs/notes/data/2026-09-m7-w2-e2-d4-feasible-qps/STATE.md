# E2-D4 STATE (live; for a successor)
Scratch: /home/ghecht/Projects/hven/.scratch/m7w2e2d4 (tree = git archive 763cda00 + D3 instrument.py + tools/instrument_d4.py + tools/fixes_d4.py;
build-rel -> install; bench = hven-bench ea1aa4e copy pinned to install; replay-build = tools/replay/qp_replay).
Runners: tools/{leg,cell,run,build,replay_trace,replay_matrix,oracle_all,addseq_all}.sh; python: tools/{d4load,qp_oracle,addseq,tracesum}.py
venv: /tmp/claude-1000/-home-ghecht-Projects-hven/0c17d1d8-660f-4914-9feb-e49a88005c27/scratchpad/venv (scipy, highspy, clarabel, osqp)
Done: 7 cells run with dumps (YORKNET_def, DALLASM_def, DALLASM_f4, ACOPP57_def, STEENBRB_def, ACOPR57_def, SAWPATH_def) = 31 QP dumps;
  cells reproduce re-take-2/D3 columns; replay 31/31 status+minors identical (facts 30/31; ACOPR57 qp3 +1 = hot K0 reuse in-cell).
  oracle: replays/oracle_all1.txt (+ .summary); traces replays/base/*.trace; addseq first exact dependency: replays/base/addseq_summary.txt
Findings so far (observed):
  DALLASM feasible QPs: exact (structural) dependent pins admitted as independent, engine rho 3e-6..3e-5 (border path) -> leaking set.
  STEENBRB QPs: exact structural dependent pins admitted with rho 5e-4 .. 1e18 (stalled/broken test) -> 1/mu multipliers, drop/re-add cycles.
  ACOPP57: reduced Hessian on the final (= optimal) face PSD singular (min eig 3e-12, next 50); mu-regularized inertia kWrong (D5).
  YORKNET elastic: 240 zero steps at the start point, all members entered-at-point => no admissible member (locks), N3. No exact dep.
  SAWPATH elastic: mandatory Ae ill-conditioned (row norms 1.7..6e10, normalized sigma_min 1.7e-11); screen -> cond1 -> cert rejected.
  ACOPR57 primary: shift-seeded over-count (+49) trimmed one per pass by the entry detector; trims the pins, they re-block, N3.
In flight: replays/<cfg>/ matrix (f5, structfirst, greys+greyu, unlock, leakpass, e2off).
Next: cells under the winning toggles; U0 replay + HS corpus quick check under the lock; REPORT.
## Update 10:50
- Replay matrix done: replays/<cfg>.summary (f5, structfirst, greys+greyd? no: greys+greyu, unlock, leakpass, e2off); focus sets replays/focus-<cfg>.summary.
- KEY: E2-OFF (--e2 0) SOLVES YORKNET qp0/qp1, SAWPATH qp0/qp2, DALLASM_f4 qp1, STEENBRB qp0, ACOPR57 qp0-3 (f = Clarabel f*). E2-ON fails them.
- leakpass (no violation check) HARMFUL: false kInfeasible on feasible DALLASM qp2/3, YORKNET qp0/1. coefrho HARMFUL (ACOPR57 qp0/1, YORKNET qp1). unlockall HARMFUL.
- Best combo D = structfirst,greys,greyd,unlock,f5,f5s,screenadmit: optimal on YORKNET qp0,qp1; STEENBRB qp0; ACOPR57 qp0,qp1,qp4; ACOPP57 qp0.
  Still failing: DALLASM (all), STEENBRB qp1-3 (cap / NaN), ACOPR57 qp2, qp8, SAWPATH qp0/qp2.
- Baseline U0 (no toggle) = M6 close within gate; HS TRAIN 21/21 optimal: u0/base-*, hs/base.summary.
- In flight: v5 build (leakpass2 = pass-over of a dependent leak-rate blocker WITH the within check) + focus D+leakpass2.
- Next: cells under D (10 TRAIN cells incl READING6/ZAMB2-8/SMBANK), per-toggle U0+HS, patch = diff tree vs git archive 763cda00.
## Final (13:45) -- handed back
- The REPORT.md write was refused by the harness (subagent); the full narrative is in the hand-back for the settler to place.
- Readout populated: dumps/ (+FORMAT.txt), transcripts/, replays/ (summaries, oracle, addseq, traces-*.tar.gz), u0/, hs/, runners/, d4-patch.diff.txt.
- STILL RUNNING at hand-back (mine, bounded by the corpus's own 900 s/phase budgets, NOT killed): the U0 leg for
  structfirst,greys,greyd (flock pid 3398676, corpus pid 3512384, child on f7_n800_path_warm). When it ends, tools/chain5.sh
  runs its HS leg and writes ALL_DONE to .scratch/m7w2e2d4/logs/u0hs5.log; u0/structfirst+greys+greyd-compare.txt appears.
  Observed already in its partial CSV: f7_n1000_path_warm Optimal -> dnf_setup, the other 25 cells identical to base.
