// SPDX-License-Identifier: MIT
//
// mist/hep/graph/binning.h — non-overlapping block reductions over a TGraph.
//
// Thin adapters over the ROOT-free primitives in <mist/algo/binning.h>:
// the X and Y coordinate arrays are pulled from the TGraph, reduced by
// mist::algo, and zipped back into a new TGraph.
//
// Header-only. Each function returns an owning
// mist::hep::owned::root_ptr<TGraph>.
//
// Ports / rewrites of:
//   - graphutils `average` -> block_average (F-14)
//   - graphutils `rms`     -> block_rms     (F-15)
//
#pragma once

#include <vector>

#include <TGraph.h>

#include <mist/algo/binning.h>
#include <mist/hep/owned.h>

namespace mist::hep::graph {

namespace owned = ::mist::hep::owned;

namespace detail {

inline std::vector<double> graph_x(const TGraph& g)
{
    std::vector<double> xs;
    xs.reserve(g.GetN());
    for (int i = 0; i < g.GetN(); ++i) xs.push_back(g.GetPointX(i));
    return xs;
}

inline std::vector<double> graph_y(const TGraph& g)
{
    std::vector<double> ys;
    ys.reserve(g.GetN());
    for (int i = 0; i < g.GetN(); ++i) ys.push_back(g.GetPointY(i));
    return ys;
}

inline owned::root_ptr<TGraph>
zip(const std::vector<double>& xs, const std::vector<double>& ys)
{
    auto out = owned::make<TGraph>();
    const std::size_t n = std::min(xs.size(), ys.size());
    for (std::size_t i = 0; i < n; ++i)
        out->SetPoint(static_cast<int>(i), xs[i], ys[i]);
    return out;
}

}  // namespace detail

// ---------------------------------------------------------------------------
// block_average: each non-overlapping block of `block_size` consecutive
// points becomes one point at (mean x, mean y) of that block.
//
// `drop_partial = false` (default) keeps the final short block; pass `true`
// to reproduce the graphutils-original behaviour of discarding it.
// ---------------------------------------------------------------------------
[[nodiscard]] inline owned::root_ptr<TGraph>
block_average(const TGraph& source, std::size_t block_size,
              bool drop_partial = false)
{
    const auto mean_x = mist::algo::block_mean(detail::graph_x(source),
                                               block_size, drop_partial);
    const auto mean_y = mist::algo::block_mean(detail::graph_y(source),
                                               block_size, drop_partial);
    return detail::zip(mean_x, mean_y);
}

// ---------------------------------------------------------------------------
// block_rms: each non-overlapping block becomes one point at (mean x of the
// block, population standard deviation of the block's y-values).
// ---------------------------------------------------------------------------
[[nodiscard]] inline owned::root_ptr<TGraph>
block_rms(const TGraph& source, std::size_t block_size,
          bool drop_partial = false)
{
    const auto mean_x = mist::algo::block_mean(detail::graph_x(source),
                                               block_size, drop_partial);
    const auto rms_y = mist::algo::block_rms(detail::graph_y(source),
                                             block_size, drop_partial);
    return detail::zip(mean_x, rms_y);
}

}  // namespace mist::hep::graph
