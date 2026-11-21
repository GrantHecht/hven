#!/usr/bin/env python3
"""PF-1b scratch diagnostic (never committed): env-tunable rung, re-admission tracer, rung trace."""
import sys
p = sys.argv[1] + "/src/qp/qp_engine.cpp"
s = open(p).read()

helpers = r'''
namespace hven::pf1b {
inline long max_rungs() { static const long v = std::getenv("HVEN_PF1B_RUNGS") ? std::atol(std::getenv("HVEN_PF1B_RUNGS")) : 2; return v; }
inline double factor() { static const double v = std::getenv("HVEN_PF1B_FACTOR") ? std::atof(std::getenv("HVEN_PF1B_FACTOR")) : 1e-2; return v; }
inline bool trace() { static const bool b = std::getenv("HVEN_PF1B_TRACE") != nullptr; return b; }
inline bool wide() { static const bool b = std::getenv("HVEN_PF1B_WIDE") != nullptr; return b; }
} // namespace hven::pf1b
'''
anchor_ns = "} // namespace hven::pf1\n"
assert s.count(anchor_ns) == 1
s = s.replace(anchor_ns, anchor_ns + helpers)

a_loop = "    for (Index iter = 0; !crossed_bounds && iter < eff_max_iter; ++iter) {\n        ++counters.minor_iters;\n"
assert s.count(a_loop) == 1
s = s.replace(a_loop, "    std::vector<long> pf1b_left(static_cast<std::size_t>(qp.mi()), -1);\n"
              "    std::vector<char> pf1b_prev(static_cast<std::size_t>(qp.mi()), 0);\n"
              "    for (Index j = 0; j < qp.mi(); ++j) pf1b_prev[(std::size_t)j] = in_working(ws, j) ? 1 : 0;\n" + a_loop)

a_ref = ("        const bool released_row_out = eff_opts.independent_working_set && dropped.active &&\n"
         "                                      dropped.is_ineq && !in_working(ws, dropped.idx);\n"
         "        refresh_shifts(qp, x, ai_row_norm1, lambda_i, ws, Aix, shift, counters, seen, eff_opts);\n")
assert s.count(a_ref) == 1
b_ref = ("        for (Index j = 0; j < qp.mi(); ++j) {\n"
         "            const char in = in_working(ws, j) ? 1 : 0;\n"
         "            if (pf1b_prev[(std::size_t)j] && !in) pf1b_left[(std::size_t)j] = (long)iter;\n"
         "            pf1b_prev[(std::size_t)j] = in;\n"
         "        }\n"
         + a_ref +
         "        bool pf1b_wide_hit = false; long pf1b_wide_row = -1;\n"
         "        for (Index j = 0; j < qp.mi(); ++j) {\n"
         "            const char in = in_working(ws, j) ? 1 : 0;\n"
         "            if (!pf1b_prev[(std::size_t)j] && in && pf1b_left[(std::size_t)j] >= 0) {\n"
         "                const bool via_dropped = dropped.active && dropped.is_ineq && dropped.idx == j;\n"
         "                if (!via_dropped) { pf1b_wide_hit = true; pf1b_wide_row = (long)j; }\n"
         "                if (pf1b::trace())\n"
         "                    std::fprintf(stderr, \"PF1BREADMIT minor=%ld row=%ld left_at=%ld gap=%ld via_dropped=%d dropped_active=%d dropped_idx=%ld kind=%d mu=%.3e\\n\",\n"
         "                                 (long)iter, (long)j, pf1b_left[(std::size_t)j], (long)iter - pf1b_left[(std::size_t)j], (int)via_dropped,\n"
         "                                 (int)dropped.active, (long)dropped.idx, (int)kind, eff_opts.dual_regularization);\n"
         "            }\n"
         "            pf1b_prev[(std::size_t)j] = in;\n"
         "        }\n")
s = s.replace(a_ref, b_ref)

a_rule = ("        if (released_row_out && in_working(ws, dropped.idx) &&\n"
          "            counters.dual_regularization_decreases < detail::kMaxDualRegDecreases &&\n"
          "            eff_opts.dual_regularization > 0.0) {\n"
          "            ++counters.dual_regularization_decreases;\n"
          "            eff_opts.dual_regularization *= detail::kDualRegDecreaseFactor;\n")
assert s.count(a_rule) == 1
b_rule = ("        if (((released_row_out && in_working(ws, dropped.idx)) || (pf1b::wide() && pf1b_wide_hit)) &&\n"
          "            counters.dual_regularization_decreases < pf1b::max_rungs() &&\n"
          "            eff_opts.dual_regularization > 0.0) {\n"
          "            ++counters.dual_regularization_decreases;\n"
          "            if (pf1b::trace()) std::fprintf(stderr, \"PF1BRUNG minor=%ld row=%ld wide=%d mu_from=%.3e\\n\", (long)iter,\n"
          "                (long)(released_row_out ? dropped.idx : pf1b_wide_row), (int)!(released_row_out && in_working(ws, dropped.idx)), eff_opts.dual_regularization);\n"
          "            eff_opts.dual_regularization *= pf1b::factor();\n")
s = s.replace(a_rule, b_rule)
open(p, "w").write(s)
print("patched pf1b", p)
