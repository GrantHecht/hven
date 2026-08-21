// The partitioned evaluation engine AS a Level 2 provider.
//
// tests/model/ pins what the contract guarantees with no engine behind it, and
// deliberately holds nothing but a fake. These are the same guarantees against
// the REAL engine: the structure epoch's ordering and failure-restore rules on
// the engine's own re-lay paths, the structural key's two captured digests,
// layout determinism and partition invariance over its actual claim arena, the
// destination binding its location tables carry, and -- the pin the whole carve
// rests on -- that an assemble request produces exactly what the evaluation
// entry point it replaces produces, once the consumer scatters the solver
// coefficients that assemble deliberately leaves to it.
//
// UNITY-BUILD NOTE: this suite is compiled with UNITY_BUILD ON, and anonymous
// namespaces do not isolate across unity-merged translation units. Every helper
// here therefore carries a distinct prefix rather than relying on the anonymous
// namespace to keep it local.

#include <gtest/gtest.h>

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

/// A problem wide enough that the partition cap does not pin the count at one.
/// The cap is num_user_kkt_elems_ / kMinKktElementsPerPartition, so a dense
/// Jacobian over this many rows and columns leaves room to renegotiate.
struct AggPinWideProblem : NLPProblem {
    static constexpr int kVars = 60;
    static constexpr int kCons = 60;

    int num_vars() const override { return kVars; }
    int num_cons() const override { return kCons; }
    int num_jac_nonzeros() const override { return kVars * kCons; }
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
        int k = 0;
        for (int row = 0; row < kCons; row++) {
            for (int col = 0; col < kVars; col++) {
                r[k] = row;
                c[k] = col;
                k++;
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
            v[row * kVars + row] = 1.0;
            v[row * kVars + ((row + 1) % kVars)] += 0.5;
        }
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd>, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v.setConstant(obj_factor);
    }
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

/// The pin the carve rests on. For each of the eight shapes: run the legacy
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

        EXPECT_DOUBLE_EQ(assembled.objective, legacy.objective);
        EXPECT_TRUE(assembled.fxe.isApprox(legacy.fxe) ||
                    (assembled.fxe.isZero() && legacy.fxe.isZero()))
            << assembled.fxe.transpose() << " vs " << legacy.fxe.transpose();
        EXPECT_TRUE(assembled.fxi.isApprox(legacy.fxi) ||
                    (assembled.fxi.isZero() && legacy.fxi.isZero()));
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
}

TEST(NlpAggregateEnginePartitions, TheRightHandSideIsBitStableAcrossPartitionCounts) {
    // The property the capability declaration rests on. Right-hand-side claims
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
