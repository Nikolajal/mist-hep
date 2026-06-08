// SPDX-License-Identifier: MIT
#include "mist/hep/fit/functions.h"

#include <cmath>

namespace mist::hep::fit {

namespace {

// Levy-Tsallis evaluated point-wise for TF1's lambda interface.
//   x[0] = pT
//   p[0] = dN/dy
//   p[1] = n
//   p[2] = C
//   p[3] = m0
double levy_tsallis_eval(double* x, double* p)
{
    const double pT   = x[0];
    const double dNdy = p[0];
    const double n    = p[1];
    const double C    = p[2];
    const double m0   = p[3];

    const double mT   = std::sqrt(pT * pT + m0 * m0);
    const double nC   = n * C;
    const double norm = dNdy * (n - 1.0) * (n - 2.0) / (nC * (nC + m0 * (n - 2.0)));

    return pT * norm * std::pow(1.0 + (mT - m0) / nC, -n);
}

// f(x) = norm * exp(-0.5 ((x-mean)/sigma)^2)  ; p = {norm, mean, sigma}
double gauss_eval(double* x, double* p)
{
    const double z = (x[0] - p[1]) / p[2];
    return p[0] * std::exp(-0.5 * z * z);
}

// q-Gaussian + baseline ; p = {norm, mean, sigma, q, baseline}
double qgauss_eval(double* x, double* p)
{
    const double dx = x[0] - p[1];
    const double arg = -(dx * dx) / (2.0 * p[2] * p[2]);
    return p[4] + p[0] * mist::hep::fit::q_exp(arg, p[3]);
}

// Exponentially-modified Gaussian ; p = {mean, sigma, lambda, norm}
double exgauss_eval(double* x, double* p)
{
    const double mean = p[0], sigma = p[1], lambda = p[2], norm = p[3];
    const double a = 0.5 * lambda * (2.0 * mean + lambda * sigma * sigma - 2.0 * x[0]);
    const double b = (mean + lambda * sigma * sigma - x[0]) / (std::sqrt(2.0) * sigma);
    return norm * 0.5 * lambda * std::exp(a) * std::erfc(b);
}

// Error-function turn-on ; p = {x0, width, height, baseline}
double erf_step_eval(double* x, double* p)
{
    return p[3] + p[2] * 0.5 * (std::erf((x[0] - p[0]) / p[1]) + 1.0);
}

// Exponential ; p = {norm, slope}
double expo_eval(double* x, double* p)
{
    return p[0] * std::exp(p[1] * x[0]);
}

// Arrhenius ; p = {lnA, Ea_eV} ; x = T [K]
double arrhenius_eval(double* x, double* p)
{
    constexpr double k_B = 8.617333e-5; // eV/K
    return std::exp(p[0] - p[1] / (k_B * x[0]));
}

}  // namespace

mist::hep::owned::root_ptr<TF1> make_levy_tsallis(const char* name, double x_min, double x_max)
{
    auto f = mist::hep::owned::make<TF1>(name, &levy_tsallis_eval, x_min, x_max, /*npar=*/4);
    f->SetParName(0, "dNdy");
    f->SetParName(1, "n");
    f->SetParName(2, "C");
    f->SetParName(3, "m0");
    f->SetParameters(1.0, 7.0, 0.25, 0.13957);
    // n must stay > 2 for the normalisation to be finite.
    f->SetParLimits(1, 2.001, 50.0);
    f->SetParLimits(2, 1e-4,  10.0);
    return f;
}

double q_exp(double x, double q)
{
    if (q == 1.0)
        return std::exp(x);
    const double base = 1.0 + (1.0 - q) * x;
    return base > 0.0 ? std::pow(base, 1.0 / (1.0 - q)) : 0.0;
}

mist::hep::owned::root_ptr<TF1> make_gauss(const char* name, double x_min, double x_max)
{
    auto f = mist::hep::owned::make<TF1>(name, &gauss_eval, x_min, x_max, /*npar=*/3);
    f->SetParName(0, "norm");
    f->SetParName(1, "mean");
    f->SetParName(2, "sigma");
    f->SetParameters(1.0, 0.5 * (x_min + x_max), 0.1 * (x_max - x_min));
    return f;
}

mist::hep::owned::root_ptr<TF1> make_qgauss(const char* name, double x_min, double x_max)
{
    auto f = mist::hep::owned::make<TF1>(name, &qgauss_eval, x_min, x_max, /*npar=*/5);
    f->SetParName(0, "norm");
    f->SetParName(1, "mean");
    f->SetParName(2, "sigma");
    f->SetParName(3, "q");
    f->SetParName(4, "baseline");
    f->SetParameters(1.0, 0.5 * (x_min + x_max), 0.1 * (x_max - x_min), 1.1, 0.0);
    return f;
}

mist::hep::owned::root_ptr<TF1> make_exgauss(const char* name, double x_min, double x_max)
{
    auto f = mist::hep::owned::make<TF1>(name, &exgauss_eval, x_min, x_max, /*npar=*/4);
    f->SetParName(0, "mean");
    f->SetParName(1, "sigma");
    f->SetParName(2, "lambda");
    f->SetParName(3, "norm");
    f->SetParameters(0.5 * (x_min + x_max), 0.1 * (x_max - x_min), 1.0, 1.0);
    return f;
}

mist::hep::owned::root_ptr<TF1> make_erf_step(const char* name, double x_min, double x_max)
{
    auto f = mist::hep::owned::make<TF1>(name, &erf_step_eval, x_min, x_max, /*npar=*/4);
    f->SetParName(0, "x0");
    f->SetParName(1, "width");
    f->SetParName(2, "height");
    f->SetParName(3, "baseline");
    f->SetParameters(0.5 * (x_min + x_max), 0.1 * (x_max - x_min), 1.0, 0.0);
    return f;
}

mist::hep::owned::root_ptr<TF1> make_expo(const char* name, double x_min, double x_max)
{
    auto f = mist::hep::owned::make<TF1>(name, &expo_eval, x_min, x_max, /*npar=*/2);
    f->SetParName(0, "norm");
    f->SetParName(1, "slope");
    f->SetParameters(1.0, -1.0);
    return f;
}

mist::hep::owned::root_ptr<TF1> make_arrhenius(const char* name, double x_min, double x_max)
{
    auto f = mist::hep::owned::make<TF1>(name, &arrhenius_eval, x_min, x_max, /*npar=*/2);
    f->SetParName(0, "lnA");
    f->SetParName(1, "Ea_eV");
    f->SetParameters(0.0, 0.1);
    return f;
}

}  // namespace mist::hep::fit
