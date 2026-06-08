// SPDX-License-Identifier: MIT
//
// mist/hep/histo/fill.h — fill histograms from graphs / functions / profiles.
//
// project_y returns a new owned histogram; the fill_* helpers add entries to
// a histogram the caller already owns (they mutate the target and return
// void — no allocation, no ownership transfer).
//
// Salvaged from the ePIC SiPM-characterisation laser/waveform utilities
// (mist-hep:F-55, F-56).
//
#pragma once

#include <TF1.h>
#include <TGraph.h>
#include <TH1.h>
#include <TH1F.h>
#include <TH2.h>
#include <TProfile.h>

#include <mist/hep/owned.h>

namespace mist::hep::histo {

namespace owned = ::mist::hep::owned;

// ---------------------------------------------------------------------------
// project_y: histogram the y-values of a graph into a new 1-D histogram.
// Returns an owned, gDirectory-detached TH1F.
// ---------------------------------------------------------------------------
[[nodiscard]] inline owned::root_ptr<TH1F>
project_y(const TGraph& source, int n_bins, double min, double max,
          const char* name = "histo_project_y")
{
    auto h = owned::make<TH1F>(name, name, n_bins, min, max);
    for (int i = 0; i < source.GetN(); ++i) h->Fill(source.GetPointY(i));
    return h;
}

// ---------------------------------------------------------------------------
// fill_profile: accumulate a graph's (x, y) points into a TProfile (e.g. to
// build the mean curve of many overlaid graphs). Mutates `target`.
// ---------------------------------------------------------------------------
inline void fill_profile(TProfile& target, const TGraph& source)
{
    for (int i = 0; i < source.GetN(); ++i)
        target.Fill(source.GetPointX(i), source.GetPointY(i));
}

// ---------------------------------------------------------------------------
// fill_persistence: accumulate into a 2-D histogram (a "persistence" / heat
// map). From a graph's points, or by sampling a function at each x-bin centre.
// Mutates `target`.
// ---------------------------------------------------------------------------
inline void fill_persistence(TH2& target, const TGraph& source)
{
    for (int i = 0; i < source.GetN(); ++i)
        target.Fill(source.GetPointX(i), source.GetPointY(i));
}

inline void fill_persistence(TH2& target, const TF1& f)
{
    auto& fn = const_cast<TF1&>(f);  // TF1::Eval is non-const in older ROOT
    for (int i = 1; i <= target.GetNbinsX(); ++i) {
        const double x = target.GetXaxis()->GetBinCenter(i);
        target.Fill(x, fn.Eval(x));
    }
}

// ---------------------------------------------------------------------------
// fill_from_interval: histogram the y-values (graph) or bin contents
// (profile) whose x falls in [min, max] into a 1-D histogram. Mutates
// `target`. The graph overload assumes ascending x and stops past max.
// ---------------------------------------------------------------------------
inline void fill_from_interval(TH1& target, const TGraph& source,
                               double min, double max)
{
    for (int i = 0; i < source.GetN(); ++i) {
        const double x = source.GetPointX(i);
        if (x < min) continue;
        if (x > max) break;
        target.Fill(source.GetPointY(i));
    }
}

inline void fill_from_interval(TH1& target, const TProfile& source,
                               double min, double max)
{
    for (int i = 1; i <= source.GetNbinsX(); ++i) {
        const double x = source.GetBinCenter(i);
        if (x < min) continue;
        if (x > max) break;
        target.Fill(source.GetBinContent(i));
    }
}

}  // namespace mist::hep::histo
