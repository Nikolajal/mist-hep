# mist-hep — ROOT-backed analysis helpers on top of mist

> *Clean-room sibling library to [mist](https://github.com/Nikolajal/mist),*
> *layering ROOT and RooFit on top of mist's primitives.*
> *Pronounced "mist-hep", not "mishep".*

[![Documentation](https://img.shields.io/badge/docs-online-blue)](https://nikolajal.github.io/mist-hep/)
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

---

## Table of Contents

- [Why a sibling, not a wrapper](#why-a-sibling-not-a-wrapper)
- [Repository structure](#repository-structure)
- [Prerequisites](#prerequisites)
- [Build & install](#build--install)
- [Usage](#usage)
- [Subsystems](#subsystems)
- [Documentation](#documentation)
- [Testing](#testing)
- [Relationship to AliAnalysisUtility (AAU)](#relationship-to-alianalysisutility-aau)
- [Open design questions](#open-design-questions)
- [License](#license)

---

## Why a sibling, not a wrapper

`mist` is deliberately ROOT-free pure C++17. Anything in `mist` builds
without ROOT in scope and links nothing from `cern/root`. Once you add
ROOT, the dependency surface explodes — Cling, RooFit, ROOT::Core, the
whole imaging stack — and every consumer of the logger or the RNG would
pay that cost just to get a progress bar.

mist-hep is the place for everything that *needs* ROOT but still benefits
from mist's primitives:

- Fit-function factories (Levy–Tsallis today; Breit–Wigner / BG-BlastWave /
  Gauss to come) that return `std::unique_ptr<TF1>` instead of leaking
  global pointers.
- Process-wide ROOT helpers (`TRandom3`, `TBenchmark`, `TLatex`) as
  Meyers-singleton accessors rather than ODR-unsafe header globals.
- Header-only analysis stats (`quadrature_sum`, `barlow_parameter`,
  `clip_outliers_nsigma`) — these don't *need* ROOT, but they belong here
  because their natural callers already have ROOT in scope.

A downstream app picks either or both:

```cmake
target_link_libraries(my_target PRIVATE mist::mist)             # primitives only
target_link_libraries(my_target PRIVATE mist::mist mist::hep)   # add ROOT helpers
```

mist-hep does **not** wrap mist (no API re-export, no shadow headers).
`#include <mist/logger/logger.h>` and `#include <mist/hep/stats.h>` are
both first-class entry points.

---

## Repository structure

```
.
├── include/
│   └── mist/hep/                    # public headers (under mist/, not mist-hep/)
│       ├── hep.h                    #   - umbrella include — pulls in everything
│       ├── stats.h                  #   - quadrature_sum, barlow_*, clip_outliers_nsigma
│       ├── globals.h                #   - Meyers singletons: rng(), benchmark(), latex()
│       ├── owned.h                  #   - root_ptr ownership: make / clone / adopt
│       ├── fit/
│       │   └── functions.h          #   - make_levy_tsallis (more to come)
│       ├── graph/
│       │   ├── transforms.h         #   - shift / negate / scale / swap_xy / from_profile
│       │   ├── binning.h            #   - block_average / block_rms
│       │   ├── smoothing.h          #   - moving_average
│       │   └── algebra.h            #   - eval / ratio / product / difference / ...
│       ├── histo/
│       │   ├── histo.h              #   - introspection, binning, scale/offset
│       │   └── fill.h               #   - project_y, fill_profile/persistence/interval
│       └── signal/
│           └── waveform.h           #   - amplitude, filters, integrate, find_peaks
├── src/
│   └── hep/
│       └── fit/
│           └── functions.cxx        # implementation TU (most modules header-only)
├── test/                            # CTest binaries (tester_<name>.cxx)
│   ├── tester_stats.cxx
│   ├── tester_fit.cxx
│   ├── tester_graph.cxx
│   ├── tester_histo.cxx
│   ├── tester_owned.cxx
│   └── tester_signal.cxx
├── docs/
│   ├── Doxyfile                     # Doxygen config (emits to docs/_site/)
│   └── CODING_CONVENTIONS.md        # naming + output discipline (inherited from mist)
├── cmake/
│   └── mist-hepConfig.cmake.in      # find_package re-resolves mist + ROOT
├── .github/workflows/
│   ├── ci.yml                       # build + ctest in the ROOT container
│   └── docs.yml                     # Doxygen + GitHub Pages on push to main
├── CHANGELOG.md
├── CONTRIBUTING.md
├── DISCUSSION.md                    # project design log (open questions, TODOs, attention)
├── LICENSE
└── README.md
```

**Headers go under `include/mist/hep/`** (not `include/mist-hep/`) so
consumers see one `mist/` include root regardless of which sibling library
a header came from.

---

## Prerequisites

- C++20 compiler (GCC ≥ 10 / Clang ≥ 10 / AppleClang from Xcode 12.5+ / MSVC ≥ 19.29)
- CMake ≥ 3.19
- [ROOT](https://root.cern/) ≥ 6.24 with components: `Core`, `Hist`,
  `Graf`, `Gpad`, `RIO`, `Tree`, `MathCore`, `Matrix`, `Minuit`, `RooFit`,
  `RooFitCore`
- [mist](https://github.com/Nikolajal/mist) ≥ 0.1 — see the
  [build-tree-export caveat](DISCUSSION.md#a-01--mist_dir-development-workflow)
  below

---

## Build & install

### Standalone

```bash
git clone https://github.com/Nikolajal/mist-hep.git
cd mist-hep
cmake -B build \
      -DMIST_HEP_BUILD_TESTS=ON \
      -Dmist_DIR=/path/to/mist-install/lib/cmake/mist
cmake --build build -j
ctest --test-dir build --output-on-failure
```

`mist_DIR` must point at an *installed* mist (or its build tree once
mist's [T-01](https://github.com/Nikolajal/mist/blob/main/DISCUSSION.md)
lands). Until then, install mist to a prefix first:

```bash
cmake --install /path/to/mist/build --prefix /tmp/mist-install
```

### Install mist-hep

```bash
cmake --install build --prefix /usr/local
```

Lays down:

- `libmist-hep.a` under `lib/`
- Headers under `include/mist/hep/`
- CMake package config under `lib/cmake/mist-hep/`

---

## Usage

### From a downstream CMake project

Either via `find_package`:

```cmake
find_package(mist-hep 0.1 REQUIRED)
target_link_libraries(my_app PRIVATE mist::hep)
# mist::mist + ROOT::* come as transitive PUBLIC dependencies — you don't
# need to find_package them yourself unless you use their symbols directly.
```

Or via `FetchContent` (two-step, in dependency order):

```cmake
include(FetchContent)

FetchContent_Declare(mist
    GIT_REPOSITORY https://github.com/Nikolajal/mist.git
    GIT_TAG        <commit-sha>)
FetchContent_MakeAvailable(mist)

FetchContent_Declare(mist-hep
    GIT_REPOSITORY https://github.com/Nikolajal/mist-hep.git
    GIT_TAG        <commit-sha>)
FetchContent_MakeAvailable(mist-hep)

target_link_libraries(my_app PRIVATE mist::mist mist::hep)
```

mist-hep's `CMakeLists.txt` short-circuits its own `find_package(mist)`
when the `mist::mist` target already exists, so the two-step
`FetchContent` works without duplicate lookups.

### From C++

```cpp
#include <mist/hep/hep.h>          // umbrella — pulls in everything

// Or cherry-pick:
#include <mist/hep/stats.h>        // ROOT-free analysis stats
#include <mist/hep/globals.h>      // singletons: rng(), benchmark(), latex()
#include <mist/hep/fit/functions.h>// fit-function factories
```

---

## Subsystems

### `mist::hep::stats` — analysis stats (ROOT-free)

Header-only. Drops in anywhere that already includes the standard library.

```cpp
namespace s = mist::hep::stats;

double sigma = s::quadrature_sum({stat_err, syst_err, lumi_err});

bool ok = s::barlow_passes(yield_std, sigma_std,
                            yield_var, sigma_var);

std::vector<double> xs = …;
auto removed = s::clip_outliers_nsigma(xs, 3.0);
```

### `mist::hep::globals` — process-wide ROOT singletons

Meyers singletons (`static T obj; return obj;` inside an inline accessor).
Lazy, thread-safe init since C++11, no ODR risk. Replaces AAU's pattern of
defining `TRandom* uRandomGen = new TRandom();` at TU scope in a header.

```cpp
mist::hep::rng().SetSeed(42);
double r = mist::hep::rng().Uniform(0.0, 1.0);

mist::hep::benchmark().Start("event_loop");
…
mist::hep::benchmark().Stop("event_loop");
```

`mist::hep::rng()` returns a `TRandom3` (Mersenne Twister), one transparent
upgrade from AAU's plain `TRandom` (LCG).

### `mist::hep::fit` — fit-function factories

Each factory returns an *owned* `root_ptr<TF1>`. Callers decide lifetime;
nothing leaks at TU scope. Parameter names and sensible defaults are wired up
at construction. Available: `make_levy_tsallis`, `make_gauss`, `make_qgauss`
(plus the public `q_exp` Tsallis primitive), `make_exgauss` (EMG),
`make_erf_step`, `make_expo`, `make_arrhenius`.

```cpp
auto f = mist::hep::fit::make_levy_tsallis("levy_tsallis", 0.0, 10.0);
// Defaults: dNdy=1, n=7, C=0.25, m0=0.13957 (charged pion)

spectrum->Fit(f.get(), "RMQ");
```

### `mist::hep::owned` — ROOT ownership correctness

`root_ptr<T>` (a `unique_ptr` with an ownership-aware deleter) plus `make` /
`clone` / `adopt` and a `scoped_directory_off` guard. Objects are detached
from `gDirectory` on creation, so they are never double-owned when a `TFile`
is open. All the factories below return `owned::root_ptr<T>`.

```cpp
auto h = mist::hep::owned::make<TH1F>("h", "h", 100, 0, 1);  // detached, safe
```

### `mist::hep::graph` — TGraph transforms and algebra

Header-only, const-ref in / owning `root_ptr` out, non-mutating. Point-wise
transforms (`relative_diff`, `shift_x_to_origin`, `negate_x/y/xy`, `scale`,
`swap_xy`, `to_graph_errors`, `from_profile`), block/window reductions
(`block_average`, `block_rms`, `moving_average` over `mist::algo`), and an
error-propagating algebra on `TGraphErrors` (`eval`, `add`, `power`,
`scale_values`, `log`, `log10`, `ratio`, `product`, `difference`, `trim`,
`mean_of`, `derivative`).

### `mist::hep::histo` — histogram helpers

Introspection (`dimension`, `pair_dimension`, `is_consistent`), construction
and binning (`make_th1_from_vector`, `uniform_binning`, `log_binning`),
manipulation (`offset`, `absolute`, `scale`), and fill bridges (`project_y`,
`fill_profile`, `fill_persistence`, `fill_from_interval`).

### `mist::hep::signal` — waveform analysis on TGraph

`amplitude`, `extremum`, `threshold_crossings`, `first_above` / `last_above`,
`maximum_filter`, `gaussian_filter`, `integrate`, `find_peaks`.

---

## Documentation

- **API reference** (Doxygen, auto-deployed on push to `main`):
  <https://nikolajal.github.io/mist-hep/>
- **Coding conventions**: [`docs/CODING_CONVENTIONS.md`](docs/CODING_CONVENTIONS.md)
  — inherited verbatim from mist (only the macro prefix changes:
  `MIST_*` → `MIST_HEP_*`).
- **Design log**: [`DISCUSSION.md`](DISCUSSION.md) — open architectural
  questions, port roadmap from AAU, TODOs, attention points.

The Doxygen site is built and published by
[`.github/workflows/docs.yml`](.github/workflows/docs.yml) on every push to
`main`; manual rebuilds are possible from the Actions tab.

---

## Testing

Off by default so downstream consumers aren't affected. Enable with
`-DMIST_HEP_BUILD_TESTS=ON`:

| Binary | Source | Coverage |
|---|---|---|
| `test_stats` | [`test/tester_stats.cxx`](test/tester_stats.cxx) | `quadrature_sum` on initializer-list + ranges; `barlow_parameter` NaN handling; `clip_outliers_nsigma` happy / no-outlier / zero-variance / empty / single-element |
| `test_fit` | [`test/tester_fit.cxx`](test/tester_fit.cxx) | `make_levy_tsallis` parameter names + defaults, finiteness + positivity over `[0, 10]` GeV, integral sanity (~0.9997 for default `dNdy=1`), `rng()` singleton smoke-test |
| `test_graph` | [`test/tester_graph.cxx`](test/tester_graph.cxx) | transforms (shift / negate / scale / swap_xy / relative_diff policies / to_graph_errors / from_profile), binning + smoothing, `derivative`, and the TGraphErrors algebra (eval, add, power, scale_values, log10 error, ratio, product, difference, trim, mean_of) |
| `test_histo` | [`test/tester_histo.cxx`](test/tester_histo.cxx) | introspection (dimension / pair_dimension / is_consistent), `uniform_binning` / `log_binning`, `make_th1_from_vector`, `offset` / `absolute` / `scale`, and the fill bridges (`project_y`, `fill_profile`, `fill_persistence`, `fill_from_interval`) |
| `test_owned` | [`test/tester_owned.cxx`](test/tester_owned.cxx) | `make` / `clone` / `adopt` detachment, `scoped_directory_off` restore, and the decisive open-`TFile` no-double-free case |
| `test_signal` | [`test/tester_signal.cxx`](test/tester_signal.cxx) | `amplitude` / `extremum`, threshold crossings + `first/last_above`, `maximum_filter` / `gaussian_filter`, `integrate`, `find_peaks` |

```bash
ctest --test-dir build --output-on-failure
```

---

## Relationship to AliAnalysisUtility (AAU)

mist-hep is a *clean-room rewrite* of the salvageable parts of AAU
(2021–2022), not a vendor drop. Each port:

- Fixes the original's bugs (e.g. operator-precedence issue in
  `fIsWorthFitting`, ODR-unsafe header globals, dead-branch stubs like
  `fRatioPlot`).
- Switches to `snake_case` + namespaces (`mist::hep::*`).
- Drops `using namespace std/RooFit` from headers.
- Replaces TU-scope `TF1*` globals with `std::unique_ptr<TF1>` factories.

Day-1 surface is intentionally small (stats + globals + Levy–Tsallis).
Subsequent ports happen on demand; see
[`D-01`](DISCUSSION.md#d-01--aau-port-roadmap) for the proposed order.

---

## Open design questions

The full set lives in [`DISCUSSION.md`](DISCUSSION.md). Currently open:

- **D-01** — AAU port roadmap: which functions next, in what order?
- **D-02** — Should RooFit be split into an optional component
  (`MIST_HEP_WITH_ROOFIT`) so the smaller cousins can build without it?
- **D-03** — Version-pin policy with mist: lock or float?
- **D-04** — Promote a ROOT-using fit test from smoke-test to full
  Levy-Tsallis sample→fit→recover roundtrip.

In-flight TODOs ([`T-01 … T-03`](DISCUSSION.md#todos--concrete-fixes-in-the-queue)):
CI build+ctest workflow, README docs badge once Pages publishes, AAU
function-port queue.

---

## License

MIT License. See [`LICENSE`](LICENSE) for details.
