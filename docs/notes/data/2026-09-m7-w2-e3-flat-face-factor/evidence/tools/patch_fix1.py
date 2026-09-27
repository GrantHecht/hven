#!/usr/bin/env python3
"""NC2 fix round 1: apply item 1(b) (the zero-length ride) and item 2 (kPtNoise) to the tree.

usage: patch_fix1.py [1b|2|all]
Every edit is an exact single-occurrence replacement; a miss aborts with no write.
"""
import sys

W = '/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9'
H = W + '/include/hven/detail/qp/qp_engine.h'
C = W + '/src/qp/qp_engine.cpp'

EDITS_1B = [
    # --- RideBlocker: the motion the ride made, and what the struct now carries -----
    (H, """    // The blocker section 4c's ride found and did not admit (kDependentBlocker):
    // the add site's rule decides it along `dir`, uncapped.
    struct RideBlocker {
        BlockKind kind = BlockKind::kNone;
        Index idx = -1;
        BoundState state = BoundState::kFree;
        double alpha = 0.0;
        Vec dir;
        detail::DepTest test;
    };
""", """    // The blocker section 4c's ride found. On kDependentBlocker the ride did NOT
    // admit it and the add site's rule decides it along `dir`, uncapped. On
    // kStepped the ride admitted it already, and `dir` is left empty: what the
    // caller needs there is the MOTION the ride made (4c, FINITENESS).
    struct RideBlocker {
        BlockKind kind = BlockKind::kNone;
        Index idx = -1;
        BoundState state = BoundState::kFree;
        double alpha = 0.0;
        // alpha * ||dir||inf -- the quantity the ordinary add path tests against
        // step_tol to decide whether the iterate moved.
        double dx_inf = 0.0;
        Vec dir;
        detail::DepTest test;
    };
""",),
    # --- the ride reports its blocker and its motion on the stepped path ------------
    (C, """#ifdef HVEN_TESTING
    // The blocker a RIDE admitted. QpCounters folds it into ws_adds with every
    // other addition (that field's own doc says so), which is one of the six
    // mechanisms M3a addendum F could not separate.
""", """    // WHAT THE RIDE DID, for the caller's point memory (4c, FINITENESS): the
    // blocker it admitted and the motion it made. `dir` is deliberately not
    // copied -- the ride has already taken it.
    blocked.kind = kind;
    blocked.idx = block_idx;
    blocked.state = block_state;
    blocked.alpha = alpha;
    blocked.dx_inf = alpha * dir.lpNorm<Eigen::Infinity>();
#ifdef HVEN_TESTING
    // The blocker a RIDE admitted. QpCounters folds it into ws_adds with every
    // other addition (that field's own doc says so), which is one of the six
    // mechanisms M3a addendum F could not separate.
""",),
    # --- the caller: a zero-length ride is a zero-motion event ----------------------
    (C, """                if (outcome == RideOutcome::kStepped) {
                    degenerate_run = 0; // observation only -- the ride moved x
                    pt_clear(seen);
""", """                if (outcome == RideOutcome::kStepped) {
                    // Observation only, and broken by a ride whether or not it
                    // moved (QpCounters::degenerate_run_max).
                    degenerate_run = 0;
                    // A RIDE THAT DID NOT MOVE IS A ZERO-MOTION EVENT (4c,
                    // FINITENESS), on the ordinary add path's own test: the point
                    // memory survives it and the blocker is admitted through it,
                    // so the release/re-entry cap bounds a drop -> zero-length
                    // ride -> drop cycle.
                    if (ride.dx_inf > step_tol) {
                        pt_clear(seen);
                    }
                    (void)pt_admit(seen, counters, ride.kind == BlockKind::kIneq, ride.idx);
""",),
]

EDITS_2 = [
    (C, """                        if (sign_veto) {
                            // THE FLAT FACE (4c): the predicted slope did not
                            // resolve either, so this multiplier is noise.
                            pt_mark(seen, dropped.is_ineq, dropped.idx, kPtNoise);
                        }
""", """                        if (sign_veto) {
                            // THE FLAT FACE (4c): the mark's premise is that the
                            // MULTIPLIER is below the resolution of the slope it
                            // was meant to predict, and that is what is tested --
                            // not |s|, which is this quantity times the cosine of
                            // p against the released face.
                            const double g_norm =
                                (qp.H.selfadjointView<Eigen::Upper>() * x + qp.g).norm();
                            if (dropped.violation * dropped_gradient_norm(qp, dropped) <=
                                detail::kEngineDenomTol * g_norm) {
                                pt_mark(seen, dropped.is_ineq, dropped.idx, kPtNoise);
                            }
                        }
""",),
]


def apply(edits):
    for path, old, new in edits:
        s = open(path).read()
        k = s.count(old)
        if k != 1:
            sys.exit(f'ABORT: {k} occurrences in {path} of:\n{old[:120]}')
        open(path, 'w').write(s.replace(old, new))
        print(f'patched {path.split("/")[-1]}: {old.splitlines()[0].strip()[:60]}')


which = sys.argv[1] if len(sys.argv) > 1 else 'all'
if which in ('1b', 'all'):
    apply(EDITS_1B)
if which in ('2', 'all'):
    apply(EDITS_2)
print('ok', which)
