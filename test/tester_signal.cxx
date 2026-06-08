// tester_signal.cxx — exercises mist::hep::signal waveform helpers.

#include "mist/hep/signal/waveform.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

#include <TGraph.h>

namespace s = mist::hep::signal;

namespace {

int failures = 0;

void check(bool cond, const char* what) {
    if (!cond) { std::printf("  FAIL: %s\n", what); ++failures; }
}

void check_close(double got, double want, double tol, const char* what) {
    if (std::fabs(got - want) > tol) {
        std::printf("  FAIL: %s — got %.6g, want %.6g\n", what, got, want);
        ++failures;
    }
}

}  // namespace

int main() {
    // A small triangular pulse: rises to 10 at x=5, falls back. x = 0..10.
    TGraph wave;
    for (int i = 0; i <= 10; ++i) {
        const double y = 10.0 - std::fabs(i - 5.0) * 2.0;  // peak 10 at i=5
        wave.SetPoint(i, i, y);
    }

    // ------------------------------------------------------------------
    std::puts("[tester_signal] amplitude / extremum");
    check_close(s::amplitude(wave, 0.0, 10.0), 10.0, 1e-9, "amplitude: peak 10");
    check_close(s::amplitude(wave, 0.5, 2.5), 4.0, 1e-9, "amplitude: windowed max (x in (0.5,2.5))");
    {
        const auto [x, y] = s::extremum(wave);
        check_close(x, 5.0, 1e-9, "extremum: peak x");
        check_close(y, 10.0, 1e-9, "extremum: peak y");
        const auto [xm, ym] = s::extremum(wave, /*find_min=*/true);
        check_close(ym, 0.0, 1e-9, "extremum: min y (endpoints = 0)");
    }

    // ------------------------------------------------------------------
    std::puts("[tester_signal] threshold / above");
    {
        // Rising crossing of threshold 6: y reaches 6 at x=3 (10-2*2=6).
        const auto crossings = s::threshold_crossings(wave, 6.0, +1.0);
        check(!crossings.empty(), "threshold_crossings: at least one rising crossing");
        check_close(crossings.front(), 3.0, 1e-9, "threshold_crossings: first at x=3");

        // strict '>': y=6 (x=3) does not qualify; first strictly-above is x=4.
        const auto [xa, ya] = s::first_above(wave, 6.0);
        check_close(xa, 4.0, 1e-9, "first_above: x=4 (strict >)");
        check(ya > 6.0, "first_above: y strictly above threshold");
        const auto [xl, yl] = s::last_above(wave, 6.0);
        check_close(xl, 6.0, 1e-9, "last_above: x=6 (strict >)");

        const auto [xn, yn] = s::first_above(wave, 999.0);
        check(std::isnan(xn) && std::isnan(yn), "first_above: none -> NaN");
    }

    // ------------------------------------------------------------------
    std::puts("[tester_signal] filters");
    {
        // maximum_filter in blocks of 2 reduces 11 points -> 6 blocks.
        auto mf = s::maximum_filter(wave, 2);
        check(mf->GetN() == 6, "maximum_filter: 6 blocks from 11 points");

        // gaussian_filter window 3, sigma 1: output length N-w+1 = 9; smoothed
        // values stay within the data range.
        auto gf = s::gaussian_filter(wave, 3, 1.0);
        check(gf->GetN() == 9, "gaussian_filter: N-w+1 outputs");
        for (int i = 0; i < gf->GetN(); ++i)
            check(gf->GetPointY(i) >= 0.0 && gf->GetPointY(i) <= 10.0,
                  "gaussian_filter: smoothed value in range");

        // empty / degenerate guards.
        check(s::maximum_filter(wave, 0)->GetN() == 0, "maximum_filter: n=0 -> empty");
        check(s::gaussian_filter(wave, 0, 1.0)->GetN() == 0, "gaussian_filter: w=0 -> empty");
    }

    // ------------------------------------------------------------------
    std::puts("[tester_signal] integrate / find_peaks");
    {
        // Sum of y over the whole pulse: symmetric triangle 0..10..0 step 2.
        // y = {0,2,4,6,8,10,8,6,4,2,0} -> sum = 50.
        check_close(s::integrate(wave, 0.0, 0.0), 50.0, 1e-9, "integrate: sum of y = 50");
        // Windowed: x in [4,6] -> y {8,10,8} = 26.
        check_close(s::integrate(wave, 4.0, 6.0), 26.0, 1e-9, "integrate: windowed = 26");

        // One clear peak with prominence >= 4 within +-3 points.
        const auto peaks = s::find_peaks(wave, 4.0, 3);
        check(peaks.size() == 1, "find_peaks: one peak");
        if (!peaks.empty()) {
            check_close(peaks.front().x, 5.0, 1e-9, "find_peaks: peak at x=5");
            check_close(peaks.front().y, 10.0, 1e-9, "find_peaks: peak y=10");
        }
    }

    // ------------------------------------------------------------------
    if (failures) {
        std::printf("[tester_signal] %d failure(s)\n", failures);
        return EXIT_FAILURE;
    }
    std::puts("[tester_signal] OK");
    return EXIT_SUCCESS;
}
