/* M8 F1 diagnosis probe (Apache-2.0, hven). Does MKL's dsecnd() -- or a cheap
 * documented service call -- warm anything a first Pardiso call would otherwise pay?
 * usage: mkl_probe <none|dsecnd|maxthreads|version> [nolate] ; prints wall microseconds per step.
 * Build: see diagnosis/build_probe.sh (static MKL LP64 + intel_thread + libiomp5, as hven). */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <mkl.h>

static double now_us(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e6 + t.tv_nsec * 1e-3;
}

static double pardiso_once(int tag) {
    /* 5x5 symmetric indefinite (mtype -2), upper triangle CSR, 1-based. */
    MKL_INT n = 5, ia[6] = {1, 4, 6, 8, 9, 10}, ja[9] = {1, 2, 5, 2, 3, 3, 4, 4, 5};
    double a[9] = {4, 1, 1, -3, 1, 2, 1, -1, 5}, b[5] = {1, 1, 1, 1, 1}, x[5];
    void *pt[64]; MKL_INT iparm[64], mtype = -2, nrhs = 1, maxfct = 1, mnum = 1, msglvl = 0,
                  err = 0, idum, phase;
    memset(pt, 0, sizeof pt); memset(iparm, 0, sizeof iparm);
    iparm[0] = 1; iparm[1] = 2; iparm[9] = 8; iparm[10] = 0; iparm[12] = 0; iparm[34] = 0;
    double t0 = now_us();
    phase = 13;
    pardiso(pt, &maxfct, &mnum, &mtype, &phase, &n, a, ia, ja, &idum, &nrhs, iparm, &msglvl, b, x, &err);
    double t1 = now_us();
    phase = -1;
    pardiso(pt, &maxfct, &mnum, &mtype, &phase, &n, a, ia, ja, &idum, &nrhs, iparm, &msglvl, b, x, &err);
    printf("pardiso_13[%d]_us=%.1f err=%lld x0=%.17g\n", tag, t1 - t0, (long long)err, x[0]);
    return t1 - t0;
}

int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "none";
    double t0 = now_us();
    if (!strcmp(mode, "dsecnd")) {
        (void)dsecnd();
    } else if (!strcmp(mode, "maxthreads")) {
        (void)mkl_get_max_threads();
    } else if (!strcmp(mode, "version")) {
        MKLVersion v; mkl_get_version(&v);
    }
    double t1 = now_us();
    printf("mode=%s pre_us=%.1f\n", mode, t1 - t0);
    pardiso_once(1);
    pardiso_once(2);
    if (argc > 2 && !strcmp(argv[2], "nolate")) return 0;
    double t2 = now_us();
    (void)dsecnd();
    double t3 = now_us();
    printf("late_dsecnd_us=%.1f\n", t3 - t2);
    return 0;
}
