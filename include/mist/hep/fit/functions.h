// SPDX-License-Identifier: MIT
//
// mist/hep/fit/functions.h — fit-function factories.
//
// Replaces AAU's pattern of "global TF1* fLevyTsallis; call fSetLevyTsallis()
// before use" with explicit factories that return owned TF1 instances.
// Callers decide lifetime; nothing leaks at TU scope.
//
// Each factory takes (name, x_min, x_max) and returns an owned TF1 handle
// (mist::hep::owned::root_ptr<TF1>). Parameters can be configured by the
// caller via the returned handle; sensible initial values + names are set on
// construction.
//
#pragma once

#include <TF1.h>

#include <mist/hep/owned.h>

namespace mist::hep::fit {

// ---------------------------------------------------------------------------
// Levy–Tsallis (Tsallis-Pareto) spectrum, the standard ALICE pT-spectrum
// parametrisation:
//
//   dN/dpT = pT * dN/dy * (n-1)(n-2) / (n*C * (n*C + m0*(n-2)))
//          * [1 + (mT - m0) / (n*C)] ^ (-n)
//
//   mT = sqrt(pT^2 + m0^2)
//
// Parameters (after construction):
//   0: dNdy   — yield normalisation     (default 1.0)
//   1: n      — power-law index         (default 7.0)
//   2: C      — slope parameter [GeV]   (default 0.25)
//   3: m0     — particle mass [GeV]     (default 0.13957, charged pion)
//
// The mass parameter is left as a free parameter by default so the caller can
// either FixParameter(3, m_particle) before fitting, or scan it.
// ---------------------------------------------------------------------------
mist::hep::owned::root_ptr<TF1> make_levy_tsallis(
    const char* name = "levy_tsallis",
    double x_min = 0.0,
    double x_max = 10.0);

// ---------------------------------------------------------------------------
// q-exponential (Tsallis) primitive — a reusable building block.
//
//   q_exp(x, q) = exp(x)                           if q == 1
//               = [1 + (1 - q) x]^(1/(1-q))        if that base is > 0
//               = 0                                 otherwise
// ---------------------------------------------------------------------------
[[nodiscard]] double q_exp(double x, double q);

// ---------------------------------------------------------------------------
// Gaussian:  norm * exp(-0.5 * ((x - mean)/sigma)^2)
//   0: norm    (default 1)
//   1: mean    (default 0.5*(x_min+x_max))
//   2: sigma   (default 0.1*(x_max-x_min))
// ---------------------------------------------------------------------------
mist::hep::owned::root_ptr<TF1> make_gauss(
    const char* name = "gauss", double x_min = 0.0, double x_max = 1.0);

// ---------------------------------------------------------------------------
// q-Gaussian (Tsallis) with a flat baseline:
//   f(x) = baseline + norm * q_exp(-(x-mean)^2 / (2 sigma^2), q)
//   0: norm  1: mean  2: sigma  3: q  4: baseline
//   defaults: norm=1, mean=mid, sigma=0.1*range, q=1.1, baseline=0
// ---------------------------------------------------------------------------
mist::hep::owned::root_ptr<TF1> make_qgauss(
    const char* name = "qgauss", double x_min = 0.0, double x_max = 1.0);

// ---------------------------------------------------------------------------
// Exponentially-modified Gaussian (EMG):
//   f(x) = norm * (lambda/2) * exp((lambda/2)*(2 mean + lambda sigma^2 - 2x))
//                * erfc((mean + lambda sigma^2 - x) / (sqrt(2) sigma))
//   0: mean  1: sigma  2: lambda  3: norm
// ---------------------------------------------------------------------------
mist::hep::owned::root_ptr<TF1> make_exgauss(
    const char* name = "exgauss", double x_min = 0.0, double x_max = 1.0);

// ---------------------------------------------------------------------------
// Error-function turn-on:
//   f(x) = baseline + height * 0.5 * (erf((x - x0)/width) + 1)
//   0: x0  1: width  2: height  3: baseline
// ---------------------------------------------------------------------------
mist::hep::owned::root_ptr<TF1> make_erf_step(
    const char* name = "erf_step", double x_min = 0.0, double x_max = 1.0);

// ---------------------------------------------------------------------------
// Exponential:  norm * exp(slope * x)
//   0: norm  1: slope
// ---------------------------------------------------------------------------
mist::hep::owned::root_ptr<TF1> make_expo(
    const char* name = "expo", double x_min = 0.0, double x_max = 1.0);

// ---------------------------------------------------------------------------
// Arrhenius activation form, linear in 1/T:
//   ln(y) = lnA - Ea / (k_B * T)   ->   f(T) = exp(lnA - Ea/(k_B T))
//   x is temperature T [K]; k_B = 8.617333e-5 eV/K.
//   0: lnA  1: Ea_eV
// ---------------------------------------------------------------------------
mist::hep::owned::root_ptr<TF1> make_arrhenius(
    const char* name = "arrhenius", double x_min = 200.0, double x_max = 350.0);

}  // namespace mist::hep::fit
