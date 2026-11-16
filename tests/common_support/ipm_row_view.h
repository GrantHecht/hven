// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// ipm_row_view.h -- test support: an interior-point `iter` row read back under the v0
// engine column names, built from the v1 `IterationRecord`. Tests that asserted on the old
// per-iterate record (`IpmIterTraceEvent::iterate`) read it through this view; a value the
// record carries as `null` reads as the v0 default for it (alphas 1, counts 0, `h_facs` -1).

#include <cmath>
#include <vector>

#include <hven/drivers/trace.h>

namespace hven::test {

struct IpmRowView {
    int iter = 0;
    int phase = 0;
    double mu = 0.0;
    double prim_obj = 0.0;
    double barr_obj = 0.0;
    double kkt_inf = 0.0;
    double barr_inf = 0.0;
    double econ_inf = 0.0;
    double icon_inf = 0.0;
    double pen_par1 = 0.0;
    double pen_par2 = 0.0;
    int ls_iters = 0;
    double alpha_p = 1.0;
    double alpha_d = 1.0;
    double alpha_t = 1.0;
    double h_pert = 0.0;
    int h_facs = -1;
    double h_pert_cum = 0.0;
    double prox_reg_dual = 0.0; ///< 0 when proximal mode is off (v0: negative).
    double merit_val = 0.0;
    bool accepted = false;
    int first_rejection_iter = -1;
    int eval_exceptions = 0;
    double step_norm = std::nan("");
    bool soc = false;
    bool watchdog = false;
    bool rolled_back = false;
    bool recovering = false;
    Index factorizations = 0;
};

inline IpmRowView make_ipm_row_view(const IterationRecord &r, double obj_scale) {
    IpmRowView v;
    v.iter = static_cast<int>(r.iteration);
    v.kkt_inf = r.stationarity * obj_scale;
    v.barr_inf = r.complementarity * obj_scale;
    v.prim_obj = r.f ? *r.f * obj_scale : (r.f_recovery ? *r.f_recovery : 0.0);
    v.accepted = r.accepted.value_or(false);
    v.step_norm = r.step_norm ? *r.step_norm : std::nan("");
    v.soc = r.soc;
    v.watchdog = r.watchdog;
    v.rolled_back = r.rolled_back;
    v.recovering = r.recovering;
    v.factorizations = r.factorizations;
    if (r.ipm) {
        const IpmIterationRow &i = *r.ipm;
        v.phase = static_cast<int>(i.phase);
        v.mu = i.mu;
        v.econ_inf = i.feasibility_eq;
        v.icon_inf = i.feasibility_ineq;
        v.pen_par1 = i.merit_penalty_1;
        v.pen_par2 = i.merit_penalty_2;
        v.ls_iters = static_cast<int>(i.line_search_trials.value_or(0));
        v.alpha_t = i.alpha_scale.value_or(1.0);
        v.alpha_p = i.alpha_primal ? (v.alpha_t > 0.0 ? *i.alpha_primal / v.alpha_t : 0.0) : 1.0;
        v.alpha_d = i.alpha_dual ? (v.alpha_t > 0.0 ? *i.alpha_dual / v.alpha_t : 0.0) : 1.0;
        v.h_pert = i.regularization_primal.value_or(0.0);
        v.h_facs = i.inertia_corrections ? static_cast<int>(*i.inertia_corrections)
                                         : (i.alpha_scale ? -1 : 0);
        v.h_pert_cum = i.regularization_primal_total.value_or(0.0);
        v.prox_reg_dual = i.regularization_dual.value_or(0.0);
        v.barr_obj = i.barrier_objective.value_or(0.0);
        v.merit_val = i.merit.value_or(0.0);
        v.first_rejection_iter = static_cast<int>(i.first_rejected_trial.value_or(-1));
        v.eval_exceptions = static_cast<int>(i.eval_exceptions);
    }
    return v;
}

/// Records every `iter` row of an interior-point solve as an `IpmRowView`. The objective
/// scale is read from the solve's `solve.begin`.
class IpmRowSink : public TraceSink {
  public:
    std::vector<IpmRowView> rows;
    int begins = 0;
    int ends = 0;
    Index phases = -1;
    void on_solve_begin(const SolveBeginTraceEvent &e) override {
        ++begins;
        if (e.ipm) {
            scale_ = e.ipm->obj_scale;
            phases = e.ipm->phases;
        }
    }
    void on_solve_end(const SolveEndTraceEvent &) override { ++ends; }
    void on_iter(const IterTraceEvent &e) override {
        if (e.record.ipm) {
            rows.push_back(make_ipm_row_view(e.record, scale_));
        }
    }

  private:
    double scale_ = 1.0;
};

} // namespace hven::test
