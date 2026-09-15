// Layout wall probe. The IPM wall leg beside it times WHOLE SOLVES of a dense
// single-piece problem; this one times the LAYOUT -- construction, the
// transcription that lays the claim arena, and one partitioned whole solve --
// on a problem shaped like the transcriptions a collocation front end produces:
// several pieces, each applied many times over overlapping variable windows,
// evaluated over more than one partition, assembled into a sparse KKT.
//
// WHY A SHAPE OF ITS OWN. A layout's cost scales with the number of CLAIMS and
// the number of PIECES, and the wall leg's problem has three pieces and one
// application each -- so a change to per-claim or per-piece layout work is
// invisible in it. The front end whose transcription cells first showed such a
// change cannot be built from this library alone (its pieces are its own), so
// the cell is written natively here in the same shape: kPieces constraint
// pieces plus an objective piece, N applications spread across them, windows
// that overlap so consecutive applications share variables (which is what makes
// claim columns contested across partitions), and bounds on every variable so
// the barrier has work.
//
// CELLS
//   construct   build the pieces and the program, layout included -- what a
//               front end pays to go from nothing to a laid problem.
//   transcribe  re-lay an already built program (make_nlp again). Layout only.
//   analyze     the sparsity analysis over a laid program -- the other half
//               of a transcription, timed apart because it moved the other
//               way from the layout.
//   clone       the deep copy of the three master piece lists, alone. The
//               partitioner refills its per-partition vectors from those lists
//               at every lay and each push_back deep-clones a type-erased
//               piece, so some share of `transcribe` is piece cloning and no
//               one had ever measured which share. INFORMATIONAL, and a LOWER
//               BOUND -- see the cell.
//   transcribe+key   the same re-lay, plus ONE model_structure_key() read.
//   transcribe+decl  the same re-lay, plus ONE declaration() read -- the cell
//               that prices the deferred PIECE COPY, which is what the first
//               assemble() after a lay pays and what the two cells above
//               never discharge.
//               INFORMATIONAL: the structural key's two digests are taken on
//               first read rather than during the lay, so this cell is where
//               that deferred cost is visible. transcribe is what a consumer
//               that never asks about structural identity pays; this is what
//               one that asks once per lay pays.
//   solve       one whole solve over the laid program, at kPartitions.
//   solve1      the same solve at ONE partition -- see solve_cell for why the
//               partitioned cell's objective is not an identity gate and this
//               one's is.
//
// Each cell prints an identity column so two arms can be compared for answers
// as well as for time: the layout cells print the structural key's folded
// digest and the claim count, the solve cells the objective, the iteration
// count and the convergence flag. Driven like ipm_time.cpp -- see
// layout_wall_leg.sh.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <Eigen/Sparse>

#include "hven/drivers/interior_point_solver.h"
#include "hven/model/non_linear_program.h"

using hven::ConstEigenRef;
using hven::EigenRef;
using hven::solvers::ConstraintFunction;
using hven::solvers::ConstraintInterface;
using hven::solvers::DirectFunctionModel;
using hven::solvers::kkt_canonical_lock_col;
using hven::solvers::NonLinearProgram;
using hven::solvers::ObjectiveFunction;
using hven::solvers::ObjectiveInterface;
using hven::solvers::SolverIndexingData;
using hven::solvers::SolverInterfaceAdapter;

namespace {

// One application reads a window of kIn consecutive variables and, for a
// constraint piece, writes kOut rows. Consecutive applications start kStride
// apart, so windows OVERLAP -- which is what puts two partitions on the same
// KKT column and exercises the contested-slot path.
constexpr int kIn = 4;
constexpr int kOut = 2;
constexpr int kStride = 2;
constexpr int kHessEntries = kIn * (kIn + 1) / 2; // dense lower triangle
constexpr int kPieces = 4;                        // constraint pieces the applications spread over
constexpr int kPartitions = 4;

int variables_for(int applications) { return (applications - 1) * kStride + kIn; }

/// Sums one value into the KKT matrix through the claim recorded at `slot`,
/// under the shared lock protocol: a slot whose canonical column is contested
/// is written under that column's mutex.
inline void scatter_one(Eigen::SparseMatrix<double, Eigen::RowMajor> &kkt,
                        EigenRef<Eigen::VectorXi> locations, EigenRef<Eigen::VectorXi> clashes,
                        std::vector<std::mutex> &locks, int slot, int row, int col, double value) {
    if (row < 0 || col < 0) {
        return;
    }
    const int lockcol = kkt_canonical_lock_col(row, col);
    const bool contested = clashes[lockcol] != -1;
    if (contested) {
        locks[clashes[lockcol]].lock();
    }
    kkt.valuePtr()[locations.data()[slot]] += value;
    if (contested) {
        locks[clashes[lockcol]].unlock();
    }
}

/// kOut nonlinear residual rows over a kIn-variable window, with a dense
/// Jacobian and a densely claimed Lagrangian Hessian triangle -- the claim
/// profile a collocation defect produces.
struct WindowConstraintPiece {
    std::string name() const { return "window constraint"; }
    int input_rows() const { return kIn; }
    int output_rows() const { return kOut; }
    bool thread_safe() const { return true; }

    static void residuals(const double *v, double *c) {
        c[0] = v[2] - v[0] - 0.5 * (v[1] + v[3]) - 0.05 * v[1] * v[1];
        c[1] = v[3] - v[1] - 0.25 * v[0] * v[2];
    }
    // Row-major kOut x kIn.
    static void jacobian(const double *v, double *j) {
        j[0] = -1.0;
        j[1] = -0.5 - 0.1 * v[1];
        j[2] = 1.0;
        j[3] = -0.5;
        j[4] = -0.25 * v[2];
        j[5] = -1.0;
        j[6] = -0.25 * v[0];
        j[7] = 1.0;
    }
    // Lower triangle in claim order (i, k <= i), of sum_j lambda_j * d2 c_j.
    static void hessian(const double *lambda, double *h) {
        for (int e = 0; e < kHessEntries; e++) {
            h[e] = 0.0;
        }
        h[2] = -0.10 * lambda[0]; // (1, 1)
        h[3] = -0.25 * lambda[1]; // (2, 0)
    }

    void constraints(ConstEigenRef<Eigen::VectorXd> X, EigenRef<Eigen::VectorXd> FX,
                     const SolverIndexingData &data) const {
        double v[kIn];
        double c[kOut];
        for (int a = 0; a < data.num_appl(); a++) {
            gather(X, data, a, v);
            residuals(v, c);
            for (int j = 0; j < kOut; j++) {
                FX[data.inner_constraint_starts_[a] + j] += c[j];
            }
        }
    }

    void constraints_adjointgradient(ConstEigenRef<Eigen::VectorXd> X,
                                     ConstEigenRef<Eigen::VectorXd> L, EigenRef<Eigen::VectorXd> FX,
                                     EigenRef<Eigen::VectorXd> AGX,
                                     const SolverIndexingData &data) const {
        double v[kIn];
        double c[kOut];
        double j[kOut * kIn];
        for (int a = 0; a < data.num_appl(); a++) {
            gather(X, data, a, v);
            residuals(v, c);
            jacobian(v, j);
            for (int r = 0; r < kOut; r++) {
                FX[data.inner_constraint_starts_[a] + r] += c[r];
            }
            adjoint_gradient(L, AGX, data, a, j);
        }
    }

    void constraints_jacobian(ConstEigenRef<Eigen::VectorXd> X, EigenRef<Eigen::VectorXd> FX,
                              Eigen::SparseMatrix<double, Eigen::RowMajor> &kkt,
                              EigenRef<Eigen::VectorXi> locations,
                              EigenRef<Eigen::VectorXi> clashes, std::vector<std::mutex> &locks,
                              const SolverIndexingData &data) const {
        double v[kIn];
        double c[kOut];
        double j[kOut * kIn];
        for (int a = 0; a < data.num_appl(); a++) {
            gather(X, data, a, v);
            residuals(v, c);
            jacobian(v, j);
            for (int r = 0; r < kOut; r++) {
                FX[data.inner_constraint_starts_[a] + r] += c[r];
            }
            scatter_jacobian(kkt, locations, clashes, locks, data, a, j);
        }
    }

    void constraints_jacobian_adjointgradient(
        ConstEigenRef<Eigen::VectorXd> X, ConstEigenRef<Eigen::VectorXd> L,
        EigenRef<Eigen::VectorXd> FX, EigenRef<Eigen::VectorXd> AGX,
        Eigen::SparseMatrix<double, Eigen::RowMajor> &kkt, EigenRef<Eigen::VectorXi> locations,
        EigenRef<Eigen::VectorXi> clashes, std::vector<std::mutex> &locks,
        const SolverIndexingData &data) const {
        double v[kIn];
        double c[kOut];
        double j[kOut * kIn];
        for (int a = 0; a < data.num_appl(); a++) {
            gather(X, data, a, v);
            residuals(v, c);
            jacobian(v, j);
            for (int r = 0; r < kOut; r++) {
                FX[data.inner_constraint_starts_[a] + r] += c[r];
            }
            adjoint_gradient(L, AGX, data, a, j);
            scatter_jacobian(kkt, locations, clashes, locks, data, a, j);
        }
    }

    void constraints_jacobian_adjointgradient_adjointhessian(
        ConstEigenRef<Eigen::VectorXd> X, ConstEigenRef<Eigen::VectorXd> L,
        EigenRef<Eigen::VectorXd> FX, EigenRef<Eigen::VectorXd> AGX,
        Eigen::SparseMatrix<double, Eigen::RowMajor> &kkt, EigenRef<Eigen::VectorXi> locations,
        EigenRef<Eigen::VectorXi> clashes, std::vector<std::mutex> &locks,
        const SolverIndexingData &data) const {
        double v[kIn];
        double c[kOut];
        double j[kOut * kIn];
        double lam[kOut];
        double h[kHessEntries];
        for (int a = 0; a < data.num_appl(); a++) {
            gather(X, data, a, v);
            residuals(v, c);
            jacobian(v, j);
            for (int r = 0; r < kOut; r++) {
                FX[data.inner_constraint_starts_[a] + r] += c[r];
                lam[r] = L[data.c_loc(r, a)];
            }
            adjoint_gradient(L, AGX, data, a, j);
            scatter_jacobian(kkt, locations, clashes, locks, data, a, j);
            hessian(lam, h);
            int slot = data.inner_kkt_starts_[a] + kOut * kIn;
            for (int i = 0, e = 0; i < kIn; i++) {
                for (int k = 0; k <= i; k++, e++, slot++) {
                    scatter_one(kkt, locations, clashes, locks, slot, data.v_scatter_loc(i, a),
                                data.v_scatter_loc(k, a), h[e]);
                }
            }
        }
    }

    void get_kkt_space(EigenRef<Eigen::VectorXi> rows, EigenRef<Eigen::VectorXi> cols, int &freeloc,
                       int conoffset, bool dojac, bool dohess, SolverIndexingData &data) {
        data.inner_kkt_starts_.resize(data.num_appl());
        for (int a = 0; a < data.num_appl(); a++) {
            data.inner_kkt_starts_[a] = freeloc;
            if (dojac) {
                for (int r = 0; r < kOut; r++) {
                    for (int i = 0; i < kIn; i++) {
                        const int col = data.v_scatter_loc(i, a);
                        rows[freeloc] = (col < 0) ? -1 : data.c_loc(r, a) + conoffset;
                        cols[freeloc] = col;
                        freeloc++;
                    }
                }
            }
            if (dohess) {
                claim_hessian_triangle(rows, cols, freeloc, data, a);
            }
        }
    }
    int num_kkt_elements(bool dojac, bool dohess) const {
        return (dojac ? kOut * kIn : 0) + (dohess ? kHessEntries : 0);
    }

    static void gather(ConstEigenRef<Eigen::VectorXd> X, const SolverIndexingData &data, int a,
                       double *v) {
        for (int i = 0; i < kIn; i++) {
            v[i] = X[data.v_loc(i, a)];
        }
    }
    static void claim_hessian_triangle(EigenRef<Eigen::VectorXi> rows,
                                       EigenRef<Eigen::VectorXi> cols, int &freeloc,
                                       const SolverIndexingData &data, int a) {
        for (int i = 0; i < kIn; i++) {
            for (int k = 0; k <= i; k++) {
                const int r = data.v_scatter_loc(i, a);
                const int c = data.v_scatter_loc(k, a);
                const bool live = r >= 0 && c >= 0;
                rows[freeloc] = live ? r : -1;
                cols[freeloc] = live ? c : -1;
                freeloc++;
            }
        }
    }

  private:
    static void adjoint_gradient(ConstEigenRef<Eigen::VectorXd> L, EigenRef<Eigen::VectorXd> AGX,
                                 const SolverIndexingData &data, int a, const double *j) {
        const int gstart = data.inner_gradient_starts_[a];
        for (int r = 0; r < kOut; r++) {
            const double lam = L[data.c_loc(r, a)];
            for (int i = 0; i < kIn; i++) {
                AGX[gstart + i] += j[r * kIn + i] * lam;
            }
        }
    }
    static void scatter_jacobian(Eigen::SparseMatrix<double, Eigen::RowMajor> &kkt,
                                 EigenRef<Eigen::VectorXi> locations,
                                 EigenRef<Eigen::VectorXi> clashes, std::vector<std::mutex> &locks,
                                 const SolverIndexingData &data, int a, const double *j) {
        int slot = data.inner_kkt_starts_[a];
        for (int r = 0; r < kOut; r++) {
            for (int i = 0; i < kIn; i++, slot++) {
                const int col = data.v_scatter_loc(i, a);
                scatter_one(kkt, locations, clashes, locks, slot, data.c_loc(r, a), col,
                            j[r * kIn + i]);
            }
        }
    }
};

/// A separable objective over the same windows: a scalar per application, a
/// gradient over its kIn variables, and a DIAGONAL Hessian claim.
struct WindowObjectivePiece {
    std::string name() const { return "window objective"; }
    int input_rows() const { return kIn; }
    int output_rows() const { return 1; }
    bool thread_safe() const { return true; }

    void objective(double scale, ConstEigenRef<Eigen::VectorXd> X, double &val,
                   const SolverIndexingData &data) const {
        for (int a = 0; a < data.num_appl(); a++) {
            for (int i = 0; i < kIn; i++) {
                const double x = X[data.v_loc(i, a)];
                val += scale * (0.5 * x * x + 0.005 * x * x * x * x);
            }
        }
    }
    void objective_gradient(double scale, ConstEigenRef<Eigen::VectorXd> X, double &val,
                            EigenRef<Eigen::VectorXd> GX, const SolverIndexingData &data) const {
        for (int a = 0; a < data.num_appl(); a++) {
            const int gstart = data.inner_gradient_starts_[a];
            for (int i = 0; i < kIn; i++) {
                const double x = X[data.v_loc(i, a)];
                val += scale * (0.5 * x * x + 0.005 * x * x * x * x);
                GX[gstart + i] += scale * (x + 0.02 * x * x * x);
            }
        }
    }
    void objective_gradient_hessian(double scale, ConstEigenRef<Eigen::VectorXd> X, double &val,
                                    EigenRef<Eigen::VectorXd> GX,
                                    Eigen::SparseMatrix<double, Eigen::RowMajor> &kkt,
                                    EigenRef<Eigen::VectorXi> locations,
                                    EigenRef<Eigen::VectorXi> clashes,
                                    std::vector<std::mutex> &locks,
                                    const SolverIndexingData &data) const {
        this->objective_gradient(scale, X, val, GX, data);
        for (int a = 0; a < data.num_appl(); a++) {
            int slot = data.inner_kkt_starts_[a];
            for (int i = 0; i < kIn; i++, slot++) {
                const double x = X[data.v_loc(i, a)];
                const int col = data.v_scatter_loc(i, a);
                scatter_one(kkt, locations, clashes, locks, slot, col, col,
                            scale * (1.0 + 0.06 * x * x));
            }
        }
    }

    // The KKT sizing pass is shared with the constraint surface, so an
    // objective has to answer it; only the two structure methods below are ever
    // called on one, and the evaluation methods say so rather than pretending.
    void constraints(ConstEigenRef<Eigen::VectorXd>, EigenRef<Eigen::VectorXd>,
                     const SolverIndexingData &) const {
        throw std::logic_error("window objective: constraint evaluation on an objective");
    }
    void constraints_adjointgradient(ConstEigenRef<Eigen::VectorXd>, ConstEigenRef<Eigen::VectorXd>,
                                     EigenRef<Eigen::VectorXd>, EigenRef<Eigen::VectorXd>,
                                     const SolverIndexingData &) const {
        throw std::logic_error("window objective: constraint evaluation on an objective");
    }
    void constraints_jacobian(ConstEigenRef<Eigen::VectorXd>, EigenRef<Eigen::VectorXd>,
                              Eigen::SparseMatrix<double, Eigen::RowMajor> &,
                              EigenRef<Eigen::VectorXi>, EigenRef<Eigen::VectorXi>,
                              std::vector<std::mutex> &, const SolverIndexingData &) const {
        throw std::logic_error("window objective: constraint evaluation on an objective");
    }
    void constraints_jacobian_adjointgradient(ConstEigenRef<Eigen::VectorXd>,
                                              ConstEigenRef<Eigen::VectorXd>,
                                              EigenRef<Eigen::VectorXd>, EigenRef<Eigen::VectorXd>,
                                              Eigen::SparseMatrix<double, Eigen::RowMajor> &,
                                              EigenRef<Eigen::VectorXi>, EigenRef<Eigen::VectorXi>,
                                              std::vector<std::mutex> &,
                                              const SolverIndexingData &) const {
        throw std::logic_error("window objective: constraint evaluation on an objective");
    }
    void constraints_jacobian_adjointgradient_adjointhessian(
        ConstEigenRef<Eigen::VectorXd>, ConstEigenRef<Eigen::VectorXd>, EigenRef<Eigen::VectorXd>,
        EigenRef<Eigen::VectorXd>, Eigen::SparseMatrix<double, Eigen::RowMajor> &,
        EigenRef<Eigen::VectorXi>, EigenRef<Eigen::VectorXi>, std::vector<std::mutex> &,
        const SolverIndexingData &) const {
        throw std::logic_error("window objective: constraint evaluation on an objective");
    }

    void get_kkt_space(EigenRef<Eigen::VectorXi> rows, EigenRef<Eigen::VectorXi> cols, int &freeloc,
                       int, bool, bool dohess, SolverIndexingData &data) {
        data.inner_kkt_starts_.resize(data.num_appl());
        for (int a = 0; a < data.num_appl(); a++) {
            data.inner_kkt_starts_[a] = freeloc;
            if (dohess) {
                for (int i = 0; i < kIn; i++) {
                    const int col = data.v_scatter_loc(i, a);
                    rows[freeloc] = col;
                    cols[freeloc] = col;
                    freeloc++;
                }
            }
        }
    }
    int num_kkt_elements(bool, bool dohess) const { return dohess ? kIn : 0; }
};

} // namespace

// Registered beside the definitions, per the placement rule in
// hven/solver_interface_adapter.h: both are plain value types.
namespace hven::solvers {
template <>
struct SolverInterfaceAdapter<WindowConstraintPiece> : DirectFunctionModel<WindowConstraintPiece> {
};
template <>
struct SolverInterfaceAdapter<WindowObjectivePiece> : DirectFunctionModel<WindowObjectivePiece> {};
} // namespace hven::solvers

namespace {

struct Program {
    std::shared_ptr<NonLinearProgram> nlp;
    int variables = 0;
    int equality_rows = 0;
};

/// The pieces and the program, laid. `applications` collocation-like windows,
/// spread over kPieces constraint pieces plus one objective piece.
Program build(int applications, int partitions) {
    const int variables = variables_for(applications);

    auto nlp = std::make_shared<NonLinearProgram>(partitions);

    // The applications, split into kPieces contiguous blocks -- several pieces
    // over one arena, which is the shape a multi-phase transcription produces.
    int first = 0;
    for (int p = 0; p < kPieces; p++) {
        const int count = applications / kPieces + (p < applications % kPieces ? 1 : 0);
        if (count == 0) {
            continue;
        }
        Eigen::MatrixXi vindex(kIn, count);
        Eigen::MatrixXi cindex(kOut, count);
        for (int a = 0; a < count; a++) {
            const int global = first + a;
            for (int i = 0; i < kIn; i++) {
                vindex(i, a) = global * kStride + i;
            }
            for (int r = 0; r < kOut; r++) {
                cindex(r, a) = global * kOut + r;
            }
        }
        nlp->equality_constraints_.push_back(
            ConstraintFunction(ConstraintInterface(WindowConstraintPiece()), vindex, cindex));
        first += count;
    }

    Eigen::MatrixXi obj_vindex(kIn, applications);
    for (int a = 0; a < applications; a++) {
        for (int i = 0; i < kIn; i++) {
            obj_vindex(i, a) = a * kStride + i;
        }
    }
    nlp->objectives_.push_back(
        ObjectiveFunction(ObjectiveInterface(WindowObjectivePiece()), obj_vindex));

    for (int i = 0; i < variables; i++) {
        nlp->set_variable_bound(i, -10.0, 10.0);
    }

    nlp->make_nlp(variables, applications * kOut, 0);
    return Program{std::move(nlp), variables, applications * kOut};
}

Eigen::VectorXd start_point(int variables) {
    Eigen::VectorXd x(variables);
    for (int i = 0; i < variables; i++) {
        x[i] = 0.35 + 0.15 * static_cast<double>(i % 7);
    }
    return x;
}

double seconds_since(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

void report(const char *cell, int applications, int reps, std::vector<double> &s,
            const std::string &identity) {
    std::sort(s.begin(), s.end());
    std::printf("%s n=%d reps=%d median_s=%.6f min_s=%.6f %s\n", cell, applications, reps,
                s[s.size() / 2], s.front(), identity.c_str());
}

/// One whole-solve cell at a given partition count.
///
/// TWO ARE RUN, and the second is not redundant. THE PARTITIONED CELL'S
/// OBJECTIVE IS NOT AN IDENTITY COLUMN: the contested-slot accumulation order
/// is decided by the thread schedule -- two partitions summing into one KKT
/// column can land in either order -- and the sparse factorization behind it
/// runs multi-threaded, which reorders its own reductions too. Both were
/// measured to move the objective's low bits between runs of the SAME binary,
/// and neither predates the contract layer, so neither is something an arm
/// comparison can be gated on. Its gate is the iteration count and the flag.
///
/// `solve1` closes both: ONE partition, so the scatter fold order is fixed, and
/// ONE factorization thread, so the factorization's is too. Its objective is
/// bit-reproducible and is the identity column. It costs a second solve per
/// rep, which is the price of having one number that a comparison can be gated
/// on rather than two that cannot.
void solve_cell(const char *label, int applications, int reps, int partitions) {
    std::vector<double> s;
    double obj = 0.0;
    int iters = 0;
    int flag = 0;
    for (int r = 0; r < reps; r++) {
        Program p = build(applications, partitions);
        hven::solvers::InteriorPointSolver solver(p.nlp);
        solver.set_print_level(3);
        if (partitions == 1) {
            solver.set_qp_threads(1);
        }
        Eigen::VectorXd x0 = start_point(p.variables);
        auto t0 = std::chrono::steady_clock::now();
        solver.optimize(x0);
        s.push_back(seconds_since(t0));
        obj = solver.result().obj_val_;
        iters = solver.result().iter_num_;
        flag = static_cast<int>(solver.result().converge_flag_);
    }
    char identity[128];
    std::snprintf(identity, sizeof(identity), "obj=%.17g iters=%d flag=%d", obj, iters, flag);
    report(label, applications, reps, s, identity);
}

void arm(int applications, int reps) {
    // construct: pieces + program + first layout.
    {
        std::vector<double> s;
        std::uint64_t digest = 0;
        int claims = 0;
        for (int r = 0; r < reps; r++) {
            auto t0 = std::chrono::steady_clock::now();
            Program p = build(applications, kPartitions);
            s.push_back(seconds_since(t0));
            digest = p.nlp->model_structure_key().digest();
            claims = p.nlp->num_user_kkt_elems_;
        }
        report("construct", applications, reps, s,
               "key=" + std::to_string(digest) + " claims=" + std::to_string(claims));
    }

    // transcribe: re-lay only, on a program that already exists.
    {
        Program p = build(applications, kPartitions);
        std::vector<double> s;
        for (int r = 0; r < reps; r++) {
            auto t0 = std::chrono::steady_clock::now();
            p.nlp->make_nlp(p.variables, p.equality_rows, 0);
            s.push_back(seconds_since(t0));
        }
        report("transcribe", applications, reps, s,
               "key=" + std::to_string(p.nlp->model_structure_key().digest()) +
                   " claims=" + std::to_string(p.nlp->num_user_kkt_elems_));
    }

    // transcribe+key: the same re-lay plus ONE structural key read, which is
    // where the deferred digests are paid. INFORMATIONAL.
    {
        Program p = build(applications, kPartitions);
        std::vector<double> s;
        std::uint64_t digest = 0;
        for (int r = 0; r < reps; r++) {
            auto t0 = std::chrono::steady_clock::now();
            p.nlp->make_nlp(p.variables, p.equality_rows, 0);
            const std::uint64_t d = p.nlp->model_structure_key().digest();
            s.push_back(seconds_since(t0));
            digest = d;
        }
        report("transcribe+key", applications, reps, s,
               "key=" + std::to_string(digest) +
                   " claims=" + std::to_string(p.nlp->num_user_kkt_elems_));
    }

    // transcribe+decl: the same re-lay plus ONE declaration() read, which is
    // where the deferred PIECE COPY is paid -- the deep clone of the three
    // master piece lists, type-erased payloads and per-piece index maps and all.
    //
    // THIS IS THE CELL THAT PRICES WHAT EVERY EVALUATING CONSUMER PAYS.
    // NlpAggregate::assemble() reads declaration() as its first statement, so
    // the first assemble after a lay pays exactly this on top of a bare
    // transcribe; everything else assemble does is evaluation, not layout, and
    // timing that here would bury the number this cell exists to show. The
    // `transcribe` and `transcribe+key` cells never call declaration(), so
    // neither of them discharges the copy -- on the head arm they measure a lay
    // that still owes it, while the base arm paid it eagerly inside every
    // make_nlp. Their deltas are therefore NOT the whole story, and this cell is
    // the rest of it.
    //
    // ASSERTED, because a cell that silently failed to discharge the copy would
    // report a saving nobody gets: the read must come back with all three lists
    // populated to the master lists' sizes, or the probe aborts rather than
    // printing a row.
    {
        Program p = build(applications, kPartitions);
        std::vector<double> s;
        std::size_t pieces = 0;
        for (int r = 0; r < reps; r++) {
            auto t0 = std::chrono::steady_clock::now();
            p.nlp->make_nlp(p.variables, p.equality_rows, 0);
            const auto &declared = p.nlp->declaration();
            const std::size_t copied = declared.objectives_.size() +
                                       declared.equality_constraints_.size() +
                                       declared.inequality_constraints_.size();
            s.push_back(seconds_since(t0));
            pieces = copied;
            const std::size_t expected = p.nlp->objectives_.size() +
                                         p.nlp->equality_constraints_.size() +
                                         p.nlp->inequality_constraints_.size();
            if (copied != expected || copied == 0) {
                std::fprintf(
                    stderr,
                    "transcribe+decl: the declaration read returned %zu pieces, expected "
                    "%zu -- the deferred copy was not discharged inside the timed region\n",
                    copied, expected);
                std::abort();
            }
        }
        report("transcribe+decl", applications, reps, s,
               "pieces=" + std::to_string(pieces) +
                   " claims=" + std::to_string(p.nlp->num_user_kkt_elems_));
    }

    // analyze: the sparsity analysis that runs against a laid program. It is
    // the other half of what a front end calls a transcription -- the layout
    // lays the claims, this derives the scatter offsets for them -- and it is
    // timed apart from the layout because the two moved in opposite
    // directions: the layout got cheaper, and this pays a pair of compares per
    // element for the claim stream the layout no longer destroys.
    {
        Program p = build(applications, kPartitions);
        Eigen::SparseMatrix<double, Eigen::RowMajor> kkt;
        std::vector<double> s;
        for (int r = 0; r < reps; r++) {
            auto t0 = std::chrono::steady_clock::now();
            p.nlp->analyze_sparsity(kkt);
            s.push_back(seconds_since(t0));
        }
        report("analyze", applications, reps, s,
               "key=" + std::to_string(p.nlp->model_structure_key().digest()) +
                   " claims=" + std::to_string(p.nlp->num_user_kkt_elems_));
    }

    // clone: the deep copy of the three master piece lists, and nothing else.
    //
    // WHY IT IS HERE. analyze_partitioning() refills its per-partition vectors
    // from the master lists at every lay, and each push_back deep-clones a
    // type-erased piece through TypeStorage's clone_into -- index maps, erased
    // payload and all. Some share of `transcribe` above is therefore piece
    // cloning rather than claim laying, and nobody had ever measured which
    // share. Measuring it is the required first step before anyone designs
    // around it, and this cell is that step.
    //
    // A LOWER BOUND, NOT AN EQUAL. This clones each master list ONCE. The
    // partitioner clones more: a ByApplication piece is thread_split into one
    // piece per partition, and every one of those is a clone. Read this as "at
    // least this much of transcribe is piece cloning", never as "this is all of
    // it".
    //
    // ASSERTED like transcribe+decl, so a cell the optimizer managed to elide
    // reports nothing rather than reporting a saving nobody gets.
    {
        Program p = build(applications, kPartitions);
        std::vector<double> s;
        std::size_t pieces = 0;
        const std::size_t expected = p.nlp->objectives_.size() +
                                     p.nlp->equality_constraints_.size() +
                                     p.nlp->inequality_constraints_.size();
        for (int r = 0; r < reps; r++) {
            auto t0 = std::chrono::steady_clock::now();
            std::vector<ObjectiveFunction> objectives = p.nlp->objectives_;
            std::vector<ConstraintFunction> equalities = p.nlp->equality_constraints_;
            std::vector<ConstraintFunction> inequalities = p.nlp->inequality_constraints_;
            const std::size_t copied =
                objectives.size() + equalities.size() + inequalities.size();
            s.push_back(seconds_since(t0));
            pieces = copied;
            if (copied != expected || copied == 0) {
                std::fprintf(stderr,
                             "clone: copied %zu pieces, expected %zu -- the copy did not "
                             "happen inside the timed region\n",
                             copied, expected);
                std::abort();
            }
        }
        report("clone", applications, reps, s,
               "pieces=" + std::to_string(pieces) +
                   " claims=" + std::to_string(p.nlp->num_user_kkt_elems_));
    }

    // solve: one whole solve per rep, program rebuilt each time so no rep
    // starts warm. Run twice -- see solve_cell for why the one-partition arm
    // is the one whose objective is a gate.
    solve_cell("solve", applications, reps, kPartitions);
    solve_cell("solve1", applications, reps, 1);
}

} // namespace

int main(int argc, char **argv) {
    const int reps = argc > 1 ? std::atoi(argv[1]) : 9;
    arm(64, reps);
    arm(256, reps);
    arm(1024, reps);
    return 0;
}
