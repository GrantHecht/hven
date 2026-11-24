# p1a-diag STATE (uncommitted)
Task: P-1a diagnosis (design .superpowers/p1a-design-2026-11-24.md (a)(b)(e)1). Branch p1a-diag from origin/m8 fd46cb1f4. Never shipped.
Done: ported P-1 wip+dev3 (commits b532d58d1, 83471c3c9); env-variant scaffolding (.scratch/legs/scaffold.py) in walk_engine.cpp.
Env: HVEN_P1_GLOBAL=1 (=P-1 live arm) | HVEN_P1_MODE=live|every|face|seedonly|trig; HVEN_P1_KEEP, _SEED, _NORUNG, _NOSAFE, _FACEFULL, _STRICTSAFE, _YMAX=v, _TRACE=1|2
Builds: .scratch/rel (Release, tests+bench). Lane: TBD.
## Findings so far
- PF1 (both algebras): base 4758 (rung); baseNR cap 20000; P-1 live = every(copy) = 4898/3735 no rung (reproduces p1-prox exactly on today's head);
  everyKeep 9336 + rung (WORSE: keep-previous refuted for PF1); face(safeguarded) never bootstraps under NR: upd=0 ref=4948 (safeguard refuses exactly the footprint passes); trig 4757 + rung.
- Build: .scratch/rel OK at 3ed075839.
- PF1 round 2: face+NOSAFE(+NORUNG) 4848/3684 no rung, lam_max 3e4 (best); every+NOSAFE+NR 5486, lam 2.7e6; facefull+NOSAFE 5830 lam 1.2e6; every+STRICTSAFE == every; face+safeguard never bootstraps.
- Lane: hven-bench-p1a (origin/i19 + never- prefix + qp_dump keys), build-p1a, prefix .scratch/prefix via .hven/install-p1adiag00000 symlink. Runner .scratch/legs/{corun.py,arms.sh,qprun.sh}.
- ALLINITC.D / DEMBO7.D losses reproduce bit-for-bit under P1live on today's head (max_iter 300 obj 30.4952646; numerical_error).
- ALLINITC mechanism: QPs identical to base through m20; under P1 x0 coefficient of the degenerate eq row (=-2*x0) stops halving at m21 (1.8e-6 -> 4.5e-6) and cycles period 5; in-QP pass-1 centring removes mu*lambda (1.3e-8) >> |be| (1e-12) stabilization. faceNS identical to base (centre never used: no drop).
- DEMBO7 mechanism: m0 rung0 elastic QP: base s3 (17 minors); P1 every-pass centre grows lambda to 1.6e19 (PMM on rows inconsistent at mu scale), safeguard (mu|lambda| allowance, scale-invariant) never refuses, exits s0 f=-46.88; SQP path changes -> later main QPs s3 -> numerical_error. YMAX=1e6 or face cadence -> base behaviour.
- TRAIN: every-pass arms all lose ALLINITC (YMAX/keep irrelevant); face arms 0 losses; seed arms lose CRESC100. faceSB (face+SAFEBOOT+NR+Y1e6) QM 1.011 LA 0.990; faceSBfp (keep WA-1 fp branch) QM 0.999 LA 0.979.
- ctest: default 0/4114; P1live 39; faceNSY 20; faceNSYfp 10 (DALLASM qp7 FALSE kOptimal f off oracle 0.015); faceSBfp 9 (qp7 fixed; SDL_DALLASM_QN_qp10 refactorize cap 1960 = SAFEBOOT bootstrap/refuse/reset toggle 323x); faceSB 19.
- qp26m42: faceSBfp kOptimal 43 minors f=-1458.316 (A2 met); faceSB (NR) cap 8240. qp25 NR -> different optimum f=-1398.6 (fixture pins -5.80).
- SDL qp10: faceSBfpK (KEEP) = s3 like base, no toggle.
