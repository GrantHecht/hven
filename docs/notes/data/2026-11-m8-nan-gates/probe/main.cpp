#include <Eigen/Dense>
#include <cmath>
#include <cstdio>
#include <limits>
using Vec = Eigen::VectorXd;
double plain_lp(const Vec &v);
double prod_lp(const Vec &v, const Vec &d);
double seg_lp(const Vec &v, Eigen::Index s, Eigen::Index n);
double diff_lp(const Vec &a, const Vec &b);
double sticky(const Vec &v);
double sticky_prod(const Vec &v, const Vec &d);
int main() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const int sizes[] = {1, 2, 3, 4, 5, 7, 8, 9, 11, 15, 16, 17, 31, 32, 33, 64, 100, 1001};
    long total = 0, miss[6] = {0, 0, 0, 0, 0, 0};
    for (int n : sizes) {
        int nmiss_plain = 0, nmiss_prod = 0, nmiss_seg = 0, nmiss_diff = 0, nmiss_st = 0, nmiss_stp = 0;
        for (int k = 0; k < n; ++k) {
            Vec v(n), d = Vec::Constant(n, 0.5), z = Vec::Zero(n);
            for (int i = 0; i < n; ++i) v(i) = 1.0 + 0.001 * ((i * 37) % n);
            v(k) = nan;
            ++total;
            if (!std::isnan(plain_lp(v))) ++nmiss_plain;
            if (!std::isnan(prod_lp(v, d))) ++nmiss_prod;
            if (n > 1 && k >= 1 && !std::isnan(seg_lp(v, 1, n - 1))) ++nmiss_seg;
            if (!std::isnan(diff_lp(v, z))) ++nmiss_diff;
            if (!std::isnan(sticky(v))) ++nmiss_st;
            if (!std::isnan(sticky_prod(v, d))) ++nmiss_stp;
        }
        std::printf("n=%5d  NaN-positions=%5d  finite-returned: lpNorm=%d cwiseProduct.lpNorm=%d segment(1,n-1).lpNorm=%d (a-b).lpNorm=%d | PropagateNaN=%d PropagateNaN(prod)=%d\n",
                    n, n, nmiss_plain, nmiss_prod, nmiss_seg, nmiss_diff, nmiss_st, nmiss_stp);
        miss[0] += nmiss_plain; miss[1] += nmiss_prod; miss[2] += nmiss_seg; miss[3] += nmiss_diff; miss[4] += nmiss_st; miss[5] += nmiss_stp;
    }
    // finite identity + inf
    Vec w(1001); for (int i = 0; i < 1001; ++i) w(i) = std::sin(1.0 + i) * (i % 7 == 0 ? 1e3 : 1.0);
    std::printf("finite identity (n=1001): %s\n", plain_lp(w) == sticky(w) ? "EQUAL" : "DIFF");
    w(500) = std::numeric_limits<double>::infinity();
    std::printf("inf: lpNorm=%g sticky=%g\n", plain_lp(w), sticky(w));
    w(900) = nan;
    std::printf("inf+NaN: lpNorm=%g sticky=%g\n", plain_lp(w), sticky(w));
    std::printf("TOTAL positions=%ld finite-returned: lpNorm=%ld prod=%ld seg=%ld diff=%ld sticky=%ld sticky_prod=%ld\n",
                total, miss[0], miss[1], miss[2], miss[3], miss[4], miss[5]);
    return 0;
}
