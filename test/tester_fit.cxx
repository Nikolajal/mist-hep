// tester_fit.cxx — smoke-test the Levy-Tsallis factory.
//
// Confirms:
//   - the factory returns a usable TF1
//   - parameter names/defaults are wired up
//   - the function evaluates to a finite, positive value across the range
//   - sampling produces a strictly-positive integral

#include "mist/hep/fit/functions.h"
#include "mist/hep/globals.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

int failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        std::printf("  FAIL: %s\n", what);
        ++failures;
    }
}

}  // namespace

int main() {
    std::puts("[tester_fit] make_levy_tsallis");

    auto f = mist::hep::fit::make_levy_tsallis("lt", 0.0, 10.0);
    check(f != nullptr, "factory returns non-null");

    check(std::string(f->GetParName(0)) == "dNdy", "param 0 name");
    check(std::string(f->GetParName(1)) == "n",    "param 1 name");
    check(std::string(f->GetParName(2)) == "C",    "param 2 name");
    check(std::string(f->GetParName(3)) == "m0",   "param 3 name");

    // Defaults: dNdy=1, n=7, C=0.25, m0=0.13957 (charged pion)
    check(std::fabs(f->GetParameter(0) - 1.0)     < 1e-9, "dNdy default");
    check(std::fabs(f->GetParameter(1) - 7.0)     < 1e-9, "n default");
    check(std::fabs(f->GetParameter(2) - 0.25)    < 1e-9, "C default");
    check(std::fabs(f->GetParameter(3) - 0.13957) < 1e-6, "m0 default");

    // Evaluate across the range — every sample must be finite and >= 0.
    bool any_positive = false;
    for (double pT = 0.0; pT <= 10.0; pT += 0.1) {
        const double y = f->Eval(pT);
        if (!std::isfinite(y)) {
            std::printf("  FAIL: non-finite value at pT=%.3f\n", pT);
            ++failures;
            break;
        }
        if (y < 0.0) {
            std::printf("  FAIL: negative value at pT=%.3f (y=%.6g)\n", pT, y);
            ++failures;
            break;
        }
        if (y > 0.0) any_positive = true;
    }
    check(any_positive, "function is not identically zero");

    // Integral over the support should be strictly positive.
    const double I = f->Integral(0.0, 10.0);
    check(std::isfinite(I) && I > 0.0, "integral is finite and positive");
    std::printf("[tester_fit] integral over [0, 10] GeV = %.6g\n", I);

    // Touch the global singletons just to prove they link/construct cleanly.
    mist::hep::rng().SetSeed(42);
    const double r = mist::hep::rng().Uniform(0.0, 1.0);
    check(r >= 0.0 && r < 1.0, "rng() Uniform in [0,1)");

    // ------------------------------------------------------------------
    namespace fit = mist::hep::fit;
    auto name = [](const TF1& g, int i) { return std::string(g.GetParName(i)); };

    std::puts("[tester_fit] q_exp primitive");
    check(std::fabs(fit::q_exp(0.0, 1.1) - 1.0) < 1e-12, "q_exp(0,q) = 1");
    check(std::fabs(fit::q_exp(1.0, 1.0) - std::exp(1.0)) < 1e-12, "q_exp(x,1) = exp(x)");
    // q>1, large positive x -> base 1+(1-q)x goes negative -> clamped to 0.
    check(fit::q_exp(1e9, 1.1) == 0.0, "q_exp clamps non-positive base to 0");

    std::puts("[tester_fit] make_gauss");
    {
        auto g = fit::make_gauss("g", -5.0, 5.0);
        check(name(*g, 0) == "norm" && name(*g, 1) == "mean" && name(*g, 2) == "sigma",
              "gauss param names");
        check(std::fabs(g->GetParameter(1) - 0.0) < 1e-9, "gauss mean default = mid");
        check(std::fabs(g->GetParameter(2) - 1.0) < 1e-9, "gauss sigma default = 0.1*range");
        // peak at mean equals norm; symmetric.
        check(std::fabs(g->Eval(0.0) - 1.0) < 1e-9, "gauss peak = norm");
        check(std::fabs(g->Eval(1.0) - g->Eval(-1.0)) < 1e-9, "gauss symmetric");
    }

    std::puts("[tester_fit] make_qgauss");
    {
        auto g = fit::make_qgauss("qg", -5.0, 5.0);
        check(name(*g, 3) == "q" && name(*g, 4) == "baseline", "qgauss param names");
        check(std::fabs(g->GetParameter(3) - 1.1) < 1e-9, "qgauss q default 1.1");
        check(std::fabs(g->Eval(0.0) - (0.0 + 1.0)) < 1e-9, "qgauss peak = baseline+norm");
        check(std::isfinite(g->Eval(4.0)), "qgauss finite in tail");
    }

    std::puts("[tester_fit] make_exgauss / erf_step / expo / arrhenius");
    {
        auto eg = fit::make_exgauss("eg", -5.0, 10.0);
        check(name(*eg, 2) == "lambda" && name(*eg, 3) == "norm", "exgauss names");
        check(std::isfinite(eg->Eval(0.0)) && eg->Eval(0.0) >= 0.0, "exgauss finite, >=0");

        auto es = fit::make_erf_step("es", -5.0, 5.0);
        es->SetParameters(0.0, 1.0, 2.0, 0.5); // x0,width,height,baseline
        check(std::fabs(es->Eval(0.0) - (0.5 + 1.0)) < 1e-9, "erf_step midpoint = baseline+height/2");
        check(es->Eval(5.0) > es->Eval(-5.0), "erf_step rises");

        auto ex = fit::make_expo("ex", 0.0, 5.0);
        ex->SetParameters(2.0, 1.0); // norm, slope
        check(std::fabs(ex->Eval(0.0) - 2.0) < 1e-9, "expo at 0 = norm");
        check(std::fabs(ex->Eval(1.0) - 2.0 * std::exp(1.0)) < 1e-9, "expo grows as exp");

        auto ar = fit::make_arrhenius("ar", 200.0, 350.0);
        check(name(*ar, 0) == "lnA" && name(*ar, 1) == "Ea_eV", "arrhenius names");
        ar->SetParameters(0.0, 0.1);
        check(ar->Eval(300.0) > ar->Eval(250.0), "arrhenius rate rises with T");
    }

    if (failures) {
        std::printf("[tester_fit] %d failure(s)\n", failures);
        return EXIT_FAILURE;
    }
    std::puts("[tester_fit] OK");
    return EXIT_SUCCESS;
}
