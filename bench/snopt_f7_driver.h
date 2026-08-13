#pragma once

// bench/snopt_f7_driver.h — PHASE-6 TASK 0. THE SNOPT BRIDGE: F7 at (N, p)
// mapped onto SNOPT's snOptA problem form, solved cold or warm-basis, with the
// counters and the accuracy metrics the first-contact note needs.
//
// =====================================================================
// THE SOURCE FIREWALL, WHICH IS THE FIRST THING TO KNOW ABOUT THIS FILE.
//
// SNOPT is commercial, third-party, and NOT vendored: it lives outside the
// repository at ${HVEN_SQP_SNOPT_DIR} (default ~/Software/snopt7), is
// referenced by path from bench/CMakeLists.txt, and nothing SNOPT-derived is
// copied into this tree. NO SNOPT SOURCE (.f/.f90) WAS READ TO WRITE THIS
// FILE. Everything here was written against exactly three public artifacts:
//
//   (1) the installed PUBLIC INTERFACE HEADERS, ~/Software/snopt7/include/
//       {snopt.h, snoptProblem.hpp} -- the snopt-interface C++ layer, which
//       the library's own banner identifies as "SNOPT C++ interface 2.2.0"
//       over "S N O P T  7.7.7 (Feb 2021)";
//   (2) the SNOPT USER'S GUIDE -- Gill, Murray and Saunders, "User's Guide
//       for SNOPT Version 7: Software for Large-Scale Nonlinear Programming",
//       June 16 2008 (Stanford SOL mirror, sndoc7.pdf). Every claim below
//       about SNOPT's behaviour carries its section number;
//   (3) MEASUREMENT against the installed library, for the handful of facts
//       where the 2008 guide and the installed 7.7.7 demonstrably differ.
//       Those are flagged MEASURED and are never attributed to the guide.
//
// DISCLOSED: a full SNOPT SOURCE distribution also exists on this development
// machine, at ~/Source/snopt7. It was NEVER referenced -- not by this
// repository, not by the build (HVEN_SQP_SNOPT_DIR points at the BINARY
// install, ~/Software/snopt7), and not by any scratch work. NO SOURCE FILE
// under that path was opened. It is named here rather than left unmentioned,
// so that the three-artifact list above is a complete account of what this
// file was written from. See the first-contact note's section 1.1
// (docs/notes/2026-08-02-snopt-first-contact.md).
//
// THE GUIDE IS A VERSION BEHIND THE INSTALL and this file does not pretend
// otherwise. The 2008 guide documents SNOPT 7; the install is 7.7.7. The
// guide is authoritative for MECHANISM (what Start = 2 means, what xstate
// encodes, how the A/G split works, what the tolerances test) because those
// are interface contracts that 7.7.7 still honours -- verified by this
// bridge's own gate test. It is NOT authoritative for DEFAULT VALUES: 7.7.7's
// resolved defaults are read off the print file's own parameter listing and
// differ from the guide in at least three places (Major optimality tolerance
// 2.0e-6 not 1.0e-6; Unbounded objective 1.0e+10 not 1.0e+15; Iterations
// limit 10000). Where a default matters to a measurement, the note pins the
// MEASURED value and says so.
//
// =====================================================================
// WHY snOptA AND NOT snOptB/C. snOptA (guide section 3) takes the problem as
// one vector of functions F(x) with bounds on each row, and lets the caller
// split F into a CONSTANT linear part A and a nonlinear part G that is
// re-evaluated at each x (section 3.3). F7's equality rows -- 3N of them,
// 60% of the row count -- are AFFINE, so that split puts the entire
// collocation Jacobian into A where it is assembled once and never
// re-evaluated. snOptB/C's form would work too, but it wants one fused
// column-compressed Jacobian with the linear and nonlinear blocks ordered by
// hand, which is more transcription for no modelling gain here.
//
// ---------------------------------------------------------------------
// THE MAPPING, STATED IN BOTH DIRECTIONS (fidelity is the whole point: a
// comparison against a DIFFERENT problem is worthless).
//
// F7 (tests/sqp/support/scale_problems.h, nlp_model.h's sign conventions) is
//
//     min f(x)  s.t.  cE(x) = 0,  cI(x) <= 0,  l <= x <= u,
//
// with n = N*(ns+nc) = 5N variables, me = ns*N equality rows (ns initial
// condition + ns*(N-1) trapezoidal defect, ALL AFFINE), mi = N path rows
// cI_k = (1/2)(||y_k||^2 - R^2), and a box on the CONTROLS only.
//
// snOptA's form (guide section 3, problem NPA) is
//
//     min F_ObjRow(x)  s.t.  lF <= F(x) <= uF,  lx <= x <= ux,
//     F(x) = f(x) + A x   (A constant, f nonlinear; section 3.3)
//
// and the transcription is:
//
//   VARIABLES -- IDENTITY. Same n, same node-major ordering, same values. No
//   slacks, no splitting, no reformulation. x[j] here IS x(j) there.
//
//   BOUNDS -- IDENTITY, AND EXACTLY SO. F7's free states carry +/-
//   kParamInf = 1e20 (parametric_families.h), and SNOPT's default Infinite
//   bound size is 1.0e+20 (guide section 7.3, p.63): a bound at or beyond
//   infBnd is "non-existent" (section 3.4, xlow/xupp). The two constants
//   coincide, so lower()/upper() are copied through UNMODIFIED and both
//   solvers see the identical box. THIS IS THE ONE PLACE THE IPM BRIDGE
//   HAD TO PAY A PENALTY AND THIS ONE DOES NOT: SNOPT handles bounds
//   natively as bounds, so there is no bound-to-row expansion here and no
//   1.8x row inflation. Stated in the note in both directions.
//
//   ROWS -- nF = 1 + me + mi, laid out
//       F[0]              the objective          (ObjRow = 0, see INDEXING)
//       F[1 .. me]        the equality rows      (all linear -> A)
//       F[1+me .. nF-1]   the path rows          (nonlinear -> G)
//
//   THE OBJECTIVE ROW goes ENTIRELY into G. f(x) = (1/2)x'Wx - g'x has
//   grad f = Wx - g, and W is block-diagonal with no zero column, so no
//   component of the objective gradient is constant and there is nothing
//   for A to hold. usrfun returns f(x) and grad f(x) verbatim from the
//   model. (The guide's section 3.3 split is worth taking where a row is
//   PARTLY constant; here it would only move the -g'x term, gaining nothing
//   and adding a place to get a sign wrong.)
//
//   THE EQUALITY ROWS go ENTIRELY into A, and their constants move to the
//   BOUNDS. snOptA's F has no constant term -- F_i = f_i(x) + (Ax)_i -- so
//   cE_r(x) = L_r(x) - c_r = 0 is transcribed as the linear row L_r with
//   Flow = Fupp = c_r (guide section 3.4: "To make the ith constraint an
//   equality, F_i(x) = beta, set Flow(i) = Fupp(i) = beta"). The constants
//   c_r are recovered through the PUBLIC model surface and not by reaching
//   into F7's private data: cE is affine, so L(0) = 0 and
//
//       c = -eval_ce(0).
//
//   The coefficients are eval_jac_e(anything) -- constant in x AND in p (it
//   is built from h, theta, sigma and the index maps only), which is why the
//   warm arm below can re-solve at a new p without touching A.
//
//   THE PATH ROWS go into G, KEPT VERBATIM: usrfun returns
//   cI_k = (1/2)(||y_k||^2 - R^2) as the nonlinear function and the row bound
//   is Flow = -infBnd, Fupp = 0. The -R^2/2 could equally have been moved to
//   the bound (Fupp = R^2/2, f_k = ||y_k||^2/2); it is left inside the
//   function so that F[1+me+k] read back from SNOPT is F7's OWN cI_k, which
//   makes the cross-check against eval_ci() a direct comparison.
//
//   THE HESSIAN IS NOT TRANSCRIBED, BECAUSE SNOPT CANNOT ACCEPT ONE. SNOPT
//   builds its own quasi-Newton BFGS approximation to the Hessian of the
//   Lagrangian (guide section 1.1, and section 7.7 "Hessian full memory /
//   Hessian limited memory", p.73), defaulting to LIMITED MEMORY whenever
//   the nonlinear variable count n1 > 75 -- which is every instance this
//   bridge measures. This engine supplies an EXACT analytic Hessian. This is
//   not a knob either side can turn: it is a structural difference between
//   the two methods and it belongs in the note next to every major-iteration
//   count, because it is the single most likely explanation of a major-count
//   gap in either direction. The bridge does not hide it and does not try to
//   compensate for it.
//
// ---------------------------------------------------------------------
// INDEXING -- 0-BASED HERE, AND THAT IS AN INTERFACE-LAYER FACT, MEASURED.
// The guide's section 3.2/3.4 examples are Fortran and 1-based. The
// installed C++ layer (snopt-interface 2.2.0) adds one to iAfun/jAvar/
// iGfun/jGvar and to ObjRow before calling the Fortran core, so callers of
// snoptProblemA::solve pass 0-BASED row/column coordinates and a 0-based
// ObjRow. This was established by measurement, not assumption: passing
// 1-based coordinates makes the library reject the problem with
//
//     XXX Nonlinear derivative element k = 2, row 2 column 3 is out of range.
//     SNOPTA EXIT 90 -- input arguments out of range
//
// on a two-variable problem whose largest legal coordinate is (2, 2) -- the
// reported coordinates are exactly the passed ones plus one. The 0-based
// convention then solves the same problem to its analytic optimum. The gate
// test re-establishes this on F7 every time it runs, because a silent
// off-by-one in the coordinate arrays is precisely the defect that would
// make this bridge measure the wrong problem.
//
// ---------------------------------------------------------------------
// COUNTERS -- FROM THE snSTOP CALLBACK, CROSS-CHECKED AGAINST THE PRINT FILE.
// SNOPT reports its iteration counts in the print file's EXIT block, as
// "No. of iterations" and "No. of major iterations" (guide section 8.6,
// p.94). That block is the DOCUMENTED source and the note quotes it -- but
// it cannot be the PROGRAM's source, because the Fortran unit is not flushed
// until the file is closed, so a caller that parses it between two solves of
// a warm sweep reads nothing (measured). The counters are therefore taken
// from the snSTOP monitor callback (the isnSTOP typedef in the public
// snopt.h, registered through snoptProblemABC::setSTOP), which is invoked
// once per major iteration and carries nMajor and itn:
//
//     majors = max nMajor over the calls,   minors = max itn over the calls.
//
// THE TWO ROUTES AGREE, and that is asserted rather than assumed: on the
// two-variable calibration problem snSTOP reports maxMajor = 3, maxItn = 3
// against a print file reading "No. of iterations 3 / No. of major
// iterations 3", and the gate test repeats the comparison on F7 itself.
//
// SEMANTICS, WHICH ARE NOT OURS: "No. of iterations" is SNOPT's TOTAL QP
// (minor) iteration count and "No. of major iterations" its SQP major count.
// They are NOT the same quantities as SqpCounters::major_iters and
// ::qp_minor_iters -- different QP solver, different active-set algebra,
// different Hessian. The CSV column names are fixed by the phase spec; the
// note's tables prefix every counter with its solver and NEVER put the two
// in one column. (snSTOP's own nMinor is the minor count of the CURRENT
// major, not a running total -- itn is the running total. Measured.)
//
// ---------------------------------------------------------------------
// WARM START -- Start = 2, THE GUIDE'S OWN MECHANISM (section 3.4, pp.17-18).
// "Start = 2 (Warm start) means that xstate and Fstate define a valid
// starting point (perhaps from an earlier call, though not necessarily)",
// with the requirements that
//   * every xstate(j) and Fstate(i) be one of 0,1,2,3 -- the on-exit state
//     encoding, nonbasic-at-lower / nonbasic-at-upper / superbasic / basic
//     (section 3.4, xstate on exit, p.18). A previous call's exit states
//     satisfy this by construction; the driver ASSERTS it rather than
//     trusting it, because a 4 or 5 leaking in would silently degrade the
//     warm start to a crash;
//   * x and Fmul be defined (section 3.4, p.17 and Fmul, p.17);
//   * nS "should retain its value from a previous call when a Warm start is
//     used" (section 3.4, nS on entry, p.18).
// The driver satisfies all four by simply NOT CLEARING the arrays between
// solves: SnoptF7Driver owns x/xstate/Fstate/xmul/Fmul/nS for its lifetime,
// so a second solve() at a new p starts from the first solve's exit state.
// THE ARRAYS ARE THE WARM START -- there is no separate hand-off object,
// which is the honest analogue of what our own WarmStart does and is
// deliberately not dressed up as more than SNOPT offers.
//
// WHAT MOVES WHEN p MOVES, and it is only two things: the equality-row
// bounds (Flow = Fupp = -eval_ce(0), which carries F7's p-dependent initial
// condition and forcing) and the objective's linear term (inside usrfun,
// read from the model at call time). A's COEFFICIENTS, the sparsity
// patterns, the box, and the path-row bounds are all p-INDEPENDENT. So
// re-solving at a new p is exactly refresh_parameter_dependent_bounds() plus
// a Start = 2 solve, and no structure is rebuilt.
//
// ---------------------------------------------------------------------
// TOLERANCES -- SNOPT'S ARE RELATIVE, OURS ARE ABSOLUTE, AND THE NOTE MUST
// NOT CONFLATE THEM. Guide section 7.7 (p.76):
//   * Major feasibility tolerance tests rowerr = max_i viol_i / ||x|| <= tau
//     (eq. 7.1) -- normalized BY THE SIZE OF THE SOLUTION;
//   * Major optimality tolerance tests maxComp = max_j Comp_j / ||pi|| <= tau
//     (eq. 7.2) -- normalized by the dual norm.
// This engine's kkt_tol/feas_tol are absolute. On F7 at n = 10^5, ||x|| is
// O(sqrt(n)), so SNOPT at its default 1e-6 admits absolute violations three
// orders of magnitude larger than the same number would mean for us. THIS
// IS WHY THE BRIDGE COMPUTES ITS OWN ABSOLUTE ACCURACY METRICS -- x_err_inf
// against the manufactured optimum and prim_inf_abs against the model's own
// cE/cI -- so the note's matched-accuracy reading rests on a quantity that
// means the same thing for both solvers rather than on either solver's
// self-report.
//
// ---------------------------------------------------------------------
// THREADING. The installed libsnopt7 links libgfortran/libm/libgcc/libc and
// NOTHING ELSE (ldd, measured) -- no MKL, no OpenMP runtime, no pthreads
// beyond libc. SNOPT's sparse linear algebra is its own LUSOL, so this arm
// is single-threaded by construction and there is no SNOPT-side threading
// knob to set. Our arm is pinned with MKL_NUM_THREADS=1 as the phase
// requires. Both arms are therefore single-threaded, for different reasons,
// and the note says so rather than implying a symmetric configuration.
//
// ---------------------------------------------------------------------
// NOT RE-ENTRANT, DELIBERATELY AND VISIBLY. snOptA's usrfun is a plain
// function pointer with no user-data slot that survives the C++ wrapper
// cleanly, so the active driver is published through a translation-unit
// static (kept in this header's own detail namespace, one definition via an
// inline variable). One SnoptF7Driver may be solving at a time per process.
// solve() enforces that with a throw rather than leaving it to chance.
//
// T5/T6: nothing here calls exit(), every rejection throws
// std::invalid_argument or std::runtime_error with the diagnostic folded
// into the message, and no diagnostic is printed that is not also thrown.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <Eigen/SparseCore>
#include <fmt/format.h>

#include <hven/detail/sqp/types.h>

#include "snoptProblem.hpp"

#include "support/scale_problems.h"

namespace hven::solvers::snopt_bridge {

// SNOPT's default Infinite bound size (guide section 7.3, p.63). Identical to
// parametric_families.h's kParamInf, which is what makes the box transcription
// an identity rather than a mapping -- see the banner.
constexpr double kSnoptInfBnd = 1.0e20;

// Options the bridge exposes. EVERY FIELD DEFAULTS TO "LEAVE SNOPT ALONE":
// a negative real or non-positive integer means the option is never set, so
// the solve runs at the installed library's own resolved default. That is
// what makes the "SNOPT at its natural tolerance" reading actually natural.
struct SnoptOptions {
    double major_feas_tol = -1.0;       // "Major feasibility tolerance"
    double major_opt_tol = -1.0;        // "Major optimality tolerance"
    double minor_feas_tol = -1.0;       // "Minor feasibility tolerance"
    long long major_iter_limit = -1;    // "Major iterations limit"
    long long minor_iter_limit = -1;    // "Minor iterations limit"
    long long iter_limit = -1;          // "Iterations limit" (total minors)
    long long superbasics_limit = -1;   // "Superbasics limit"
    long long reduced_hessian_dim = -1; // "Reduced Hessian dimension"
    int verify_level = -1;              // "Verify level" (>=0 sets it)
    // "Time limit (secs)", whose resolved default on the installed 7.7.7 is
    // 9999999.0 (MEASURED from the print file's parameter listing). Set so a
    // bounded measurement campaign ends INSIDE SNOPT, returning a resource
    // limit that becomes an honest CSV row, rather than being killed from the
    // shell -- which would lose the row entirely and leave a gap that looks
    // like an untried instance instead of a recorded DNF.
    double time_limit_seconds = -1.0;
    // VERBATIM option strings, handed to snoptProblem::setParameter (guide
    // section 7.5, snSet). The typed fields above cover the options this task
    // needed by name; this is the escape hatch for everything else, and it
    // exists for one specific reason worth recording: guide section 2.4 says
    // "Other QPSolver options are available for problems with many degrees of
    // freedom", which is precisely F7's regime, and a note claiming SNOPT
    // scales badly here without having TRIED them would be claiming more than
    // it measured. Applied in order, after every typed option, so a raw string
    // can deliberately override one.
    std::vector<std::string> raw_options;
    // The print file is where SNOPT's own EXIT block lands (guide section
    // 8.6). It is not the counter source (see the banner) but it IS the
    // artifact the note cites, so it is kept when a path is given.
    std::string print_file;
};

// One solve's outcome. Counters and accuracy are kept strictly separate:
// `info`/`majors`/`minors`/`superbasics` are SNOPT'S SELF-REPORT, while
// `x_err_inf`/`f_err_rel`/`prim_inf_abs` are OUR measurements against F7's
// manufactured optimum and F7's own residuals -- the referee, computed
// identically for both solvers.
struct SnoptResult {
    int info = -1;
    std::string status;
    long long majors = 0;
    long long minors = 0;
    long long stop_calls = 0;
    int superbasics = 0;
    int n_inf = 0;
    double s_inf = 0.0;
    double wall_seconds = 0.0;
    double f = 0.0;
    double x_err_inf = 0.0;
    double f_err_rel = 0.0;
    double prim_inf_abs = 0.0;
};

// Decode SNOPT's INFO into the guide's own words (section 3.4, p.19 and
// section 8.6). Anything unlisted is reported as its number rather than
// guessed at.
inline std::string snopt_info_string(int info) {
    switch (info) {
    case 1:
        return "Optimal"; // optimality conditions satisfied
    case 2:
        return "FeasiblePoint"; // feasible point found
    case 3:
        return "AccuracyNotAchieved";
    case 11:
        return "InfeasibleLinear";
    case 12:
        return "InfeasibleLinearEq";
    case 13:
        return "InfeasibleNonlinear";
    case 14:
        return "InfeasibilitiesMinimized";
    case 21:
        return "UnboundedObjective";
    case 22:
        return "ViolationLimit";
    case 31:
        return "IterationLimit";
    case 32:
        return "MajorIterationLimit";
    case 33:
        return "SuperbasicsLimitTooSmall";
    case 34:
        // NOT IN THE 2008 GUIDE'S LIST (section 3.4, p.19, stops at 33).
        // Established by MEASUREMENT against the installed 7.7.7, whose own
        // print file writes "SNOPTA EXIT 30 -- resource limit error / SNOPTA
        // INFO 34 -- time limit reached" when the Time limit option fires.
        // Decoded here rather than left as "Info34" because a bounded
        // measurement campaign turns this into an ORDINARY, EXPECTED outcome
        // -- a recorded DNF -- and a DNF row must say why it stopped.
        return "TimeLimit";
    case 41:
        return "PointCannotBeImproved";
    case 42:
        return "SingularBasis";
    case 43:
        return "CannotSatisfyConstraints";
    case 44:
        return "IllConditionedNullSpace";
    case 51:
        return "BadObjectiveDerivatives";
    case 52:
        return "BadConstraintDerivatives";
    case 62:
        return "UndefinedAtInitialPoint";
    case 71:
        return "TerminatedDuringEval";
    case 74:
        return "TerminatedFromMonitor";
    case 82:
    case 83:
    case 84:
        return "InsufficientStorage";
    case 91:
        return "InvalidInputArgument";
    default:
        return fmt::format("Info{}", info);
    }
}

class SnoptF7Driver;

namespace detail {
// The active driver, for the two C-style callbacks. See the banner's
// re-entrancy paragraph: this is the visible cost of snOptA's function-pointer
// interface, not an accident.
inline SnoptF7Driver *g_active = nullptr;
} // namespace detail

// Maps ONE F7CollocationChain onto snOptA and solves it, cold or warm.
//
// LIFETIME IS THE WARM-START STORY: the driver owns x/xstate/Fstate/xmul/Fmul
// and nS for as long as it lives, so solving twice from the same object IS a
// warm start in SNOPT's sense (guide section 3.4). Construct one per N, call
// solve() once per p.
class SnoptF7Driver {
  public:
    // `model` must outlive the driver. Its (N, ns, nc) fix every dimension and
    // every sparsity pattern below; only p may move afterwards.
    SnoptF7Driver(test_support::F7CollocationChain &model, SnoptOptions opts)
        : model_(model), opts_(std::move(opts)) {
        n_ = static_cast<int>(model_.n());
        me_ = static_cast<int>(model_.me());
        mi_ = static_cast<int>(model_.mi());
        nf_ = 1 + me_ + mi_;

        build_structure();
        build_static_bounds();
        refresh_parameter_dependent_bounds();

        x_.assign(static_cast<std::size_t>(n_), 0.0);
        xstate_.assign(static_cast<std::size_t>(n_), 0);
        xmul_.assign(static_cast<std::size_t>(n_), 0.0);
        f_.assign(static_cast<std::size_t>(nf_), 0.0);
        fstate_.assign(static_cast<std::size_t>(nf_), 0);
        fmul_.assign(static_cast<std::size_t>(nf_), 0.0);

        problem_.initialize(opts_.print_file.empty() ? "" : opts_.print_file.c_str(), 0);
        problem_.setProbName("F7");
        // Derivative option 1: usrfun supplies EVERY element of G (guide
        // section 7.3, p.64 -- "ONLY FOR snOptA"). F7's derivatives are
        // analytic, so nothing here is ever finite-differenced. This is the
        // one option the bridge always sets, because leaving it to the default
        // would be leaving the accuracy of the comparison to a default.
        set_int("Derivative option", 1);
        apply_optional_parameters();
        problem_.setSTOP(&SnoptF7Driver::stop_callback);
        problem_.setWorkspace(nf_, n_, nea_, neg_);
    }

    SnoptF7Driver(const SnoptF7Driver &) = delete;
    SnoptF7Driver &operator=(const SnoptF7Driver &) = delete;

    // Seed the primal iterate. Used for the cold arm (F7's own start_point)
    // and never called between the two halves of a warm sweep -- that is the
    // point of the warm arm.
    void set_start_point(const Vec &x0) {
        if (x0.size() != n_) {
            throw std::invalid_argument(
                fmt::format("SnoptF7Driver::set_start_point: expected {} entries, got {}", n_,
                            static_cast<long long>(x0.size())));
        }
        for (int j = 0; j < n_; ++j) {
            x_[static_cast<std::size_t>(j)] = x0(j);
        }
        std::fill(xstate_.begin(), xstate_.end(), 0);
        std::fill(fstate_.begin(), fstate_.end(), 0);
        std::fill(fmul_.begin(), fmul_.end(), 0.0);
        std::fill(f_.begin(), f_.end(), 0.0);
        ns_ = 0;
    }

    // Move the family parameter and refresh exactly what depends on it (the
    // equality-row bounds; the objective's linear term is read from the model
    // inside usrfun). Structure and A's values are untouched -- see the banner.
    void set_parameter(double p) {
        Vec pv(1);
        pv(0) = p;
        model_.set_parameters(pv);
        refresh_parameter_dependent_bounds();
    }

    // `start` is snOptA's Start argument: 0 = Cold, 2 = Warm (guide section
    // 3.4, p.15). 1 is rejected -- it means "cold, but from a basis file", and
    // this bridge never writes one, so accepting it would silently mean Cold.
    SnoptResult solve(int start) {
        if (start != 0 && start != 2) {
            throw std::invalid_argument(fmt::format(
                "SnoptF7Driver::solve: start must be 0 (Cold) or 2 (Warm), got {}", start));
        }
        if (start == 2) {
            // Guide section 3.4, pp.17-18: for Warm starts every xstate(j) and
            // Fstate(i) must be 0, 1, 2 or 3. Asserted, not assumed -- a 4 or 5
            // leaking in would quietly degrade the warm start to a CRASH and
            // the measurement would be of something else entirely.
            check_warm_states(xstate_, "xstate");
            check_warm_states(fstate_, "Fstate");
        }
        if (detail::g_active != nullptr) {
            throw std::runtime_error(
                "SnoptF7Driver::solve: another driver is already solving in this process; "
                "snOptA's usrfun is a bare function pointer, so solves cannot nest");
        }

        majors_ = 0;
        minors_ = 0;
        stop_calls_ = 0;
        detail::g_active = this;
        int info = 0;
        int n_inf = 0;
        double s_inf = 0.0;
        double wall = 0.0;
        try {
            const auto t0 = std::chrono::steady_clock::now();
            info = problem_.solve(
                start, nf_, n_, 0.0, /*ObjRow=*/0, &SnoptF7Driver::usrfun_callback, iafun_.data(),
                javar_.data(), a_.data(), nea_, igfun_.data(), jgvar_.data(), neg_, xlow_.data(),
                xupp_.data(), flow_.data(), fupp_.data(), x_.data(), xstate_.data(), xmul_.data(),
                f_.data(), fstate_.data(), fmul_.data(), ns_, n_inf, s_inf);
            const auto t1 = std::chrono::steady_clock::now();
            wall = std::chrono::duration<double>(t1 - t0).count();
        } catch (...) {
            detail::g_active = nullptr;
            throw;
        }
        detail::g_active = nullptr;

        SnoptResult r;
        r.info = info;
        r.status = snopt_info_string(info);
        r.majors = majors_;
        r.minors = minors_;
        r.stop_calls = stop_calls_;
        r.superbasics = ns_;
        r.n_inf = n_inf;
        r.s_inf = s_inf;
        r.wall_seconds = wall;
        r.f = f_[0];
        score(r);
        return r;
    }

    // The solution as an Eigen vector, for --dump-solution and for callers
    // that want to score it themselves.
    Vec solution() const {
        Vec out(n_);
        for (int j = 0; j < n_; ++j) {
            out(j) = x_[static_cast<std::size_t>(j)];
        }
        return out;
    }

    int nf() const { return nf_; }
    int nea() const { return nea_; }
    int neg() const { return neg_; }

    // SNOPT's own value of the row functions, so a caller can check the
    // path rows it returned against F7's eval_ci at the same x.
    double row_value(int i) const { return f_[static_cast<std::size_t>(i)]; }

  private:
    // ---- structure ----------------------------------------------------
    //
    // A holds every equality-row coefficient (rows 1..me of F), taken from
    // eval_jac_e -- constant in x and in p. G holds the objective gradient
    // (row 0, all n columns) followed by the path-row gradients (row 1+me+k,
    // node k's ns state columns). THE ORDER OF THE G COORDINATES IS A
    // CONTRACT with usrfun_callback below: "usrfun must define the values of
    // G in exactly the same order" (guide section 3.4, iGfun/jGvar, p.16).
    void build_structure() {
        const Eigen::SparseMatrix<double, Eigen::RowMajor> je = model_.eval_jac_e(Vec::Zero(n_));
        nea_ = static_cast<int>(je.nonZeros());
        iafun_.reserve(static_cast<std::size_t>(nea_));
        javar_.reserve(static_cast<std::size_t>(nea_));
        a_.reserve(static_cast<std::size_t>(nea_));
        for (int r = 0; r < je.outerSize(); ++r) {
            for (Eigen::SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(je, r); it; ++it) {
                iafun_.push_back(1 + static_cast<int>(it.row())); // +1: F[0] is the objective
                javar_.push_back(static_cast<int>(it.col()));
                a_.push_back(it.value());
            }
        }

        const int ns = static_cast<int>(model_.state_dim());
        const int per_node = static_cast<int>(model_.vars_per_node());
        neg_ = n_ + mi_ * ns;
        igfun_.reserve(static_cast<std::size_t>(neg_));
        jgvar_.reserve(static_cast<std::size_t>(neg_));
        for (int j = 0; j < n_; ++j) { // objective gradient: row 0, every column
            igfun_.push_back(0);
            jgvar_.push_back(j);
        }
        for (int k = 0; k < mi_; ++k) { // path row k: node k's state block
            for (int i = 0; i < ns; ++i) {
                igfun_.push_back(1 + me_ + k);
                jgvar_.push_back(k * per_node + i);
            }
        }
    }

    // The box (identity -- see the banner) and the row bounds that do NOT
    // move with p: the free objective row and the path rows' cI <= 0.
    void build_static_bounds() {
        xlow_.resize(static_cast<std::size_t>(n_));
        xupp_.resize(static_cast<std::size_t>(n_));
        const Vec &lo = model_.lower();
        const Vec &up = model_.upper();
        for (int j = 0; j < n_; ++j) {
            xlow_[static_cast<std::size_t>(j)] = lo(j);
            xupp_[static_cast<std::size_t>(j)] = up(j);
        }
        flow_.assign(static_cast<std::size_t>(nf_), 0.0);
        fupp_.assign(static_cast<std::size_t>(nf_), 0.0);
        flow_[0] = -kSnoptInfBnd; // the objective row is free (guide section 3.4)
        fupp_[0] = kSnoptInfBnd;
        for (int k = 0; k < mi_; ++k) {
            flow_[static_cast<std::size_t>(1 + me_ + k)] = -kSnoptInfBnd;
            fupp_[static_cast<std::size_t>(1 + me_ + k)] = 0.0; // cI_k <= 0, verbatim
        }
    }

    // cE is affine, so its constant vector is -cE(0) and the equality rows
    // become Flow = Fupp = that constant. THIS is the only thing p moves.
    void refresh_parameter_dependent_bounds() {
        const Vec ce0 = model_.eval_ce(Vec::Zero(n_));
        for (int r = 0; r < me_; ++r) {
            const double c = -ce0(r);
            flow_[static_cast<std::size_t>(1 + r)] = c;
            fupp_[static_cast<std::size_t>(1 + r)] = c;
        }
    }

    // ---- options ------------------------------------------------------
    void set_int(const char *key, int value) {
        if (problem_.setIntParameter(key, value) != 0) {
            throw std::runtime_error(
                fmt::format("SnoptF7Driver: SNOPT rejected integer option '{}' = {}", key, value));
        }
    }
    void set_real(const char *key, double value) {
        if (problem_.setRealParameter(key, value) != 0) {
            throw std::runtime_error(
                fmt::format("SnoptF7Driver: SNOPT rejected real option '{}' = {}", key, value));
        }
    }

    void apply_optional_parameters() {
        if (opts_.major_feas_tol > 0.0) {
            set_real("Major feasibility tolerance", opts_.major_feas_tol);
        }
        if (opts_.major_opt_tol > 0.0) {
            set_real("Major optimality tolerance", opts_.major_opt_tol);
        }
        if (opts_.minor_feas_tol > 0.0) {
            set_real("Minor feasibility tolerance", opts_.minor_feas_tol);
        }
        if (opts_.major_iter_limit > 0) {
            set_int("Major iterations limit", static_cast<int>(opts_.major_iter_limit));
        }
        if (opts_.minor_iter_limit > 0) {
            set_int("Minor iterations limit", static_cast<int>(opts_.minor_iter_limit));
        }
        if (opts_.iter_limit > 0) {
            set_int("Iterations limit", static_cast<int>(opts_.iter_limit));
        }
        if (opts_.superbasics_limit > 0) {
            set_int("Superbasics limit", static_cast<int>(opts_.superbasics_limit));
        }
        if (opts_.reduced_hessian_dim > 0) {
            set_int("Reduced Hessian dimension", static_cast<int>(opts_.reduced_hessian_dim));
        }
        if (opts_.verify_level >= 0) {
            set_int("Verify level", opts_.verify_level);
        }
        if (opts_.time_limit_seconds > 0.0) {
            set_real("Time limit", opts_.time_limit_seconds);
        }
        for (const std::string &opt : opts_.raw_options) {
            if (problem_.setParameter(opt.c_str()) != 0) {
                throw std::runtime_error(
                    fmt::format("SnoptF7Driver: SNOPT rejected option string '{}'", opt));
            }
        }
    }

    static void check_warm_states(const std::vector<int> &states, const char *what) {
        for (std::size_t i = 0; i < states.size(); ++i) {
            if (states[i] < 0 || states[i] > 3) {
                throw std::runtime_error(fmt::format(
                    "SnoptF7Driver::solve: Warm start requires every {}(i) in 0..3 (guide "
                    "section 3.4), but {}({}) = {}",
                    what, what, i, states[i]));
            }
        }
    }

    // ---- scoring ------------------------------------------------------
    //
    // OUR measurements, not SNOPT's. Same formulas as bench_f7_cold.cpp so the
    // two solvers' accuracy columns are literally the same computation:
    // x_err_inf against the manufactured optimum, f_err_rel against f*, and an
    // ABSOLUTE primal infeasibility from F7's own cE/cI at SNOPT's x (which
    // SNOPT itself never reports in absolute terms -- see the banner on
    // relative tolerances).
    void score(SnoptResult &r) const {
        const double p = model_.p();
        const Vec x = solution();
        r.x_err_inf = (x - model_.x_star(p)).lpNorm<Eigen::Infinity>();
        const double fstar = model_.f_star(p);
        // f_ is SNOPT's F(ObjRow); recompute from the model so the objective
        // column is ours too and a mis-mapped objective row cannot hide.
        const double f_model = model_.eval_f(x);
        r.f = f_model;
        r.f_err_rel = std::abs(f_model - fstar) / std::max(1.0, std::abs(fstar));
        const Vec ce = model_.eval_ce(x);
        const Vec ci = model_.eval_ci(x);
        double viol = ce.size() > 0 ? ce.lpNorm<Eigen::Infinity>() : 0.0;
        for (Index k = 0; k < ci.size(); ++k) {
            viol = std::max(viol, ci(k)); // cI <= 0, so only positive parts violate
        }
        r.prim_inf_abs = viol;
    }

    // ---- callbacks ----------------------------------------------------
    //
    // usrfun supplies the NONLINEAR part f(x) of F(x) = f(x) + Ax and its
    // derivatives G (guide sections 3.3 and 3.6). The equality rows are
    // entirely linear, so their f_i are identically zero and F is zeroed
    // wholesale before the nonlinear rows are written.
    //
    // A HAZARD CHECKED AND CLEARED: guide section 3.5 notes that snOptA
    // "removes linear variables from the calculation of F by setting them to
    // zero before calling usrfun". A "linear variable" is one appearing only
    // in A and never in G. Here the objective gradient covers ALL n columns,
    // so F7 has no such variable and the behaviour cannot bite. If the
    // objective's linear term were ever moved into A, this would stop being
    // true.
    static void usrfun_callback(int *status, int *n, double x[], int *need_f, int *ne_f, double f[],
                                int *need_g, int *ne_g, double g[], char *cu, int *lencu, int *iu,
                                int *leniu, double *ru, int *lenru) {
        (void)status;
        (void)ne_f;
        (void)ne_g;
        (void)cu;
        (void)lencu;
        (void)iu;
        (void)leniu;
        (void)ru;
        (void)lenru;
        SnoptF7Driver *self = detail::g_active;
        if (self == nullptr) {
            return; // cannot happen: solve() sets it around the call
        }
        const int nn = *n;
        Vec xv(nn);
        for (int j = 0; j < nn; ++j) {
            xv(j) = x[j];
        }
        if (*need_f > 0) {
            for (int i = 0; i < self->nf_; ++i) {
                f[i] = 0.0; // the equality rows' nonlinear part IS zero
            }
            f[0] = self->model_.eval_f(xv);
            const Vec ci = self->model_.eval_ci(xv);
            for (int k = 0; k < self->mi_; ++k) {
                f[1 + self->me_ + k] = ci(k);
            }
        }
        if (*need_g > 0) {
            // Written in exactly the order build_structure() declared.
            const Vec gr = self->model_.eval_grad(xv);
            for (int j = 0; j < nn; ++j) {
                g[j] = gr(j);
            }
            const int ns = static_cast<int>(self->model_.state_dim());
            const int per_node = static_cast<int>(self->model_.vars_per_node());
            int at = nn;
            for (int k = 0; k < self->mi_; ++k) {
                for (int i = 0; i < ns; ++i) {
                    g[at++] = xv(k * per_node + i); // d cI_k / d y_{k,i} = y_{k,i}
                }
            }
        }
    }

    // Invoked once per major iteration; see the banner on why the counters
    // come from here and not from the print file.
    static void stop_callback(int *i_abort, int kt_cond[], int *mjr_prt, int *minimz, int *m,
                              int *max_s, int *n, int *nb, int *nn_con0, int *nn_con, int *nn_obj0,
                              int *nn_obj, int *n_s, int *itn, int *n_major, int *n_minor,
                              int *n_swap, double *cond_hz, int *i_obj, double *scl_obj,
                              double *obj_add, double *f_obj, double *f_mrt, double pen_nrm[],
                              double *step, double *pr_inf, double *du_inf, double *vimax,
                              double *virel, int hs[], int *ne, int *nloc_j, int loc_j[],
                              int ind_j[], double jcol[], int *neg_con, double ascale[],
                              double bl[], double bu[], double fx[], double f_con[], double g_con[],
                              double g_obj[], double y_con[], double pi[], double rc[], double rg[],
                              double x[], char cu[], int *lencu, int iu[], int *leniu, double ru[],
                              int *lenru, char cw[], int *lencw, int iw[], int *leniw, double rw[],
                              int *lenrw) {
        (void)kt_cond;
        (void)mjr_prt;
        (void)minimz;
        (void)m;
        (void)max_s;
        (void)n;
        (void)nb;
        (void)nn_con0;
        (void)nn_con;
        (void)nn_obj0;
        (void)nn_obj;
        (void)n_s;
        (void)n_minor; // the CURRENT major's minors, not a running total
        (void)n_swap;
        (void)cond_hz;
        (void)i_obj;
        (void)scl_obj;
        (void)obj_add;
        (void)f_obj;
        (void)f_mrt;
        (void)pen_nrm;
        (void)step;
        (void)pr_inf;
        (void)du_inf;
        (void)vimax;
        (void)virel;
        (void)hs;
        (void)ne;
        (void)nloc_j;
        (void)loc_j;
        (void)ind_j;
        (void)jcol;
        (void)neg_con;
        (void)ascale;
        (void)bl;
        (void)bu;
        (void)fx;
        (void)f_con;
        (void)g_con;
        (void)g_obj;
        (void)y_con;
        (void)pi;
        (void)rc;
        (void)rg;
        (void)x;
        (void)cu;
        (void)lencu;
        (void)iu;
        (void)leniu;
        (void)ru;
        (void)lenru;
        (void)cw;
        (void)lencw;
        (void)iw;
        (void)leniw;
        (void)rw;
        (void)lenrw;
        *i_abort = 0;
        SnoptF7Driver *self = detail::g_active;
        if (self == nullptr) {
            return;
        }
        ++self->stop_calls_;
        self->majors_ = std::max<long long>(self->majors_, *n_major);
        self->minors_ = std::max<long long>(self->minors_, *itn);
    }

    test_support::F7CollocationChain &model_;
    SnoptOptions opts_;
    snoptProblemA problem_;

    int n_ = 0;
    int me_ = 0;
    int mi_ = 0;
    int nf_ = 0;
    int nea_ = 0;
    int neg_ = 0;
    int ns_ = 0; // superbasics; retained across solves for the warm arm

    std::vector<int> iafun_, javar_, igfun_, jgvar_;
    std::vector<double> a_;
    std::vector<double> xlow_, xupp_, flow_, fupp_;
    std::vector<double> x_, xmul_, f_, fmul_;
    std::vector<int> xstate_, fstate_;

    long long majors_ = 0;
    long long minors_ = 0;
    long long stop_calls_ = 0;
};

} // namespace hven::solvers::snopt_bridge
