# Changelog

All notable changes to mist-hep are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).
Versioning follows [Semantic Versioning](https://semver.org/).

mist-hep tracks its own version train, independent of
[mist](https://github.com/Nikolajal/mist); its dependency on mist is
expressed via `find_package(mist <major> REQUIRED)` with `SameMajorVersion`
semantics.

---

## [1.0.0] — first release

First tagged release, cut alongside [mist](https://github.com/Nikolajal/mist)
v1.0.0. mist-hep tracks its own version train but shares the 1.x milestone;
its dependency is expressed as `find_package(mist 1 REQUIRED)`
(`SameMajorVersion`). Everything below — including the initial scaffold
previously logged under 0.1.0 — ships in this release.

### Added
- **`mist::hep::fit` factory set** — `make_gauss`, `make_qgauss` (+ the public
  `q_exp` Tsallis primitive), `make_exgauss` (EMG), `make_erf_step`,
  `make_expo`, `make_arrhenius`. Each mirrors `make_levy_tsallis`: named
  parameters and sensible defaults set on construction, returning an owned
  `root_ptr<TF1>`. (`make_qtof` is intentionally **not** provided — its
  functional form is lost; see `DISCUSSION.md` D-06.)
- **`mist::hep::graph` helpers** — `eval_with_errors` (array form of `eval`),
  `offset` (`{value,error}` form of `add`), and `average_rms` (per-bin mean and
  RMS spread of scattered points via a `TProfile` in "S" mode).
- **`mist::hep::stats::weighted_mean_rms`** — reduce `{value, error}`
  measurements to `{mean, rms}`, skimming outliers with `clip_outliers_nsigma`;
  `{NaN, NaN}` on empty / all-clipped input.
- **`mist::hep` alias under `find_package`** — `cmake/mist-hepConfig.cmake.in`
  now recreates the `mist::hep` ALIAS (CMake aliases are not exported), so the
  documented `target_link_libraries(x mist::hep)` works under `find_package`
  as well as `add_subdirectory` (Gap 4).
- **`mist::hep::graph` algebra** (`graph/algebra.h`) — error-propagating
  operations on TGraphErrors: `eval` (interpolation with error), `add`,
  `power`, `scale_values`, `log`, `log10` (corrected error factor),
  `ratio`, `product`, `difference` (graph + TF1), `trim`, `mean_of`, and
  `graph::derivative`. Plus `swap_xy(TGraphErrors)`.
- **`mist::hep::signal`** module (`signal/waveform.h`) — waveform analysis on
  TGraph: `amplitude`, `extremum`, `threshold_crossings`, `first_above` /
  `last_above`, `maximum_filter`, `gaussian_filter` (real Gaussian weights),
  `integrate`, `find_peaks` (no canvas side-effect).
- **`mist::hep::histo` fill helpers** (`histo/fill.h`) — `project_y`,
  `fill_profile`, `fill_persistence` (graph / function), `fill_from_interval`
  (graph / profile); plus `graph::from_profile` (TProfile → TGraphErrors).
  Salvaged from the ePIC SiPM-characterisation sweep (F-19, F-35..F-58).
- **`mist::hep::owned`** module (`owned.h`) — ROOT ownership correctness
  (D-05). `root_ptr<T>` (a `unique_ptr` with an ownership-aware deleter),
  `make`, `clone`, `adopt`, and the `scoped_directory_off` RAII guard.
  Objects are detached from `gDirectory` on creation, so they are not
  double-owned when a `TFile` is open. Fixes a **latent double-free** in
  `make_th1_from_vector` and the histo `scale`/`offset`/`absolute`, which
  previously returned histograms attached to the current directory.
- **`mist::hep::graph`** module — header-only TGraph transforms, split by
  concern across `graph/transforms.h`, `graph/binning.h`,
  `graph/smoothing.h`. All return owning `std::unique_ptr`, take const-ref
  inputs, and leave the source untouched:
  - `relative_diff` (F-11) — relative difference vs an interpolated
    reference, with an `out_of_range` policy enum and a zero-reference NaN
    guard.
  - `shift_x_to_origin` (F-12), `negate_y` / `negate_x` / `negate_xy`
    (F-13, F-18).
  - `scale` (F-17) — TGraph and TGraphErrors overloads; the error-aware
    overload scales the error bars.
  - `swap_xy` (F-34) — exchange x/y coordinates (from DHL).
  - `to_graph_errors` (F-32) — TGraph → TGraphErrors with zero errors.
  - `block_average` (F-14), `block_rms` (F-15), `moving_average` (F-16) —
    thin adapters over the `mist::algo` primitives shipped in mist 1.0.
- **`mist::hep::histo`** module (`histo/histo.h`) — histogram introspection
  and basic manipulation:
  - `dimension`, `pair_dimension`, `is_consistent` (F-20, F-21, F-22) —
    runtime TH1/TH2/TH3 introspection; diagnostics via `mist::logger`.
  - `uniform_binning` (F-25) — returns `std::vector<double>` bin edges.
  - `log_binning` (F-33) — logarithmically-spaced bin edges for pT/energy
    spectra.
  - `make_th1_from_vector` (F-23) — build a TH1 from a value vector with an
    auto-binning heuristic.
  - `offset` / `absolute` (F-26), `scale` (F-27) — bin manipulation with
    clean error propagation.
- New test binaries `test_graph` and `test_histo` (CTest).
- `.github/workflows/ci.yml` — build + ctest in the ROOT container
  (Release + Debug), building and installing mist from `main` first.
- Build-tree CMake export (`export(EXPORT mist-hepTargets …)`) so consumers
  can point `-Dmist-hep_DIR` at an un-installed build tree.
- `CONTRIBUTING.md`, `docs/CODING_CONVENTIONS.md`, `DISCUSSION.md`,
  `LICENSE`, and the Doxygen + GitHub Pages workflow.

### Changed
- Fit, graph, and histo factories now return `mist::hep::owned::root_ptr<T>`
  instead of `std::unique_ptr<T>` (the ownership-correct handle). Call sites
  using `auto` are unaffected; `.get()` / `operator->` / move semantics are
  identical.
- **C++20** is now the minimum language standard (was C++17), matching
  mist 1.0.
- `find_package(mist 0.1)` → `find_package(mist 1 REQUIRED)` —
  `SameMajorVersion` against mist's 1.x line.
- `quadrature_sum` / `clip_outliers_nsigma` (`stats.h`) migrated from
  `std::enable_if` SFINAE to C++20 concepts.

### Removed
- **RooFit / RooFitCore** dropped from the link surface. Nothing in the
  current API uses them; they will be re-added when the first RooFit-using
  port (the systematics Barlow pipeline) lands.

---

## [0.1.0] — initial scaffold

### Added
- `mist::hep::stats` — `quadrature_sum`, `barlow_parameter`,
  `barlow_passes`, `clip_outliers_nsigma` (header-only, ROOT-free).
- `mist::hep` Meyers-singleton accessors — `rng()`, `benchmark()`,
  `latex()`.
- `mist::hep::fit::make_levy_tsallis` — owning `std::unique_ptr<TF1>`
  factory.
- CMake package (STATIC lib, `mist::hep` alias, `find_package(mist-hep)`
  support); test binaries `test_stats`, `test_fit`.

---

[1.0.0]: https://github.com/Nikolajal/mist-hep/releases/tag/v1.0.0
