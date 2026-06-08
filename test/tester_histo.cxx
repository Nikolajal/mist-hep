// tester_histo.cxx — exercises mist::hep::histo introspection + manipulation.

#include "mist/hep/histo/histo.h"
#include "mist/hep/histo/fill.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include <TF1.h>
#include <TGraph.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TH3F.h>
#include <TProfile.h>

namespace h = mist::hep::histo;

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
    // Suppress the [ERROR] lines the introspection negative-tests emit.
    const auto prev = mist::logger::get_min_level();
    mist::logger::set_min_level(mist::logger::LevelTag::Plain);

    // ------------------------------------------------------------------
    std::puts("[tester_histo] introspection");

    TH1F h1("h1", "h1", 10, 0, 10);
    TH2F h2("h2", "h2", 10, 0, 10, 5, 0, 5);
    TH3F h3("h3", "h3", 4, 0, 4, 4, 0, 4, 4, 0, 4);

    check(h::dimension(&h1) == 1, "dimension TH1 -> 1");
    check(h::dimension(&h2) == 2, "dimension TH2 -> 2");
    check(h::dimension(&h3) == 3, "dimension TH3 -> 3");
    check(h::dimension<TH1F>(nullptr) == -1, "dimension null -> -1");

    check(h::pair_dimension(&h1, &h1) == 1, "pair_dimension matching -> 1");
    check(h::pair_dimension(&h1, &h2) == -1, "pair_dimension mismatch -> -1");

    {
        TH1F a("a", "a", 10, 0, 10);
        TH1F b("b", "b", 10, 0, 10);
        TH1F c("c", "c", 20, 0, 10);
        check(h::is_consistent(&a, &b), "is_consistent same binning");
        check(!h::is_consistent(&a, &c), "is_consistent different binning");
    }

    // ------------------------------------------------------------------
    std::puts("[tester_histo] uniform_binning");

    {
        const auto edges = h::uniform_binning(1.0, 0.0, 5.0);
        check(edges.size() == 6, "uniform_binning: 6 edges for 5 bins");
        check_close(edges.front(), 0.0, 1e-12, "uniform_binning: first edge");
        check_close(edges.back(), 5.0, 1e-12, "uniform_binning: last edge");
        check_close(edges[2], 2.0, 1e-12, "uniform_binning: interior edge");
    }
    {
        // Partial last bin: high rounds up from 5 to 6 (width 2 over [0,5]).
        const auto edges = h::uniform_binning(2.0, 0.0, 5.0);
        check(edges.size() == 4, "uniform_binning: rounds up partial bin");
        check_close(edges.back(), 6.0, 1e-12, "uniform_binning: rounded high edge");
    }
    {
        const auto bad = h::uniform_binning(-1.0, 0.0, 5.0);
        check(bad.empty(), "uniform_binning: invalid width -> empty");
    }

    // ------------------------------------------------------------------
    std::puts("[tester_histo] log_binning");

    {
        // [0.1, 100] in 3 decades -> with 3 bins, edges at 0.1, 1, 10, 100.
        const auto edges = h::log_binning(3, 0.1, 100.0);
        check(edges.size() == 4, "log_binning: n+1 edges");
        check_close(edges[0], 0.1, 1e-12, "log_binning: low pinned");
        check_close(edges[1], 1.0, 1e-9, "log_binning: 1st decade");
        check_close(edges[2], 10.0, 1e-9, "log_binning: 2nd decade");
        check_close(edges[3], 100.0, 1e-12, "log_binning: high pinned");
        // Constant ratio between adjacent edges.
        check_close(edges[1] / edges[0], edges[2] / edges[1], 1e-9,
                    "log_binning: constant ratio");
    }
    {
        check(h::log_binning(0, 0.1, 10.0).empty(), "log_binning: n=0 -> empty");
        check(h::log_binning(5, 0.0, 10.0).empty(), "log_binning: low=0 -> empty");
        check(h::log_binning(5, -1.0, 10.0).empty(), "log_binning: low<0 -> empty");
        check(h::log_binning(5, 10.0, 1.0).empty(), "log_binning: high<low -> empty");
    }

    // ------------------------------------------------------------------
    std::puts("[tester_histo] make_th1_from_vector");

    {
        std::vector<double> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        auto built = h::make_th1_from_vector(data);
        check(built != nullptr, "make_th1: non-null");
        check_close(built->Integral(), 10.0, 1e-9, "make_th1: all entries filled");
    }
    {
        std::vector<int> data = {5, 5, 5};
        auto built = h::make_th1_from_vector(data, 4, 0.0, 0.0, 10.0);
        check(built->GetNbinsX() == 4, "make_th1: explicit n_bins respected");
        check_close(built->Integral(), 3.0, 1e-9, "make_th1: int vector filled");
    }
    {
        std::vector<double> empty;
        auto built = h::make_th1_from_vector(empty);
        check(built != nullptr && built->GetNbinsX() == 1, "make_th1: empty -> 1-bin sentinel");
    }

    // ------------------------------------------------------------------
    std::puts("[tester_histo] offset / absolute");

    {
        TH1F src("src", "src", 3, 0, 3);
        src.SetBinContent(1, 1.0);
        src.SetBinContent(2, -2.0);
        src.SetBinContent(3, 3.0);

        auto off = h::offset(&src, 1.0);
        check_close(off->GetBinContent(1), 2.0, 1e-12, "offset: 1+1");
        check_close(off->GetBinContent(2), -1.0, 1e-12, "offset: -2+1");
        // Source untouched.
        check_close(src.GetBinContent(2), -2.0, 1e-12, "offset: source intact");

        auto ab = h::absolute(&src);
        check_close(ab->GetBinContent(2), 2.0, 1e-12, "absolute: |-2|");
        check_close(ab->GetBinContent(3), 3.0, 1e-12, "absolute: |3|");
    }

    // ------------------------------------------------------------------
    std::puts("[tester_histo] scale");

    {
        TH1F src("s2", "s2", 2, 0, 2);
        src.SetBinContent(1, 4.0);  src.SetBinError(1, 0.4);
        src.SetBinContent(2, 10.0); src.SetBinError(2, 1.0);

        // Linear error scaling (no factor error).
        auto lin = h::scale(&src, 2.0);
        check_close(lin->GetBinContent(1), 8.0, 1e-12, "scale: content*2");
        check_close(lin->GetBinError(1), 0.8, 1e-12, "scale: error*|2|");

        // Quadrature error with a factor error.
        // content=4, e=0.4 (rel 0.1); factor=2, fe=0.2 (rel 0.1).
        // new content=8; rel = sqrt(0.1^2+0.1^2)=0.141421; e_new=8*that=1.13137.
        auto quad = h::scale(&src, 2.0, 0.2);
        check_close(quad->GetBinContent(1), 8.0, 1e-12, "scale(q): content*2");
        check_close(quad->GetBinError(1), 8.0 * std::sqrt(0.02), 1e-9, "scale(q): quadrature error");
    }

    // ------------------------------------------------------------------
    std::puts("[tester_histo] fill helpers");

    // project_y: histogram a graph's y-values.
    {
        TGraph src;
        for (int i = 0; i < 5; ++i) src.SetPoint(i, i, 2.0);  // all y = 2
        auto h2 = h::project_y(src, 10, 0, 10);
        check_close(h2->Integral(), 5.0, 1e-9, "project_y: 5 entries");
        check(h2->GetDirectory() == nullptr, "project_y: detached");
    }

    // fill_from_interval (graph): only y of points with x in [1,3].
    {
        TGraph src;
        for (int i = 0; i < 5; ++i) src.SetPoint(i, i, 1.0);  // x=0..4
        TH1F target("om_fi", "om_fi", 10, 0, 10);
        h::fill_from_interval(target, src, 1.0, 3.0);
        check_close(target.Integral(), 3.0, 1e-9, "fill_from_interval: x in [1,3] -> 3 entries");
    }

    // fill_profile: accumulate a graph into a profile.
    {
        TProfile prof("om_fp", "om_fp", 4, 0, 4);
        TGraph src;
        src.SetPoint(0, 0.5, 10.0);
        src.SetPoint(1, 0.5, 20.0);
        h::fill_profile(prof, src);
        check_close(prof.GetBinContent(1), 15.0, 1e-9, "fill_profile: bin mean");
    }

    // fill_persistence (function): sample f over x-bins into a TH2.
    {
        TH2F p2("om_pe", "om_pe", 4, 0, 4, 100, 0, 10);
        TF1 f("om_pf", "x", 0, 4);
        h::fill_persistence(p2, f);
        check(p2.GetEntries() == 4, "fill_persistence(TF1): one entry per x-bin");
    }

    // ------------------------------------------------------------------
    mist::logger::set_min_level(prev);

    if (failures) {
        std::printf("[tester_histo] %d failure(s)\n", failures);
        return EXIT_FAILURE;
    }
    std::puts("[tester_histo] OK");
    return EXIT_SUCCESS;
}
