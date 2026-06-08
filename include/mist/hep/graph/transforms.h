// SPDX-License-Identifier: MIT
//
// mist/hep/graph/transforms.h — point-wise TGraph transforms.
//
// Header-only. Each function takes a const reference and returns an owning
// mist::hep::owned::root_ptr — callers decide lifetime, nothing leaks at TU
// scope, and returned histograms/graphs carry ROOT-correct ownership.
//
// Ports / rewrites of:
//   - graphutils `diff`     -> relative_diff   (F-11)
//   - graphutils `fromZero` -> shift_x_to_origin (F-12)
//   - graphutils `invertY`  -> negate_y         (F-13)
//   - BLU `uInvertX`/`uInvertXY` -> negate_x / negate_xy (F-18)
//   - BLU `uScale`          -> scale            (F-17)
//   - BLU `uMakeMeTGraphErrors` -> to_graph_errors (F-32)
//   - DHL `DHLgraph::Swap_xy` -> swap_xy         (F-34)
//
#pragma once

#include <cmath>

#include <TGraph.h>
#include <TGraphErrors.h>
#include <TProfile.h>

#include <mist/hep/owned.h>

namespace mist::hep::graph {

namespace owned = ::mist::hep::owned;

// ---------------------------------------------------------------------------
// Out-of-range policy for relative_diff: what to do with a subject point
// whose x lies outside the reference's x-range.
// ---------------------------------------------------------------------------
enum class out_of_range {
    skip,        ///< Drop the point (the graphutils-original behaviour).
    nan,         ///< Emit the point with a NaN y-value.
    extrapolate  ///< Use TGraph::Eval's linear extrapolation regardless.
};

// ---------------------------------------------------------------------------
// relative_diff: (y - y_ref) / y_ref, evaluated point-wise at the subject's
// x coordinates against a linearly-interpolated reference.
//
// Returns NaN for any point where the reference value is zero (the
// graphutils original divided silently, yielding +-inf).
//
// `reference` is assumed sorted ascending in x (its first/last points define
// the in-range interval).
// ---------------------------------------------------------------------------
[[nodiscard]] inline owned::root_ptr<TGraph>
relative_diff(const TGraph& subject,
              const TGraph& reference,
              out_of_range policy = out_of_range::skip)
{
    auto out = owned::make<TGraph>();
    const int n_ref = reference.GetN();
    if (n_ref == 0) return out;

    const double x_lo = reference.GetPointX(0);
    const double x_hi = reference.GetPointX(n_ref - 1);

    for (int i = 0; i < subject.GetN(); ++i) {
        const double x = subject.GetPointX(i);
        const double y = subject.GetPointY(i);
        const bool in_range = (x >= x_lo && x <= x_hi);

        if (!in_range && policy == out_of_range::skip) continue;

        double value;
        if (!in_range && policy == out_of_range::nan) {
            value = std::nan("");
        } else {
            const double ref = reference.Eval(x);
            value = (ref == 0.0) ? std::nan("") : (y - ref) / ref;
        }
        out->SetPoint(out->GetN(), x, value);
    }
    return out;
}

// ---------------------------------------------------------------------------
// shift_x_to_origin: translate the x-axis so the first point lands at x = 0.
// ---------------------------------------------------------------------------
[[nodiscard]] inline owned::root_ptr<TGraph>
shift_x_to_origin(const TGraph& source)
{
    auto out = owned::make<TGraph>();
    if (source.GetN() == 0) return out;
    const double x0 = source.GetPointX(0);
    for (int i = 0; i < source.GetN(); ++i)
        out->SetPoint(i, source.GetPointX(i) - x0, source.GetPointY(i));
    return out;
}

// ---------------------------------------------------------------------------
// negate_y / negate_x / negate_xy: copy with the chosen coordinate(s) negated.
// ---------------------------------------------------------------------------
[[nodiscard]] inline owned::root_ptr<TGraph>
negate_y(const TGraph& source)
{
    auto out = owned::make<TGraph>();
    for (int i = 0; i < source.GetN(); ++i)
        out->SetPoint(i, source.GetPointX(i), -source.GetPointY(i));
    return out;
}

[[nodiscard]] inline owned::root_ptr<TGraph>
negate_x(const TGraph& source)
{
    auto out = owned::make<TGraph>();
    for (int i = 0; i < source.GetN(); ++i)
        out->SetPoint(i, -source.GetPointX(i), source.GetPointY(i));
    return out;
}

[[nodiscard]] inline owned::root_ptr<TGraph>
negate_xy(const TGraph& source)
{
    auto out = owned::make<TGraph>();
    for (int i = 0; i < source.GetN(); ++i)
        out->SetPoint(i, -source.GetPointX(i), -source.GetPointY(i));
    return out;
}

// ---------------------------------------------------------------------------
// scale: multiply x by scale_x and y by scale_y, point-wise.
//
// The TGraphErrors overload also scales the error bars (by |scale_x| /
// |scale_y| so errors stay non-negative). The BLU original scaled the input
// graph in place by accident; this writes only the returned copy.
// ---------------------------------------------------------------------------
[[nodiscard]] inline owned::root_ptr<TGraph>
scale(const TGraph& source, double scale_x, double scale_y)
{
    auto out = owned::make<TGraph>();
    for (int i = 0; i < source.GetN(); ++i)
        out->SetPoint(i, scale_x * source.GetPointX(i),
                         scale_y * source.GetPointY(i));
    return out;
}

[[nodiscard]] inline owned::root_ptr<TGraphErrors>
scale(const TGraphErrors& source, double scale_x, double scale_y)
{
    auto out = owned::make<TGraphErrors>();
    for (int i = 0; i < source.GetN(); ++i) {
        out->SetPoint(i, scale_x * source.GetPointX(i),
                         scale_y * source.GetPointY(i));
        out->SetPointError(i, std::fabs(scale_x) * source.GetErrorX(i),
                              std::fabs(scale_y) * source.GetErrorY(i));
    }
    return out;
}

// ---------------------------------------------------------------------------
// swap_xy: exchange the x and y coordinates of every point. The inverse of
// reflecting across the diagonal; turns y = f(x) data into x = f(y).
//
// Rewrite of DHL's DHLgraph::Swap_xy. The DHL original was an in-place
// method on a TGraph subclass; this is the mist-hep idiom — a free function
// that returns an owning copy and leaves the source untouched.
// ---------------------------------------------------------------------------
[[nodiscard]] inline owned::root_ptr<TGraph>
swap_xy(const TGraph& source)
{
    auto out = owned::make<TGraph>();
    for (int i = 0; i < source.GetN(); ++i)
        out->SetPoint(i, source.GetPointY(i), source.GetPointX(i));
    return out;
}

// TGraphErrors overload: x/y values and their errors are exchanged together.
[[nodiscard]] inline owned::root_ptr<TGraphErrors>
swap_xy(const TGraphErrors& source)
{
    auto out = owned::make<TGraphErrors>();
    for (int i = 0; i < source.GetN(); ++i) {
        out->SetPoint(i, source.GetPointY(i), source.GetPointX(i));
        out->SetPointError(i, source.GetErrorY(i), source.GetErrorX(i));
    }
    return out;
}

// ---------------------------------------------------------------------------
// to_graph_errors: copy a TGraph into a TGraphErrors with zero error bars.
// Useful as a bridge before feeding a plain TGraph to error-aware code.
// ---------------------------------------------------------------------------
[[nodiscard]] inline owned::root_ptr<TGraphErrors>
to_graph_errors(const TGraph& source)
{
    auto out = owned::make<TGraphErrors>();
    for (int i = 0; i < source.GetN(); ++i) {
        out->SetPoint(i, source.GetPointX(i), source.GetPointY(i));
        out->SetPointError(i, 0.0, 0.0);
    }
    return out;
}

// ---------------------------------------------------------------------------
// from_profile: convert a TProfile to a TGraphErrors. Each filled bin becomes
// a point at (bin centre, mean) with x-error = half bin width and y-error =
// the profile's bin error. Salvaged from the SiPM TProfile_to_TGraphErrors
// (and fixed: the original dropped the last bin with a `< GetNbinsX()` loop).
// ---------------------------------------------------------------------------
[[nodiscard]] inline owned::root_ptr<TGraphErrors>
from_profile(const TProfile& source)
{
    auto out = owned::make<TGraphErrors>();
    for (int i = 1; i <= source.GetNbinsX(); ++i) {
        const int n = out->GetN();
        out->SetPoint(n, source.GetBinCenter(i), source.GetBinContent(i));
        out->SetPointError(n, source.GetBinWidth(i) * 0.5, source.GetBinError(i));
    }
    return out;
}

}  // namespace mist::hep::graph
