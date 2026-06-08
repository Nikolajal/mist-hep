// tester_stats.cxx — exercises the ROOT-free stats helpers.
//
// Deliberately uses no ROOT symbols: mist::hep::stats is header-only and
// ROOT-free, and this tester proves it (ROOT will still be pulled in
// transitively via PUBLIC linkage on mist::hep, but nothing here uses it).

#include "mist/hep/stats.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace s = mist::hep::stats;

namespace {

int failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        std::printf("  FAIL: %s\n", what);
        ++failures;
    }
}

void check_close(double got, double want, double tol, const char* what) {
    const bool ok = std::fabs(got - want) <= tol;
    if (!ok) {
        std::printf("  FAIL: %s — got %.6g, want %.6g (tol %.3g)\n",
                    what, got, want, tol);
        ++failures;
    }
}

}  // namespace

int main() {
    std::puts("[tester_stats] quadrature_sum");
    check_close(s::quadrature_sum({3.0, 4.0}), 5.0, 1e-12, "3,4 -> 5");
    check_close(s::quadrature_sum({1.0, 2.0, 2.0}), 3.0, 1e-12, "1,2,2 -> 3");
    check_close(s::quadrature_sum(std::vector<double>{}), 0.0, 1e-12, "empty -> 0");
    {
        std::vector<int> xs = {6, 8};
        check_close(s::quadrature_sum(xs), 10.0, 1e-12, "int range 6,8 -> 10");
    }

    std::puts("[tester_stats] barlow_parameter / barlow_passes");
    // Identical results, same errors -> denom underflow, returns NaN, passes by convention.
    check(std::isnan(s::barlow_parameter(1.0, 0.1, 1.0, 0.1)),
          "equal errors yield NaN");
    check(s::barlow_passes(1.0, 0.1, 1.0, 0.1),
          "equal errors pass (no info)");

    // Different result, larger std error than var error: t = (1-1.5)/sqrt(0.04-0.01) = -2.886...
    {
        const double t = s::barlow_parameter(1.0, 0.2, 1.5, 0.1);
        check_close(t, -2.8867513459, 1e-6, "barlow t value");
        check(!s::barlow_passes(1.0, 0.2, 1.5, 0.1), "|t|>1 -> fail");
        check(s::barlow_passes(1.0, 0.2, 1.5, 0.1, /*threshold=*/3.0),
              "loosened threshold passes");
    }

    std::puts("[tester_stats] clip_outliers_nsigma");
    {
        std::vector<double> xs = {1, 1, 1, 1, 1, 1, 1, 1, 1, 100};
        const auto removed = s::clip_outliers_nsigma(xs, /*n_sigma=*/2.0);
        check(removed == 1, "drops the single far outlier");
        check(xs.size() == 9, "size after drop");
    }
    {
        // No outliers -> nothing removed, vector unchanged.
        std::vector<double> xs = {0.9, 1.0, 1.1, 0.95, 1.05};
        const auto removed = s::clip_outliers_nsigma(xs, 3.0);
        check(removed == 0, "no outliers -> no drops");
        check(xs.size() == 5, "size preserved");
    }
    {
        // Degenerate: all equal -> sd==0, function returns 0 removals.
        std::vector<double> xs = {2.0, 2.0, 2.0};
        const auto removed = s::clip_outliers_nsigma(xs);
        check(removed == 0, "zero-variance input is a no-op");
        check(xs.size() == 3, "size preserved on zero-variance");
    }
    {
        // Empty / single element -> no-op, no division by zero.
        std::vector<double> empty;
        check(s::clip_outliers_nsigma(empty) == 0, "empty -> 0");
        std::vector<double> one = {42.0};
        check(s::clip_outliers_nsigma(one) == 0, "single -> 0");
    }

    std::puts("[tester_stats] weighted_mean_rms");
    {
        // Five clean measurements {value, error}; no outliers.
        std::vector<std::array<double, 2>> m = {
            {10.0, 1.0}, {12.0, 1.0}, {11.0, 1.0}, {9.0, 1.0}, {13.0, 1.0}};
        const auto r = s::weighted_mean_rms(m);
        check_close(r[0], 11.0, 1e-9, "mean of {9..13} = 11");
        check(r[1] > 0.0, "nonzero RMS");
    }
    {
        // A gross outlier is clipped at a tight n-sigma, pulling the mean back.
        std::vector<std::array<double, 2>> m = {
            {10.0, 1.0}, {10.0, 1.0}, {10.0, 1.0}, {10.0, 1.0}, {1000.0, 1.0}};
        const auto loose = s::weighted_mean_rms(m, 10.0);
        const auto tight = s::weighted_mean_rms(m, 1.5);
        check(tight[0] < loose[0], "tighter clip removes the outlier -> lower mean");
        check_close(tight[0], 10.0, 1e-9, "survivors all 10 -> mean 10");
    }
    {
        std::vector<std::array<double, 2>> empty;
        const auto r = s::weighted_mean_rms(empty);
        check(std::isnan(r[0]) && std::isnan(r[1]), "empty -> {NaN, NaN}");
    }

    if (failures) {
        std::printf("[tester_stats] %d failure(s)\n", failures);
        return EXIT_FAILURE;
    }
    std::puts("[tester_stats] OK");
    return EXIT_SUCCESS;
}
