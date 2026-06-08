// SPDX-License-Identifier: MIT
//
// mist/hep/graph/algebra.h — error-propagating algebra on TGraphErrors.
//
// Binary and unary operations on TGraphErrors that carry the uncertainty
// through correctly. Header-only; every operation returns an owning
// mist::hep::owned::root_ptr and leaves its inputs untouched.
//
// Salvaged and corrected from the ePIC SiPM-characterisation graphutils
// (mist-hep:F-35..F-44). Notable fix: the original log10 propagated the
// error as ey/y (the natural-log rule); the correct factor is ey/(y*ln10),
// applied here.
//
// Where a second graph is combined with the first (ratio/product/
// difference), it is linearly interpolated at the first graph's x values via
// eval(); points outside the second graph's x-range are skipped. Inputs are
// assumed sorted ascending in x.
//
#pragma once

#include <array>
#include <cmath>
#include <utility>
#include <vector>

#include <TF1.h>
#include <TGraphErrors.h>
#include <TProfile.h>

#include <mist/hep/owned.h>

namespace mist::hep::graph {

namespace owned = ::mist::hep::owned;

// ---------------------------------------------------------------------------
// eval: linear interpolation of a TGraphErrors at x, WITH error propagation.
// Returns {value, error}. ROOT's TGraph::Eval gives no error; this does.
//
// Out of range (x below the first point or above the last) returns
// {NaN, NaN}. Assumes the graph is sorted ascending in x.
// ---------------------------------------------------------------------------
[[nodiscard]] inline std::pair<double, double>
eval(const TGraphErrors& g, double x)
{
    const int n = g.GetN();
    const double nan = std::nan("");
    if (n == 0) return {nan, nan};
    if (x < g.GetPointX(0) || x > g.GetPointX(n - 1)) return {nan, nan};

    for (int i = 1; i < n; ++i) {
        const double x0 = g.GetPointX(i - 1);
        const double x1 = g.GetPointX(i);
        if (x >= x0 && x <= x1) {
            if (x1 == x0) return {g.GetPointY(i), g.GetErrorY(i)};
            const double w1 = (x - x0) / (x1 - x0);
            const double w0 = (x1 - x) / (x1 - x0);
            const double y  = w1 * g.GetPointY(i) + w0 * g.GetPointY(i - 1);
            const double e1 = w1 * g.GetErrorY(i);
            const double e0 = w0 * g.GetErrorY(i - 1);
            return {y, std::sqrt(e0 * e0 + e1 * e1)};
        }
    }
    return {nan, nan};
}

// ---------------------------------------------------------------------------
// eval_with_errors: like eval(), but returns {value, error} as a std::array
// (the form some callers and the conventions doc expect).
// ---------------------------------------------------------------------------
[[nodiscard]] inline std::array<double, 2>
eval_with_errors(const TGraphErrors& g, double x_target)
{
    const auto [y, ey] = eval(g, x_target);
    return {y, ey};
}

// ---------------------------------------------------------------------------
// add: shift every y by a constant (with optional uncertainty on it).
//   y' = y + addend ;  ey' = sqrt(ey^2 + addend_error^2)
// ---------------------------------------------------------------------------
[[nodiscard]] inline owned::root_ptr<TGraphErrors>
add(const TGraphErrors& g, double addend, double addend_error = 0.0)
{
    auto out = owned::make<TGraphErrors>();
    for (int i = 0; i < g.GetN(); ++i) {
        const double ey = g.GetErrorY(i);
        out->SetPoint(i, g.GetPointX(i), g.GetPointY(i) + addend);
        out->SetPointError(i, g.GetErrorX(i),
                           std::sqrt(ey * ey + addend_error * addend_error));
    }
    return out;
}

// ---------------------------------------------------------------------------
// offset: shift every y by add[0] with uncertainty add[1] — the {value,error}
// pair form of add().
// ---------------------------------------------------------------------------
[[nodiscard]] inline owned::root_ptr<TGraphErrors>
offset(const TGraphErrors& g, std::array<double, 2> add_value)
{
    return add(g, add_value[0], add_value[1]);
}

// ---------------------------------------------------------------------------
// power: raise every y to an exponent.
//   y' = y^p ;  ey' = |p| * |y|^(p-1) * ey
// Points with y == 0 and p < 0 are skipped (the value would diverge).
// ---------------------------------------------------------------------------
[[nodiscard]] inline owned::root_ptr<TGraphErrors>
power(const TGraphErrors& g, double exponent)
{
    auto out = owned::make<TGraphErrors>();
    for (int i = 0; i < g.GetN(); ++i) {
        const double y = g.GetPointY(i);
        if (exponent < 0.0 && y == 0.0) continue;
        const double yp = std::pow(y, exponent);
        const double ey = (y == 0.0) ? 0.0
                          : std::fabs(exponent) * std::fabs(yp / y) * g.GetErrorY(i);
        const int n = out->GetN();
        out->SetPoint(n, g.GetPointX(i), yp);
        out->SetPointError(n, g.GetErrorX(i), ey);
    }
    return out;
}

// ---------------------------------------------------------------------------
// scale_values: multiply every y by a scalar factor, optionally with an
// uncertainty on the factor (relative errors added in quadrature). Distinct
// from graph::scale (which scales the x/y *axes*); this scales the values by
// a measured constant.
//   y' = factor * y
//   ey' = |y'| * sqrt((ey/y)^2 + (factor_error/factor)^2)   if applicable
//       = |factor| * ey                                      otherwise
// ---------------------------------------------------------------------------
[[nodiscard]] inline owned::root_ptr<TGraphErrors>
scale_values(const TGraphErrors& g, double factor, double factor_error = 0.0)
{
    auto out = owned::make<TGraphErrors>();
    for (int i = 0; i < g.GetN(); ++i) {
        const double y     = g.GetPointY(i);
        const double ey    = g.GetErrorY(i);
        const double y_new = factor * y;
        double ey_new;
        if (factor_error == 0.0 || y == 0.0 || factor == 0.0) {
            ey_new = std::fabs(factor) * ey;
        } else {
            const double rel_y = ey / y;
            const double rel_f = factor_error / factor;
            ey_new = std::fabs(y_new) * std::sqrt(rel_y * rel_y + rel_f * rel_f);
        }
        out->SetPoint(i, g.GetPointX(i), y_new);
        out->SetPointError(i, g.GetErrorX(i), ey_new);
    }
    return out;
}

// ---------------------------------------------------------------------------
// log / log10: natural and base-10 logarithm of every y (y > 0 required;
// non-positive points are skipped).
//   ln:    y' = ln(y)    ; ey' = ey / y
//   log10: y' = log10(y) ; ey' = ey / (y * ln 10)
// ---------------------------------------------------------------------------
[[nodiscard]] inline owned::root_ptr<TGraphErrors>
log(const TGraphErrors& g)
{
    auto out = owned::make<TGraphErrors>();
    for (int i = 0; i < g.GetN(); ++i) {
        const double y = g.GetPointY(i);
        if (y <= 0.0) continue;
        const int n = out->GetN();
        out->SetPoint(n, g.GetPointX(i), std::log(y));
        out->SetPointError(n, g.GetErrorX(i), g.GetErrorY(i) / y);
    }
    return out;
}

[[nodiscard]] inline owned::root_ptr<TGraphErrors>
log10(const TGraphErrors& g)
{
    static const double ln10 = std::log(10.0);
    auto out = owned::make<TGraphErrors>();
    for (int i = 0; i < g.GetN(); ++i) {
        const double y = g.GetPointY(i);
        if (y <= 0.0) continue;
        const int n = out->GetN();
        out->SetPoint(n, g.GetPointX(i), std::log10(y));
        out->SetPointError(n, g.GetErrorX(i), g.GetErrorY(i) / (y * ln10));
    }
    return out;
}

// ---------------------------------------------------------------------------
// ratio: numerator / denominator, the denominator interpolated at the
// numerator's x values. Points outside the denominator's range, or where
// either value is zero, are skipped.
//   r = yn / yd
//   er = |r| * sqrt((eyn/yn)^2 + (eyd/yd)^2)   if propagate_error
//      = eyn / yd                              otherwise
// ---------------------------------------------------------------------------
[[nodiscard]] inline owned::root_ptr<TGraphErrors>
ratio(const TGraphErrors& numerator, const TGraphErrors& denominator,
      bool propagate_error = true)
{
    auto out = owned::make<TGraphErrors>();
    for (int i = 0; i < numerator.GetN(); ++i) {
        const double x  = numerator.GetPointX(i);
        const double yn = numerator.GetPointY(i);
        const auto [yd, eyd] = eval(denominator, x);
        if (std::isnan(yd) || yd == 0.0 || yn == 0.0) continue;
        const double eyn = numerator.GetErrorY(i);
        const double r   = yn / yd;
        double er;
        if (propagate_error) {
            const double rel_n = eyn / yn;
            const double rel_d = eyd / yd;
            er = std::fabs(r) * std::sqrt(rel_n * rel_n + rel_d * rel_d);
        } else {
            er = eyn / yd;
        }
        const int n = out->GetN();
        out->SetPoint(n, x, r);
        out->SetPointError(n, numerator.GetErrorX(i), er);
    }
    return out;
}

// ---------------------------------------------------------------------------
// product: a * b, the second interpolated at the first's x values.
//   p = ya * yb ;  ep = |p| * sqrt((eya/ya)^2 + (eyb/yb)^2)
// ---------------------------------------------------------------------------
[[nodiscard]] inline owned::root_ptr<TGraphErrors>
product(const TGraphErrors& a, const TGraphErrors& b,
        bool propagate_error = true)
{
    auto out = owned::make<TGraphErrors>();
    for (int i = 0; i < a.GetN(); ++i) {
        const double x  = a.GetPointX(i);
        const double ya = a.GetPointY(i);
        const auto [yb, eyb] = eval(b, x);
        if (std::isnan(yb) || ya == 0.0 || yb == 0.0) continue;
        const double eya = a.GetErrorY(i);
        const double p   = ya * yb;
        double ep;
        if (propagate_error) {
            const double rel_a = eya / ya;
            const double rel_b = eyb / yb;
            ep = std::fabs(p) * std::sqrt(rel_a * rel_a + rel_b * rel_b);
        } else {
            ep = eya * yb;
        }
        const int n = out->GetN();
        out->SetPoint(n, x, p);
        out->SetPointError(n, a.GetErrorX(i), ep);
    }
    return out;
}

// ---------------------------------------------------------------------------
// difference: a - b, the second interpolated at the first's x values.
//   d = ya - yb ;  ed = sqrt(eya^2 + eyb^2)   if propagate_error, else eya
// ---------------------------------------------------------------------------
[[nodiscard]] inline owned::root_ptr<TGraphErrors>
difference(const TGraphErrors& a, const TGraphErrors& b,
           bool propagate_error = true)
{
    auto out = owned::make<TGraphErrors>();
    for (int i = 0; i < a.GetN(); ++i) {
        const double x  = a.GetPointX(i);
        const auto [yb, eyb] = eval(b, x);
        if (std::isnan(yb)) continue;
        const double eya = a.GetErrorY(i);
        const double ed  = propagate_error ? std::sqrt(eya * eya + eyb * eyb) : eya;
        const int n = out->GetN();
        out->SetPoint(n, x, a.GetPointY(i) - yb);
        out->SetPointError(n, a.GetErrorX(i), ed);
    }
    return out;
}

// ---------------------------------------------------------------------------
// difference (vs a function): subtract f(x) from every y. The function is
// treated as exact, so the y-errors are carried through unchanged.
// ---------------------------------------------------------------------------
[[nodiscard]] inline owned::root_ptr<TGraphErrors>
difference(const TGraphErrors& g, const TF1& f)
{
    auto out = owned::make<TGraphErrors>();
    // TF1::Eval is non-const in older ROOT; take a mutable copy of the pointer.
    auto& fn = const_cast<TF1&>(f);
    for (int i = 0; i < g.GetN(); ++i) {
        const double x = g.GetPointX(i);
        out->SetPoint(i, x, g.GetPointY(i) - fn.Eval(x));
        out->SetPointError(i, g.GetErrorX(i), g.GetErrorY(i));
    }
    return out;
}

// ---------------------------------------------------------------------------
// trim: copy keeping only the points whose x lies in [min_x, max_x].
// (Non-mutating rewrite of the SiPM remove_points, which edited in place.)
// ---------------------------------------------------------------------------
[[nodiscard]] inline owned::root_ptr<TGraphErrors>
trim(const TGraphErrors& g, double min_x, double max_x)
{
    auto out = owned::make<TGraphErrors>();
    for (int i = 0; i < g.GetN(); ++i) {
        const double x = g.GetPointX(i);
        if (x < min_x || x > max_x) continue;
        const int n = out->GetN();
        out->SetPoint(n, x, g.GetPointY(i));
        out->SetPointError(n, g.GetErrorX(i), g.GetErrorY(i));
    }
    return out;
}

// ---------------------------------------------------------------------------
// mean_of: point-wise mean across several graphs that share a point layout.
// Point i of the result is the mean of point i over all input graphs, with
// the error set to the standard error of the mean (population stdev / sqrt N).
// Uses the shortest input graph's length and the first graph's x values.
// ---------------------------------------------------------------------------
[[nodiscard]] inline owned::root_ptr<TGraphErrors>
mean_of(const std::vector<TGraphErrors*>& graphs)
{
    auto out = owned::make<TGraphErrors>();
    if (graphs.empty() || !graphs.front()) return out;

    int n_points = graphs.front()->GetN();
    for (auto* g : graphs) {
        if (!g) return out;
        n_points = std::min(n_points, g->GetN());
    }
    const double count = static_cast<double>(graphs.size());

    for (int i = 0; i < n_points; ++i) {
        double sum = 0.0;
        for (auto* g : graphs) sum += g->GetPointY(i);
        const double mean = sum / count;

        double var = 0.0;
        for (auto* g : graphs) {
            const double d = g->GetPointY(i) - mean;
            var += d * d;
        }
        var /= count;
        const double sem = std::sqrt(var / count);  // standard error of the mean

        out->SetPoint(i, graphs.front()->GetPointX(i), mean);
        out->SetPointError(i, 0.0, sem);
    }
    return out;
}

// ---------------------------------------------------------------------------
// average_rms: bin the scattered (x, y) points in x and report, per populated
// bin, the mean and RMS (spread) of y. Backed by a TProfile in "S" mode, so
// the bin error is the standard deviation of the points in the bin. Output:
// one point per populated bin at (bin centre, mean) with y-error = RMS.
//
// Out-of-range x lands in the TProfile under/overflow bins, which are not
// iterated here (only physical bins 1..N), so they are skipped naturally —
// no -1 bin is ever indexed.
// ---------------------------------------------------------------------------
[[nodiscard]] inline owned::root_ptr<TGraphErrors>
average_rms(const TGraphErrors& g, int n_bins, double x_min, double x_max)
{
    auto out = owned::make<TGraphErrors>();
    if (n_bins <= 0 || !(x_max > x_min)) return out;

    TProfile profile("", "", n_bins, x_min, x_max, "S");
    profile.SetDirectory(nullptr); // do not let gDirectory own this scratch object
    for (int i = 0; i < g.GetN(); ++i)
        profile.Fill(g.GetPointX(i), g.GetPointY(i));

    for (int b = 1; b <= profile.GetNbinsX(); ++b)
    {
        if (profile.GetBinEntries(b) <= 0.0) continue; // skip empty bins
        const int n = out->GetN();
        out->SetPoint(n, profile.GetBinCenter(b), profile.GetBinContent(b));
        out->SetPointError(n, 0.0, profile.GetBinError(b)); // "S" -> RMS spread
    }
    return out;
}

// ---------------------------------------------------------------------------
// derivative: discrete derivative of a graph (F-19). Each consecutive pair
// (x0,y0),(x1,y1) yields one point at the midpoint with slope
// `factor * (y1 - y0) / (x1 - x0)`. `factor` defaults to 1 (set -1 to negate,
// e.g. for inverse-log-derivative conventions). Pairs with x1 == x0 are
// skipped. Resolves the long-deferred central/forward-difference item; the
// SiPM-original error-bar variant was inconsistent, so this is the clean
// value-only TGraph form.
// ---------------------------------------------------------------------------
[[nodiscard]] inline owned::root_ptr<TGraph>
derivative(const TGraph& g, double factor = 1.0)
{
    auto out = owned::make<TGraph>();
    for (int i = 0; i + 1 < g.GetN(); ++i) {
        const double x0 = g.GetPointX(i);
        const double x1 = g.GetPointX(i + 1);
        if (x1 == x0) continue;
        const double slope = factor * (g.GetPointY(i + 1) - g.GetPointY(i)) / (x1 - x0);
        out->SetPoint(out->GetN(), 0.5 * (x0 + x1), slope);
    }
    return out;
}

}  // namespace mist::hep::graph
