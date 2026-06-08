// tester_graph.cxx — exercises mist::hep::graph transforms, binning, smoothing.
//
// Hand-computed expectations against small fixed TGraphs.

#include "mist/hep/graph/transforms.h"
#include "mist/hep/graph/binning.h"
#include "mist/hep/graph/smoothing.h"
#include "mist/hep/graph/algebra.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <TF1.h>
#include <TGraph.h>
#include <TGraphErrors.h>
#include <TProfile.h>

namespace g = mist::hep::graph;

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
    // ------------------------------------------------------------------
    std::puts("[tester_graph] transforms");

    // Build a simple subject graph y = 2x over x = 1..4.
    TGraph subject;
    for (int i = 0; i < 4; ++i) subject.SetPoint(i, i + 1, 2.0 * (i + 1));

    // shift_x_to_origin: first x (1) -> 0.
    {
        auto out = g::shift_x_to_origin(subject);
        check(out->GetN() == 4, "shift: point count");
        check_close(out->GetPointX(0), 0.0, 1e-12, "shift: x[0]=0");
        check_close(out->GetPointX(3), 3.0, 1e-12, "shift: x[3]=3");
        check_close(out->GetPointY(0), 2.0, 1e-12, "shift: y unchanged");
    }

    // negate_y / negate_x / negate_xy.
    {
        auto ny = g::negate_y(subject);
        check_close(ny->GetPointY(0), -2.0, 1e-12, "negate_y: y");
        check_close(ny->GetPointX(0), 1.0, 1e-12, "negate_y: x kept");
        auto nx = g::negate_x(subject);
        check_close(nx->GetPointX(0), -1.0, 1e-12, "negate_x: x");
        auto nxy = g::negate_xy(subject);
        check_close(nxy->GetPointX(0), -1.0, 1e-12, "negate_xy: x");
        check_close(nxy->GetPointY(0), -2.0, 1e-12, "negate_xy: y");
    }

    // scale (TGraph).
    {
        auto out = g::scale(subject, 2.0, 0.5);
        check_close(out->GetPointX(0), 2.0, 1e-12, "scale: x*2");
        check_close(out->GetPointY(0), 1.0, 1e-12, "scale: y*0.5");
        // Source must be untouched (BLU original mutated it in place).
        check_close(subject.GetPointX(0), 1.0, 1e-12, "scale: source intact");
    }

    // scale (TGraphErrors) scales errors by |factor|.
    {
        TGraphErrors ge;
        ge.SetPoint(0, 2.0, 4.0);
        ge.SetPointError(0, 0.2, 0.4);
        auto out = g::scale(ge, -3.0, 2.0);
        check_close(out->GetPointX(0), -6.0, 1e-12, "scaleE: x*-3");
        check_close(out->GetPointY(0), 8.0, 1e-12, "scaleE: y*2");
        check_close(out->GetErrorX(0), 0.6, 1e-12, "scaleE: ex*|-3|");
        check_close(out->GetErrorY(0), 0.8, 1e-12, "scaleE: ey*2");
    }

    // swap_xy: x and y exchanged, source untouched.
    {
        auto out = g::swap_xy(subject);
        check(out->GetN() == 4, "swap_xy: point count");
        check_close(out->GetPointX(0), 2.0, 1e-12, "swap_xy: x <- old y");
        check_close(out->GetPointY(0), 1.0, 1e-12, "swap_xy: y <- old x");
        check_close(subject.GetPointX(0), 1.0, 1e-12, "swap_xy: source intact");
    }

    // to_graph_errors: zero errors, points preserved.
    {
        auto out = g::to_graph_errors(subject);
        check(out->GetN() == 4, "to_graph_errors: count");
        check_close(out->GetErrorY(0), 0.0, 1e-12, "to_graph_errors: zero ey");
        check_close(out->GetPointY(3), 8.0, 1e-12, "to_graph_errors: y kept");
    }

    // relative_diff: subject vs reference y_ref = x (so (2x - x)/x = 1 everywhere).
    {
        TGraph reference;
        for (int i = 0; i < 5; ++i) reference.SetPoint(i, i, static_cast<double>(i));
        // reference spans x=0..4; subject x=1..4 all in range.
        auto out = g::relative_diff(subject, reference);
        check(out->GetN() == 4, "relative_diff: all in range");
        for (int i = 0; i < out->GetN(); ++i)
            check_close(out->GetPointY(i), 1.0, 1e-9, "relative_diff: (2x-x)/x = 1");
    }

    // relative_diff: zero reference value -> NaN guard.
    {
        TGraph zero_ref;
        zero_ref.SetPoint(0, 0.0, 0.0);
        zero_ref.SetPoint(1, 10.0, 0.0);  // ref == 0 across the range
        TGraph subj;
        subj.SetPoint(0, 5.0, 3.0);
        auto out = g::relative_diff(subj, zero_ref);
        check(out->GetN() == 1, "relative_diff: point emitted");
        check(std::isnan(out->GetPointY(0)), "relative_diff: zero ref -> NaN");
    }

    // relative_diff: out-of-range skip vs nan policy.
    {
        TGraph reference;
        reference.SetPoint(0, 0.0, 1.0);
        reference.SetPoint(1, 2.0, 1.0);   // range [0, 2]
        TGraph subj;
        subj.SetPoint(0, 1.0, 2.0);        // in range
        subj.SetPoint(1, 9.0, 2.0);        // out of range
        auto skipped = g::relative_diff(subj, reference, g::out_of_range::skip);
        check(skipped->GetN() == 1, "relative_diff skip: drops out-of-range");
        auto nanned = g::relative_diff(subj, reference, g::out_of_range::nan);
        check(nanned->GetN() == 2, "relative_diff nan: keeps out-of-range");
        check(std::isnan(nanned->GetPointY(1)), "relative_diff nan: NaN y");
    }

    // ------------------------------------------------------------------
    std::puts("[tester_graph] binning");

    // 4 points (x=1..4, y=2..8 step 2); block_size=2 -> 2 blocks.
    // block 0: x=(1,2)->1.5, y=(2,4)->3 ; block 1: x=(3,4)->3.5, y=(6,8)->7.
    {
        auto out = g::block_average(subject, 2);
        check(out->GetN() == 2, "block_average: 2 blocks");
        check_close(out->GetPointX(0), 1.5, 1e-12, "block_average: x0");
        check_close(out->GetPointY(0), 3.0, 1e-12, "block_average: y0");
        check_close(out->GetPointX(1), 3.5, 1e-12, "block_average: x1");
        check_close(out->GetPointY(1), 7.0, 1e-12, "block_average: y1");
    }

    // block_rms: x = block mean, y = population stdev of block y-values.
    // block 0 y=(2,4): mean 3, dev +-1, rms 1. block 1 y=(6,8): rms 1.
    {
        auto out = g::block_rms(subject, 2);
        check(out->GetN() == 2, "block_rms: 2 blocks");
        check_close(out->GetPointX(0), 1.5, 1e-12, "block_rms: x = block mean");
        check_close(out->GetPointY(0), 1.0, 1e-12, "block_rms: y0 rms");
        check_close(out->GetPointY(1), 1.0, 1e-12, "block_rms: y1 rms");
    }

    // ------------------------------------------------------------------
    std::puts("[tester_graph] smoothing");

    // moving_average window=2 over 4 points -> 3 outputs.
    // x: (1,2)->1.5, (2,3)->2.5, (3,4)->3.5 ; y: 3, 5, 7.
    {
        auto out = g::moving_average(subject, 2);
        check(out->GetN() == 3, "moving_average: N-w+1 outputs");
        check_close(out->GetPointX(0), 1.5, 1e-12, "moving_average: x0");
        check_close(out->GetPointY(0), 3.0, 1e-12, "moving_average: y0");
        check_close(out->GetPointY(2), 7.0, 1e-12, "moving_average: y2");
    }

    // ------------------------------------------------------------------
    std::puts("[tester_graph] algebra (TGraphErrors)");

    // Build a, b with errors. a: y=2x, ey=0.2x ; b: y=x, ey=0.1x over x=1..4.
    TGraphErrors a, b;
    for (int i = 0; i < 4; ++i) {
        const double x = i + 1;
        a.SetPoint(i, x, 2.0 * x);   a.SetPointError(i, 0.0, 0.2 * x);
        b.SetPoint(i, x, x);         b.SetPointError(i, 0.0, 0.1 * x);
    }

    // eval with errors: at x=2.5 (between points 2 and 3 of a: (2,4) and (3,6)).
    {
        const auto [y, ey] = g::eval(a, 2.5);
        check_close(y, 5.0, 1e-9, "eval: interpolated value");
        check(ey > 0.0, "eval: nonzero interpolated error");
        const auto [yo, eyo] = g::eval(a, 99.0);
        check(std::isnan(yo) && std::isnan(eyo), "eval: out-of-range -> NaN");
    }

    // add constant.
    {
        auto out = g::add(a, 1.0);
        check_close(out->GetPointY(0), 3.0, 1e-9, "add: y+1");
        check_close(out->GetErrorY(0), 0.2, 1e-9, "add: error unchanged (no addend err)");
    }

    // power: y^2 at point 0 (y=2) -> 4; ey' = 2*y*ey = 2*2*0.2 = 0.8.
    {
        auto out = g::power(a, 2.0);
        check_close(out->GetPointY(0), 4.0, 1e-9, "power: 2^2");
        check_close(out->GetErrorY(0), 0.8, 1e-9, "power: error 2*y*ey");
    }

    // scale_values by 3 (exact): y*3, ey*3.
    {
        auto out = g::scale_values(a, 3.0);
        check_close(out->GetPointY(0), 6.0, 1e-9, "scale_values: y*3");
        check_close(out->GetErrorY(0), 0.6, 1e-9, "scale_values: ey*3");
    }

    // log10 error fix: at y=10 (construct), ey'=ey/(y*ln10).
    {
        TGraphErrors g10; g10.SetPoint(0, 1.0, 10.0); g10.SetPointError(0, 0.0, 2.0);
        auto out = g::log10(g10);
        check_close(out->GetPointY(0), 1.0, 1e-12, "log10: log10(10)=1");
        check_close(out->GetErrorY(0), 2.0 / (10.0 * std::log(10.0)), 1e-12,
                    "log10: error ey/(y ln10)");
    }

    // ratio a/b = 2 everywhere; rel error = sqrt(0.1^2+0.1^2) at matching x.
    {
        auto out = g::ratio(a, b);
        check(out->GetN() == 4, "ratio: all points");
        for (int i = 0; i < out->GetN(); ++i)
            check_close(out->GetPointY(i), 2.0, 1e-9, "ratio: a/b = 2");
    }

    // product and difference at a matching grid point (x=2: a=4±0.4, b=2±0.2).
    {
        auto p = g::product(a, b);
        check_close(p->GetPointY(1), 8.0, 1e-9, "product: 4*2 = 8");
        auto d = g::difference(a, b);
        check_close(d->GetPointY(1), 2.0, 1e-9, "difference: 4-2 = 2");
        check_close(d->GetErrorY(1), std::sqrt(0.4 * 0.4 + 0.2 * 0.2), 1e-9,
                    "difference: quadrature error");
    }

    // difference vs a function f(x) = x.
    {
        TF1 f("om_line", "x", 0, 10);
        auto d = g::difference(a, f);
        check_close(d->GetPointY(0), 1.0, 1e-9, "difference(TF1): 2-1");
        check_close(d->GetErrorY(0), 0.2, 1e-9, "difference(TF1): error carried");
    }

    // trim to [2, 3].
    {
        auto out = g::trim(a, 2.0, 3.0);
        check(out->GetN() == 2, "trim: keeps in-range points");
        check_close(out->GetPointX(0), 2.0, 1e-9, "trim: first kept x");
    }

    // mean_of two identical graphs -> same values, zero spread.
    {
        std::vector<TGraphErrors*> gs = {&a, &a};
        auto m = g::mean_of(gs);
        check(m->GetN() == 4, "mean_of: point count");
        check_close(m->GetPointY(0), 2.0, 1e-9, "mean_of: mean of identical");
        check_close(m->GetErrorY(0), 0.0, 1e-9, "mean_of: zero spread");
    }

    // swap_xy on TGraphErrors.
    {
        auto out = g::swap_xy(a);
        check_close(out->GetPointX(0), 2.0, 1e-9, "swap_xy(E): x <- old y");
        check_close(out->GetPointY(0), 1.0, 1e-9, "swap_xy(E): y <- old x");
    }

    // eval_with_errors: array form of eval.
    {
        const auto r = g::eval_with_errors(a, 2.5);
        check_close(r[0], 5.0, 1e-9, "eval_with_errors: value");
        check(r[1] > 0.0, "eval_with_errors: error");
    }

    // offset: {value, error} pair form of add.
    {
        auto out = g::offset(a, {1.0, 0.0});
        check_close(out->GetPointY(0), 3.0, 1e-9, "offset: y+1");
        check_close(out->GetErrorY(0), 0.2, 1e-9, "offset: error unchanged");
    }

    // average_rms: bin scattered points, report mean + RMS (spread) per bin.
    {
        TGraphErrors scatter;
        scatter.SetPoint(0, 0.5, 10.0);
        scatter.SetPoint(1, 0.5, 20.0);   // bin 1: mean 15, spread 5
        scatter.SetPoint(2, 1.5, 4.0);
        scatter.SetPoint(3, 1.5, 6.0);    // bin 2: mean 5, spread 1
        auto ar = g::average_rms(scatter, 2, 0.0, 2.0);
        check(ar->GetN() == 2, "average_rms: 2 populated bins");
        check_close(ar->GetPointY(0), 15.0, 1e-9, "average_rms: bin1 mean");
        check_close(ar->GetErrorY(0), 5.0, 1e-9, "average_rms: bin1 RMS spread");
        check_close(ar->GetPointY(1), 5.0, 1e-9, "average_rms: bin2 mean");
        check_close(ar->GetErrorY(1), 1.0, 1e-9, "average_rms: bin2 RMS spread");
    }

    // ------------------------------------------------------------------
    std::puts("[tester_graph] derivative / from_profile");

    // derivative of y=2x is the constant 2; midpoints between x=1..4.
    {
        auto d = g::derivative(subject);
        check(d->GetN() == 3, "derivative: N-1 points");
        for (int i = 0; i < d->GetN(); ++i)
            check_close(d->GetPointY(i), 2.0, 1e-9, "derivative: slope of 2x is 2");
        check_close(d->GetPointX(0), 1.5, 1e-9, "derivative: midpoint x");
    }

    // from_profile: fill a profile, convert, check point count and a value.
    {
        TProfile p("om_p", "om_p", 4, 0, 4);
        p.Fill(0.5, 10.0);
        p.Fill(0.5, 20.0);   // bin 1 mean = 15
        p.Fill(2.5, 7.0);    // bin 3 mean = 7
        auto gp = g::from_profile(p);
        check(gp->GetN() == 4, "from_profile: one point per bin");
        check_close(gp->GetPointY(0), 15.0, 1e-9, "from_profile: bin-1 mean");
        check_close(gp->GetPointX(0), 0.5, 1e-9, "from_profile: bin-1 centre");
    }

    // ------------------------------------------------------------------
    if (failures) {
        std::printf("[tester_graph] %d failure(s)\n", failures);
        return EXIT_FAILURE;
    }
    std::puts("[tester_graph] OK");
    return EXIT_SUCCESS;
}
