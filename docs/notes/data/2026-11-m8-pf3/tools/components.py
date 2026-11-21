"""PF-3 symbol -> component table (declared in DECLARATION-PF-3.md; its sha256 is stamped in every output).

RULE OF ATTRIBUTION (innermost owner): the callchain of each sample is walked from the leaf outward;
the first frame matched by any rule decides the sample's component (rules are tried in table order
within one frame).  Frames that match nothing are GENERIC (libc, libstdc++, libm, libgfortran,
Eigen out-of-line instantiations, MKL BLAS/LAPACK/spblas, libiomp5, run_phase) and the sample is
charged to the nearest owner above them.  A sample whose chain matches no rule is `unattributed`.

Each rule is (component, sym_regex or None, dso_regex or None); a frame matches when every given
regex `search`es its field.  The owner category of a component is its prefix before the dot.
"""
import re

OWNER_CATEGORY = {
    "proc": "PROC  process / harness (not solver)",
    "eval": "EVAL  function / derivative evaluation",
    "la": "FACTOR factorization backend (MKL Pardiso, dense, matching/scaling)",
    "kkt": "KKT   KKT assembly and factor glue",
    "qp": "QP    QP subproblem logic (walk / SSN / IPQP, excl. backend)",
    "outer": "OUTER outer-loop solver logic (globalization, IPM loop, QN, scaling, restoration)",
    "setup": "SETUP setup, library init, teardown",
    "instr": "INSTR instrumentation and trace",
}

# CHAIN-LEVEL OVERRIDES: if ANY frame of the chain matches, the whole sample is charged to the component
# (problem decode/load and post-solve reporting are harness work whatever library code they call).
CHAIN_OVERRIDES = [
    ("proc.problem_load", r"CutestModel::CutestModel|CutestSource::|CUTEST_load_routines|HsSource::|small_dense_(problems|sources)|cutest_problems|hven_bench::roster|dlopen|_dl_open"),
    ("proc.harness_report", r"hven::dump_counters|hven::dump_options|add_counters_dump|ANON::(add_side|spell|fill_shared|kv_|CellReport)|compute_declared_diagnostics|DeclaredRows"),
]

RULES = [
    # --- harness / process ------------------------------------------------------------------
    ("eval.callback", r"hven_bench::cutest::CutestModel::|Hs\d+Model|SplitModelNlp", None),
    ("eval.callback", None, r"libpb\.so|libcutest_double\.so"),
    ("proc.harness_other", r"hven_bench::", None),
    ("proc.loader_startup", r"^_dl_|^__libc_start|^_init$|^__do_global|^_GLOBAL__sub_I|^dl_main$|^_start$", None),
    ("proc.loader_startup", None, r"ld-linux"),
    # --- evaluation callbacks (innermost) -----------------------------------------------------
    # --- library one-time init (MKL / threading runtime) -------------------------------------
    ("setup.library_init", r"MKL_Read_ThreadENV|_Init_MKL_Loader|mkl_serv_(cpu|get_max|init|set_num|mpi|domain|verbose)|get_core_count|mkl_pds_lp64_pds_get_num_threads|__kmp_(env|init|register|runtime|middle_initialize|do_serial|hardware)|mkl_cbwr|mkl_serv_load", None),
    # --- instrumentation ----------------------------------------------------------------------
    ("instr.counting_wrapper", r"CountingNlp", None),
    ("instr.ledger_trace", r"Ledger|TraceSink|JsonLinesTraceSink|ConsoleTraceSink|on_(iter|solve|ipqp|ipm|qp|message|fallback)|QpCounters|stop_poll|trace_names", None),
    # --- linear algebra backend ---------------------------------------------------------------
    ("la.sparse_analyze", r"FactorSession::analyze", None),
    ("la.sparse_factorize", r"FactorSession::factorize", None),
    ("la.sparse_solve", r"FactorSession::solve", None),
    ("la.dense_solve", r"DenseSession::solve|run_dense_solve|DenseSymmetricFactor::solve", None),
    ("la.dense_factorize", r"DenseSession::|DenseSymmetricFactor::|hven::linear::detail::dense_|validate_dense", None),
    ("la.matching_scaling", r"matching_scaling|ruiz_symmetric_scale|max_product_matching", None),
    ("la.other", r"hven::linear::(?!detail::FactorSession::run_phase)|FactorSession::(?!run_phase)", None),
    # --- KKT glue ------------------------------------------------------------------------------
    ("kkt.assembly", r"assemble_kkt|build_bordered_system|KktAssembly|IpqpKktLayout|kkt_assembly|KktVector", None),
    ("kkt.factor_glue", r"KktFactor|factorize_checked|solve_vec|inertia_regularization|dense_selection|DenseShiftSite", None),
    # --- QP subproblem logic -------------------------------------------------------------------
    ("qp.schur_border", r"SchurComplement|BorderOps|BorderState|sync_borders|append_border|add_border", None),
    ("qp.refine_polish", r"refine_bordered|polish_|bordered_(regularized_)?residual|eqp_refine", None),
    ("qp.eqp_dependency", r"solve_eqp|solve_bordered_eqp|certificate_coefficients|detect_dependency|dependency_test|DepSystem|verify_farkas|test_candidate", None),
    ("qp.ssn", r"SsnEngine", None),
    ("qp.ipqp", r"Ipqp", None),
    ("qp.elastic", r"run_elastic_ladder|ElasticQp", None),
    ("qp.walk", r"QpEngine|WorkingSet|build_subproblem", None),
    # --- outer-loop -----------------------------------------------------------------------------
    ("outer.scaling_basis", r"ProblemScaling|ColumnScaledModel|EqualityBasis|EqualityRowSubsetModel|recompute_equality_basis", None),
    ("outer.quasi_newton", r"Qn|Lbfgs|DenseDampedBfgs|MixedLift|MaskedQn", None),
    ("outer.restoration", r"Restoration|SocRecovery", None),
    ("outer.acceptance_linesearch", r"Acceptance|Funnel|LineSearch|BacktrackingLineSearch", None),
    # --- evaluation assembly (hven-side partitioned evaluation engine) --------------------------
    ("eval.assembly", r"NlpAssembly|assemble_impl|full_kkt_pass|objective_constraints_pass|NlpProblemModel|NlpAdapterCore|NlpConstraintPiece|NlpObjectivePiece|ConstraintModel|ObjectiveModel|AssemblyEvalSeam|NlpModelAssembly|refresh_(g|jac)|eval_.*_in_place|evaluate_kkt", None),
    # --- setup / teardown -------------------------------------------------------------------------
    ("setup.init_teardown", r"solver_init|init_impl|prepare_solve|SolverImpl::(Sqp|Ipm)SolverImpl|(Ipm|Sqp)Solver::(Ipm|Sqp)Solver|(Ipm|Sqp)Solver::~|Problem::Problem|Problem::~|ProblemState|make_nlp_program|NonLinearProgram::(make_nlp|analyze_partitioning)|AssemblyDeclaration|problem_from|ipm_program|ThreadPool|thread_pool|validate::|ProblemAccess|SolveResult|restate_claim", None),
    # --- remaining solver loop bodies ----------------------------------------------------------------
    ("outer.ipm_nlp_core", r"IpmSolverImpl|Governor|NonLinearProgram|barrier|IpmSolver::", None),
    ("outer.sqp_loop", r"SqpSolverImpl|SqpSolver::|AssemblyEvalSeam|continuation", None),
    # --- chain roots (reached only when no solver frame matched) ----------------------------------------
    ("proc.main_other", r"^main$|^__libc_start_call_main$|^_start$", None),
]
COMPILED_OVERRIDES = [(c, re.compile(r)) for c, r in CHAIN_OVERRIDES]
COMPILED = [(c, re.compile(s) if s else None, re.compile(d) if d else None) for c, s, d in RULES]

ALLOC_RE = re.compile(r"^(malloc|_int_malloc|_int_free\w*|free|cfree(@\S+)?|calloc|realloc|_int_realloc|operator new\S*|operator delete\S*|malloc_consolidate|__libc_malloc|__libc_free|mkl_serv_(malloc|free|calloc)|mm_internal_malloc|mm_internal_free|mkl_serv_(i_)?malloc\w*|_mkl_serv_\w*alloc\w*|mm_\w*(alloc|free)\w*|unlink_chunk\S*|sysmalloc|__libc_calloc|tcache_\w+)")


_ANON = "(anonymous namespace)"


def func_name(sym):
    """The demangled function name WITHOUT its parameter list (rules must never match argument types:
    `QpEngine::ratio_test(..., QpCounters&)` is not instrumentation)."""
    s = sym.replace(_ANON, "ANON")
    i = s.find("(")
    return s[:i] if i >= 0 else s


def classify(frames):
    """frames: list of (sym, dso) leaf-first.  Returns component or 'unattributed'."""
    frames = [(func_name(sym), dso) for sym, dso in frames]
    for comp, rre in COMPILED_OVERRIDES:
        for sym, dso in frames:
            if rre.search(sym):
                return comp
    for sym, dso in frames:
        for comp, sre, dre in COMPILED:
            if sre is not None and not sre.search(sym):
                continue
            if dre is not None and not dre.search(dso):
                continue
            if sre is None and dre is None:
                continue
            return comp
    # chains cut short inside MKL assembly kernels (no CFI): the owner cannot be recovered.
    return "unattributed.truncated_unwind" if len(frames) <= 3 else "unattributed.other"


BLAS_FAMILY_RE = re.compile(r"mkl_blas\w*?_?x?d(ger|gemv|gemm|syrk|syr|trsm|trsv|trmv|scal|swap|axpy|copy|dot|symv|symm)")


def blas_family(sym):
    """BLAS routine family of an MKL BLAS frame (None if not one).  Used ONLY to redistribute chains truncated inside MKL
    assembly kernels (see DECLARATION-PF-3.md section 4): a truncated leaf in family F is split in proportion to the component
    split of the same cell's fully-unwound samples whose chain contains a family-F MKL BLAS frame."""
    m = BLAS_FAMILY_RE.search(sym)
    return m.group(1) if m else None
