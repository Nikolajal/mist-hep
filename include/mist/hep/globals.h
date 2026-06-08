// SPDX-License-Identifier: MIT
//
// mist/hep/globals.h — Meyers-singleton accessors for the kinds of
// long-lived ROOT helpers that AAU originally held as header-globals.
//
// Why singletons instead of `inline` namespace objects?
//   - TRandom / TBenchmark / TLatex are stateful and not cheap to construct.
//   - Lazy first-use construction defers ROOT initialisation until actually
//     needed (matters when consumers include mist/hep/globals.h transitively
//     but never touch the accessors).
//   - One instance per process, zero ODR risk across translation units.
//
#pragma once

#include <TBenchmark.h>
#include <TLatex.h>
#include <TRandom3.h>

namespace mist::hep {

// Process-wide RNG. TRandom3 (Mersenne Twister) is the modern default —
// AAU's plain TRandom was the LCG; we upgrade transparently.
inline TRandom3& rng() {
    static TRandom3 instance;
    return instance;
}

inline TBenchmark& benchmark() {
    static TBenchmark instance;
    return instance;
}

inline TLatex& latex() {
    static TLatex instance;
    return instance;
}

}  // namespace mist::hep
