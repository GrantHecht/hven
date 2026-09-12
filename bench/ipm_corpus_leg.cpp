// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include "ipm_corpus_leg.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

#include <fmt/format.h>

#include <hven/core/solver_status.h>
#include <hven/detail/interior/utils/thread_pool.h>
#include <hven/detail/model/nlp_adapter.h>
#include <hven/drivers/solve_result.h>
#include <hven/drivers/solve_status.h>
#include <hven/warmstart/ipm_polish_extension.h>
#include <hven/warmstart/warm_start_data.h>

#include "crossover_legs.h"

namespace hven::solvers::corpus {

namespace {

using hven::solvers::crossover::ModelAsNlpProblem;

// The test fixture's own spelling of an absent upper bound
// (tests/interior/test_ipm_solver_entry.cpp:20), copied with it.
constexpr double kSolverInf = std::numeric_limits<double>::infinity();

double seconds_since(const std::chrono::steady_clock::time_point &t0) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

// The Ipopt HS071 example with x1 PINNED by equal bounds, copied from
// tests/interior/test_ipm_solver_entry.cpp:33 because a bench binary does not
// include a test file. The known optimum has x1 = 1.0 exactly, so the pin does not move
// the answer and the three treatments stay comparable on obj_val.
struct Hs071FixedProblem final : NlpTripletModel {
    int num_vars() const override { return 4; }
    int num_cons() const override { return 2; }
    int num_jac_nonzeros() const override { return 8; }
    int num_hess_nonzeros() const override { return 10; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << 1.0, 1.0, 1.0, 1.0;
        xu << 1.0, 5.0, 5.0, 5.0;
        gl << 25.0, 40.0;
        gu << kSolverInf, 40.0;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = x[0] * x[3] * (x[0] + x[1] + x[2]) + x[2];
    }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[3] * (2.0 * x[0] + x[1] + x[2]);
        g[1] = x[0] * x[3];
        g[2] = x[0] * x[3] + 1.0;
        g[3] = x[0] * (x[0] + x[1] + x[2]);
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0] * x[1] * x[2] * x[3];
        g[1] = x[0] * x[0] + x[1] * x[1] + x[2] * x[2] + x[3] * x[3];
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 0, 0, 0, 1, 1, 1, 1;
        c << 0, 1, 2, 3, 0, 1, 2, 3;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1, 1, 2, 2, 2, 3, 3, 3, 3;
        c << 0, 0, 1, 0, 1, 2, 0, 1, 2, 3;
    }
    void eval_jac(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = x[1] * x[2] * x[3];
        v[1] = x[0] * x[2] * x[3];
        v[2] = x[0] * x[1] * x[3];
        v[3] = x[0] * x[1] * x[2];
        v[4] = 2.0 * x[0];
        v[5] = 2.0 * x[1];
        v[6] = 2.0 * x[2];
        v[7] = 2.0 * x[3];
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd> x, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd> lambda,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        const double x0 = x[0], x1 = x[1], x2 = x[2], x3 = x[3];
        v[0] = obj_factor * 2 * x3 + lambda[1] * 2;
        v[1] = obj_factor * x3 + lambda[0] * x2 * x3;
        v[2] = lambda[1] * 2;
        v[3] = obj_factor * x3 + lambda[0] * x1 * x3;
        v[4] = lambda[0] * x0 * x3;
        v[5] = lambda[1] * 2;
        v[6] = obj_factor * (2 * x0 + x1 + x2) + lambda[0] * x1 * x2;
        v[7] = obj_factor * x0 + lambda[0] * x0 * x2;
        v[8] = obj_factor * x0 + lambda[0] * x0 * x1;
        v[9] = lambda[1] * 2;
    }
    std::string name() const override { return "Hs071FixedProblem"; }
};

InteriorRowIdentity hs071_fixed_identity() {
    // `hs`/`none`/`fixed_variable` are this cell's own words: family, window and
    // taxonomy are F7 vocabulary and none of them applies to it.
    InteriorRowIdentity id;
    id.cell_id = kHs071FixedCellId;
    id.family = "hs";
    id.n_nodes = 4;
    id.window = "none";
    id.taxonomy = "fixed_variable";
    return id;
}

Vec hs071_fixed_start() {
    Vec x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;
    return x0;
}

// c(x) = 1 + 1e-4*x0 + x0^10 = 0 has no real root: the near-flat Jacobian at the
// start throws the unlinesearched feasibility step out to |x0| ~ 1e4 and the
// pure power walks it back a tenth at a time, which is the sustained worsening
// the stall detector certifies. Shares its shape with the live pin in
// tests/interior/test_ipm_stop_reason.cpp; a bench binary does not include a
// test file, so it is a copy.
struct PowerSpikeProblem final : NlpTripletModel {
    static constexpr double kEps = 1.0e-4;
    static constexpr int kPower = 10;

    int num_vars() const override { return 2; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return 2; }
    int num_hess_nonzeros() const override { return 2; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << -kSolverInf, -kSolverInf;
        xu << kSolverInf, kSolverInf;
        gl << 0.0;
        gu << 0.0;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override { f = x[1]; }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> g) const override {
        g << 0.0, 1.0;
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = 1.0 + kEps * x[0] + std::pow(x[0], kPower);
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 0;
        c << 0, 1;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1;
        c << 0, 1;
    }
    void eval_jac(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> v) const override {
        v << kEps + kPower * std::pow(x[0], kPower - 1), 0.0;
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd> x, double, ConstEigenRef<Eigen::VectorXd> lambda,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        v << lambda[0] * kPower * (kPower - 1) * std::pow(x[0], kPower - 2), 0.0;
    }
    std::string name() const override { return "PowerSpikeProblem"; }
};

// c(x) = x0^2 + x1^2 + 1 = 0 has no real solution and its violation has a STRICT
// stationary point at the origin -- the shape a restoration phase converges to
// and then declares locally infeasible. Copied for the same reason as above.
struct LocallyInfeasibleProblem final : NlpTripletModel {
    int num_vars() const override { return 2; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return 2; }
    int num_hess_nonzeros() const override { return 2; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << -kSolverInf, -kSolverInf;
        xu << kSolverInf, kSolverInf;
        gl << 0.0;
        gu << 0.0;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override { f = x[0] + x[1]; }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> g) const override {
        g << 1.0, 1.0;
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0] * x[0] + x[1] * x[1] + 1.0;
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 0;
        c << 0, 1;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1;
        c << 0, 1;
    }
    void eval_jac(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> v) const override {
        v << 2.0 * x[0], 2.0 * x[1];
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd>, double, ConstEigenRef<Eigen::VectorXd> lambda,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        v << 2.0 * lambda[0], 2.0 * lambda[0];
    }
    std::string name() const override { return "LocallyInfeasibleProblem"; }
};

InteriorRowIdentity infeasible_identity(std::string_view cell_id) {
    // `infeasible` is this pair's taxonomy word: they exist to reach the two
    // abnormal exits, and no F7 start class describes them.
    InteriorRowIdentity id;
    id.cell_id = std::string(cell_id);
    id.family = "synthetic";
    id.n_nodes = 2;
    id.window = "none";
    id.taxonomy = "infeasible";
    return id;
}

Vec two_var_start(double a, double b) {
    Vec x0(2);
    x0 << a, b;
    return x0;
}

const char *entry_tag(InteriorVariant::Entry entry) {
    switch (entry) {
    case InteriorVariant::Entry::kOptimize:
        return "optimize";
    case InteriorVariant::Entry::kSolve:
        return "solve";
    case InteriorVariant::Entry::kSolveOptimize:
        return "solve_optimize";
    }
    return "unknown";
}

// The warm-start arm's name, for the variant stamp. `<none>` on every row
// before M6 W5 T8.5 and on every row that is not one of the two warm ones.
const char *warm_tag(InteriorVariant::Warm warm) {
    switch (warm) {
    case InteriorVariant::Warm::kPayload:
        return "payload";
    case InteriorVariant::Warm::kSeed:
        return "multiplier_seed";
    case InteriorVariant::Warm::kNone:
        break;
    }
    return "<none>";
}

const char *restoration_tag(RestorationModes mode) {
    switch (mode) {
    case RestorationModes::off:
        return "off";
    case RestorationModes::proximal_switch:
        return "proximal_switch";
    case RestorationModes::l1_nested:
        return "l1_nested";
    }
    throw std::invalid_argument(
        fmt::format("restoration_tag: unrecognized mode ({})", static_cast<int>(mode)));
}

} // namespace

const char *interior_treatment_tag(FixedVariableTreatments treatment) {
    switch (treatment) {
    case FixedVariableTreatments::MakeParameter:
        return "MakeParameter";
    case FixedVariableTreatments::MakeConstraint:
        return "MakeConstraint";
    case FixedVariableTreatments::RelaxBounds:
        return "RelaxBounds";
    }
    throw std::invalid_argument(fmt::format("interior_treatment_tag: unrecognized treatment ({})",
                                            static_cast<int>(treatment)));
}

const std::vector<FixedVariableTreatments> &interior_treatments() {
    static const std::vector<FixedVariableTreatments> kAll{FixedVariableTreatments::MakeParameter,
                                                           FixedVariableTreatments::MakeConstraint,
                                                           FixedVariableTreatments::RelaxBounds};
    return kAll;
}

const InteriorVariant &interior_base_variant() {
    static const InteriorVariant kBase{};
    return kBase;
}

const std::vector<InteriorVariant> &interior_exit_variants() {
    // The three abnormal exits, with the levers each one needs. `cap1` is a true
    // variant of a converging cell; the other two only exist on the infeasible
    // cells, because a feasible problem reaches neither exit by lever.
    static const std::vector<InteriorVariant> kVariants = [] {
        std::vector<InteriorVariant> v;
        InteriorVariant cap1;
        cap1.name = "cap1";
        cap1.entry = InteriorVariant::Entry::kOptimize;
        cap1.max_iters = 1;
        v.push_back(cap1);

        InteriorVariant stalled;
        stalled.name = "stalled";
        stalled.entry = InteriorVariant::Entry::kSolve;
        stalled.max_iters = 600;
        stalled.restoration_mode = RestorationModes::l1_nested;
        stalled.max_feas_rest = 1;
        stalled.con_tol = 2.0e-2;
        stalled.acc_kkt_tol = 1.0e-6;
        stalled.acc_con_tol = 2.0e-1;
        stalled.acc_barr_tol = 1.0e-6;
        stalled.div_tol = 1.0e300;
        v.push_back(stalled);

        InteriorVariant resto;
        resto.name = "resto_infeasible";
        resto.entry = InteriorVariant::Entry::kOptimize;
        resto.max_iters = 200;
        resto.restoration_mode = RestorationModes::l1_nested;
        resto.max_feas_rest = 1;
        v.push_back(resto);

        // THE MULTI-PHASE ROW (M6 W5 T8.4). {kSolve, kOptimize} -- what the
        // removed solve_optimize() entry ran -- on a cell that converges, so
        // what it shows is a sequence where BOTH phases run (the second is
        // unconditional; only a kSolve AFTER a kOptimize is conditional) and
        // each reports its own status and iteration count in the packed
        // `phases` column. It is a variant rather than a lever for the same
        // reason the three above are: what it pins is a shape of the account,
        // not a cell.
        InteriorVariant sequence;
        sequence.name = "solve_optimize";
        sequence.entry = InteriorVariant::Entry::kSolveOptimize;
        v.push_back(sequence);

        // THE TWO WARM-START ROWS (M6 W5 T8.5), the third instalment of the T8
        // coverage rule. They are variants for the same reason every row above
        // is: what they pin is a SHAPE of the hand-off, not a cell. Both run
        // under MakeParameter only and unconditionally, and both are measured
        // on the HS071 fixed-variable cell, whose base row takes 9 iterations
        // -- small, dense, and already in the artifact, so the comparison is
        // against a number a reader can see two rows up.
        //
        // WHY TWO. The payload row shows the whole hand-off applied: point,
        // multipliers and the polish extension's bound duals. The seed row
        // shows the multipliers-only form, whose point is `x0`. The three rows
        // together are the leg's evidence that each half of the payload
        // reaches the solve.
        //
        // WHAT THE THREE ROWS MEASURED, AND WHAT WAS EXPECTED (M6 W5 T8.5 fix
        // round 1). The brief's A6 asked for `iter_num(payload) <
        // iter_num(seed) < iter_num(base)`. The MEASUREMENT is 3 / 9 / 9: the
        // payload row is worth six iterations here and the seed row is worth
        // none. The criterion was a guess made before the rows existed and the
        // measurement is the fact; the inequality is NOT asserted anywhere and
        // the claim that it was has been removed from this comment, from the
        // report and from the guide.
        //
        // SO THE SEED ROW IS READ ON THE APPLIED RUNG, not on `iter_num`. The
        // engine knows which rung a payload reached and says so in
        // `IpmResult::payload_ignored` / `polish_ignored`, and
        // `run_interior_problem`'s APPLIED-RUNG CHECK (search that function for
        // "wrong warm-start rung") refuses to emit either row unless the rung
        // is the one the variant asked for -- so the two rows cannot be
        // captured from a solve that quietly ignored what it was handed. The
        // terminal residuals stay as the NUMERIC witness that the trajectory
        // CHANGED (not that it improved), asserted against the committed
        // artifact by `CorpusCells.TheWarmRowsShowThePayloadAndTheSeedReached
        // TheSolve`.
        InteriorVariant warm_payload;
        warm_payload.name = "warm_payload";
        warm_payload.entry = InteriorVariant::Entry::kOptimize;
        warm_payload.warm = InteriorVariant::Warm::kPayload;
        v.push_back(warm_payload);

        InteriorVariant warm_seed;
        warm_seed.name = "warm_multiplier_seed";
        warm_seed.entry = InteriorVariant::Entry::kOptimize;
        warm_seed.warm = InteriorVariant::Warm::kSeed;
        v.push_back(warm_seed);

        // THE PARTITIONED ROWS (M6 W5 T8.9), the fourth instalment of the T8
        // coverage rule and the first partitioned cell set this leg has had:
        // before T8.9 no path from an NlpTripletModel reached a layout with more
        // than one partition at all.
        //
        // WHAT THEY ARE, EXACTLY. `make_nlp_program(problem, 2)` lays TWO
        // partitions; all three adapter pieces are ThreadingFlags::MainThread,
        // and analyze_partitioning forces every MainThread function into the
        // LAST partition to run inline on the calling thread. So both rows are
        // LAYOUT rows: partition 1 holds the whole problem and runs serially,
        // partition 0 holds whatever the TREATMENT added. Under MakeConstraint
        // that would be the RoundRobin fixing rows -- but no F7 cell has a
        // bound-fixed variable (F7 pins node 0 through equality rows), so
        // partition 0 is empty on BOTH rows here. The two rows differ in the
        // treatment and in nothing else; neither is a two-thread evaluation,
        // and the artifact's header says so.
        //
        // WHY NOT HS071, which does have a bound-fixed variable: its ~18 KKT
        // elements are far below the 1000-per-partition clamp, so it ADOPTS 1
        // and the row would be a one-partition row under a two-partition name.
        // A cell that is both big enough to adopt two partitions and carries a
        // bound-fixed variable does not exist in this corpus; a genuinely
        // two-thread evaluation over an NlpTripletModel is registered for the M7
        // ClaimStreamSource widening, not claimed here.
        InteriorVariant parts2;
        parts2.name = kParts2VariantName;
        parts2.entry = InteriorVariant::Entry::kOptimize;
        parts2.num_partitions = 2;
        v.push_back(parts2);
        return v;
    }();
    return kVariants;
}

std::string interior_variant_stamp(const InteriorVariant &variant) {
    return fmt::format(
        "variant: {} entry={} warm={} max_iters={} restoration={} max_feas_rest={} "
        "con_tol={} acc_tols={}/{}/{} div_tol={}",
        variant.name[0] == '\0' ? "base" : variant.name, entry_tag(variant.entry),
        warm_tag(variant.warm),
        variant.max_iters > 0 ? std::to_string(variant.max_iters) : std::string("<levers>"),
        restoration_tag(variant.restoration_mode),
        variant.restoration_mode == RestorationModes::off ? std::string("<unused>")
                                                          : std::to_string(variant.max_feas_rest),
        variant.con_tol > 0.0 ? fmt::format("{:.9e}", variant.con_tol) : std::string("<levers>"),
        variant.acc_con_tol > 0.0 ? fmt::format("{:.9e}", variant.acc_kkt_tol)
                                  : std::string("<default>"),
        variant.acc_con_tol > 0.0 ? fmt::format("{:.9e}", variant.acc_con_tol)
                                  : std::string("<default>"),
        variant.acc_con_tol > 0.0 ? fmt::format("{:.9e}", variant.acc_barr_tol)
                                  : std::string("<default>"),
        variant.div_tol > 0.0 ? fmt::format("{:.9e}", variant.div_tol) : std::string("<default>"));
}

// The PHASE SEQUENCE each entry names (M6 W5 T8.4's option; M6 W5 T8.9 writes
// it here, where the five retired entry points used to write it from their own
// names).
std::vector<IpmPhase> phases_for(InteriorVariant::Entry entry) {
    switch (entry) {
    case InteriorVariant::Entry::kSolve:
        return {IpmPhase::kSolve};
    case InteriorVariant::Entry::kSolveOptimize:
        return {IpmPhase::kSolve, IpmPhase::kOptimize};
    case InteriorVariant::Entry::kOptimize:
    default:
        return {IpmPhase::kOptimize};
    }
}

// Every lever, variant override and treatment this leg applies to one solver,
// in ONE place (M6 W5 T8.5 factored it out of run_interior_problem so the warm
// rows' second solver is configured identically to the first). Does not
// transcribe: the caller does that, because it is the step after which the
// program exists.
void configure_interior_solver(IpmSolver &ipm, FixedVariableTreatments treatment,
                               const InteriorLevers &levers, const InteriorVariant &variant) {
    // One options value, built here and handed over once. Every lever and every
    // variant override below is a field write on it, so a dependent pair (the
    // stall variant's restoration_mode and max_feas_rest) can never reach the
    // solver half-applied.
    //
    // M6 W5 T8.9: the PARTITION COUNT is the program's, not an option, so it is
    // no longer written here -- make_configured() passes it to
    // make_nlp_program() instead. The PHASE SEQUENCE is a field now too, set
    // from the variant's entry rather than by which entry point is called.
    IpmOptions o = ipm.options();
    o.phases = phases_for(variant.entry);
    o.common.threads = levers.qp_threads;
    o.common.print_level = levers.print_level;
    o.max_iters = levers.max_iters;
    o.kkt_tol = levers.kkt_tol;
    o.econ_tol = levers.econ_tol;
    o.icon_tol = levers.icon_tol;
    o.bar_tol = levers.barr_tol;
    o.fixed_variable_treatment = treatment;
    o.bound_relax_factor = levers.bound_relax_factor;
    // The variant's overrides, applied on top of the levers above. Every field
    // is inert at its default, so a base row runs exactly what T8.1 captured.
    if (variant.max_iters > 0) {
        o.max_iters = variant.max_iters;
    }
    if (variant.con_tol > 0.0) {
        o.econ_tol = variant.con_tol;
        o.icon_tol = variant.con_tol;
    }
    if (variant.acc_con_tol > 0.0) {
        o.acc_kkt_tol = variant.acc_kkt_tol;
        o.acc_econ_tol = variant.acc_con_tol;
        o.acc_icon_tol = variant.acc_con_tol;
        o.acc_bar_tol = variant.acc_barr_tol;
    }
    if (variant.div_tol > 0.0) {
        o.div_kkt_tol = variant.div_tol;
        o.div_econ_tol = variant.div_tol;
        o.div_icon_tol = variant.div_tol;
        o.div_bar_tol = variant.div_tol;
    }
    if (variant.restoration_mode != RestorationModes::off) {
        o.restoration_mode = variant.restoration_mode;
        o.max_feas_rest = variant.max_feas_rest;
    }
    ipm.set_options(std::move(o));
}

// Forward-declared: the callback-events instrument below prints the row's own
// key, which is defined with the CSV writers further down (M6 W5 T8.6 fix1).
std::string interior_row_key(const InteriorRow &row);

InteriorRow run_interior_problem(const std::shared_ptr<NlpTripletModel> &problem,
                                 const InteriorRowIdentity &identity, const Vec &x0,
                                 FixedVariableTreatments treatment, const InteriorLevers &levers,
                                 const InteriorVariant &variant) {
    if (problem == nullptr) {
        throw std::invalid_argument(
            fmt::format("run_interior_problem: cell '{}' has a null problem", identity.cell_id));
    }

    // ONE CONFIGURED SOLVE -- A PROGRAM AND AN ENGINE -- built by this lambda
    // rather than inline, so the warm arm below can build a SECOND one under
    // exactly the same levers, variant overrides and treatment (M6 W5 T8.5).
    // Every field write is here, once, and neither caller can drift from the
    // other.
    //
    // THE PROGRAM IS PER SOLVE, NOT SHARED (M6 W5 T8.9). The retired wrapper
    // owned a program each, and a program CARRIES STATE a solve writes: the
    // fixed-variable treatment reconfiguration re-lays it and moves its
    // structure epoch, which is what makes the measured solve pay its own
    // analysis. Sharing one program between the producer and the measured solve
    // would hand the measured row an already-reduced layout and drop its
    // `analyses` from 2 to 1 -- measured, not assumed.
    //
    // The requested partition count reaches the LAYOUT now, where the wrapper's
    // field reached nothing -- and it is clamped, so the ADOPTED count is what
    // the header stamps.
    struct ConfiguredSolve {
        std::shared_ptr<NonLinearProgram> program;
        std::unique_ptr<IpmSolver> engine;
    };
    const int requested_partitions =
        variant.num_partitions > 0 ? variant.num_partitions : levers.num_partitions;
    const auto make_configured = [&]() {
        ConfiguredSolve configured{hven::solvers::make_nlp_program(problem, requested_partitions),
                                   std::make_unique<IpmSolver>()};
        configure_interior_solver(*configured.engine, treatment, levers, variant);
        return configured;
    };

    // THE WARM ARM'S PRODUCING SOLVE (M6 W5 T8.5), when this variant has one.
    //
    // ON A SOLVER OF ITS OWN, thrown away before the measured solve begins.
    // That is not tidiness: `IpmResult::kkt_factor_counters` and
    // `kkt_analyses_total` ACCUMULATE across calls on one solver, so a measured
    // solve that shared a solver with its producer would report the producing
    // solve's factorizations and analyses in its own columns -- 18 where the
    // base row reads 9 -- and the artifact would say the warm row cost twice
    // what it cost. A second solver is also the truer shape: a warm start's
    // value is realised on a LATER solve, and nothing about the hand-off
    // requires the two to be the same object.
    //
    // The producing solve is SETUP and is neither timed nor reported, exactly
    // as the bridge lay is on the other engine.
    std::optional<hven::solvers::WarmStartData> payload;
    if (variant.warm != InteriorVariant::Warm::kNone) {
        ConfiguredSolve producer = make_configured();
        const hven::solvers::IpmResult produced_result =
            producer.engine->solve(*producer.program, x0);
        const hven::solvers::SolveStatus produced = produced_result.status;
        if (produced != hven::solvers::SolveStatus::kOptimal) {
            throw std::runtime_error(fmt::format(
                "run_interior_problem: the '{}' variant's producing solve of cell '{}' did not "
                "converge (status {}); a warm row measured against a hand-off from a failed solve "
                "would pin nothing",
                variant.name, identity.cell_id, to_string(produced)));
        }
        payload = produced_result.export_warm_start();
        if (!payload.has_value()) {
            throw std::runtime_error(
                fmt::format("run_interior_problem: the '{}' variant's producing solve of cell "
                            "'{}' captured no warm-start snapshot",
                            variant.name, identity.cell_id));
        }
        if (variant.warm == InteriorVariant::Warm::kSeed) {
            // THE MULTIPLIERS-ONLY FORM, built from the very same export the
            // payload row uses, so the two rows differ in exactly one thing:
            // whether the point and the bound state travel with the prices.
            payload->primal_.resize(0);
            payload->bound_lmults_.resize(0);
            // THE POLISH EXTENSION IS LEFT ON (M6 W5 T8.5 fix round 1), and
            // that is deliberate. It changes NOTHING about the solve -- the
            // seed form's `have_warm` is false, so the extension is never
            // applied at any ceiling -- but it makes the engine COUNT the
            // drop, and `polish_ignored == 1` is then the direct, legible
            // statement that this row really did travel the seed route rather
            // than some other one. Clearing it here would have thrown away the
            // one observable the row has that is not a residual.
            if (hven::solvers::find_ipm_polish(*payload) == nullptr) {
                throw std::runtime_error(fmt::format(
                    "run_interior_problem: the '{}' variant's export of cell '{}' carries no "
                    "\"{}\" extension, so the row's polish_ignored observable would be vacuous",
                    variant.name, identity.cell_id, hven::solvers::kIpmPolishTag));
            }
        }
    }

    // THE MEASURED SOLVE'S SOLVER, fresh, so its counters start at zero
    // whatever ran above.
    ConfiguredSolve ipm_owner = make_configured();
    IpmSolver &ipm = *ipm_owner.engine;
    NonLinearProgram &program = *ipm_owner.program;

    // THE COVERAGE RULE'S SECOND RUN (M6 W5 T8.6), and it is a MEASUREMENT
    // INSTRUMENT rather than a row: with HVEN_LEG_COUNT_CALLBACK set in the
    // environment, every row of this leg runs with a per-iteration callback
    // attached that does nothing but count. The leg is then captured twice --
    // once without it, once with it -- and EVERY COLUMN of the two captures
    // must agree, which is what proves that attaching a callback moves no
    // counter, no status and no residual. The count itself goes to stderr, so
    // it lands in the leg's transcript and NOT in the CSV: the schema does not
    // move for an instrument.
    //
    // OFF BY DEFAULT AND FREE WHEN OFF: an unset variable installs nothing, so
    // the baseline capture is the same code path it always was.
    const bool count_events = std::getenv("HVEN_LEG_COUNT_CALLBACK") != nullptr;
    long long events_seen = 0;
    if (count_events) {
        ipm.set_iteration_callback([&events_seen](const hven::solvers::IterationEvent &) {
            ++events_seen;
            return hven::solvers::CallbackAction::kContinue;
        });
    }

    // THE SEQUENCE IS AN OPTION SINCE M6 W5 T8.4, and since M6 W5 T8.9 this leg
    // WRITES it (configure_interior_solver, from the variant's entry) rather
    // than choosing it by which retired entry point it called. One call either
    // way, and the TIMED WINDOW is what it always was: it brackets the solve
    // only -- the transcription ran above, before `t0`.
    const auto t0 = std::chrono::steady_clock::now();
    const hven::solvers::IpmResult measured =
        payload.has_value() ? ipm.solve(program, x0, *payload) : ipm.solve(program, x0);
    const double wall_s = seconds_since(t0);
    const hven::solvers::SolveStatus flag = measured.status;
    const hven::solvers::IpmResult &warm_result = measured;

    // THE APPLIED RUNG, CHECKED BEFORE THE ROW IS BUILT (M6 W5 T8.5 fix round
    // 1; the SQP lane's M1). A warm row whose payload was silently ignored
    // would still converge, still land on the same objective, and still write
    // a plausible-looking line -- and this leg's other observable, the terminal
    // residual, is a number a future change could move for reasons that have
    // nothing to do with the hand-off. The engine knows which rung it reached
    // and reports it, so the row is refused unless the rung is the one the
    // variant asked for.
    //
    //   warm_payload         nothing dropped: payload_ignored 0, polish_ignored 0.
    //   warm_multiplier_seed the point and the polish dropped, the prices
    //                        applied: payload_ignored 0, polish_ignored 1.
    if (variant.warm != InteriorVariant::Warm::kNone) {
        const int want_polish = variant.warm == InteriorVariant::Warm::kSeed ? 1 : 0;
        if (warm_result.payload_ignored != 0 || warm_result.polish_ignored != want_polish) {
            throw std::runtime_error(fmt::format(
                "run_interior_problem: the '{}' variant's measured solve of cell '{}' reached the "
                "wrong warm-start rung: payload_ignored={} polish_ignored={}, expected 0 and {}. "
                "The row would report a warm start that was not applied",
                variant.name, identity.cell_id, warm_result.payload_ignored,
                warm_result.polish_ignored, want_polish));
        }
    }

    // The MEASURED solve's result, whichever door it came through.
    const hven::solvers::IpmResult &result = measured;
    InteriorRow row;
    row.cell_id = identity.cell_id;
    row.family = identity.family;
    row.n_nodes = identity.n_nodes;
    row.window = identity.window;
    row.taxonomy = identity.taxonomy;
    row.status = crossover::flag_string(flag);
    row.iter_num = result.iterations;
    row.obj_val = result.f;
    row.kkt_inf = result.kkt_inf;
    row.barr_inf = result.barr_inf;
    row.econ_inf = result.econ_inf;
    row.icon_inf = result.icon_inf;
    row.factorizations = result.kkt_factor_counters.factorize_count;
    row.solves = result.kkt_factor_counters.solve_count;
    row.analyses = result.kkt_analyses_total;
    row.soc_steps = result.soc_steps_taken;
    row.watchdog_activations = result.watchdog_activations;
    row.fixed_treatment = interior_treatment_tag(result.fixed_variable_treatment);
    row.stop_reason = to_string(ipm.last_stop_reason());
    row.variant = variant.name;

    // THE PER-PHASE ACCOUNT, packed into one column so the schema does not grow
    // with the longest sequence this leg ever runs. A phase that did not run
    // says so in place of its status and count, rather than reporting a zero
    // that would read as "ran and took none".
    row.phase_count = static_cast<Index>(result.phases.size());
    row.phases_ran = 0;
    std::string packed;
    for (const auto &phase : result.phases) {
        if (!packed.empty()) {
            packed += "|";
        }
        const char *name =
            phase.phase == hven::solvers::IpmPhase::kOptimize ? "kOptimize" : "kSolve";
        if (phase.ran) {
            ++row.phases_ran;
            packed += fmt::format("{}:{}:{}", name, to_string(phase.status), phase.iterations);
        } else {
            packed += fmt::format("{}:skipped", name);
        }
    }
    row.phases = packed;

    // The DECLARED widths of the four returned blocks, and the four shared
    // declared diagnostics beside them.
    row.x_size = result.x.size();
    row.lambda_e_size = result.lambda_e.size();
    row.lambda_i_size = result.lambda_i.size();
    row.z_size = result.z.size();
    row.stationarity = result.stationarity;
    row.feasibility_e = result.feasibility_e;
    row.feasibility_i = result.feasibility_i;
    row.complementarity = result.complementarity;

    row.wall_s = wall_s;

    // THE INSTRUMENT'S LINE, ROW-ADDRESSABLE (M6 W5 T8.6 fix1, the SQP lane's
    // M8). It printed `variant.name`, which is EMPTY on the 33 base rows, so a
    // transcript carried three identical `f7_n5000_bound_physics variant=
    // events=5` lines and no count could be attributed to a treatment row. The
    // ROW'S OWN KEY -- cell, fixed treatment and variant, the same triple
    // interior_csv_row writes -- is unique per row, which is what "the count
    // per row recorded in the transcripts" asks for. Printed here, after the
    // row is built, because that is where the key exists; `iter_num` goes
    // beside it so the "one event per iterate" pairing the leg is the
    // large-scale proof of can be read straight off the transcript. STDERR, so
    // the CSV and its schema are untouched.
    if (count_events) {
        fmt::print(stderr, "callback-events row={} events={} iter_num={}\n", interior_row_key(row),
                   events_seen, row.iter_num);
    }
    return row;
}

std::string interior_cell_refusal(const CorpusCell &cell) {
    return crossover::dual_bind_refusal(cell);
}

InteriorPartitionStamp interior_partition_stamp(const CorpusCell &cell, int requested) {
    if (!crossover::cell_dual_binds(cell)) {
        throw std::invalid_argument(
            fmt::format("interior_partition_stamp: cell '{}' does not dual-bind: {}", cell.id,
                        crossover::dual_bind_refusal(cell)));
    }
    const auto f7 = std::make_shared<F7CollocationChain>(detail::make_model(cell));
    const auto declared = std::make_shared<ModelAsNlpProblem>(f7, std::string(cell.id));
    const auto program = hven::solvers::make_nlp_program(declared, requested);
    InteriorPartitionStamp stamp;
    stamp.requested = requested;
    stamp.adopted = program->num_partitions_;
    stamp.pool_threads = hven::utils::get_num_threads();
    return stamp;
}

InteriorRow run_interior_cell(const CorpusCell &cell, FixedVariableTreatments treatment,
                              const InteriorLevers &levers, const InteriorVariant &variant) {
    if (!crossover::cell_dual_binds(cell)) {
        throw std::invalid_argument(
            fmt::format("run_interior_cell: cell '{}' does not dual-bind: {}", cell.id,
                        crossover::dual_bind_refusal(cell)));
    }
    const auto f7 = std::make_shared<F7CollocationChain>(detail::make_model(cell));
    const Vec x0 = crossover::detail::start_point_for(cell, *f7);
    const auto declared = std::make_shared<ModelAsNlpProblem>(f7, std::string(cell.id));

    InteriorRowIdentity id;
    id.cell_id = cell.id;
    id.family = to_string(cell.family);
    id.n_nodes = static_cast<int>(cell.n_nodes);
    id.window = to_string(cell.ctag);
    id.taxonomy = to_string(cell.start);
    return run_interior_problem(declared, id, x0, treatment, levers, variant);
}

InteriorRow run_interior_hs071(FixedVariableTreatments treatment, const InteriorLevers &levers,
                               const InteriorVariant &variant) {
    return run_interior_problem(std::make_shared<Hs071FixedProblem>(), hs071_fixed_identity(),
                                hs071_fixed_start(), treatment, levers, variant);
}

InteriorRow run_interior_infeasible(std::string_view cell_id, FixedVariableTreatments treatment,
                                    const InteriorLevers &levers, const InteriorVariant &variant) {
    std::shared_ptr<NlpTripletModel> problem;
    if (cell_id == kSpikeCellId) {
        problem = std::make_shared<PowerSpikeProblem>();
    } else if (cell_id == kStationaryCellId) {
        problem = std::make_shared<LocallyInfeasibleProblem>();
    } else {
        throw std::invalid_argument(
            fmt::format("run_interior_infeasible: '{}' is neither '{}' nor '{}'", cell_id,
                        kSpikeCellId, kStationaryCellId));
    }
    // Both fixtures start where their own exit needs them to: the spike at the
    // flat point its first step is thrown from, the stationary one away from the
    // origin its restoration converges to.
    const Vec x0 = cell_id == kSpikeCellId ? two_var_start(0.0, 0.0) : two_var_start(1.0, 1.0);
    return run_interior_problem(problem, infeasible_identity(cell_id), x0, treatment, levers,
                                variant);
}

FixedVariableTreatments interior_treatment_from_tag(std::string_view tag) {
    for (const FixedVariableTreatments treatment : interior_treatments()) {
        if (tag == interior_treatment_tag(treatment)) {
            return treatment;
        }
    }
    throw std::invalid_argument(fmt::format("interior_treatment_from_tag: '{}' is not one of "
                                            "MakeParameter|MakeConstraint|RelaxBounds",
                                            tag));
}

InteriorRow run_interior_single_row(std::string_view cell_id, FixedVariableTreatments treatment,
                                    const InteriorLevers &levers) {
    // The fixed-variable cell is not a corpus cell -- the leg runs it
    // unconditionally from its own id, and find_cell would not know it.
    if (cell_id == kHs071FixedCellId) {
        return run_interior_hs071(treatment, levers, interior_base_variant());
    }
    const CorpusCell *cell = find_cell(std::string(cell_id));
    if (cell == nullptr) {
        throw std::invalid_argument(
            fmt::format("run_interior_single_row: unknown cell id '{}' (a corpus cell id, or "
                        "'{}')",
                        cell_id, kHs071FixedCellId));
    }
    // run_interior_cell refuses a cell that does not dual-bind, with the reason
    // folded into the message; this mode does not need its own copy of that.
    return run_interior_cell(*cell, treatment, levers, interior_base_variant());
}

std::string interior_row_key(const InteriorRow &row) {
    if (row.variant.empty()) {
        return fmt::format("{}/{}", row.cell_id, row.fixed_treatment);
    }
    return fmt::format("{}/{}/{}", row.cell_id, row.fixed_treatment, row.variant);
}

std::string interior_csv_header() {
    return "cell_id,family,n_nodes,window,taxonomy,status,iter_num,obj_val,kkt_inf,barr_inf,"
           "econ_inf,icon_inf,factorizations,solves,analyses,soc_steps,watchdog_activations,"
           "fixed_treatment,stop_reason,phase_count,phases_ran,phases,x_size,lambda_e_size,"
           "lambda_i_size,z_size,stationarity,feasibility_e,feasibility_i,complementarity,"
           "wall_s\n";
}

std::string interior_csv_row(const InteriorRow &row) {
    return fmt::format("{},{},{},{},{},{},{},{:.9e},{:.9e},{:.9e},{:.9e},{:.9e},{},{},{},{},{},"
                       "{},{},{},{},{},{},{},{},{},{:.9e},{:.9e},{:.9e},{:.9e},{:.9f}\n",
                       interior_row_key(row), row.family, row.n_nodes, row.window, row.taxonomy,
                       row.status, row.iter_num, row.obj_val, row.kkt_inf, row.barr_inf,
                       row.econ_inf, row.icon_inf, row.factorizations, row.solves, row.analyses,
                       row.soc_steps, row.watchdog_activations, row.fixed_treatment,
                       row.stop_reason, row.phase_count, row.phases_ran, row.phases, row.x_size,
                       row.lambda_e_size, row.lambda_i_size, row.z_size, row.stationarity,
                       row.feasibility_e, row.feasibility_i, row.complementarity, row.wall_s);
}

} // namespace hven::solvers::corpus
