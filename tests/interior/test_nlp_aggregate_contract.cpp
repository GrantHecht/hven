// The partitioned evaluation engine AS a Level 2 provider.
//
// tests/model/ pins what the contract guarantees with no engine behind it, and
// deliberately holds nothing but a fake. These are the same guarantees against
// the REAL engine: the structure epoch's ordering and failure-restore rules on
// the engine's own re-lay paths, the structural key's two captured digests,
// layout determinism and partition invariance over its actual claim arena, the
// destination binding its location tables carry, and -- the central property
// here -- that an assemble request produces exactly what the evaluation entry
// point it replaces produces, once the consumer scatters the solver
// coefficients that assemble deliberately leaves to it.
//
// UNITY-BUILD NOTE: this suite is compiled with UNITY_BUILD ON, and anonymous
// namespaces do not isolate across unity-merged translation units. Every helper
// here therefore carries a distinct prefix rather than relying on the anonymous
// namespace to keep it local.

#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

#include "hven/detail/model/nlp_adapter.h"
#include "hven/model/non_linear_program.h"

using hven::ConstEigenRef;
using hven::solvers::AggregateCapability;
using hven::solvers::CandidateFirstOrder;
using hven::solvers::CandidatePoint;
using hven::solvers::CandidateValues;
using hven::solvers::EvalRequest;
using hven::solvers::FixedVariableTreatments;
using hven::solvers::has_capability;
using hven::solvers::KktScatterView;
using hven::solvers::NLPAdapterCore;
using hven::solvers::NLPProblem;
using hven::solvers::NonLinearProgram;
using hven::solvers::RhsArenaView;
using hven::solvers::RhsLocationTable;
using hven::solvers::RhsScatterView;

namespace {

constexpr double kAggPinInf = std::numeric_limits<double>::infinity();

/// A small dense problem with a genuinely nonlinear objective and constraints,
/// so every evaluation shape has something to compute and no two shapes agree
/// by accident. Two equality rows, one inequality row, four variables.
///
/// `fix_` pins a variable by giving it equal bounds, which is what drives the
/// fixed-variable treatments.
struct AggPinProblem : NLPProblem {
    int fix_ = -1;
    double fix_at_ = 0.0;
    bool fix_all_ = false;

    int num_vars() const override { return 4; }
    int num_cons() const override { return 3; }
    int num_jac_nonzeros() const override { return 12; }
    int num_hess_nonzeros() const override { return 4; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl.setConstant(-kAggPinInf);
        xu.setConstant(kAggPinInf);
        if (fix_all_) {
            xl.setConstant(fix_at_);
            xu.setConstant(fix_at_);
        } else if (fix_ >= 0) {
            xl[fix_] = fix_at_;
            xu[fix_] = fix_at_;
        }
        // Two equalities and one inequality, in that row order.
        gl[0] = 0.0;
        gu[0] = 0.0;
        gl[1] = 0.0;
        gu[1] = 0.0;
        gl[2] = -kAggPinInf;
        gu[2] = 1.0;
    }

    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = 0.5 * x[0] * x[0] + 1.5 * x[1] * x[1] + x[2] * x[3] + 0.25 * x[3];
    }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0];
        g[1] = 3.0 * x[1];
        g[2] = x[3];
        g[3] = x[2] + 0.25;
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0] + 2.0 * x[1] - x[2];
        g[1] = x[0] * x[0] + x[3];
        g[2] = x[1] * x[2] + x[3];
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        int k = 0;
        for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 4; col++) {
                r[k] = row;
                c[k] = col;
                k++;
            }
        }
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1, 3, 2;
        c << 0, 1, 2, 2;
    }
    void eval_jac(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> v) const override {
        v.setZero();
        v[0] = 1.0;
        v[1] = 2.0;
        v[2] = -1.0;
        v[4] = 2.0 * x[0];
        v[7] = 1.0;
        v[9] = x[2];
        v[10] = x[1];
        v[11] = 1.0;
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd>, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd> lambda,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = obj_factor + 2.0 * lambda[1];
        v[1] = 3.0 * obj_factor;
        v[2] = obj_factor;
        v[3] = lambda[2];
    }
};

/// A problem wide enough that the partition cap does not pin the count at one,
/// with a Jacobian pattern that is SPARSE and ASYMMETRIC.
///
/// The asymmetry is load-bearing for the invariance test rather than
/// decoration. Over a dense Cartesian pattern the set of claimed (row, column)
/// pairs is invariant under any renumbering of the columns, so an assertion on
/// that set cannot fail however badly a re-lay renumbers -- it is blind to the
/// thing it exists to catch. Each row here claims a wrapped band of kBand
/// columns starting at its own index, which makes the set asymmetric over part
/// of that band: (r+k, r) is claimed only when the wrap brings it back inside
/// the band, i.e. when kVars - k < kBand. So for k in 1..kVars-kBand -- here
/// 1..20 -- the pair (r, r+k) is claimed while (r+k, r) is NOT, and a
/// renumbering moves the set. (For k in 21..39 the wrap does claim both, which
/// is why the quantifier is that range and not the whole band.)
struct AggPinWideProblem : NLPProblem {
    static constexpr int kVars = 60;
    static constexpr int kCons = 60;
    static constexpr int kBand = 40;

    static int band_col(int row, int k) { return (row + k) % kVars; }

    int num_vars() const override { return kVars; }
    int num_cons() const override { return kCons; }
    int num_jac_nonzeros() const override { return kCons * kBand; }
    int num_hess_nonzeros() const override { return kVars; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl.setConstant(-kAggPinInf);
        xu.setConstant(kAggPinInf);
        gl.setZero();
        gu.setZero();
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = 0.5 * x.squaredNorm();
    }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g = x;
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        for (int row = 0; row < kCons; row++) {
            g[row] = x[row] + 0.5 * x[(row + 1) % kVars];
        }
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        int slot = 0;
        for (int row = 0; row < kCons; row++) {
            for (int k = 0; k < kBand; k++) {
                r[slot] = row;
                c[slot] = band_col(row, k);
                slot++;
            }
        }
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        for (int i = 0; i < kVars; i++) {
            r[i] = i;
            c[i] = i;
        }
    }
    void eval_jac(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v.setZero();
        for (int row = 0; row < kCons; row++) {
            v[row * kBand + 0] = 1.0; // d g_row / d x_row
            v[row * kBand + 1] = 0.5; // d g_row / d x_(row+1)
        }
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd>, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v.setConstant(obj_factor);
    }
};

/// A model with constraint rows of ONE kind only. `inequality_` picks which:
/// false declares equality rows and no inequality rows, true the other way
/// round. Either way one of the two residual blocks is legitimately
/// zero-length, which is the case a destination check has to accept rather than
/// mistake for a missing view -- the residual flag names both arenas at once,
/// so getting this wrong locks such a model out of evaluating the block it does
/// have.
struct AggPinOneKindProblem : NLPProblem {
    bool inequality_ = false;

    int num_vars() const override { return 3; }
    int num_cons() const override { return 2; }
    int num_jac_nonzeros() const override { return 4; }
    int num_hess_nonzeros() const override { return 3; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl.setConstant(-kAggPinInf);
        xu.setConstant(kAggPinInf);
        if (inequality_) {
            // One-sided rows: every row classifies as an inequality.
            gl.setConstant(-kAggPinInf);
            gu.setConstant(1.0);
        } else {
            // Rows pinned to a point: every row classifies as an equality.
            gl.setZero();
            gu.setZero();
        }
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = 0.5 * x.squaredNorm() + x[2];
    }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g = x;
        g[2] += 1.0;
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0] + 2.0 * x[1];
        g[1] = x[1] - x[2];
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 0, 1, 1;
        c << 0, 1, 1, 2;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1, 2;
        c << 0, 1, 2;
    }
    void eval_jac(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v << 1.0, 2.0, 1.0, -1.0;
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd>, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v.setConstant(obj_factor);
    }
    std::string name() const override { return "AggPinOneKindProblem"; }
};

std::shared_ptr<NonLinearProgram> agg_pin_build(const std::shared_ptr<NLPProblem> &problem) {
    auto core = std::make_shared<NLPAdapterCore>(problem);
    return hven::solvers::make_nlp_program(core);
}

std::shared_ptr<NonLinearProgram> agg_pin_build_small() {
    return agg_pin_build(std::make_shared<AggPinProblem>());
}

/// Every destination one evaluation writes, in the space the SOLVER works in --
/// the same buffers the legacy entry points are handed. Zeroed on construction
/// and re-zeroable, because both sides of an equivalence check must start from
/// the same state.
struct AggPinSolverStorage {
    Eigen::SparseMatrix<double, Eigen::RowMajor> kkt;
    Eigen::VectorXd pgx;
    Eigen::VectorXd agx;
    Eigen::VectorXd fxe;
    Eigen::VectorXd fxi;
    double objective = 0.0;

    explicit AggPinSolverStorage(NonLinearProgram &nlp) {
        kkt.resize(nlp.kkt_dim_, nlp.kkt_dim_);
        nlp.analyze_sparsity(kkt);
        pgx.setZero(nlp.reduced_primal_vars_count_);
        agx.setZero(nlp.reduced_primal_vars_count_);
        fxe.setZero(nlp.equal_cons_);
        fxi.setZero(nlp.inequal_cons_);
        this->reset();
    }

    /// analyze_sparsity leaves placeholder ones at every physical slot, so a
    /// fresh scatter has to start from an explicit zero.
    void reset() {
        Eigen::Map<Eigen::VectorXd>(kkt.valuePtr(), kkt.nonZeros()).setZero();
        pgx.setZero();
        agx.setZero();
        fxe.setZero();
        fxi.setZero();
        objective = 0.0;
    }

    RhsScatterView rhs_view(NonLinearProgram &nlp) {
        RhsScatterView rhs;
        rhs.objective_ = &objective;
        rhs.objective_gradient_ =
            RhsArenaView{pgx.data(), static_cast<int>(pgx.size()), &nlp.objective_gradient_table()};
        rhs.constraint_adjoint_gradient_ = RhsArenaView{agx.data(), static_cast<int>(agx.size()),
                                                        &nlp.constraint_adjoint_gradient_table()};
        rhs.equality_residuals_ =
            RhsArenaView{fxe.data(), static_cast<int>(fxe.size()), &nlp.equality_residual_table()};
        rhs.inequality_residuals_ = RhsArenaView{fxi.data(), static_cast<int>(fxi.size()),
                                                 &nlp.inequality_residual_table()};
        return rhs;
    }

    KktScatterView kkt_view(NonLinearProgram &nlp) {
        return KktScatterView{kkt.valuePtr(), static_cast<int>(kkt.nonZeros()),
                              &nlp.kkt_location_table()};
    }
};

/// How many distinct partitions actually own user KKT claims. Recorded by the
/// invariance and stability tests rather than assumed, because what those tests
/// establish depends on the answer: with every piece on one partition they
/// establish the properties over a single-partition claim assignment and say
/// nothing about cross-partition contention.
int agg_pin_distinct_claim_partitions(const NonLinearProgram &program) {
    std::vector<int> ids;
    ids.reserve(static_cast<std::size_t>(program.num_user_kkt_elems_));
    for (int slot = 0; slot < program.num_user_kkt_elems_; slot++) {
        ids.push_back(program.kkt_coeff_part_ids_[slot]);
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return static_cast<int>(ids.size());
}

Eigen::VectorXd agg_pin_x() {
    Eigen::VectorXd x(4);
    x << 1.3, -0.7, 0.4, 2.1;
    return x;
}

Eigen::VectorXd agg_pin_le() {
    Eigen::VectorXd le(2);
    le << 0.4, -1.1;
    return le;
}

Eigen::VectorXd agg_pin_li() {
    Eigen::VectorXd li(1);
    li << 0.9;
    return li;
}

/// The KKT values as a plain vector, for exact or near comparison.
Eigen::VectorXd agg_pin_kkt_values(const Eigen::SparseMatrix<double, Eigen::RowMajor> &m) {
    return Eigen::Map<const Eigen::VectorXd>(m.valuePtr(), m.nonZeros());
}

} // namespace

// ---------------------------------------------------------------------------
// The mapping table, against the live engine
// ---------------------------------------------------------------------------

/// The central equivalence property. For each of the eight shapes: run the legacy
/// entry point into one set of destinations, run the assemble request the
/// mapping table pairs it with into another, and require the two to agree.
///
/// The KKT-bearing shapes carry the responsibility transfer with them. A legacy
/// entry also scatters the SOLVER's own KKT coefficients at the end of its
/// dispatch, and assemble deliberately does not -- so the check for those four
/// shapes is "assemble, then the consumer's own fill_solver_coeffs" against
/// "the legacy entry", which is exactly the statement the mapping table makes.
TEST(NlpAggregateEngineContract, EveryRequestReproducesTheEntryPointItReplaces) {
    auto nlp = agg_pin_build_small();
    // CONSTRUCTION ORDER IS LOAD-BEARING, on the same ground the re-patterning
    // test states: this fixture runs the sparsity analysis in its constructor,
    // so each construction RE-BINDS the engine's location tables to that
    // fixture's own matrix. `assembled` is built second, so the engine ends up
    // bound to the destination the assemble side presents -- which is what lets
    // the entry's destination check pass. Swapping these two lines leaves the
    // engine bound to `legacy.kkt` and the assemble calls refused as stale.
    AggPinSolverStorage legacy(*nlp);
    AggPinSolverStorage assembled(*nlp);

    const Eigen::VectorXd x = agg_pin_x();
    const Eigen::VectorXd le = agg_pin_le();
    const Eigen::VectorXd li = agg_pin_li();
    const double scale = 1.75;

    struct AggPinShape {
        const char *name;
        EvalRequest request;
        bool kkt_bearing;
    };
    const std::vector<AggPinShape> shapes = {
        {"objective only", hven::solvers::kRequestObjectiveOnly, false},
        {"objective and constraints", hven::solvers::kRequestObjectiveAndConstraints, false},
        {"objective gradient and constraints",
         hven::solvers::kRequestObjectiveGradientAndConstraints, false},
        {"first-order right-hand side", hven::solvers::kRequestFirstOrderRhs, false},
        {"constraint Jacobian only", hven::solvers::kRequestConstraintJacobianOnly, true},
        {"first-order KKT", hven::solvers::kRequestFirstOrderKkt, true},
        {"constraint KKT", hven::solvers::kRequestConstraintKkt, true},
        {"full KKT", hven::solvers::kRequestFullKkt, true},
    };

    for (const AggPinShape &shape : shapes) {
        SCOPED_TRACE(shape.name);
        legacy.reset();
        assembled.reset();

        if (shape.request == hven::solvers::kRequestObjectiveOnly) {
            nlp->eval_obj(scale, x, legacy.objective);
        } else if (shape.request == hven::solvers::kRequestObjectiveAndConstraints) {
            nlp->eval_occ(scale, x, legacy.objective, legacy.fxe, legacy.fxi);
        } else if (shape.request == hven::solvers::kRequestObjectiveGradientAndConstraints) {
            nlp->eval_ogc(scale, x, legacy.objective, legacy.pgx, legacy.fxe, legacy.fxi);
        } else if (shape.request == hven::solvers::kRequestFirstOrderRhs) {
            nlp->eval_rhs(scale, x, le, li, legacy.objective, legacy.pgx, legacy.agx, legacy.fxe,
                          legacy.fxi);
        } else if (shape.request == hven::solvers::kRequestConstraintJacobianOnly) {
            nlp->eval_soe(scale, x, le, li, legacy.objective, legacy.pgx, legacy.agx, legacy.fxe,
                          legacy.fxi, legacy.kkt);
        } else if (shape.request == hven::solvers::kRequestFirstOrderKkt) {
            nlp->eval_aug(scale, x, le, li, legacy.objective, legacy.pgx, legacy.agx, legacy.fxe,
                          legacy.fxi, legacy.kkt);
        } else if (shape.request == hven::solvers::kRequestConstraintKkt) {
            nlp->eval_kkt_no(scale, x, le, li, legacy.objective, legacy.pgx, legacy.agx, legacy.fxe,
                             legacy.fxi, legacy.kkt);
        } else {
            nlp->eval_kkt(scale, x, le, li, legacy.objective, legacy.pgx, legacy.agx, legacy.fxe,
                          legacy.fxi, legacy.kkt);
        }

        // The assemble side is handed the same destinations minus the ones the
        // request does not name; those stay empty, which is the permission the
        // mapping table's empty column records.
        RhsScatterView rhs = assembled.rhs_view(*nlp);
        if (!has_request(shape.request, EvalRequest::kObjectiveGradient)) {
            rhs.objective_gradient_ = RhsArenaView{};
        }
        if (!has_request(shape.request, EvalRequest::kConstraintAdjointGradient)) {
            rhs.constraint_adjoint_gradient_ = RhsArenaView{};
        }
        if (!has_request(shape.request, EvalRequest::kConstraintValues)) {
            rhs.equality_residuals_ = RhsArenaView{};
            rhs.inequality_residuals_ = RhsArenaView{};
        }
        const KktScatterView kkt = shape.kkt_bearing ? assembled.kkt_view(*nlp) : KktScatterView{};

        nlp->assemble(CandidatePoint{x, le, li, scale}, shape.request, kkt, rhs);

        if (shape.kkt_bearing) {
            // The step the contract transfers to the consumer, performed here
            // by the consumer. Without it the assembled matrix is missing its
            // slack Jacobian and its pivots.
            nlp->fill_solver_coeffs(assembled.kkt);
        }

        // BIT identity, not approximate agreement. Both sides run the same
        // pass over the same pieces in the same order and fold the same claim
        // slots, so equality is the claim being made and anything weaker would
        // pass through a real behaviour change.
        EXPECT_EQ(assembled.objective, legacy.objective);
        EXPECT_EQ(assembled.fxe, legacy.fxe)
            << assembled.fxe.transpose() << " vs " << legacy.fxe.transpose();
        EXPECT_EQ(assembled.fxi, legacy.fxi);
        if (has_request(shape.request, EvalRequest::kObjectiveGradient)) {
            EXPECT_EQ(assembled.pgx, legacy.pgx);
        }
        if (has_request(shape.request, EvalRequest::kConstraintAdjointGradient)) {
            EXPECT_EQ(assembled.agx, legacy.agx);
        }
        if (shape.kkt_bearing) {
            EXPECT_EQ(agg_pin_kkt_values(assembled.kkt), agg_pin_kkt_values(legacy.kkt));
        }
    }
}

TEST(NlpAggregateEngineContract, ShapesFiveAndSevenLeaveTheirZeroArenasEmpty) {
    // The two shapes whose legacy counterparts passed a gradient buffer in and
    // summed an identically zero contribution into it. Declining to write them
    // is what the request permits; this asserts the contribution really was
    // zero, so the permission is observational identity rather than a loss.
    auto nlp = agg_pin_build_small();
    AggPinSolverStorage legacy(*nlp);

    const Eigen::VectorXd x = agg_pin_x();
    const Eigen::VectorXd le = agg_pin_le();
    const Eigen::VectorXd li = agg_pin_li();

    legacy.reset();
    nlp->eval_soe(1.0, x, le, li, legacy.objective, legacy.pgx, legacy.agx, legacy.fxe, legacy.fxi,
                  legacy.kkt);
    EXPECT_TRUE(legacy.pgx.isZero()) << "shape 5's objective-gradient contribution";
    EXPECT_TRUE(legacy.agx.isZero()) << "shape 5's adjoint-gradient contribution";

    legacy.reset();
    nlp->eval_kkt_no(1.0, x, le, li, legacy.objective, legacy.pgx, legacy.agx, legacy.fxe,
                     legacy.fxi, legacy.kkt);
    EXPECT_TRUE(legacy.pgx.isZero()) << "shape 7's objective-gradient contribution";
    EXPECT_FALSE(legacy.agx.isZero()) << "shape 7 genuinely produces the adjoint gradient";
}

// ---------------------------------------------------------------------------
// The structure epoch, on the engine's own re-lay paths
// ---------------------------------------------------------------------------

TEST(NlpAggregateEngineEpoch, EveryRelayBumpsAndTheBumpPrecedesTheNewStructures) {
    auto nlp = agg_pin_build_small();
    const auto after_transcription = nlp->structure_epoch();
    EXPECT_NE(after_transcription, hven::solvers::StructureEpoch());

    // A renegotiation re-lays, so it bumps -- and by the time it returns, the
    // structures it laid are the ones on hand. There is no ordering in which an
    // evaluation of the new claim arena is reachable under the old epoch,
    // because the bump is the last statement of the routine that laid it.
    nlp->negotiate_partition_count(1);
    const auto after_renegotiation = nlp->structure_epoch();
    EXPECT_NE(after_renegotiation, after_transcription);

    // A re-transcription re-lays too.
    nlp->make_nlp(nlp->primal_vars_, nlp->user_equal_cons_, nlp->inequal_cons_);
    EXPECT_NE(nlp->structure_epoch(), after_renegotiation);
}

TEST(NlpAggregateEngineEpoch, AnEvaluationNeverRunsUnderAStaleEpoch) {
    // The ordering guarantee's observable form on this provider: a re-lay
    // resets the KKT location table (only the sparsity analysis fills it), so
    // an evaluation of the new structures is refused until the consumer has
    // re-analysed -- and by then the epoch has already moved. A consumer
    // therefore cannot evaluate the new arena while still reading the old
    // epoch.
    auto nlp = agg_pin_build_small();
    AggPinSolverStorage storage(*nlp);
    const auto before = nlp->structure_epoch();

    nlp->negotiate_partition_count(1);
    EXPECT_NE(nlp->structure_epoch(), before);

    const Eigen::VectorXd x = agg_pin_x();
    const Eigen::VectorXd le = agg_pin_le();
    const Eigen::VectorXd li = agg_pin_li();
    EXPECT_THROW(nlp->assemble(CandidatePoint{x, le, li}, hven::solvers::kRequestFullKkt,
                               storage.kkt_view(*nlp), storage.rhs_view(*nlp)),
                 std::invalid_argument);
}

TEST(NlpAggregateEngineEpoch, ARejectedReconfigurationThatRelaidBumpsToo) {
    // The failure-restore rule. The first configuration eliminates a fixed
    // variable, so the structures are laid over a narrower primal block. The
    // second is rejected during classification -- but the restore has to put
    // the problem back on the full variable space, which re-lays. The
    // structures on hand are not the ones the consumer last saw, so the epoch
    // must say so even though the call threw.
    auto problem = std::make_shared<AggPinProblem>();
    problem->fix_ = 2;
    problem->fix_at_ = 0.4;
    auto nlp = agg_pin_build(problem);

    ASSERT_TRUE(nlp->configure_variable_treatment(FixedVariableTreatments::MakeParameter, 1.0e-8));
    ASSERT_TRUE(nlp->is_reduced());
    const auto before = nlp->structure_epoch();

    // Rejected: the relax treatment needs a positive factor to separate a pair
    // of equal bounds, and this one has none.
    EXPECT_THROW(nlp->configure_variable_treatment(FixedVariableTreatments::RelaxBounds, 0.0),
                 std::invalid_argument);

    EXPECT_NE(nlp->structure_epoch(), before);
    EXPECT_FALSE(nlp->is_reduced());
    EXPECT_EQ(nlp->reduced_primal_vars(), nlp->primal_vars_);
}

TEST(NlpAggregateEngineEpoch, ARejectionThatTouchedNoLayoutDoesNotBump) {
    // The complement, and the reason a bump belongs inside the routine that
    // re-lays rather than at every exit that throws: a configuration rejected
    // before it laid anything leaves the structures the consumer already had.
    // Bumping there would report a structural event that did not happen and
    // would throw away every consumer's cached state for nothing.
    auto problem = std::make_shared<AggPinProblem>();
    problem->fix_all_ = true;
    problem->fix_at_ = 0.5;
    auto nlp = agg_pin_build(problem);

    const auto before = nlp->structure_epoch();
    EXPECT_THROW(nlp->configure_variable_treatment(FixedVariableTreatments::MakeParameter, 1.0e-8),
                 std::invalid_argument);
    EXPECT_EQ(nlp->structure_epoch(), before);
}

// ---------------------------------------------------------------------------
// The structural key and its two captured digests
// ---------------------------------------------------------------------------

TEST(NlpAggregateEngineKey, TheKeySurvivesTheSparsityAnalysis) {
    // The claim digest is captured at claim time because the sparsity analysis
    // REWRITES the claim arrays in place, canonicalising every claim whose
    // column exceeds its row. A digest computed on demand from those arrays
    // would move here, with no structural event to justify it.
    auto nlp = agg_pin_build_small();
    const auto before = nlp->model_structure_key();
    const auto epoch_before = nlp->structure_epoch();

    Eigen::SparseMatrix<double, Eigen::RowMajor> kkt(nlp->kkt_dim_, nlp->kkt_dim_);
    nlp->analyze_sparsity(kkt);

    EXPECT_EQ(nlp->model_structure_key(), before);
    EXPECT_EQ(nlp->structure_epoch(), epoch_before);
}

TEST(NlpAggregateEngineKey, DroppingStagedBoundsDoesNotMoveTheKeyUntilTheNextLay) {
    // The bound digest is captured at lay time for the mirror reason. Dropping
    // the staged bound declarations re-materialises nothing and re-lays
    // nothing, so the structures on hand still have the bound structure they
    // were laid with -- and the key, which describes the declared model AS
    // CURRENTLY LAID, must still report it. It moves when the next
    // transcription lays the new bounds, and not before.
    auto problem = std::make_shared<AggPinProblem>();
    problem->fix_ = 1;
    problem->fix_at_ = -0.7;
    auto nlp = agg_pin_build(problem);

    const auto laid = nlp->model_structure_key();
    const auto epoch = nlp->structure_epoch();

    nlp->clear_variable_bounds();
    EXPECT_EQ(nlp->model_structure_key(), laid) << "the layout did not change, so the key must not";
    EXPECT_EQ(nlp->structure_epoch(), epoch);

    nlp->make_nlp(nlp->primal_vars_, nlp->user_equal_cons_, nlp->inequal_cons_);
    EXPECT_NE(nlp->model_structure_key(), laid) << "the re-lay carries the new bound structure";
    EXPECT_NE(nlp->structure_epoch(), epoch);
}

TEST(NlpAggregateEngineKey, ABoundThatFixesAVariableKeysDifferentlyFromOneThatDoesNot) {
    auto fixed_problem = std::make_shared<AggPinProblem>();
    fixed_problem->fix_ = 0;
    fixed_problem->fix_at_ = 1.0;
    auto unbounded = agg_pin_build(std::make_shared<AggPinProblem>());
    auto fixed = agg_pin_build(fixed_problem);

    EXPECT_NE(unbounded->model_structure_key().bound_digest_,
              fixed->model_structure_key().bound_digest_);
}

// ---------------------------------------------------------------------------
// Layout determinism and partition invariance
// ---------------------------------------------------------------------------

TEST(NlpAggregateEngineLayout, TheSameDeclarationLaysTheSameLayoutTwice) {
    auto first = agg_pin_build_small();
    auto second = agg_pin_build_small();

    ASSERT_EQ(first->model_structure_key(), second->model_structure_key());
    EXPECT_EQ(first->kkt_coeff_rows_, second->kkt_coeff_rows_);
    EXPECT_EQ(first->kkt_coeff_cols_, second->kkt_coeff_cols_);
    EXPECT_EQ(first->rhs_coeff_rows_, second->rhs_coeff_rows_);
    EXPECT_EQ(first->kkt_coeff_part_ids_, second->kkt_coeff_part_ids_);

    Eigen::SparseMatrix<double, Eigen::RowMajor> kkt_first(first->kkt_dim_, first->kkt_dim_);
    Eigen::SparseMatrix<double, Eigen::RowMajor> kkt_second(second->kkt_dim_, second->kkt_dim_);
    first->analyze_sparsity(kkt_first);
    second->analyze_sparsity(kkt_second);
    EXPECT_EQ(first->kkt_locations_, second->kkt_locations_);
}

TEST(NlpAggregateEngineLayout, TheEvaluationThreadCountDecidesNoPartOfTheLayout) {
    // Layout is a function of the declaration and the adopted partition count
    // alone. The evaluation thread budget is neither, so moving it must leave
    // the key, the claim stream and the assembly order untouched.
    const int restore = hven::utils::get_num_threads();

    auto one = agg_pin_build_small();
    one->set_evaluation_threads(1);
    one->negotiate_partition_count(one->num_partitions_);
    const auto key_at_one = one->model_structure_key();
    const Eigen::VectorXi claims_at_one = one->kkt_coeff_rows_;
    const Eigen::VectorXi rhs_at_one = one->rhs_coeff_rows_;

    auto many = agg_pin_build_small();
    many->set_evaluation_threads(4);
    many->negotiate_partition_count(many->num_partitions_);

    EXPECT_EQ(many->model_structure_key(), key_at_one);
    EXPECT_EQ(many->kkt_coeff_rows_, claims_at_one);
    EXPECT_EQ(many->rhs_coeff_rows_, rhs_at_one);

    hven::utils::set_num_threads(restore);
}

TEST(NlpAggregateEnginePartitions, ARenegotiationReportsWhatItAdoptedAndRelays) {
    auto nlp = agg_pin_build(std::make_shared<AggPinWideProblem>());
    const auto before = nlp->structure_epoch();

    const int adopted = nlp->negotiate_partition_count(2);
    EXPECT_GE(adopted, 1);
    EXPECT_LE(adopted, 2);
    EXPECT_EQ(adopted, nlp->num_partitions_);
    EXPECT_EQ(nlp->model_structure_key().partition_count_, adopted);
    EXPECT_NE(nlp->structure_epoch(), before);

    // A count the work cannot support comes down rather than being honoured.
    // Returning the adopted count is what keeps a consumer's key honest.
    auto small = agg_pin_build_small();
    EXPECT_EQ(small->negotiate_partition_count(64), small->num_partitions_);
    EXPECT_EQ(small->model_structure_key().partition_count_, small->num_partitions_);
}

TEST(NlpAggregateEnginePartitions, NoClaimIsRenumberedByARenegotiation) {
    // Partition invariance. Partitions decide which thread evaluates a piece
    // and in what order claims are handed out; they never decide what a claim
    // NAMES. So the multiset of claimed (row, column) pairs -- and the
    // assembled sparsity pattern it produces -- must survive a renegotiation
    // that changes the adopted count.
    auto nlp = agg_pin_build(std::make_shared<AggPinWideProblem>());

    auto claim_multiset = [](const NonLinearProgram &program) {
        std::vector<std::pair<int, int>> claims;
        claims.reserve(static_cast<std::size_t>(program.num_user_kkt_elems_));
        for (int slot = 0; slot < program.num_user_kkt_elems_; slot++) {
            claims.emplace_back(program.kkt_coeff_rows_[slot], program.kkt_coeff_cols_[slot]);
        }
        std::sort(claims.begin(), claims.end());
        return claims;
    };

    nlp->negotiate_partition_count(1);
    const auto claims_at_one = claim_multiset(*nlp);
    const int dim_at_one = nlp->kkt_dim_;
    Eigen::SparseMatrix<double, Eigen::RowMajor> pattern_at_one(dim_at_one, dim_at_one);
    nlp->analyze_sparsity(pattern_at_one);

    const int adopted = nlp->negotiate_partition_count(3);
    const auto claims_after = claim_multiset(*nlp);
    Eigen::SparseMatrix<double, Eigen::RowMajor> pattern_after(nlp->kkt_dim_, nlp->kkt_dim_);
    nlp->analyze_sparsity(pattern_after);

    EXPECT_EQ(claims_after, claims_at_one);
    EXPECT_EQ(nlp->kkt_dim_, dim_at_one);
    EXPECT_EQ(pattern_after.nonZeros(), pattern_at_one.nonZeros());
    for (int k = 0; k < pattern_after.nonZeros(); k++) {
        EXPECT_EQ(pattern_after.innerIndexPtr()[k], pattern_at_one.innerIndexPtr()[k]);
    }

    // The claim ORDER is partition-dependent even when the claim SET is not,
    // which is why the adopted count is a conjunct of the key rather than
    // something a consumer may assume away.
    EXPECT_EQ(nlp->model_structure_key().partition_count_, adopted);

    // HOW FAR THIS ACTUALLY REACHES, recorded rather than left to be assumed.
    // The pieces this vehicle builds all carry the main-thread policy, so the
    // partitioner puts every one of them on a single partition and no claim
    // ever lands on another. The invariance above is therefore established over
    // a single-partition claim assignment: it proves a renegotiation renumbers
    // nothing, and it does NOT exercise several partitions claiming into one
    // arena. Genuine cross-partition contention needs a provider whose pieces
    // split by application, which is a transcription-side vehicle.
    EXPECT_EQ(agg_pin_distinct_claim_partitions(*nlp), 1)
        << "the vehicle's spread changed; the comment above no longer describes what this "
           "test establishes";
}

TEST(NlpAggregateEnginePartitions, TheRightHandSideIsBitStableAcrossPartitionCounts) {
    // The property behind the capability declaration. Right-hand-side claims
    // are contention-free per slot and folded in claim order, so the
    // accumulation order is a function of the layout alone and the assembled
    // blocks come back bit-identical at any adopted partition count. An
    // in-place locked scatter could not promise this, which is why the
    // intermediate the fold reads from is load-bearing rather than a copy to be
    // optimised away.
    auto nlp = agg_pin_build(std::make_shared<AggPinWideProblem>());

    const Eigen::VectorXd x = Eigen::VectorXd::LinSpaced(AggPinWideProblem::kVars, -1.0, 1.0);
    const Eigen::VectorXd le = Eigen::VectorXd::LinSpaced(AggPinWideProblem::kCons, 0.25, -0.75);
    const Eigen::VectorXd li;

    nlp->negotiate_partition_count(1);
    AggPinSolverStorage at_one(*nlp);
    at_one.reset();
    nlp->eval_rhs(1.0, x, le, li, at_one.objective, at_one.pgx, at_one.agx, at_one.fxe, at_one.fxi);

    nlp->negotiate_partition_count(3);
    AggPinSolverStorage at_many(*nlp);
    at_many.reset();
    nlp->eval_rhs(1.0, x, le, li, at_many.objective, at_many.pgx, at_many.agx, at_many.fxe,
                  at_many.fxi);

    EXPECT_EQ(at_many.pgx, at_one.pgx);
    EXPECT_EQ(at_many.agx, at_one.agx);
    EXPECT_EQ(at_many.fxe, at_one.fxe);

    // Same honesty as the invariance test: every piece here sits on one
    // partition, so what this establishes is that the fold is layout-ordered
    // and reproducible, not that it survives several partitions accumulating
    // into one arena at once. The claim-slot ownership that makes the latter
    // safe is structural, but it is not what this test exercises.
    EXPECT_EQ(agg_pin_distinct_claim_partitions(*nlp), 1)
        << "the vehicle's spread changed; the comment above no longer describes what this "
           "test establishes";
}

// ---------------------------------------------------------------------------
// The destination binding
// ---------------------------------------------------------------------------

TEST(NlpAggregateEngineBinding, TheAnalysisBindsTheTablesAndTheEntryChecksTheView) {
    auto nlp = agg_pin_build_small();
    EXPECT_EQ(nlp->bound_kkt_destination(), nullptr) << "nothing analysed yet";

    AggPinSolverStorage bound(*nlp);
    EXPECT_EQ(nlp->bound_kkt_destination(), bound.kkt.valuePtr());

    // A second matrix of the same shape is still the wrong destination: the
    // location table describes offsets into the array the analysis walked.
    Eigen::SparseMatrix<double, Eigen::RowMajor> other = bound.kkt;
    KktScatterView wrong{other.valuePtr(), static_cast<int>(other.nonZeros()),
                         &nlp->kkt_location_table()};

    const Eigen::VectorXd x = agg_pin_x();
    const Eigen::VectorXd le = agg_pin_le();
    const Eigen::VectorXd li = agg_pin_li();
    try {
        nlp->assemble(CandidatePoint{x, le, li}, hven::solvers::kRequestFullKkt, wrong,
                      bound.rhs_view(*nlp));
        FAIL() << "a view naming a different destination must be refused";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("re-run the analysis"), std::string::npos) << message;
    }
}

TEST(NlpAggregateEngineBinding, ARelayDropsTheBindingBecauseItDropsTheOffsets) {
    auto nlp = agg_pin_build_small();
    AggPinSolverStorage storage(*nlp);
    ASSERT_NE(nlp->bound_kkt_destination(), nullptr);

    nlp->negotiate_partition_count(1);
    EXPECT_EQ(nlp->bound_kkt_destination(), nullptr)
        << "a re-lay resets the location table, so it names no destination";
}

// ---------------------------------------------------------------------------
// The candidate surface
// ---------------------------------------------------------------------------

TEST(NlpAggregateEngineCandidate, TheEntriesAssignRatherThanAccumulate) {
    // The rule the candidate surface keeps and assemble deliberately does not.
    // Twice at one point must produce the values at that point, not twice them:
    // the caller is a scorer holding a scratch buffer, and requiring it to
    // pre-zero would be an obligation with nothing to buy it.
    auto nlp = agg_pin_build_small();
    const Eigen::VectorXd x = agg_pin_x();
    const Eigen::VectorXd le = agg_pin_le();
    const Eigen::VectorXd li = agg_pin_li();

    double objective = 0.0;
    Eigen::VectorXd fxe(nlp->equal_cons_);
    Eigen::VectorXd fxi(nlp->inequal_cons_);
    CandidateValues values{objective, fxe, fxi};
    const Eigen::VectorXd none;

    nlp->evaluate_candidate_values(CandidatePoint{x, none, none}, values);
    const double first_objective = objective;
    const Eigen::VectorXd first_fxe = fxe;
    const Eigen::VectorXd first_fxi = fxi;

    nlp->evaluate_candidate_values(CandidatePoint{x, none, none}, values);
    EXPECT_DOUBLE_EQ(objective, first_objective);
    EXPECT_EQ(fxe, first_fxe);
    EXPECT_EQ(fxi, first_fxi);

    Eigen::VectorXd pgx(nlp->primal_vars_);
    Eigen::VectorXd agx(nlp->primal_vars_);
    CandidateFirstOrder first_order{values, pgx, agx};
    nlp->evaluate_candidate_first_order(CandidatePoint{x, le, li}, first_order);
    const Eigen::VectorXd first_pgx = pgx;
    const Eigen::VectorXd first_agx = agx;

    nlp->evaluate_candidate_first_order(CandidatePoint{x, le, li}, first_order);
    EXPECT_EQ(pgx, first_pgx);
    EXPECT_EQ(agx, first_agx);
}

TEST(NlpAggregateEngineCandidate, TheSurfaceIsInDeclarationSpaceWhateverTheTreatment) {
    // The identity-space principle. A variable eliminated from the system the
    // solver factorizes still has its declared index here, so a scorer written
    // against the declaration alone keeps working across a treatment change --
    // it never learns that the provider narrowed anything.
    auto problem = std::make_shared<AggPinProblem>();
    problem->fix_ = 2;
    problem->fix_at_ = 0.4;
    auto nlp = agg_pin_build(problem);

    ASSERT_TRUE(nlp->configure_variable_treatment(FixedVariableTreatments::MakeParameter, 1.0e-8));
    ASSERT_TRUE(nlp->is_reduced());
    ASSERT_LT(nlp->reduced_primal_vars(), nlp->primal_vars_);

    // Declaration space, not the narrowed one.
    EXPECT_EQ(nlp->declaration().primal_vars_, nlp->primal_vars_);

    const Eigen::VectorXd x = agg_pin_x();
    const Eigen::VectorXd le = agg_pin_le();
    const Eigen::VectorXd li = agg_pin_li();

    double objective = 0.0;
    Eigen::VectorXd fxe(nlp->equal_cons_);
    Eigen::VectorXd fxi(nlp->inequal_cons_);
    Eigen::VectorXd pgx(nlp->primal_vars_);
    Eigen::VectorXd agx(nlp->primal_vars_);
    CandidateFirstOrder out{CandidateValues{objective, fxe, fxi}, pgx, agx};

    EXPECT_NO_THROW(nlp->evaluate_candidate_first_order(CandidatePoint{x, le, li}, out));
    EXPECT_EQ(pgx.size(), nlp->primal_vars_);

    // The eliminated coordinate is pinned at its declared value however the
    // caller filled it, so the residuals do not depend on what was passed
    // there.
    Eigen::VectorXd moved = x;
    moved[2] = x[2] + 5.0;
    Eigen::VectorXd fxe_moved(nlp->equal_cons_);
    Eigen::VectorXd fxi_moved(nlp->inequal_cons_);
    double objective_moved = 0.0;
    CandidateValues values_moved{objective_moved, fxe_moved, fxi_moved};
    const Eigen::VectorXd none;
    nlp->evaluate_candidate_values(CandidatePoint{moved, none, none}, values_moved);

    Eigen::VectorXd fxe_original(nlp->equal_cons_);
    Eigen::VectorXd fxi_original(nlp->inequal_cons_);
    double objective_original = 0.0;
    CandidateValues values_original{objective_original, fxe_original, fxi_original};
    nlp->evaluate_candidate_values(CandidatePoint{x, none, none}, values_original);

    EXPECT_EQ(fxe_moved, fxe_original);
    EXPECT_DOUBLE_EQ(objective_moved, objective_original);
}

TEST(NlpAggregateEngineCandidate, TheProbePairsTheEpochWithTheValuesAtThePoint) {
    auto nlp = agg_pin_build_small();
    const Eigen::VectorXd x = agg_pin_x();
    Eigen::VectorXd y = x;
    y[0] += 1.0;

    const auto here = nlp->probe_identity(x);
    EXPECT_EQ(here, nlp->probe_identity(x));
    EXPECT_NE(here, nlp->probe_identity(y));
    EXPECT_EQ(here.epoch_, nlp->structure_epoch());

    nlp->negotiate_partition_count(1);
    const auto after = nlp->probe_identity(x);
    EXPECT_NE(after.epoch_, here.epoch_);
    EXPECT_EQ(after.value_digest_, here.value_digest_) << "same point, same values";
}

// ---------------------------------------------------------------------------
// The declaration and the capability declaration
// ---------------------------------------------------------------------------

TEST(NlpAggregateEngineDeclaration, ItDescribesTheStructuresOnHandAndIsStoredState) {
    auto nlp = agg_pin_build_small();
    const hven::solvers::AggregateDeclaration &first = nlp->declaration();
    EXPECT_EQ(&first, &nlp->declaration()) << "stored state, not rebuilt per call";

    EXPECT_EQ(first.primal_vars_, nlp->primal_vars_);
    EXPECT_EQ(first.equality_rows_, nlp->equal_cons_);
    EXPECT_EQ(first.inequality_rows_, nlp->inequal_cons_);
    EXPECT_EQ(first.partition_count_, nlp->num_partitions_);
    EXPECT_EQ(first.objectives_.size(), nlp->objectives_.size());
    EXPECT_EQ(first.equality_constraints_.size(), nlp->equality_constraints_.size());
    EXPECT_NO_THROW(first.validate());
}

TEST(NlpAggregateEngineDeclaration, StagingABoundChangesNothingUntilItIsMaterialized) {
    auto nlp = agg_pin_build_small();
    const std::size_t staged = nlp->declaration().variable_bounds_.size();
    const auto key = nlp->model_structure_key();

    nlp->set_variable_bound(0, -2.0, 2.0);
    EXPECT_EQ(nlp->declaration().variable_bounds_.size(), staged)
        << "a staged declaration is not a structural mutation";
    EXPECT_EQ(nlp->model_structure_key(), key);

    nlp->make_nlp(nlp->primal_vars_, nlp->user_equal_cons_, nlp->inequal_cons_);
    EXPECT_EQ(nlp->declaration().variable_bounds_.size(), staged + 1);
}

TEST(NlpAggregateEngineCapabilities, TheValuesFastPathIsDeclaredAndDirectScatterIsNot) {
    auto nlp = agg_pin_build_small();
    const AggregateCapability declared = nlp->capabilities();

    EXPECT_TRUE(has_capability(declared, AggregateCapability::kValuesFastPath));
    // Not declared, and the reason is the right-hand-side fold rather than the
    // KKT fill: the flag is the weakest claim over the whole provider, so one
    // path holding an intermediate settles it for all of them.
    EXPECT_FALSE(has_capability(declared, AggregateCapability::kDirectScatter));
}

// ---------------------------------------------------------------------------
// The destination binding, continued: the same container, re-patterned
// ---------------------------------------------------------------------------

TEST(NlpAggregateEngineBinding, RePatterningTheAnalysedMatrixIsCaughtAsAStaleTable) {
    // The failure a live re-read of the matrix would let through, and the
    // reason the bound address is captured as a VALUE. Nothing here swaps in a
    // different matrix: it is the same object the analysis was handed, given a
    // new pattern. Its value array moves; every offset the location table holds
    // is left describing storage that is gone. A provider that re-read
    // valuePtr() on demand would move with it and see two addresses that agree.
    auto nlp = agg_pin_build_small();
    Eigen::SparseMatrix<double, Eigen::RowMajor> kkt(nlp->kkt_dim_, nlp->kkt_dim_);
    nlp->analyze_sparsity(kkt);

    const double *bound = nlp->bound_kkt_destination();
    ASSERT_NE(bound, nullptr);
    ASSERT_EQ(bound, kkt.valuePtr());

    // A new, much larger pattern on the SAME object.
    const int wide = 2 * nlp->kkt_dim_;
    std::vector<Eigen::Triplet<double>> pattern;
    for (int row = 0; row < wide; row++) {
        for (int col = 0; col <= row; col++) {
            pattern.emplace_back(row, col, 1.0);
        }
    }
    kkt.resize(wide, wide);
    kkt.setFromTriplets(pattern.begin(), pattern.end());
    kkt.makeCompressed();

    // The half that is deterministic regardless of where the new array landed,
    // and the half a live re-read gets wrong: the accessor still reports the
    // address captured at analysis time.
    EXPECT_EQ(nlp->bound_kkt_destination(), bound);
    ASSERT_NE(kkt.valuePtr(), bound)
        << "the re-pattern did not move the value array, so there is no stale table to detect";

    const Eigen::VectorXd x = agg_pin_x();
    const Eigen::VectorXd le = agg_pin_le();
    const Eigen::VectorXd li = agg_pin_li();

    // Built by hand rather than through the storage fixture: that fixture runs
    // the sparsity analysis in its constructor, which would re-bind the engine
    // to its own matrix and dissolve the very staleness under test.
    Eigen::VectorXd pgx = Eigen::VectorXd::Zero(nlp->reduced_primal_vars_count_);
    Eigen::VectorXd agx = Eigen::VectorXd::Zero(nlp->reduced_primal_vars_count_);
    Eigen::VectorXd fxe = Eigen::VectorXd::Zero(nlp->equal_cons_);
    Eigen::VectorXd fxi = Eigen::VectorXd::Zero(nlp->inequal_cons_);
    double objective = 0.0;
    RhsScatterView rhs;
    rhs.objective_ = &objective;
    rhs.objective_gradient_ =
        RhsArenaView{pgx.data(), static_cast<int>(pgx.size()), &nlp->objective_gradient_table()};
    rhs.constraint_adjoint_gradient_ = RhsArenaView{agx.data(), static_cast<int>(agx.size()),
                                                    &nlp->constraint_adjoint_gradient_table()};
    rhs.equality_residuals_ =
        RhsArenaView{fxe.data(), static_cast<int>(fxe.size()), &nlp->equality_residual_table()};
    rhs.inequality_residuals_ =
        RhsArenaView{fxi.data(), static_cast<int>(fxi.size()), &nlp->inequality_residual_table()};

    KktScatterView now{kkt.valuePtr(), static_cast<int>(kkt.nonZeros()),
                       &nlp->kkt_location_table()};
    try {
        nlp->assemble(CandidatePoint{x, le, li}, hven::solvers::kRequestFullKkt, now, rhs);
        FAIL() << "a re-patterned destination must be refused, not written through";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("re-run the analysis"), std::string::npos) << message;
    }
}

// ---------------------------------------------------------------------------
// The bound snapshot
// ---------------------------------------------------------------------------

TEST(NlpAggregateEngineKey, StagingABoundThenConfiguringATreatmentDoesNotMoveTheBoundDigest) {
    // A treatment configuration re-lays the structures, and a re-lay refreshes
    // the declaration. What it must NOT do is fold in a bound record staged
    // since the last materialization: that record describes a problem these
    // structures were not laid for, and the key answers for the layout on hand.
    auto problem = std::make_shared<AggPinProblem>();
    problem->fix_ = 2;
    problem->fix_at_ = 0.4;
    auto nlp = agg_pin_build(problem);

    const auto laid = nlp->model_structure_key();

    nlp->set_variable_bound(0, -3.0, 3.0); // staged; nothing has materialized it
    ASSERT_TRUE(nlp->configure_variable_treatment(FixedVariableTreatments::MakeParameter, 1.0e-8));

    EXPECT_EQ(nlp->model_structure_key().bound_digest_, laid.bound_digest_)
        << "a staged record reached the key without a re-materialization";

    // And it does move once the record is actually laid.
    nlp->make_nlp(nlp->primal_vars_, nlp->user_equal_cons_, nlp->inequal_cons_);
    EXPECT_NE(nlp->model_structure_key().bound_digest_, laid.bound_digest_);
}

TEST(NlpAggregateEngineEpoch, AnInvalidStagedRecordCannotBreakARenegotiation) {
    // The failure mode the snapshot closes, at the point where it would hurt
    // most. A record staged with an out-of-range index is not validated until
    // something materializes it. If the declaration refresh read the live
    // staging list, a renegotiation would carry that record into the bound
    // digest -- which runs AFTER the structures have been re-laid and BEFORE
    // the epoch is bumped, so the throw would leave the old epoch standing over
    // new tables, which is exactly the state the epoch exists to prevent.
    auto nlp = agg_pin_build_small();
    const auto epoch = nlp->structure_epoch();
    const auto key = nlp->model_structure_key();

    nlp->set_variable_bound(nlp->primal_vars_ + 5, 0.0, 1.0);

    EXPECT_NO_THROW(nlp->negotiate_partition_count(1));
    EXPECT_NE(nlp->structure_epoch(), epoch) << "the re-lay must still be reported";
    EXPECT_EQ(nlp->model_structure_key(), key) << "the unlaid record must not reach the key";

    // The record is still staged, and materializing it is still an error --
    // the snapshot defers the rejection to the boundary that owns it rather
    // than swallowing it.
    EXPECT_THROW(nlp->make_nlp(nlp->primal_vars_, nlp->user_equal_cons_, nlp->inequal_cons_),
                 std::invalid_argument);
}

// ---------------------------------------------------------------------------
// Models with rows of only one kind
// ---------------------------------------------------------------------------

namespace {

void agg_pin_check_one_kind(bool inequality) {
    auto problem = std::make_shared<AggPinOneKindProblem>();
    problem->inequality_ = inequality;
    auto nlp = agg_pin_build(problem);

    if (inequality) {
        ASSERT_EQ(nlp->equal_cons_, 0);
        ASSERT_GT(nlp->inequal_cons_, 0);
    } else {
        ASSERT_GT(nlp->equal_cons_, 0);
        ASSERT_EQ(nlp->inequal_cons_, 0);
    }

    Eigen::VectorXd x(nlp->primal_vars_);
    x << 0.5, -1.25, 2.0;
    const Eigen::VectorXd none;

    // Through the hot-path entry. One residual arena is legitimately
    // zero-length; the request names both, and both must be accepted.
    AggPinSolverStorage storage(*nlp);
    storage.reset();
    EXPECT_NO_THROW(nlp->assemble(CandidatePoint{x, none, none},
                                  hven::solvers::kRequestObjectiveAndConstraints, KktScatterView{},
                                  storage.rhs_view(*nlp)));

    double reference = 0.0;
    problem->eval_f(x, reference);
    EXPECT_DOUBLE_EQ(storage.objective, reference);

    // And through the off-path values entry, which sizes its blocks from the
    // declaration and so meets the same zero-length block.
    double objective = 0.0;
    Eigen::VectorXd fxe(nlp->declaration().equality_rows_);
    Eigen::VectorXd fxi(nlp->declaration().inequality_rows_);
    CandidateValues values{objective, fxe, fxi};
    EXPECT_NO_THROW(nlp->evaluate_candidate_values(CandidatePoint{x, none, none}, values));
    EXPECT_DOUBLE_EQ(objective, reference);

    const Eigen::VectorXd &populated = inequality ? fxi : fxe;
    EXPECT_FALSE(populated.isZero()) << "the block the model does declare was not filled";
}

} // namespace

TEST(NlpAggregateEngineContract, AnEqualityOnlyModelEvaluatesValuesThroughTheContract) {
    agg_pin_check_one_kind(/*inequality=*/false);
}

TEST(NlpAggregateEngineContract, AnInequalityOnlyModelEvaluatesValuesThroughTheContract) {
    agg_pin_check_one_kind(/*inequality=*/true);
}

// ---------------------------------------------------------------------------
// Request legality, reached through the engine's own entry
// ---------------------------------------------------------------------------

TEST(NlpAggregateEngineContract, AnUnmappedRequestIsRefusedAtTheEngineEntry) {
    // The refusal is the entry's, not the engine's, and it is reachable through
    // the engine like any other implementation: a flag set outside the eight
    // mapped shapes never reaches a hook.
    auto nlp = agg_pin_build_small();
    AggPinSolverStorage storage(*nlp);
    const Eigen::VectorXd x = agg_pin_x();
    const Eigen::VectorXd none;

    EXPECT_THROW(nlp->assemble(CandidatePoint{x, none, none}, EvalRequest::kObjectiveGradient,
                               storage.kkt_view(*nlp), storage.rhs_view(*nlp)),
                 std::invalid_argument);
}

// ---------------------------------------------------------------------------
// Declared identities across a treatment reconfiguration
// ---------------------------------------------------------------------------

TEST(NlpAggregateEngineDeclaration, DeclaredRowIdentitiesSurviveATreatmentReconfiguration) {
    // The append-only property. The treatment that adds rows adds them AFTER
    // every row the transcription declared, so the row space grows at its tail
    // and nothing a consumer addresses by declared identity moves. The
    // reconfiguration is a structural mutation and shows up as one -- in the
    // declaration, the key and the epoch -- while every previously declared row
    // keeps the destination it had.
    auto problem = std::make_shared<AggPinProblem>();
    problem->fix_ = 2;
    problem->fix_at_ = 0.4;
    auto nlp = agg_pin_build(problem);

    const int declared_rows_before = nlp->declaration().equality_rows_;
    const auto epoch_before = nlp->structure_epoch();
    const auto key_before = nlp->model_structure_key();

    const RhsLocationTable &econ = nlp->equality_residual_table();
    std::vector<int> rows_before;
    rows_before.reserve(static_cast<std::size_t>(econ.size()));
    for (int slot = 0; slot < econ.size(); slot++) {
        rows_before.push_back(econ.location(slot));
    }

    ASSERT_TRUE(nlp->configure_variable_treatment(FixedVariableTreatments::MakeConstraint, 1.0e-8));

    // Visible as a structural mutation.
    EXPECT_NE(nlp->structure_epoch(), epoch_before);
    EXPECT_NE(nlp->model_structure_key(), key_before);
    EXPECT_EQ(nlp->internal_fixed_constraints(), 1);
    EXPECT_EQ(nlp->declaration().equality_rows_, declared_rows_before + 1);

    // Invisible in indexing: every claim the transcription's own pieces held
    // still lands on the row it landed on before, and the new row is appended
    // past them.
    const RhsLocationTable &econ_after = nlp->equality_residual_table();
    ASSERT_GE(econ_after.size(), static_cast<int>(rows_before.size()));
    for (std::size_t slot = 0; slot < rows_before.size(); slot++) {
        EXPECT_EQ(econ_after.location(static_cast<int>(slot)), rows_before[slot])
            << "declared row identity moved at claim slot " << slot;
    }
}

// ---------------------------------------------------------------------------
// What a scorer needs, from the declaration alone
// ---------------------------------------------------------------------------

TEST(NlpAggregateEngineCandidate, TheScorerExclusionSetIsComputableFromTheDeclarationAlone) {
    // A declared-fixed coordinate carries no degree of freedom, so its
    // stationarity row is not a stationarity condition and a correct scorer
    // skips it. The whole point of the rule is that the set of such coordinates
    // is a fact about the DECLARATION: this computes it without reading one
    // piece of engine state, and gets the same answer whatever treatment is
    // configured -- which is what makes a scorer written against this surface
    // independent of the provider behind it.
    auto problem = std::make_shared<AggPinProblem>();
    problem->fix_ = 2;
    problem->fix_at_ = 0.4;
    auto nlp = agg_pin_build(problem);

    auto exclusion_set = [](const hven::solvers::AggregateDeclaration &declaration) {
        std::vector<int> fixed;
        for (const hven::solvers::VariableBound &bound :
             declaration.materialize_variable_bounds()) {
            if (bound.lower_ == bound.upper_) {
                fixed.push_back(bound.index_);
            }
        }
        return fixed;
    };

    const std::vector<int> expected{2};
    EXPECT_EQ(exclusion_set(nlp->declaration()), expected);

    // The same answer under every treatment: the declaration is what decides
    // it, and the treatment decides only what the provider does about it.
    ASSERT_TRUE(nlp->configure_variable_treatment(FixedVariableTreatments::MakeParameter, 1.0e-8));
    EXPECT_EQ(exclusion_set(nlp->declaration()), expected);
    ASSERT_TRUE(nlp->configure_variable_treatment(FixedVariableTreatments::RelaxBounds, 1.0e-8));
    EXPECT_EQ(exclusion_set(nlp->declaration()), expected);
    ASSERT_TRUE(nlp->configure_variable_treatment(FixedVariableTreatments::MakeConstraint, 1.0e-8));
    EXPECT_EQ(exclusion_set(nlp->declaration()), expected);
}

TEST(NlpAggregateEnginePartitions, ANonPositivePartitionCountIsRefused) {
    // Capping a request the work cannot support is honest and is reported
    // through the return value. A request for zero or fewer partitions is not a
    // request this can serve, and serving one instead would hand the caller a
    // count it never asked for and would then key its structural key on.
    auto nlp = agg_pin_build_small();
    const auto epoch = nlp->structure_epoch();
    try {
        nlp->negotiate_partition_count(0);
        FAIL() << "a non-positive partition count must be refused";
    } catch (const std::invalid_argument &error) {
        EXPECT_NE(std::string(error.what()).find('0'), std::string::npos) << error.what();
    }
    EXPECT_THROW(nlp->negotiate_partition_count(-3), std::invalid_argument);
    EXPECT_EQ(nlp->structure_epoch(), epoch) << "a refused request must re-lay nothing";
}

TEST(NlpAggregateEngineContract, AShortGradientViewIsRefusedByTheProvider) {
    // The half of the destination checks the entry cannot make. A gradient
    // arena's length is the solver's own primal block, not the declared
    // variable count, so the contract can only ask whether the view is THERE.
    // Non-null but short is the hole that leaves, and it is not benign: the
    // fill writes target[row] for every claim the table names, so a short view
    // is written past its end. The provider closes it at its own hook.
    auto nlp = agg_pin_build_small();
    AggPinSolverStorage storage(*nlp);
    storage.reset();

    const Eigen::VectorXd x = agg_pin_x();
    const Eigen::VectorXd le = agg_pin_le();
    const Eigen::VectorXd li = agg_pin_li();
    const int laid = nlp->reduced_primal_vars_count_;
    ASSERT_GT(laid, 1);

    RhsScatterView rhs = storage.rhs_view(*nlp);
    rhs.objective_gradient_ =
        RhsArenaView{storage.pgx.data(), laid - 1, &nlp->objective_gradient_table()};

    try {
        nlp->assemble(CandidatePoint{x, le, li}, hven::solvers::kRequestFirstOrderRhs,
                      KktScatterView{}, rhs);
        FAIL() << "a gradient view shorter than the laid width must be refused";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find(std::to_string(laid - 1)), std::string::npos) << message;
        EXPECT_NE(message.find(std::to_string(laid)), std::string::npos) << message;
        EXPECT_NE(message.find("objective gradient"), std::string::npos) << message;
    }

    // Nothing was evaluated: the refusal comes before any pass runs.
    EXPECT_EQ(storage.objective, 0.0);

    // The same view at the laid width is accepted, so what was refused is the
    // length and not the arena.
    RhsScatterView correct = storage.rhs_view(*nlp);
    EXPECT_NO_THROW(nlp->assemble(CandidatePoint{x, le, li}, hven::solvers::kRequestFirstOrderRhs,
                                  KktScatterView{}, correct));

    // And the adjoint-gradient arena is checked the same way.
    RhsScatterView short_adjoint = storage.rhs_view(*nlp);
    short_adjoint.constraint_adjoint_gradient_ =
        RhsArenaView{storage.agx.data(), laid - 1, &nlp->constraint_adjoint_gradient_table()};
    EXPECT_THROW(nlp->assemble(CandidatePoint{x, le, li}, hven::solvers::kRequestFirstOrderRhs,
                               KktScatterView{}, short_adjoint),
                 std::invalid_argument);
}
