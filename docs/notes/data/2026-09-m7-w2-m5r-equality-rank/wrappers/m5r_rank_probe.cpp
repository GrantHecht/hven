// M7 W2 M5r -- THE OFFLINE EQUALITY-JACOBIAN RANK PROBE.
//
// WHAT IT ANSWERS. M5's read-out (Q1) INFERRED from counters that the QP
// working set's rank deficit sits in the MANDATORY rows -- equality rows plus
// FIXED bounds -- on SMBANK, SAWPATH, DALLASM and STEENBRB/C/G. No Jacobian was
// evaluated there. This program evaluates it, at the model's start point, and
// nothing else: NO SOLVER IS CONSTRUCTED, NO SOLVE RUNS. It reads the problem
// through the harness's own CUTEst bridge (the same `CutestSource` the M3c/M4/
// M5 probes load a cell with) and does dense linear algebra on what it gets.
//
// PER CELL, AT x0 (and at the projected start min(max(x0,l),u) when x0 has a
// component outside its box or f(x0) is non-finite):
//   * n, me (rows with g_lower == g_upper), mi, fixed bounds (l == u), finite
//     bounds, bounds active at the point within an ABSOLUTE 1e-8, components
//     strictly outside the box;
//   * rank of three row sets, each by Eigen's ColPivHouseholderQR on the
//     TRANSPOSE (n x k, one column per row), at TWO thresholds -- Eigen's own
//     default (diagSize * eps, relative to the largest pivot) and 1e-10
//     relative:
//        Ae                              -> "eq"
//        [Ae; e_i for every fixed i]     -> "skel"  (M5's mandatory skeleton)
//        [Ae; e_i for every active i]    -> "entry" (the QP's seeded set)
//   * |R_11| / |R_rr| at the computed rank r, and |R_{r+1,r+1}| / |R_11| (the
//     gap the threshold sat in);
//   * an INDEPENDENT IDENTITY CHECK of the two stacked ranks:
//        rank([Ae; E_S]) == |S| + rank(Ae restricted to the columns not in S)
//     computed by a second QR on a different matrix -- the two sides must agree
//     at the same threshold, or the row says so;
//   * the DEPENDENT rows: the columns the pivoting leaves past the rank, mapped
//     back to CUTEst row indices and names (`cutest_cnames`), each with the
//     support and coefficient pattern of the combination of pivot-basis rows
//     that reproduces it (a +-1 pattern is the network-balance signature, a
//     support of 1 a duplicated row, an all-zero row a zero row).
//
// THE ROSTER CHECK. The name must be in the set's own roster
// (`cutest_problems()`) AND carry split TRAIN in the harness's split pin
// (`sets/split-v1.json`, read, never retyped). A HOLDOUT name is REFUSED with a
// non-zero exit before any CUTEst call.
//
// Nothing under src/, include/ or tests/ is touched by this file or its build.
// /home/ghecht/Projects/hven-bench is READ-ONLY; the probe is run with it as
// the working directory because the bridge's decoded-problem path is relative.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <Eigen/QR>

#include "lanes/cutest/cutest_model.h"
#include "lanes/cutest/cutest_source.h"
#include "lanes/hven/problem_source.h"

extern "C" {
#include "cutest.h"
}

namespace {

using Eigen::MatrixXd;
using Eigen::VectorXd;

constexpr double kActiveTol = 1e-8;   // ABSOLUTE, declared
constexpr double kAltThreshold = 1e-10; // relative to the largest pivot
// A THIRD READ, added after the first run and declared as such: M5's arm judged
// dependence with `setThreshold(dependency_tol)`, dependency_tol = 1e-8
// (include/hven/detail/qp/qp_degeneracy_arm.h:453, M5 RULE.md section 147), on
// the QP's own rows. Reading the same R at that cut makes the comparison with
// M5's counter direct rather than inferred.
constexpr double kArmThreshold = 1e-8;
constexpr double kCoefTol = 1e-9;       // support of a dependency combination, relative

std::string trim(const std::string &s) {
    const auto b = s.find_first_not_of(' ');
    if (b == std::string::npos) return "";
    const auto e = s.find_last_not_of(' ');
    return s.substr(b, e - b + 1);
}

// The split pin, read as text. Each assignment is an object whose keys appear
// in the order problem, problem_set, split, u (the file is generated that way);
// the lookup REFUSES rather than guesses if it finds zero or two matches.
std::string split_of(const std::string &pin_path, const std::string &problem) {
    std::ifstream in(pin_path);
    if (!in) return "NO_PIN";
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string text = ss.str();
    const std::string key = "\"problem\": \"" + problem + "\"";
    std::string found;
    int hits = 0;
    std::size_t pos = 0;
    while ((pos = text.find(key, pos)) != std::string::npos) {
        const std::size_t end = text.find('}', pos);
        const std::string obj = text.substr(pos, end - pos);
        if (obj.find("\"problem_set\": \"cutest\"") != std::string::npos) {
            const std::size_t sp = obj.find("\"split\": \"");
            if (sp != std::string::npos) {
                const std::size_t a = sp + 10;
                found = obj.substr(a, obj.find('"', a) - a);
                ++hits;
            }
        }
        pos = end;
    }
    if (hits != 1) return "AMBIGUOUS_OR_ABSENT(" + std::to_string(hits) + ")";
    return found;
}

std::string fmts(double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.6g", v);
    return buf;
}

std::string fmtd(double v) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.17g", v);
    return buf;
}

struct RankRead {
    Eigen::Index rows = 0;       // k, the number of stacked rows
    Eigen::Index rank = 0;
    double threshold = 0.0;      // the RELATIVE threshold Eigen applied
    double r11 = 0.0;
    double rrr = 0.0;            // |R_rr| at the rank (0 if rank 0)
    double rnext = 0.0;          // |R_{r+1,r+1}| (0 if full)
    std::vector<Eigen::Index> dependent;          // original row positions past the rank
    std::vector<Eigen::Index> perm;               // full pivot order
};

// QR on M^T (n x k). `thr < 0` means Eigen's default threshold.
struct QrPair {
    RankRead dflt, alt, arm;
    Eigen::ColPivHouseholderQR<MatrixXd> qr;
};

RankRead read_rank(Eigen::ColPivHouseholderQR<MatrixXd> &qr, Eigen::Index k) {
    RankRead r;
    r.rows = k;
    r.rank = qr.rank();
    r.threshold = qr.threshold();
    const MatrixXd &R = qr.matrixQR();
    const Eigen::Index d = std::min(R.rows(), R.cols());
    r.r11 = d > 0 ? std::abs(R(0, 0)) : 0.0;
    r.rrr = r.rank > 0 ? std::abs(R(r.rank - 1, r.rank - 1)) : 0.0;
    r.rnext = r.rank < d ? std::abs(R(r.rank, r.rank)) : 0.0;
    const auto &P = qr.colsPermutation().indices();
    for (Eigen::Index i = 0; i < P.size(); ++i) r.perm.push_back(P[i]);
    for (Eigen::Index i = r.rank; i < P.size(); ++i) r.dependent.push_back(P[i]);
    return r;
}

// Rank of M (k x n) by QR of M^T at both thresholds; the factorization is done
// ONCE and the threshold only changes the count (Eigen's rank() reads the
// stored diagonal), so the two reads are of the same R.
QrPair rank_both(const MatrixXd &M) {
    QrPair p;
    const MatrixXd Mt = M.transpose();
    p.qr.compute(Mt);
    p.qr.setThreshold(Eigen::Default);
    p.dflt = read_rank(p.qr, M.rows());
    p.qr.setThreshold(kAltThreshold);
    p.alt = read_rank(p.qr, M.rows());
    p.qr.setThreshold(kArmThreshold);
    p.arm = read_rank(p.qr, M.rows());
    p.qr.setThreshold(Eigen::Default);
    return p;
}

struct Labels {
    std::vector<std::string> cname; // CUTEst constraint names, by CUTEst row
    std::vector<std::string> vname; // CUTEst variable names
};

// A stacked row's label: "c<cutest-row>:<name>" or "b<var>:<name>".
struct StackRow {
    bool is_bound = false;
    int index = 0;
};

std::string label(const StackRow &s, const Labels &L) {
    if (s.is_bound) return "b" + std::to_string(s.index) + ":" + L.vname[s.index];
    return "c" + std::to_string(s.index) + ":" + L.cname[s.index];
}

// For every dependent column j (past rank r in pivot order), the coefficients c
// with  col_j = sum_i c_i col_{perm_i}, i < r  -- solved from R11 c = R12[:, j].
// Returns one descriptive string per dependent column, and tallies patterns.
struct DepSummary {
    std::vector<std::string> lines;
    int zero_rows = 0;
    int duplicates = 0;     // support 1
    int unit_pattern = 0;   // every coefficient +-1 within 1e-8
    int bound_only = 0;     // support made only of bound rows
    int max_support = 0;
    double max_resid = 0.0; // || col_j - sum c_i col_i || / max(1, ||col_j||)
};

DepSummary describe(const MatrixXd &M, const QrPair &p, const RankRead &r,
                    const std::vector<StackRow> &rows, const Labels &L) {
    DepSummary out;
    const Eigen::Index rk = r.rank;
    const MatrixXd &QR = p.qr.matrixQR();
    const Eigen::Index k = M.rows();
    MatrixXd R11 = QR.topLeftCorner(rk, rk).triangularView<Eigen::Upper>();
    for (Eigen::Index t = rk; t < k; ++t) {
        const Eigen::Index j = r.perm[t];
        std::ostringstream os;
        os << label(rows[j], L);
        const double nj = M.row(j).norm();
        if (nj == 0.0) {
            ++out.zero_rows;
            os << " [ZERO ROW]";
            out.lines.push_back(os.str());
            continue;
        }
        VectorXd c = VectorXd::Zero(rk);
        if (rk > 0) {
            const VectorXd rhs = QR.col(t).head(rk);
            c = R11.triangularView<Eigen::Upper>().solve(rhs);
        }
        // Residual of the combination, in the ORIGINAL rows.
        VectorXd comb = VectorXd::Zero(M.cols());
        for (Eigen::Index i = 0; i < rk; ++i) comb += c[i] * M.row(r.perm[i]).transpose();
        const double resid = (M.row(j).transpose() - comb).norm() / std::max(1.0, nj);
        out.max_resid = std::max(out.max_resid, resid);
        const double cmax = rk > 0 ? c.cwiseAbs().maxCoeff() : 0.0;
        int support = 0;
        bool unit = true, only_bounds = true;
        std::ostringstream sup;
        for (Eigen::Index i = 0; i < rk; ++i) {
            if (std::abs(c[i]) > kCoefTol * std::max(1.0, cmax)) {
                ++support;
                if (std::abs(std::abs(c[i]) - 1.0) > 1e-8) unit = false;
                if (!rows[r.perm[i]].is_bound) only_bounds = false;
                if (support <= 6) {
                    sup << (support > 1 ? " " : "") << fmts(c[i]) << "*"
                        << label(rows[r.perm[i]], L);
                }
            }
        }
        if (support == 0) {
            os << " [NUMERICALLY ZERO ROW norm=" << fmtd(nj) << "]";
            ++out.zero_rows;
        } else {
            if (support == 1) ++out.duplicates;
            if (unit) ++out.unit_pattern;
            if (only_bounds) ++out.bound_only;
            out.max_support = std::max(out.max_support, support);
            os << " = support " << support << (unit ? " (+-1)" : "")
               << (only_bounds ? " (bound rows only)" : "") << " resid " << fmts(resid)
               << " : " << sup.str() << (support > 6 ? " ..." : "");
        }
        out.lines.push_back(os.str());
    }
    return out;
}

void print_read(const std::string &tag, const RankRead &r) {
    std::printf("%s_rows=%lld\n", tag.c_str(), static_cast<long long>(r.rows));
    std::printf("%s_rank=%lld\n", tag.c_str(), static_cast<long long>(r.rank));
    std::printf("%s_deficiency=%lld\n", tag.c_str(), static_cast<long long>(r.rows - r.rank));
    std::printf("%s_threshold_rel=%s\n", tag.c_str(), fmtd(r.threshold).c_str());
    std::printf("%s_R11=%s\n", tag.c_str(), fmtd(r.r11).c_str());
    std::printf("%s_Rrr=%s\n", tag.c_str(), fmtd(r.rrr).c_str());
    std::printf("%s_cond_ratio=%s\n", tag.c_str(),
                r.rrr > 0 ? fmtd(r.r11 / r.rrr).c_str() : "NA");
    std::printf("%s_gap_next_over_R11=%s\n", tag.c_str(),
                r.r11 > 0 ? fmtd(r.rnext / r.r11).c_str() : "NA");
}

// One row set: QR at both thresholds, the identity check, and (at the default
// threshold) the dependent rows.
void analyse(const std::string &tag, const MatrixXd &M, const std::vector<StackRow> &rows,
             const Labels &L, const MatrixXd *Ae_reduced, Eigen::Index n_bound_rows) {
    const QrPair p = rank_both(M);
    print_read(tag + "_dflt", p.dflt);
    print_read(tag + "_alt", p.alt);
    print_read(tag + "_arm", p.arm);
    std::printf("%s_thresholds_agree=%d\n", tag.c_str(), p.dflt.rank == p.alt.rank ? 1 : 0);
    if (Ae_reduced != nullptr) {
        // THE IDENTITY: rank([Ae; E_S]) = |S| + rank(Ae[:, not S]).
        long long red_d = 0, red_a = 0;
        if (Ae_reduced->rows() > 0 && Ae_reduced->cols() > 0) {
            QrPair q = rank_both(*Ae_reduced);
            red_d = q.dflt.rank;
            red_a = q.alt.rank;
        }
        const long long id_d = static_cast<long long>(n_bound_rows) + red_d;
        const long long id_a = static_cast<long long>(n_bound_rows) + red_a;
        std::printf("%s_identity_rank_dflt=%lld\n", tag.c_str(), id_d);
        std::printf("%s_identity_rank_alt=%lld\n", tag.c_str(), id_a);
        std::printf("%s_identity_holds=%d\n", tag.c_str(),
                    (id_d == p.dflt.rank && id_a == p.alt.rank) ? 1 : 0);
        std::printf("%s_reduced_eq_cols=%lld\n", tag.c_str(),
                    static_cast<long long>(Ae_reduced->cols()));
    }
    // THE ROW-EQUILIBRATED READ (declared after the first run showed why it is
    // needed): each stacked row scaled to unit 2-norm before the same QR at the
    // same two thresholds. Row rank is invariant under row scaling in exact
    // arithmetic, but a RELATIVE threshold is not: one row 1e10 times larger
    // than the rest moves |R_11| and with it every cut. Zero rows stay zero.
    MatrixXd Mz = M;
    for (Eigen::Index i = 0; i < Mz.rows(); ++i) {
        const double nr = Mz.row(i).norm();
        if (nr > 0.0) Mz.row(i) /= nr;
    }
    const double rmax = M.rows() > 0 ? M.rowwise().norm().maxCoeff() : 0.0;
    double rmin = std::numeric_limits<double>::infinity();
    for (Eigen::Index i = 0; i < M.rows(); ++i) {
        const double nr = M.row(i).norm();
        if (nr > 0.0) rmin = std::min(rmin, nr);
    }
    std::printf("%s_row_norm_max=%s\n", tag.c_str(), fmtd(rmax).c_str());
    std::printf("%s_row_norm_min_nonzero=%s\n", tag.c_str(), fmtd(rmin).c_str());
    const QrPair pz = rank_both(Mz);
    print_read(tag + "_eqz_dflt", pz.dflt);
    print_read(tag + "_eqz_alt", pz.alt);
    print_read(tag + "_eqz_arm", pz.arm);
    std::printf("%s_eqz_thresholds_agree=%d\n", tag.c_str(), pz.dflt.rank == pz.alt.rank ? 1 : 0);
    const DepSummary dz = describe(Mz, pz, pz.dflt, rows, L);
    std::printf("%s_eqz_dep_count=%zu\n", tag.c_str(), dz.lines.size());
    std::printf("%s_eqz_dep_unit_pattern=%d\n", tag.c_str(), dz.unit_pattern);
    std::printf("%s_eqz_dep_max_support=%d\n", tag.c_str(), dz.max_support);
    std::printf("%s_eqz_dep_max_resid=%s\n", tag.c_str(), fmtd(dz.max_resid).c_str());
    {
        int e = 0, b = 0;
        for (Eigen::Index t = pz.dflt.rank; t < Mz.rows(); ++t) (rows[pz.dflt.perm[t]].is_bound ? b : e)++;
        std::printf("%s_eqz_dep_eq_rows=%d\n", tag.c_str(), e);
        std::printf("%s_eqz_dep_bound_rows=%d\n", tag.c_str(), b);
    }
    for (const std::string &line : dz.lines) std::printf("%s_eqz_DEP %s\n", tag.c_str(), line.c_str());
    std::printf("%s_count_excess=%lld\n", tag.c_str(),
                static_cast<long long>(std::max<Eigen::Index>(0, M.rows() - M.cols())));

    const DepSummary d = describe(M, p, p.dflt, rows, L);
    std::printf("%s_dep_count=%zu\n", tag.c_str(), d.lines.size());
    std::printf("%s_dep_zero_rows=%d\n", tag.c_str(), d.zero_rows);
    std::printf("%s_dep_support1=%d\n", tag.c_str(), d.duplicates);
    std::printf("%s_dep_unit_pattern=%d\n", tag.c_str(), d.unit_pattern);
    std::printf("%s_dep_bound_rows_only=%d\n", tag.c_str(), d.bound_only);
    std::printf("%s_dep_max_support=%d\n", tag.c_str(), d.max_support);
    std::printf("%s_dep_max_resid=%s\n", tag.c_str(), fmtd(d.max_resid).c_str());
    int eq_dep = 0, bd_dep = 0;
    for (Eigen::Index t = p.dflt.rank; t < M.rows(); ++t) {
        (rows[p.dflt.perm[t]].is_bound ? bd_dep : eq_dep)++;
    }
    std::printf("%s_dep_eq_rows=%d\n", tag.c_str(), eq_dep);
    std::printf("%s_dep_bound_rows=%d\n", tag.c_str(), bd_dep);
    for (const std::string &line : d.lines) std::printf("%s_DEP %s\n", tag.c_str(), line.c_str());
}

void dump_triplets(const std::string &path, const MatrixXd &M) {
    std::ofstream f(path);
    f << "row,col,val\n";
    f.precision(17);
    for (Eigen::Index i = 0; i < M.rows(); ++i)
        for (Eigen::Index j = 0; j < M.cols(); ++j)
            if (M(i, j) != 0.0) f << i << "," << j << "," << M(i, j) << "\n";
    f << "# shape," << M.rows() << "," << M.cols() << "\n";
}

// Evaluate everything at one point.
void at_point(const std::string &pt, const hven_bench::CountedModel &model, const VectorXd &x,
              const VectorXd &xl, const VectorXd &xu, const std::vector<int> &eq_rows,
              const Eigen::VectorXi &jr, const Eigen::VectorXi &jc, const Labels &L,
              const std::string &dump_dir) {
    const int n = model.num_vars();
    const int m = model.num_cons();
    std::printf("POINT_BEGIN %s\n", pt.c_str());
    double f = std::numeric_limits<double>::quiet_NaN();
    bool f_threw = false;
    try {
        model.eval_f(x, f);
    } catch (const std::exception &e) {
        f_threw = true;
        std::printf("f_eval_error=%s\n", e.what());
    }
    std::printf("f=%s\n", f_threw ? "THREW" : fmtd(f).c_str());
    std::printf("f_finite=%d\n", (!f_threw && std::isfinite(f)) ? 1 : 0);

    VectorXd vals(model.num_jac_nonzeros());
    bool jac_ok = true;
    try {
        model.eval_jac(x, vals);
    } catch (const std::exception &e) {
        jac_ok = false;
        std::printf("jac_eval_error=%s\n", e.what());
    }
    const bool jac_finite = jac_ok && vals.allFinite();
    std::printf("jac_finite=%d\n", jac_finite ? 1 : 0);

    int active = 0, outside = 0, fixed_here = 0;
    std::vector<int> active_idx, fixed_idx;
    for (int i = 0; i < n; ++i) {
        const bool fx = xl[i] == xu[i];
        if (x[i] < xl[i] || x[i] > xu[i]) ++outside;
        const bool act = (std::isfinite(xl[i]) && std::abs(x[i] - xl[i]) <= kActiveTol) ||
                         (std::isfinite(xu[i]) && std::abs(x[i] - xu[i]) <= kActiveTol);
        if (fx) {
            ++fixed_here;
            fixed_idx.push_back(i);
        }
        if (act || fx) {
            ++active;
            active_idx.push_back(i);
        }
    }
    std::printf("active_bounds_tol_abs=%s\n", fmtd(kActiveTol).c_str());
    std::printf("active_bounds=%d\n", active);
    std::printf("active_bounds_nonfixed=%d\n", active - fixed_here);
    std::printf("outside_box=%d\n", outside);

    if (!jac_finite) {
        std::printf("RANKS=UNEVALUABLE (the Jacobian is not finite at this point)\n");
        std::printf("POINT_END %s\n", pt.c_str());
        return;
    }
    const int me = static_cast<int>(eq_rows.size());
    std::vector<int> pos(m, -1);
    for (int r = 0; r < me; ++r) pos[eq_rows[r]] = r;
    MatrixXd Ae = MatrixXd::Zero(me, n);
    for (Eigen::Index s = 0; s < vals.size(); ++s)
        if (pos[jr[s]] >= 0) Ae(pos[jr[s]], jc[s]) += vals[s];
    int zero_eq_rows = 0;
    for (int r = 0; r < me; ++r)
        if (Ae.row(r).norm() == 0.0) ++zero_eq_rows;
    std::printf("eq_zero_rows=%d\n", zero_eq_rows);
    std::printf("eq_nnz=%lld\n", static_cast<long long>((Ae.array() != 0.0).count()));
    // A STRUCTURAL READ of the two stacked sets: equality rows whose every
    // nonzero sits on a column the stacked unit rows already pin. Each such row
    // is dependent on those unit rows by construction (it is a combination of
    // them), whatever its values.
    {
        std::vector<char> is_fixed(n, 0), is_active(n, 0);
        for (int i : fixed_idx) is_fixed[i] = 1;
        for (int i : active_idx) is_active[i] = 1;
        int on_fixed = 0, on_active = 0;
        for (int r = 0; r < me; ++r) {
            bool f_only = true, a_only = true, any = false;
            for (int j = 0; j < n; ++j) {
                if (Ae(r, j) == 0.0) continue;
                any = true;
                if (!is_fixed[j]) f_only = false;
                if (!is_active[j]) a_only = false;
            }
            if (any && f_only) ++on_fixed;
            if (any && a_only) ++on_active;
        }
        std::printf("eq_rows_on_fixed_cols_only=%d\n", on_fixed);
        std::printf("eq_rows_on_active_cols_only=%d\n", on_active);
    }

    std::vector<StackRow> eq_labels;
    for (int r = 0; r < me; ++r) eq_labels.push_back({false, eq_rows[r]});

    auto stack = [&](const std::vector<int> &bnd, MatrixXd &M, std::vector<StackRow> &lab,
                     MatrixXd &red) {
        M = MatrixXd::Zero(me + static_cast<int>(bnd.size()), n);
        M.topRows(me) = Ae;
        lab = eq_labels;
        std::vector<char> in_s(n, 0);
        for (std::size_t t = 0; t < bnd.size(); ++t) {
            M(me + static_cast<Eigen::Index>(t), bnd[t]) = 1.0;
            lab.push_back({true, bnd[t]});
            in_s[bnd[t]] = 1;
        }
        std::vector<int> keep;
        for (int i = 0; i < n; ++i)
            if (!in_s[i]) keep.push_back(i);
        red = MatrixXd(me, static_cast<Eigen::Index>(keep.size()));
        for (std::size_t t = 0; t < keep.size(); ++t) red.col(static_cast<Eigen::Index>(t)) = Ae.col(keep[t]);
    };

    if (me > 0) {
        analyse("eq", Ae, eq_labels, L, nullptr, 0);
    } else {
        std::printf("eq_rank=NA (me = 0)\n");
    }
    MatrixXd S, Sred, E, Ered;
    std::vector<StackRow> Slab, Elab;
    stack(fixed_idx, S, Slab, Sred);
    if (S.rows() > 0) analyse("skel", S, Slab, L, &Sred, static_cast<Eigen::Index>(fixed_idx.size()));
    stack(active_idx, E, Elab, Ered);
    if (E.rows() > 0) analyse("entry", E, Elab, L, &Ered, static_cast<Eigen::Index>(active_idx.size()));

    if (!dump_dir.empty()) {
        dump_triplets(dump_dir + "/" + pt + "-eq.csv", Ae);
        dump_triplets(dump_dir + "/" + pt + "-skel.csv", S);
        dump_triplets(dump_dir + "/" + pt + "-entry.csv", E);
        std::printf("dumped=%s/%s-{eq,skel,entry}.csv\n", dump_dir.c_str(), pt.c_str());
    }
    std::printf("POINT_END %s\n", pt.c_str());
}

} // namespace

int main(int argc, char **argv) {
    std::setvbuf(stdout, nullptr, _IOLBF, 0); // a crash must not eat the row
    std::string problem, pin = "sets/split-v1.json", dump_dir;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--problem" && i + 1 < argc) problem = argv[++i];
        else if (a == "--split-pin" && i + 1 < argc) pin = argv[++i];
        else if (a == "--dump-dir" && i + 1 < argc) dump_dir = argv[++i];
        else {
            std::fprintf(stderr, "usage: m5r_rank_probe --problem NAME [--split-pin PATH] "
                                 "[--dump-dir DIR]\n");
            return 2;
        }
    }
    if (problem.empty()) {
        std::fprintf(stderr, "usage: m5r_rank_probe --problem NAME [--split-pin PATH] [--dump-dir DIR]\n");
        return 2;
    }
    const auto &names = hven_bench::cutest::cutest_problems();
    if (std::find(names.begin(), names.end(), problem) == names.end()) {
        std::fprintf(stderr, "m5r_rank_probe: %s is not in the cutest-nlp-35 roster\n", problem.c_str());
        return 2;
    }
    const std::string split = split_of(pin, problem);
    if (split != "TRAIN") {
        std::fprintf(stderr, "m5r_rank_probe: %s has split %s in %s -- REFUSED (TRAIN only; "
                             "HOLDOUT is never run)\n", problem.c_str(), split.c_str(), pin.c_str());
        return 3;
    }

    std::printf("PROBE_BEGIN m5r_rank_probe\n");
    std::printf("problem=%s\n", problem.c_str());
    std::printf("split=%s\n", split.c_str());
    std::printf("split_pin=%s\n", pin.c_str());
    std::printf("eigen_version=%d.%d.%d\n", EIGEN_WORLD_VERSION, EIGEN_MAJOR_VERSION, EIGEN_MINOR_VERSION);
    std::printf("threshold_default=Eigen::Default (diagSize*eps, relative to |R_11|)\n");
    std::printf("threshold_alt_rel=%s\n", fmtd(kAltThreshold).c_str());
    std::printf("threshold_arm_rel=%s\n", fmtd(kArmThreshold).c_str());
    std::printf("solver_constructed=0\n");

    const auto source = std::make_shared<hven_bench::cutest::CutestSource>(problem);
    try {
        const auto model = source->make_model();
        const int n = model->num_vars();
        const int m = model->num_cons();
        VectorXd xl(n), xu(n), gl(m), gu(m);
        model->bounds(xl, xu, gl, gu);

        Labels L;
        {
            integer st = 0, nn = n, mm = m;
            // The C interface (`cutest_cint_cnames`) writes NULL-TERMINATED
            // names at a stride of FSTRING_LEN + 1 (CUTEst src/tools/cnames.F90:
            // DIMENSION( 11, n )), exactly as its own genc_main.c allocates.
            constexpr std::size_t kStride = FSTRING_LEN + 1;
            std::vector<char> pname(kStride, '\0');
            std::vector<char> vn(static_cast<std::size_t>(n) * kStride, '\0');
            std::vector<char> gn(static_cast<std::size_t>(std::max(m, 1)) * kStride, '\0');
            CUTEST_cnames(&st, &nn, &mm, pname.data(), vn.data(), gn.data());
            std::printf("cnames_status=%d\n", static_cast<int>(st));
            auto name_at = [&](const std::vector<char> &buf, int i) {
                const char *b = buf.data() + static_cast<std::size_t>(i) * kStride;
                return trim(std::string(b, strnlen(b, FSTRING_LEN)));
            };
            for (int i = 0; i < n; ++i) L.vname.push_back(name_at(vn, i));
            for (int i = 0; i < m; ++i) L.cname.push_back(name_at(gn, i));
            std::printf("cutest_pname=%s\n", name_at(pname, 0).c_str());
        }

        std::vector<int> eq_rows;
        int mi = 0, free_rows = 0;
        for (int r = 0; r < m; ++r) {
            const bool lo = std::isfinite(gl[r]), up = std::isfinite(gu[r]);
            if (lo && up && gl[r] == gu[r]) eq_rows.push_back(r);
            else if (lo || up) ++mi;
            else ++free_rows;
        }
        int fixed = 0, finite_bounds = 0, boxed = 0;
        for (int i = 0; i < n; ++i) {
            if (xl[i] == xu[i]) ++fixed;
            finite_bounds += (std::isfinite(xl[i]) ? 1 : 0) + (std::isfinite(xu[i]) ? 1 : 0);
            if (std::isfinite(xl[i]) && std::isfinite(xu[i])) ++boxed;
        }
        std::printf("n=%d\n", n);
        std::printf("m=%d\n", m);
        std::printf("me=%zu\n", eq_rows.size());
        std::printf("mi=%d\n", mi);
        std::printf("free_rows=%d\n", free_rows);
        std::printf("fixed_bounds=%d\n", fixed);
        std::printf("finite_bounds=%d\n", finite_bounds);
        std::printf("boxed_vars=%d\n", boxed);
        const auto *cm = dynamic_cast<const hven_bench::cutest::CutestModel *>(model.get());
        if (cm != nullptr) {
            std::printf("cutest_equatn_rows=%d\n", cm->equality_rows());
            std::printf("me_matches_equatn=%d\n", cm->equality_rows() == static_cast<int>(eq_rows.size()) ? 1 : 0);
        }

        Eigen::VectorXi jr(model->num_jac_nonzeros()), jc(model->num_jac_nonzeros());
        model->jac_structure(jr, jc);

        const VectorXd x0 = source->x0();
        at_point("x0", *model, x0, xl, xu, eq_rows, jr, jc, L, dump_dir);

        // A SECOND POINT, for ONE question only: is the equality Jacobian's
        // deficiency a property of the rows or of x0? xpert = x0 moved by
        // 1e-3*(1+|x0_i|) with alternating sign, then projected into the box.
        // It is NOT a solve iterate and nothing else is read there.
        {
            integer st = 0, nvo = 0, nvc = 0, neq = 0, nlin = 0;
            CUTEST_cstats(&st, &nvo, &nvc, &neq, &nlin);
            std::printf("cstats_status=%d\n", static_cast<int>(st));
            std::printf("cstats_linear_constraints=%d\n", static_cast<int>(nlin));
            std::printf("cstats_equality_constraints=%d\n", static_cast<int>(neq));
            VectorXd xq = x0;
            for (int i = 0; i < n; ++i)
                xq[i] += 1e-3 * (1.0 + std::abs(x0[i])) * ((i % 2) ? 1.0 : -1.0);
            xq = xq.cwiseMax(xl).cwiseMin(xu);
            VectorXd v0(model->num_jac_nonzeros()), v1(model->num_jac_nonzeros());
            bool ok = true;
            try { model->eval_jac(x0, v0); model->eval_jac(xq, v1); } catch (...) { ok = false; }
            if (!ok || !v0.allFinite() || !v1.allFinite()) {
                std::printf("xpert=UNEVALUABLE (a Jacobian at x0 or xpert is not finite)\n");
            } else {
                std::vector<int> pos(m, -1);
                for (std::size_t r = 0; r < eq_rows.size(); ++r) pos[eq_rows[r]] = static_cast<int>(r);
                std::vector<char> moved(eq_rows.size(), 0);
                const int me = static_cast<int>(eq_rows.size());
                MatrixXd A1 = MatrixXd::Zero(me, n);
                for (Eigen::Index s2 = 0; s2 < v1.size(); ++s2) {
                    if (pos[jr[s2]] < 0) continue;
                    A1(pos[jr[s2]], jc[s2]) += v1[s2];
                    if (v0[s2] != v1[s2]) moved[pos[jr[s2]]] = 1;
                }
                int nm = 0;
                for (char c : moved) nm += c;
                std::printf("xpert_eq_rows_with_moved_jacobian=%d\n", nm);
                if (me > 0) {
                    const QrPair q = rank_both(A1);
                    MatrixXd A1z = A1;
                    for (Eigen::Index i = 0; i < A1z.rows(); ++i) {
                        const double nr = A1z.row(i).norm();
                        if (nr > 0.0) A1z.row(i) /= nr;
                    }
                    const QrPair qz = rank_both(A1z);
                    std::printf("xpert_eq_deficiency_dflt=%lld\n", static_cast<long long>(me - q.dflt.rank));
                    std::printf("xpert_eq_deficiency_alt=%lld\n", static_cast<long long>(me - q.alt.rank));
                    std::printf("xpert_eq_deficiency_arm=%lld\n", static_cast<long long>(me - q.arm.rank));
                    std::printf("xpert_eq_eqz_deficiency_dflt=%lld\n", static_cast<long long>(me - qz.dflt.rank));
                    std::printf("xpert_eq_eqz_deficiency_alt=%lld\n", static_cast<long long>(me - qz.alt.rank));
                    std::printf("xpert_eq_eqz_deficiency_arm=%lld\n", static_cast<long long>(me - qz.arm.rank));
                }
            }
        }

        double f0 = std::numeric_limits<double>::quiet_NaN();
        try { model->eval_f(x0, f0); } catch (...) {}
        const VectorXd xp = x0.cwiseMax(xl).cwiseMin(xu);
        int clamps = 0;
        for (int i = 0; i < n; ++i) if (x0[i] < xl[i] || x0[i] > xu[i]) ++clamps;
        std::printf("projected_start_clamps=%d\n", clamps);
        if (clamps > 0 || !std::isfinite(f0)) {
            if (clamps == 0) {
                std::printf("projected_start=IDENTICAL_TO_x0 (no component outside the box; "
                            "f(x0) non-finite is a property of x0 itself)\n");
            } else {
                at_point("xproj", *model, xp, xl, xu, eq_rows, jr, jc, L, dump_dir);
            }
        } else {
            std::printf("projected_start=NOT_EVALUATED (x0 in box, f(x0) finite)\n");
        }
    } catch (const std::exception &e) {
        std::printf("PROBE_ERROR=%s\n", e.what());
        std::printf("PROBE_END\n");
        return 4;
    }
    std::printf("PROBE_END\n");
    return 0;
}
