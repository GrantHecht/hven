#!/usr/bin/env python3
# M8 AP-5 G3: applies mutation <id> to the scratch source copy (the mutation table of ../DECLARATION.md).
# usage: mutate.py <src-root> <id>
import sys, os
root, mid = sys.argv[1], sys.argv[2]
SQP = 'src/drivers/sqp_solver.cpp'
IPM = 'src/drivers/ipm_solver.cpp'
IMPL = 'include/hven/detail/drivers/ipm_solver_impl.h'
PROB = 'src/model/problem.cpp'
M = {
 # --- interior-point
 'a': (IMPL, "return opts_.return_best || static_cast<bool>(iteration_callback_) ||\n               stop_token_.stop_possible();",
       "return opts_.return_best;", 1),
 'b': (IPM, "            // STOP PATH ONLY (M8 AP-5 G3, design page section 3.3): the point being\n            // returned is scored into the best tracker before the exit, so a stop at\n            // an improving point reports it as best. The ordinary tracking runs after\n            // an iteration's step, which a stop never reaches. Not in restoration,\n            // where the tracker is suspended.\n            if (this->best_tracking_armed() &&",
       "            if (false && this->best_tracking_armed() &&", 1),
 'c': (IPM, "if (callback_stop && this->stop_return_best_ && best_recorded) {", "if (false && callback_stop && this->stop_return_best_ && best_recorded) {", 1),
 'd': (IPM, "        this->stop_return_best_ = action == CallbackAction::kStopReturnBest;", "        this->stop_return_best_ = false;", 1),
 'e': (IPM, "best_recorded && ExitCode != SolveStatus::kOptimal &&\n                             ExitCode != SolveStatus::kAcceptable && this->best_tracking_armed());",
       "best_recorded && this->best_tracking_armed());", 1),
 'f': (IPM, "    snap.step_norm = this->best_step_norm_scratch_;", "    snap.step_norm = 0.0;", 1),
 'g': (IPM, "    rows.best_ce = std::move(pt.ce);\n    rows.best_ci = std::move(pt.ci);", "    (void)pt.ce;", 1),
 'h': (IPM, "    this->solve_exit_stash_point_ = stash_exit || stop_best_applied;", "    this->solve_exit_stash_point_ = stash_exit;", 1),
 'i': (IPM, "        if (this->best_tracking_armed() &&\n            !(this->restoration_ && this->restoration_->is_active())) {\n            this->track_best_iterate(iters.back(), i, XSL, RHS, eval_mu, last_step_norm,",
       "        if (opts_.return_best &&\n            !(this->restoration_ && this->restoration_->is_active())) {\n            this->track_best_iterate(iters.back(), i, XSL, RHS, eval_mu, last_step_norm,", 1),
 'j': (IPM, "        if (this->iteration_callback_ || this->best_tracking_armed()) {\n            last_step_norm", "        if (this->iteration_callback_) {\n            last_step_norm", 1),
 'k': (IPM, "    snap.iteration = static_cast<Index>(this->best_row_scratch_.iter);", "    snap.iteration = 0;", 1),
 # --- SQP
 'l': (SQP, "(!is_restoration_sub_driver_ && (iteration_callback_ || budget.stop.stop_possible()))) {", "false) {", 1),
 'm': (SQP, "                if (st.interrupted && st.stop_return_best && !mj.token_exit && st.mb.snap_valid) {\n                    return finish_best(SolveStatus::kInterrupted);\n                }",
       "                (void)0;", 1),
 'n': (SQP, "            st.mb.snap.step_norm = exported.step_norm;", "            (void)exported;", 1),
 'o': (SQP, "    if (!st.mb.snap_valid || status == SolveStatus::kOptimal ||\n        status == SolveStatus::kAcceptable) {",
       "    if (!st.mb.snap_valid) {", 1),
 'p': (SQP, "                        st.stop_return_best = action == CallbackAction::kStopReturnBest;\n                    }\n                    return action;",
       "                    }\n                    return action;", 1),
 'q': (SQP, "            st.out.returned_pre_restoration_point = false;\n            return finish(seam, std::move(st.out), status, st.mb.x,", "            return finish(seam, std::move(st.out), status, st.mb.x,", 1),
 'r': (SQP, "        if (column_units_ != nullptr) {\n            if (cp.x.size() == column_units_->size()) {", "        if (false && column_units_ != nullptr) {\n            if (cp.x.size() == column_units_->size()) {", 1),
 's': (SQP, "            if (!mj.converged && mj.interrupted_exit && stop_returns_best) {\n                return finish_best(SolveStatus::kInterrupted);\n            }",
       "            (void)0;", 1),
 't': (SQP, "            if (mj.interrupted_exit && stop_returns_best) {\n                publish_stop(StopReason::kCallbackStop, std::string());\n                check_major_pushed_once(iter);\n                return finish_best(SolveStatus::kInterrupted);\n            }\n            publish_stop(mj.interrupted_exit ? interrupt_reason(mj) : st.resto.reason,\n                         mj.interrupted_exit ? interrupt_detail(st) : st.resto.detail);\n            check_major_pushed_once(iter);\n            return finish(\n",
       "            publish_stop(mj.interrupted_exit ? interrupt_reason(mj) : st.resto.reason,\n                         mj.interrupted_exit ? interrupt_detail(st) : st.resto.detail);\n            check_major_pushed_once(iter);\n            return finish(\n", 1),
 'u': (SQP, "            if (mj.interrupted_exit && stop_returns_best) {\n                publish_stop(StopReason::kCallbackStop, std::string());\n                check_major_pushed_once(iter);\n                return finish_best(SolveStatus::kInterrupted);\n            }\n            publish_stop(mj.interrupted_exit ? interrupt_reason(mj) : st.resto.reason,\n                         mj.interrupted_exit ? interrupt_detail(st) : st.resto.detail);\n            check_major_pushed_once(iter);\n            return finish(seam, std::move(st.out),\n",
       "            publish_stop(mj.interrupted_exit ? interrupt_reason(mj) : st.resto.reason,\n                         mj.interrupted_exit ? interrupt_detail(st) : st.resto.detail);\n            check_major_pushed_once(iter);\n            return finish(seam, std::move(st.out),\n", 1),
 # --- the row-space map of best_iterate
 'v': (PROB, "        if (result.best_iterate) {\n            map_rows(", "        if (false && result.best_iterate) {\n            map_rows(", 1),
}
f, old, new, count = M[mid]
p = os.path.join(root, f)
s = open(p).read()
c = s.count(old)
assert c == count, (mid, c)
open(p, 'w').write(s.replace(old, new))
print("applied", mid, f)
