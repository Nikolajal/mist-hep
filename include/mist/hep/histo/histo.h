// SPDX-License-Identifier: MIT
//
// mist/hep/histo/histo.h — histogram introspection and basic manipulation.
//
// Foundational subset of the BLU histogram engine port (mist-hep:F-20..F-27).
// Header-only. ROOT-typed; depends on mist::logger for diagnostics and
// mist::hep::stats for error propagation.
//
// The heavier BLU histogram items — rebin (F-24), randomize_points (F-28),
// the N-dimensional TFile loader (F-29), add_sum (F-30), transpose (F-31) —
// are deferred to a follow-up; see DISCUSSION.md. Each needs either careful
// re-derivation (the originals never compiled) or TFile test fixtures.
//
#pragma once

#include <cmath>
#include <string>
#include <type_traits>
#include <vector>

#include <TH1.h>
#include <TH2.h>
#include <TH3.h>
#include <TH1F.h>

#include <mist/logger/logger.h>
#include <mist/hep/owned.h>
#include <mist/hep/stats.h>

namespace mist::hep::histo {

namespace owned = ::mist::hep::owned;

// ===========================================================================
//  Introspection (F-20 / F-21 / F-22)
// ===========================================================================

// ---------------------------------------------------------------------------
// dimension: runtime axis count of a histogram (1, 2, or 3), or -1 on error
// (null, or not a histogram). Replaces BLU uGetTHDimension; routes the error
// path through mist::logger instead of raw cout.
// ---------------------------------------------------------------------------
template <typename TH>
[[nodiscard]] int dimension(const TH* h)
{
    if (!h) {
        mist::logger::error("(histo::dimension) target is null");
        return -1;
    }
    const auto* h1 = dynamic_cast<const TH1*>(h);
    if (!h1) {
        mist::logger::error("(histo::dimension) target is not a TH1-derived histogram");
        return -1;
    }
    if (dynamic_cast<const TH3*>(h)) return 3;
    if (dynamic_cast<const TH2*>(h)) return 2;
    return 1;
}

// ---------------------------------------------------------------------------
// pair_dimension: common dimension of two histograms, or -1 if they disagree
// or either is invalid.
// ---------------------------------------------------------------------------
template <typename TH1Type, typename TH2Type>
[[nodiscard]] int pair_dimension(const TH1Type* a, const TH2Type* b)
{
    const int da = dimension(a);
    const int db = dimension(b);
    if (da < 0 || db < 0) return -1;
    if (da != db) {
        mist::logger::error("(histo::pair_dimension) dimensions disagree");
        return -1;
    }
    return da;
}

// ---------------------------------------------------------------------------
// is_consistent: true when two histograms share the same dimension, the same
// per-axis bin counts, and the same per-bin low edges.
// ---------------------------------------------------------------------------
template <typename TH1Type, typename TH2Type>
[[nodiscard]] bool is_consistent(const TH1Type* a, const TH2Type* b)
{
    if (pair_dimension(a, b) < 0) return false;
    if (a->GetNbinsX() != b->GetNbinsX()) return false;
    if (a->GetNbinsY() != b->GetNbinsY()) return false;
    if (a->GetNbinsZ() != b->GetNbinsZ()) return false;
    for (int i = 1; i <= a->GetNbinsX(); ++i)
        if (a->GetXaxis()->GetBinLowEdge(i) != b->GetXaxis()->GetBinLowEdge(i))
            return false;
    for (int j = 1; j <= a->GetNbinsY(); ++j)
        if (a->GetYaxis()->GetBinLowEdge(j) != b->GetYaxis()->GetBinLowEdge(j))
            return false;
    for (int k = 1; k <= a->GetNbinsZ(); ++k)
        if (a->GetZaxis()->GetBinLowEdge(k) != b->GetZaxis()->GetBinLowEdge(k))
            return false;
    return true;
}

// ===========================================================================
//  Binning helpers (F-25)
// ===========================================================================

// ---------------------------------------------------------------------------
// uniform_binning: bin edges for [low, high] split into bins of width `width`.
// Returns the edge array as a std::vector<double> (the BLU original leaked a
// raw new[]). If `high` is not an integer number of widths above `low`, the
// last edge is rounded up and a warning is logged.
//
// ROOT-free arithmetic; lives in mist::hep only because its callers (TH1
// constructors) do. Could migrate to mist::algo if a ROOT-free caller appears.
// ---------------------------------------------------------------------------
[[nodiscard]] inline std::vector<double>
uniform_binning(double width, double low, double high)
{
    std::vector<double> edges;
    if (width <= 0.0 || high <= low) {
        mist::logger::error("(histo::uniform_binning) require width > 0 and high > low");
        return edges;
    }
    int n = static_cast<int>((high - low) / width);
    if ((high - low) - n * width > 0.0) ++n;  // round up a partial last bin
    edges.reserve(n + 1);
    for (int i = 0; i <= n; ++i) edges.push_back(low + i * width);
    if (edges.back() != high)
        mist::logger::warning("(histo::uniform_binning) high edge adjusted to fit width");
    return edges;
}

// ---------------------------------------------------------------------------
// log_binning: `n_bins + 1` logarithmically-spaced bin edges spanning
// [low, high], i.e. edges equally spaced in log10. Common for pT / energy
// spectra where a constant *ratio* between adjacent edges is wanted rather
// than a constant difference.
//
// Both bounds must be strictly positive (log is undefined at and below zero)
// and high > low; otherwise an empty vector is returned and an error logged.
//
// The result is suitable for the variable-binning TH1 constructor:
//   auto e = mist::hep::histo::log_binning(50, 0.1, 100.0);
//   TH1F h("h", "h", e.size() - 1, e.data());
// ---------------------------------------------------------------------------
[[nodiscard]] inline std::vector<double>
log_binning(int n_bins, double low, double high)
{
    std::vector<double> edges;
    if (n_bins <= 0 || low <= 0.0 || high <= low) {
        mist::logger::error("(histo::log_binning) require n_bins > 0 and 0 < low < high");
        return edges;
    }
    const double log_lo = std::log10(low);
    const double log_hi = std::log10(high);
    const double step = (log_hi - log_lo) / static_cast<double>(n_bins);
    edges.reserve(n_bins + 1);
    for (int i = 0; i <= n_bins; ++i)
        edges.push_back(std::pow(10.0, log_lo + i * step));
    // Pin the endpoints exactly — pow(10, log10(x)) can drift by an ULP.
    edges.front() = low;
    edges.back()  = high;
    return edges;
}

// ===========================================================================
//  Construction (F-23)
// ===========================================================================

namespace detail {
// Function-local counter for default unique names — one instance per process,
// no header-global. Not thread-safe; pass an explicit name from concurrent code.
inline int& build_counter() { static int c = 0; return c; }
}  // namespace detail

// ---------------------------------------------------------------------------
// make_th1_from_vector: build a 1-D histogram from a vector of values, with an
// automatic binning heuristic when n_bins <= 0. Replaces BLU uBuildTH1; fixes
// the duplicate `>= 1e3` branch that made the original always end at 216 bins.
//
// Returned via owned::root_ptr and born detached from gDirectory (owned::make):
// safe to own even if a TFile is open at call time — the BLU original and the
// pre-owned mist-hep version both risked a double-free there.
//
// Heuristic (n_bins <= 0):
//   default            -> 12 bins
//   size >= 1e2        -> size/3 + 2
//   size >= 1e3        -> size/5 + 2
//   size >= 1e4        -> 216 (cap)
// ---------------------------------------------------------------------------
template <typename TH1Type = TH1F, typename T>
    requires std::is_arithmetic_v<T>
[[nodiscard]] owned::root_ptr<TH1Type>
make_th1_from_vector(const std::vector<T>& data,
                     int n_bins = -1,
                     double offset = 0.0,
                     double low = 0.0,
                     double high = 0.0)
{
    const std::string name = "histo_from_vector_" +
                             std::to_string(detail::build_counter()++);

    if (data.empty())
        return owned::make<TH1Type>(name.c_str(), name.c_str(), 1, 0.0, 1.0);

    const auto max_it = *std::max_element(data.begin(), data.end());
    const auto min_it = *std::min_element(data.begin(), data.end());
    const double span = static_cast<double>(max_it) - static_cast<double>(min_it);

    if (low == high) {
        low  = static_cast<double>(min_it) - 0.2 * span + offset;
        high = static_cast<double>(max_it) + 0.2 * span + offset;
    }

    if (n_bins <= 0) {
        const std::size_t sz = data.size();
        n_bins = 12;
        if (sz >= static_cast<std::size_t>(1e2)) n_bins = static_cast<int>(sz / 3) + 2;
        if (sz >= static_cast<std::size_t>(1e3)) n_bins = static_cast<int>(sz / 5) + 2;
        if (sz >= static_cast<std::size_t>(1e4)) n_bins = 216;
    }

    auto h = owned::make<TH1Type>(name.c_str(), name.c_str(), n_bins, low, high);
    for (auto v : data) h->Fill(static_cast<double>(v) + offset);
    return h;
}

// ===========================================================================
//  Manipulation (F-26 / F-27)
// ===========================================================================

namespace detail {
// Clone a histogram into an owned, gDirectory-detached copy of the same
// concrete type. Delegates to owned::clone so the copy carries ROOT-correct
// ownership (the previous raw-Clone() form attached the copy to gDirectory).
template <typename TH>
[[nodiscard]] owned::root_ptr<TH> clone_as(const TH* h)
{
    return owned::clone(*h);
}
}  // namespace detail

// ---------------------------------------------------------------------------
// offset: add a constant to every bin. With absolute = true, stores the
// magnitude |content + value|. Operates on all cells (including under/overflow),
// matching ROOT's own Add/Scale convention. Replaces BLU uOffset.
// ---------------------------------------------------------------------------
template <typename TH>
[[nodiscard]] owned::root_ptr<TH>
offset(const TH* h, double value, bool absolute = false)
{
    auto out = detail::clone_as(h);
    if (!out) return out;
    const int n_cells = out->GetNcells();
    for (int i = 0; i < n_cells; ++i) {
        const double shifted = out->GetBinContent(i) + value;
        out->SetBinContent(i, absolute ? std::fabs(shifted) : shifted);
    }
    return out;
}

// ---------------------------------------------------------------------------
// absolute: store the magnitude of every bin content. Replaces BLU uAbsolute.
// ---------------------------------------------------------------------------
template <typename TH>
[[nodiscard]] owned::root_ptr<TH> absolute(const TH* h)
{
    return offset(h, 0.0, /*absolute=*/true);
}

// ---------------------------------------------------------------------------
// scale: multiply every bin content by `factor`. The error is propagated:
//   - factor_error == 0 (default): error scales linearly (error *= |factor|).
//   - factor_error  > 0: relative errors add in quadrature, i.e.
//       e_new = |content_new| * sqrt( (e/content)^2 + (factor_error/factor)^2 ).
//
// Replaces BLU uScale (TH). The BLU original carried -1/-2 magic sentinels
// and read GetBinContent(iBin) instead of the global bin in the 3-D loop;
// both are dropped here. Operates on all cells.
// ---------------------------------------------------------------------------
template <typename TH>
[[nodiscard]] owned::root_ptr<TH>
scale(const TH* h, double factor, double factor_error = 0.0)
{
    auto out = detail::clone_as(h);
    if (!out) return out;
    const int n_cells = out->GetNcells();
    for (int i = 0; i < n_cells; ++i) {
        const double content = out->GetBinContent(i);
        const double error   = out->GetBinError(i);
        const double scaled  = factor * content;
        out->SetBinContent(i, scaled);
        if (factor_error == 0.0 || content == 0.0 || factor == 0.0) {
            out->SetBinError(i, std::fabs(factor) * error);
        } else {
            const double rel = mist::hep::stats::quadrature_sum(
                {error / content, factor_error / factor});
            out->SetBinError(i, std::fabs(scaled) * rel);
        }
    }
    return out;
}

}  // namespace mist::hep::histo
