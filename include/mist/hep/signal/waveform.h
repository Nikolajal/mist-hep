// SPDX-License-Identifier: MIT
//
// mist/hep/signal/waveform.h — signal/waveform analysis on TGraph.
//
// Feature extraction and filtering for sampled waveforms represented as a
// TGraph of (time, amplitude). Header-only. Functions that return a new
// graph hand back an owning mist::hep::owned::root_ptr; query functions
// return plain values and never allocate.
//
// Salvaged from the ePIC SiPM-characterisation laser/waveform utilities
// (mist-hep:F-49..F-54), cleaned: the original find_peaks created and drew
// into a TCanvas as a side-effect — removed here; gauss_filter was a
// mislabelled plain average — implemented as a real Gaussian-weighted
// window.
//
#pragma once

#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

#include <TGraph.h>

#include <mist/hep/owned.h>

namespace mist::hep::signal {

namespace owned = ::mist::hep::owned;

// ---------------------------------------------------------------------------
// amplitude: maximum y over the time window (tmin, tmax). Returns 0 if no
// point falls in the window (matching the SiPM-original convention).
// ---------------------------------------------------------------------------
[[nodiscard]] inline double
amplitude(const TGraph& g, double tmin, double tmax)
{
    double amp = 0.0;
    for (int i = 0; i < g.GetN(); ++i) {
        const double x = g.GetPointX(i);
        if (x > tmin && x < tmax && g.GetPointY(i) > amp) amp = g.GetPointY(i);
    }
    return amp;
}

// ---------------------------------------------------------------------------
// extremum: {x, y} of the maximum (or minimum, if find_min) over [min_x,
// max_x]. Passing min_x == max_x scans the whole graph. Returns {NaN, NaN}
// for an empty graph.
// ---------------------------------------------------------------------------
[[nodiscard]] inline std::pair<double, double>
extremum(const TGraph& g, bool find_min = false,
         double min_x = 0.0, double max_x = 0.0)
{
    const bool whole = (min_x == max_x);
    const double sign = find_min ? -1.0 : 1.0;
    double best_x = std::nan(""), best_y = std::nan("");
    double best = -std::numeric_limits<double>::infinity();
    for (int i = 0; i < g.GetN(); ++i) {
        const double x = g.GetPointX(i);
        if (!whole && (x < min_x || x > max_x)) continue;
        const double v = sign * g.GetPointY(i);
        if (v > best) { best = v; best_x = x; best_y = g.GetPointY(i); }
    }
    return {best_x, best_y};
}

// ---------------------------------------------------------------------------
// threshold_crossings: x positions where the signal crosses `threshold`.
// `direction` > 0 detects rising crossings, < 0 falling. Uses a hysteresis
// arm at half-threshold to avoid re-triggering on noise (port of the SiPM
// get_transitions logic). Assumes time-ordered points.
// ---------------------------------------------------------------------------
[[nodiscard]] inline std::vector<double>
threshold_crossings(const TGraph& g, double threshold, double direction = 1.0)
{
    std::vector<double> crossings;
    bool armed = false;
    for (int i = 0; i < g.GetN(); ++i) {
        const double y = g.GetPointY(i);
        if (!armed && direction * y > direction * threshold * 0.5) continue;
        armed = true;
        if (direction * y < direction * threshold) continue;
        crossings.push_back(g.GetPointX(i));
        armed = false;
    }
    return crossings;
}

// ---------------------------------------------------------------------------
// first_above / last_above: the first / last {x, y} where the signal exceeds
// `threshold` (or falls below it, if negative = true). {NaN, NaN} if none.
// ---------------------------------------------------------------------------
[[nodiscard]] inline std::pair<double, double>
first_above(const TGraph& g, double threshold, bool negative = false)
{
    const double sign = negative ? -1.0 : 1.0;
    for (int i = 0; i < g.GetN(); ++i)
        if (sign * g.GetPointY(i) > sign * threshold)
            return {g.GetPointX(i), g.GetPointY(i)};
    return {std::nan(""), std::nan("")};
}

[[nodiscard]] inline std::pair<double, double>
last_above(const TGraph& g, double threshold, bool negative = false)
{
    const double sign = negative ? -1.0 : 1.0;
    for (int i = g.GetN() - 1; i >= 0; --i)
        if (sign * g.GetPointY(i) > sign * threshold)
            return {g.GetPointX(i), g.GetPointY(i)};
    return {std::nan(""), std::nan("")};
}

// ---------------------------------------------------------------------------
// maximum_filter: non-overlapping blocks of `block_size` points, each
// reduced to the (x, y) of the block's maximum. Returns an owned graph.
// n == 0 returns an empty graph.
// ---------------------------------------------------------------------------
[[nodiscard]] inline owned::root_ptr<TGraph>
maximum_filter(const TGraph& g, std::size_t block_size)
{
    auto out = owned::make<TGraph>();
    if (block_size == 0) return out;
    const int n = g.GetN();
    const int step = static_cast<int>(block_size);
    for (int i = 0; i < n; i += step) {
        double best_x = g.GetPointX(i), best_y = g.GetPointY(i);
        for (int j = i + 1; j < std::min(i + step, n); ++j)
            if (g.GetPointY(j) > best_y) { best_y = g.GetPointY(j); best_x = g.GetPointX(j); }
        out->SetPoint(out->GetN(), best_x, best_y);
    }
    return out;
}

// ---------------------------------------------------------------------------
// gaussian_filter: sliding-window smoother with Gaussian weights, window of
// `window` points (advancing one at a time), standard deviation `sigma` (in
// points). Output length is N - window + 1. Unlike the SiPM original — which
// was labelled "gauss" but computed a plain mean — this applies real
// Gaussian weighting. window == 0 returns an empty graph.
// ---------------------------------------------------------------------------
[[nodiscard]] inline owned::root_ptr<TGraph>
gaussian_filter(const TGraph& g, std::size_t window, double sigma)
{
    auto out = owned::make<TGraph>();
    const int n = g.GetN();
    const int w = static_cast<int>(window);
    if (w == 0 || w > n || sigma <= 0.0) return out;

    const double centre = 0.5 * (w - 1);
    for (int i = 0; i + w <= n; ++i) {
        double sum_w = 0.0, sum_x = 0.0, sum_y = 0.0;
        for (int j = 0; j < w; ++j) {
            const double d = (j - centre) / sigma;
            const double weight = std::exp(-0.5 * d * d);
            sum_w += weight;
            sum_x += weight * g.GetPointX(i + j);
            sum_y += weight * g.GetPointY(i + j);
        }
        out->SetPoint(out->GetN(), sum_x / sum_w, sum_y / sum_w);
    }
    return out;
}

// ---------------------------------------------------------------------------
// integrate: sum of y^power over points whose x lies in [min_x, max_x]
// (min_x == max_x integrates the whole graph). With use_bin_width = true each
// term is multiplied by the (assumed uniform) sample spacing, giving a
// trapezoid-free Riemann estimate of the integral of y^power. A simple
// charge/energy proxy for waveforms.
// ---------------------------------------------------------------------------
[[nodiscard]] inline double
integrate(const TGraph& g, double min_x, double max_x,
          int power = 1, bool use_bin_width = false)
{
    if (g.GetN() < 2) return 0.0;
    const bool whole = (min_x == max_x);
    const double dx = use_bin_width ? (g.GetPointX(1) - g.GetPointX(0)) : 1.0;
    double integral = 0.0;
    for (int i = 0; i < g.GetN(); ++i) {
        const double x = g.GetPointX(i);
        if (!whole && (x < min_x || x > max_x)) continue;
        integral += dx * std::pow(g.GetPointY(i), power);
    }
    return integral;
}

// ---------------------------------------------------------------------------
// peak: a detected local maximum — its coordinates and point index.
// ---------------------------------------------------------------------------
struct peak {
    double x;
    double y;
    int index;
};

// ---------------------------------------------------------------------------
// find_peaks: local maxima whose rise above the neighbours `half_window`
// points away (on both sides) is at least `min_prominence`. After a hit the
// scan skips `half_window` points to avoid reporting the same peak twice.
// `negative = true` finds troughs. Pure: no canvas, no drawing (the SiPM
// original drew into a new TCanvas as a side-effect).
// ---------------------------------------------------------------------------
[[nodiscard]] inline std::vector<peak>
find_peaks(const TGraph& g, double min_prominence,
           int half_window = 10, bool negative = false)
{
    std::vector<peak> peaks;
    const int n = g.GetN();
    if (half_window < 1) half_window = 1;
    const double sign = negative ? -1.0 : 1.0;

    for (int i = 0; i < n; ++i) {
        const double y = sign * g.GetPointY(i);
        const int lo = std::max(0, i - half_window);
        const int hi = std::min(n - 1, i + half_window);
        const double y_lo = sign * g.GetPointY(lo);
        const double y_hi = sign * g.GetPointY(hi);
        if (y - y_lo < min_prominence) continue;
        if (y - y_hi < min_prominence) continue;
        peaks.push_back({g.GetPointX(i), g.GetPointY(i), i});
        i += half_window;  // don't re-report the same peak
    }
    return peaks;
}

}  // namespace mist::hep::signal
