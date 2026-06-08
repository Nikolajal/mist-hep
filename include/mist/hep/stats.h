// SPDX-License-Identifier: MIT
//
// mist/hep/stats.h — small analysis-stats primitives.
//
// Header-only and ROOT-free. They live under mist::hep (not mist::)
// because the rest of mist-hep is the natural caller and we don't want
// to creep this kind of helper into the ROOT-free core.
//
// Ports / rewrites of:
//   - AAU SquareSum            -> quadrature_sum
//   - AAU uBarlowPar           -> barlow_parameter
//   - AAU fBarlowCheck         -> barlow_passes
//   - AAU uCleanOutsiders      -> clip_outliers_nsigma
//
#pragma once

#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <ranges>
#include <type_traits>
#include <vector>

namespace mist::hep::stats {

// ---------------------------------------------------------------------------
// quadrature_sum: sqrt(sum_i x_i^2).
//
// Generic over any input range of arithmetic values, plus a convenience
// initializer_list overload for callers that want to write the sum inline.
// ---------------------------------------------------------------------------
template <std::ranges::input_range R>
    requires std::is_arithmetic_v<std::ranges::range_value_t<R>>
[[nodiscard]] double quadrature_sum(R&& xs)
{
    double acc = 0.0;
    for (auto x : xs) acc += static_cast<double>(x) * static_cast<double>(x);
    return std::sqrt(acc);
}

[[nodiscard]] inline double quadrature_sum(std::initializer_list<double> xs)
{
    double acc = 0.0;
    for (auto x : xs) acc += x * x;
    return std::sqrt(acc);
}

// ---------------------------------------------------------------------------
// Barlow parameter (Barlow 2002, "Systematic errors: facts and fictions"):
//
//   t = (x_std - x_var) / sqrt(|sigma_std^2 - sigma_var^2|)
//
// Returns NaN when the two errors are identical (the formula is undefined
// there — the original AAU returned `false` cast to Double_t, which silently
// turned an "undefined" answer into a passing 0; NaN forces the caller to
// notice).
// ---------------------------------------------------------------------------
[[nodiscard]] inline double barlow_parameter(
    double x_std, double sigma_std,
    double x_var, double sigma_var)
{
    const double s2 = sigma_std * sigma_std;
    const double v2 = sigma_var * sigma_var;
    const double denom = std::sqrt(std::fabs(s2 - v2));
    if (denom == 0.0) return std::nan("");
    return (x_std - x_var) / denom;
}

// True when |t| <= threshold (default 1, i.e. the standard Barlow criterion).
[[nodiscard]] inline bool barlow_passes(
    double x_std, double sigma_std,
    double x_var, double sigma_var,
    double threshold = 1.0)
{
    const double t = barlow_parameter(x_std, sigma_std, x_var, sigma_var);
    if (std::isnan(t)) return true;  // identical errors — no information, don't flag
    return std::fabs(t) <= threshold;
}

// ---------------------------------------------------------------------------
// clip_outliers_nsigma: iterative N-sigma outlier rejection.
//
// Recomputes mean/std after each pass and removes any sample beyond
// n_sigma * std from the running mean. Terminates when a full pass removes
// nothing. Returns the number of samples dropped.
//
// Notes vs. the AAU original:
//   - returns count of removals so callers can detect "nothing to clean"
//   - guarded against empty / 1-element inputs (originally would divide by zero)
//   - std uses N (population), not N-1, matching the AAU behaviour
// ---------------------------------------------------------------------------
template <typename T>
    requires std::is_arithmetic_v<T>
std::size_t clip_outliers_nsigma(std::vector<T>& xs, double n_sigma = 10.0)
{
    std::size_t removed = 0;
    bool changed = true;
    while (changed) {
        changed = false;
        if (xs.size() < 2) return removed;

        double mean = 0.0;
        for (auto v : xs) mean += static_cast<double>(v);
        mean /= static_cast<double>(xs.size());

        double var = 0.0;
        for (auto v : xs) {
            const double d = static_cast<double>(v) - mean;
            var += d * d;
        }
        var /= static_cast<double>(xs.size());
        const double sd = std::sqrt(var);
        if (sd == 0.0) return removed;

        const double cut = n_sigma * sd;
        for (auto it = xs.begin(); it != xs.end();) {
            if (std::fabs(static_cast<double>(*it) - mean) >= cut) {
                it = xs.erase(it);
                ++removed;
                changed = true;
            } else {
                ++it;
            }
        }
    }
    return removed;
}

// ---------------------------------------------------------------------------
// weighted_mean_rms: reduce a set of {value, error} measurements to
// {mean, rms}. The values are first skimmed with clip_outliers_nsigma (the
// per-measurement errors are retained in the input but not used for weighting
// — an unweighted mean and the population RMS of the survivors are returned).
// Empty input, or everything clipped away, yields {NaN, NaN} (no div-by-zero).
// ---------------------------------------------------------------------------
[[nodiscard]] inline std::array<double, 2>
weighted_mean_rms(const std::vector<std::array<double, 2>>& measurements,
                  double clip_nsigma = 10.0)
{
    const double nan = std::nan("");
    std::vector<double> values;
    values.reserve(measurements.size());
    for (const auto& m : measurements) values.push_back(m[0]);

    clip_outliers_nsigma(values, clip_nsigma);
    if (values.empty()) return {nan, nan};

    double sum = 0.0;
    for (const double v : values) sum += v;
    const double mean = sum / static_cast<double>(values.size());

    double var = 0.0;
    for (const double v : values) {
        const double d = v - mean;
        var += d * d;
    }
    var /= static_cast<double>(values.size());
    return {mean, std::sqrt(var)};
}

}  // namespace mist::hep::stats
