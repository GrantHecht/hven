#!/usr/bin/env python3
"""NC2 scratch: insert (or strip) env-gated stderr probes in src/qp/qp_engine.cpp.
Every inserted line carries the marker NC2PROBE; `probe.py strip` removes them.
Never committed."""
import sys

PATH = sys.argv[2] if len(sys.argv) > 2 else 'src/qp/qp_engine.cpp'
MARK = '// NC2PROBE'


def strip(src):
    return ''.join(l for l in src.splitlines(keepends=True) if MARK not in l)


HEAD = '''#include <cstdio> // NC2PROBE
#include <cstdlib> // NC2PROBE
namespace { inline bool nc2p_on() { static const bool on = std::getenv("HVEN_NC2_PROBE") != nullptr; return on; } double nc2_zs = 0.0; } // NC2PROBE
namespace { inline bool nc2_on(const char *v) { return std::getenv(v) == nullptr; } } // NC2PROBE
#define NC2P(...) do { if (nc2p_on()) std::fprintf(stderr, __VA_ARGS__); } while (0) // NC2PROBE
#define NC2EQ() (qp.me() > 0 ? (qp.Ae * x - qp.be).lpNorm<Eigen::Infinity>() : 0.0) // NC2PROBE
'''

# (anchor text, position 'after'|'before', lines to insert); anchor must be unique.
EDITS = [
    ('#include <hven/detail/qp/qp_engine.h>\n', 'after', HEAD),
    ('        const Vec p = eqp.x - x;\n', 'after',
     '        NC2P("NC2 PASS it=%ld verdict=%d eq=%.3e wsz=%ld pinf=%.3e pnorm=%.3e elim=%d ptev=%ld\\n", (long)iter, (int)verdict, NC2EQ(), (long)(me + (Index)ws.active_ineq().size() + (n - ws.num_free())), p.lpNorm<Eigen::Infinity>(), p.norm(), eliminated_only ? 1 : 0, (long)seen.pt_events); // NC2PROBE\n'),
    ('            if (dropped_one && eff_opts.independent_working_set) {\n', 'before',
     '            if (dropped_one) { nc2_zs = last_drop.is_ineq ? lambda_i(last_drop.idx) : z(last_drop.idx); NC2P("NC2 DROP it=%ld %s %ld from=%d mult=%.4e exempt_worst=%.3e\\n", (long)iter, last_drop.is_ineq ? "row" : "pin", (long)last_drop.idx, (int)last_drop.from, nc2_zs, exempt_worst); } // NC2PROBE\n'),
    ('            if (!dropped_one) {\n', 'after',
     '                NC2P("NC2 TERM it=%ld verdict=%d exempt_worst=%.4e eq=%.3e\\n", (long)iter, (int)verdict, exempt_worst, NC2EQ()); // NC2PROBE\n'),
    ('                const RideOutcome outcome = ride_negative_curvature(\n', 'before',
     '                { const Vec gx = qp.H.selfadjointView<Eigen::Upper>() * x + qp.g; NC2P("NC2 GATE it=%ld curv=%.4e tol=%.4e pnorm=%.4e slope=%.4e slope_tol=%.4e dd=%.4e leaves_tol=%.4e zs=%.4e pred=%.4e nsok=%d aep=%.3e lamaep=%.3e lam_e=%.3e pinsp=%.3e verdict=%d\\n", (long)iter, curvature, curvature_tol, p.norm(), gx.dot(p), detail::kEngineDenomTol * gx.norm() * p.norm(), dropped_directional(qp, dropped, p), detail::kEngineDenomTol * dropped_gradient_norm(qp, dropped) * p.norm(), nc2_zs, nc2_zs * (dropped.is_ineq ? qp.Ai.row(dropped.idx).dot(p) : p(dropped.idx)), ride_stays_in_working_set(qp, ws, p) ? 1 : 0, me > 0 ? (qp.Ae * p).lpNorm<Eigen::Infinity>() : 0.0, me > 0 ? lambda_e.dot(qp.Ae * p) : 0.0, me > 0 ? lambda_e.lpNorm<Eigen::Infinity>() : 0.0, [&] { double m = 0.0; for (Index i = 0; i < n; ++i) if (ws.bound_state()[(std::size_t)i] != BoundState::kFree) m = std::max(m, std::abs(p(i))); return m; }(), (int)verdict); } // NC2PROBE\n'),
    ('                if (outcome == RideOutcome::kUnbounded) {\n', 'before',
     '                NC2P("NC2 RIDE it=%ld outcome=%d\\n", (long)iter, (int)outcome); // NC2PROBE\n'),
    ('                        ++counters.ride_declines_fallthrough_blocked;\n', 'after',
     '                        NC2P("NC2 UNDO it=%ld %s %ld\\n", (long)iter, dropped.is_ineq ? "row" : "pin", (long)dropped.idx); // NC2PROBE\n'),
    ('                ++counters.dep_border_retreats;\n', 'after',
     '                NC2P("NC2 RETREAT it=%ld\\n", (long)iter); // NC2PROBE\n'),
    ('                ++counters.ws_exchanges;\n', 'after',
     '                NC2P("NC2 EXCH it=%ld out=%s %ld in=%s %ld alpha=%.3e ride=%d unlocked=%d\\n", (long)iter, xpick.j_is_ineq ? "row" : "pin", (long)xpick.j, kind == BlockKind::kIneq ? "row" : "pin", (long)block_idx, alpha, ride_blocked ? 1 : 0, xpick.unlocked ? 1 : 0); // NC2PROBE\n'),
    ('        refresh_shifts(qp, x, ai_row_norm1, lambda_i, ws, Aix, shift, counters, seen, eff_opts);\n    }\n', 'before',
     '        NC2P("NC2 STEP it=%ld kind=%d idx=%ld alpha=%.3e dx=%.3e ride=%d eq=%.3e\\n", (long)iter, (int)kind, (long)block_idx, alpha, alpha * sdir.lpNorm<Eigen::Infinity>(), ride_blocked ? 1 : 0, NC2EQ()); // NC2PROBE\n'),
    ('        if (ride_blocked) {\n            x += alpha * sdir; // a ride step: no landing point, uncapped\n', 'before',
     '        if (kind != BlockKind::kNone) { const double aesd = qp.me() > 0 ? (qp.Ae * sdir).lpNorm<Eigen::Infinity>() : 0.0; const double eqx = qp.me() > 0 ? (qp.Ae * eqp.x - qp.be).lpNorm<Eigen::Infinity>() : 0.0; double clamp = 0.0; if (kind == BlockKind::kBound) { const double b = block_state == BoundState::kAtUpper ? qp.upper(block_idx) : qp.lower(block_idx); clamp = x(block_idx) + alpha * sdir(block_idx) - b; } NC2P("NC2 PRESTEP it=%ld |Ae*sdir|=%.3e |Ae*eqpx-be|=%.3e alpha=%.3e clamp=%.3e rate=%.3e exch=%d lam_e=%.3e nfree=%ld\\n", (long)iter, aesd, eqx, alpha, clamp, kind == BlockKind::kBound ? sdir(block_idx) : 0.0, xpick.found ? 1 : 0, me > 0 ? lambda_e.lpNorm<Eigen::Infinity>() : 0.0, (long)ws.num_free()); } // NC2PROBE\n'),
    ('        const Vec p = eqp.x - x;\n', 'after',
     '        if (const char *dit = std::getenv("HVEN_NC2_DUMPIT"); dit != nullptr && std::atol(dit) == (long)iter) { FILE *df = std::fopen(std::getenv("HVEN_NC2_DUMPFILE"), "w"); std::fprintf(df, "x %ld\\n", (long)n); for (Index i = 0; i < n; ++i) std::fprintf(df, "%.17g\\n", x(i)); std::fprintf(df, "eqpx %ld\\n", (long)n); for (Index i = 0; i < n; ++i) std::fprintf(df, "%.17g\\n", eqp.x(i)); std::fprintf(df, "bs %ld\\n", (long)n); for (Index i = 0; i < n; ++i) std::fprintf(df, "%d\\n", (int)ws.bound_state()[(std::size_t)i]); std::fprintf(df, "aw %zu\\n", ws.active_ineq().size()); for (Index r : ws.active_ineq()) std::fprintf(df, "%ld\\n", (long)r); std::fclose(df); } // NC2PROBE\n'),
    ('#ifdef HVEN_TESTING\n    detail::testing::QpWalkObserver::end_solve', 'before',
     '    NC2P("NC2 END status=%d minors=%ld eq=%.3e\\n", (int)status, (long)counters.minor_iters, NC2EQ()); // NC2PROBE\n'),
]


def insert(src):
    for anchor, pos, text in EDITS:
        k = src.count(anchor)
        if k != 1:
            sys.exit(f'anchor count {k}: {anchor!r}')
        i = src.index(anchor)
        if pos == 'after':
            j = i + len(anchor)
            src = src[:j] + text + src[j:]
        else:
            src = src[:i] + text + src[i:]
    return src


src = open(PATH).read()
if sys.argv[1] == 'strip':
    out = strip(src)
else:
    out = insert(strip(src))
open(PATH, 'w').write(out)
print('ok', sys.argv[1])
