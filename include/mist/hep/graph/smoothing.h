// SPDX-License-Identifier: MIT
//
// mist/hep/graph/smoothing.h — sliding-window smoothing over a TGraph.
//
// Thin adapter over <mist/algo/smoothing.h>: the X and Y coordinate arrays
// are pulled from the TGraph, smoothed by mist::algo::moving_mean, and
// zipped back into a new TGraph.
//
// Header-only. Returns an owning mist::hep::owned::root_ptr<TGraph>.
//
// Port / rewrite of:
//   - graphutils `moving_average` -> moving_average (F-16)
//
#pragma once

#include <TGraph.h>

#include <mist/algo/smoothing.h>
#include <mist/hep/graph/binning.h>  // detail::graph_x / graph_y / zip, owned alias

namespace mist::hep::graph {

// ---------------------------------------------------------------------------
// moving_average: sliding window of `window_size`, advancing one point at a
// time. Output length is N - window_size + 1 for input length N. Each output
// point is (window-mean x, window-mean y).
// ---------------------------------------------------------------------------
[[nodiscard]] inline owned::root_ptr<TGraph>
moving_average(const TGraph& source, std::size_t window_size)
{
    const auto mean_x = mist::algo::moving_mean(detail::graph_x(source),
                                                window_size);
    const auto mean_y = mist::algo::moving_mean(detail::graph_y(source),
                                                window_size);
    return detail::zip(mean_x, mean_y);
}

}  // namespace mist::hep::graph
