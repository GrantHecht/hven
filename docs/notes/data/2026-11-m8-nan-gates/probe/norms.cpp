#include <Eigen/Dense>
using Vec = Eigen::VectorXd;
// Out of line so nothing constant-folds.
double plain_lp(const Vec &v) { return v.lpNorm<Eigen::Infinity>(); }
double prod_lp(const Vec &v, const Vec &d) { return v.cwiseProduct(d).lpNorm<Eigen::Infinity>(); }
double seg_lp(const Vec &v, Eigen::Index s, Eigen::Index n) { return v.segment(s, n).lpNorm<Eigen::Infinity>(); }
double diff_lp(const Vec &a, const Vec &b) { return (a - b).lpNorm<Eigen::Infinity>(); }
double sticky(const Vec &v) {
    return v.size() == 0 ? 0.0 : v.cwiseAbs().template maxCoeff<Eigen::PropagateNaN>();
}
double sticky_prod(const Vec &v, const Vec &d) {
    return v.size() == 0 ? 0.0 : v.cwiseProduct(d).cwiseAbs().template maxCoeff<Eigen::PropagateNaN>();
}
