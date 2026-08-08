// SAMPLE INPUT FOR static_scan.sh's SELF-TEST. NOT COMPILED BY ANY TARGET and
// not valid as a standalone translation unit -- no includes, no declarations,
// undefined identifiers throughout. It is here so the scanner can be checked
// against a fixed, committed set of touchpoints on a machine that has neither
// sibling checkout, which is what makes the self-test run in ordinary CI.
//
// Every source list in this repository is written out explicitly rather than
// globbed, so nothing picks this file up. If a glob is ever introduced, this
// directory has to be excluded from it.
//
// One line per touchpoint class the scan must find, plus two negative controls
// it must NOT report. The per-class counts below are exactly what the
// self-test asserts, so adding a line here means updating that count there.

void sample() {
    // --- parameter writes (3) ---
    iparm_[34] = 1;
    iparm_[7] = 0;
    solver.iparm_[12] = 1;

    // --- parameter reads (2) ---
    const int refinement = iparm_[6];
    n_pos = iparm[21];

    // --- phase-entry calls (3) ---
    pardisoinit(pt, &mtype, iparm);
    pardiso(pt, &maxfct, &mnum, &mtype, &phase, &n, a, ia, ja, perm, &nrhs, iparm, &msglvl, b, x,
            &error);
    pardiso_64(pt, &maxfct, &mnum, &mtype, &phase, &n, a, ia, ja, perm, &nrhs, iparm, &msglvl, b,
               x, &error);

    // --- Accelerate calls (3) ---
    SparseFactor(SparseFactorizationLDLTTPP, structure, fopts);
    SparseGetInertia(numeric, &np, &nz, &nn);
    SparseCleanup(numeric);

    // --- dense LAPACK calls (2) ---
    LAPACKE_dsytrf(LAPACK_COL_MAJOR, 'L', m, factored, m, ipiv);
    LAPACKE_dsytrs(LAPACK_COL_MAJOR, 'L', m, 1, factored, m, ipiv, y, m);

    // --- thread control (2) ---
    mkl_set_num_threads(1);
    mkl_set_num_threads_local(n_threads);

    // --- negative controls (must NOT be reported) ---
    int negative_control_identifier_iparmish = 0;
    LAPACKE_dgesv(LAPACK_COL_MAJOR, n, 1, a, n, ipiv, b, n);
}
