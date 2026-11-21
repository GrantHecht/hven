/* M8 DN-0 instrument: link-time wrap of MKL's public `pardiso` entry point.
 *
 * Linked ONLY into a separate dump build of the harness lane binary with
 * -Wl,--wrap=pardiso (MKL is linked statically there, so LD_PRELOAD cannot
 * reach it). No hven source or libhven.a object changes: the observation is
 * taken at MKL's public ABI, which any consumer could make.
 *
 * With HVEN_DN0_DUMP_DIR unset the wrapper is a pure pass-through.
 * With it set, every call is appended to <dir>/index.tsv, and the matrix of
 * each of the first HVEN_DN0_DUMP_MAX (default 50) numeric-factorization
 * calls (phase 12, 13, 22, 23) is written to <dir>/mat_NNNN.bin together with
 * the iparm vector the call was made with and the inertia it returned.
 *
 * mat_NNNN.bin layout (little-endian, native):
 *   char[8]  "DN0MAT1\0"
 *   int32    n, nnz, phase, mtype, error_out, index_base (iparm[34])
 *   int32    neg_out (iparm[22]), pos_out (iparm[21]), perturbed_out (iparm[13])
 *   int32    iparm_in[64]
 *   int32    ia[n+1], ja[nnz]       (as passed; base per index_base)
 *   double   a[nnz]
 * LP64 only (MKL_INT == int), which is what hven links (mkl_intel_lp64).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int MKL_INT;

void __real_pardiso(void *pt, const MKL_INT *maxfct, const MKL_INT *mnum, const MKL_INT *mtype,
                    const MKL_INT *phase, const MKL_INT *n, const void *a, const MKL_INT *ia,
                    const MKL_INT *ja, MKL_INT *perm, const MKL_INT *nrhs, MKL_INT *iparm,
                    const MKL_INT *msglvl, void *b, void *x, MKL_INT *error);

static int g_init = 0;
static const char *g_dir = NULL;
static long g_max = 50;
static long g_call = 0;
static long g_fact = 0;

static void dn0_init(void) {
    g_init = 1;
    g_dir = getenv("HVEN_DN0_DUMP_DIR");
    const char *m = getenv("HVEN_DN0_DUMP_MAX");
    if (m != NULL) {
        g_max = strtol(m, NULL, 10);
    }
}

static int is_numeric_factorization(MKL_INT phase) {
    return phase == 12 || phase == 13 || phase == 22 || phase == 23;
}

void __wrap_pardiso(void *pt, const MKL_INT *maxfct, const MKL_INT *mnum, const MKL_INT *mtype,
                    const MKL_INT *phase, const MKL_INT *n, const void *a, const MKL_INT *ia,
                    const MKL_INT *ja, MKL_INT *perm, const MKL_INT *nrhs, MKL_INT *iparm,
                    const MKL_INT *msglvl, void *b, void *x, MKL_INT *error) {
    if (!g_init) {
        dn0_init();
    }
    if (g_dir == NULL) {
        __real_pardiso(pt, maxfct, mnum, mtype, phase, n, a, ia, ja, perm, nrhs, iparm, msglvl, b,
                       x, error);
        return;
    }
    MKL_INT iparm_in[64];
    memcpy(iparm_in, iparm, sizeof(iparm_in));
    __real_pardiso(pt, maxfct, mnum, mtype, phase, n, a, ia, ja, perm, nrhs, iparm, msglvl, b, x,
                   error);

    const long call = g_call++;
    long fact = -1;
    int dumped = 0;
    const int base = iparm_in[34] ? 0 : 1;
    const MKL_INT nn = *n;
    const MKL_INT nnz = (ia != NULL && nn > 0) ? ia[nn] - base : 0;
    if (is_numeric_factorization(*phase)) {
        fact = g_fact++;
        if (fact < g_max && a != NULL && ia != NULL && ja != NULL) {
            char path[4096];
            snprintf(path, sizeof(path), "%s/mat_%04ld.bin", g_dir, fact);
            FILE *f = fopen(path, "wb");
            if (f != NULL) {
                const char magic[8] = "DN0MAT1";
                const int32_t hdr[9] = {nn,           nnz,       *phase,    *mtype,   *error,
                                        iparm_in[34], iparm[22], iparm[21], iparm[13]};
                fwrite(magic, 1, 8, f);
                fwrite(hdr, sizeof(int32_t), 9, f);
                fwrite(iparm_in, sizeof(int32_t), 64, f);
                fwrite(ia, sizeof(int32_t), (size_t)nn + 1, f);
                fwrite(ja, sizeof(int32_t), (size_t)nnz, f);
                fwrite(a, sizeof(double), (size_t)nnz, f);
                fclose(f);
                dumped = 1;
            }
        }
    }
    char ipath[4096];
    snprintf(ipath, sizeof(ipath), "%s/index.tsv", g_dir);
    FILE *idx = fopen(ipath, "a");
    if (idx != NULL) {
        /* call fact phase pt n nnz nrhs error pos neg perturbed dumped */
        fprintf(idx, "%ld\t%ld\t%d\t%p\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", call, fact, (int)*phase,
                pt, (int)nn, (int)nnz, nrhs != NULL ? (int)*nrhs : 0, (int)*error,
                is_numeric_factorization(*phase) ? (int)iparm[21] : -1,
                is_numeric_factorization(*phase) ? (int)iparm[22] : -1,
                is_numeric_factorization(*phase) ? (int)iparm[13] : -1, dumped);
        fclose(idx);
    }
}
