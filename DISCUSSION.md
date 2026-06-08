# Project design log

Project-wide design reference + hub. This file is the **main entry point**
for anyone reading mist-hep's architectural thinking. mist-hep is small
today (one fit factory, three stats helpers, three singletons) — no
satellite `DISCUSSION.md` files exist yet. Everything lives here. When a
subsystem grows enough to warrant its own design notes, add a satellite
under `include/mist/hep/<subsystem>/DISCUSSION.md` and index it from the
hub table below.

| Section | What it holds | Removal trigger |
|---|---|---|
| [Satellite discussions](#satellite-discussions--hub) | Pointers to per-area `DISCUSSION.md` files. None yet. | Satellite file added / removed → update the hub. |
| [Triage taxonomy](#triage-taxonomy) | Convention for tagging items (Design / TODO / Attention / Feature). | Taxonomy change → update the chapter. |
| [Design discussions](#design-discussions) | Open architectural questions — **decision needed before any code change**. | Decision made → entry deleted or moved to TODOs. |
| [TODOs](#todos--concrete-fixes-in-the-queue) | Concrete code-work items. No design pending. | Fix lands on `main` → row removed. |
| [Attention points](#attention-points--latent-issues-to-be-careful-about) | Latent caveats — heads-up so they don't get propagated. | Caveat resolved or captured as a design question / TODO. |
| [AAU port queue](#aau-port-queue) | Specific functions from AliAnalysisUtility still worth porting; ordered by value/effort. | Function ported → row struck through with the commit SHA. |
| [Coding conventions](#coding-conventions) | Naming + style reference. | Reference material — no removal trigger. |

> mist-hep inherits the same hub/satellite convention as the sibling
> [mist](https://github.com/Nikolajal/mist/blob/main/DISCUSSION.md) and the
> downstream `beam-test-analysis` repo. Same structure → same review
> patterns across the family.

---

## Satellite discussions — hub

No satellites yet. Add a row here when one lands. mist-hep's structure
suggests these are likely homes for future satellite files:

- `include/mist/hep/fit/DISCUSSION.md` — once the fit module grows beyond
  Levy-Tsallis (Breit-Wigner, BG-BlastWave, …) and the inevitable
  fitter-configuration design questions appear.
- `include/mist/hep/systematics/DISCUSSION.md` — once the Barlow-pipeline
  port (see [AAU port queue](#aau-port-queue)) introduces non-trivial
  variation-handling.

---

## Triage taxonomy

Items in this log carry one of four tags:

| Tag | Lives in | Means |
|---|---|---|
| **D-XX** | Design discussions | Architectural question; decision needed before code. |
| **TODO** | TODOs | Concrete fix; no design pending. |
| **Attention** | Attention points | Latent caveat; readers should know. |
| **F-XX** | AAU port queue | Future port from AAU; not yet started. |

---

## Design discussions

### D-01 — AAU port roadmap

**Observation:**
mist-hep is a clean-room rewrite of the parts of AliAnalysisUtility (AAU,
2021–2022) that are worth saving. Day-1 surface is intentionally small:
`stats.h` (3 helpers), `globals.h` (3 singletons), `fit/functions.h`
(Levy-Tsallis only). The next round of ports needs an order.

**Candidate ports, grouped by source AAU header:**

| AAU header | Worth porting? | Notes |
|---|---|---|
| `AAU_Functions.h` | ✅ partial | Already done: Levy-Tsallis. Next: Breit-Wigner (most-used after Levy), BG-BlastWave, Gauss / AsymmGauss. `fSetAllFunctions` umbrella is unnecessary; per-function factories are cleaner. |
| `AAU_Histograms.h` (~830 lines, templated) | ✅ partial | `fSumErrors` family (TH1 + TGraph variants), `fEfficiencycorrection` (TH1 + TH2), `uRandomizePointsSymm`. Skip the duplicate scaffolding (`uMakeMeTH1F` etc.) — `TH1`'s own ctors do it cleaner once the AAU sloppiness is removed. |
| `AAU_Graphs.h` | ✅ small | `uMakeMeTGE` (histogram → TGraphErrors). Strip the debug `cout << "xValue: …"` (left in the original by mistake). |
| `AAU_Resolution.h` | ✅ | `EvalResolution` family — RMS / Gaussian fit per slice with 1–6 σ ranges; well-scoped. |
| `AAU_Efficiency.h` | ✅ | `fGetEfficiency` family — generic correction with binomial/Bayesian errors. |
| `AAU_Systematics.h` (~820 lines) | ⚠️ deferred | Multi-variation Barlow-pipeline. Substantial; should land **after** stats helpers + a fit factory pool exist. Needs its own design discussion (D-XX) at port time. |
| `AAU_Extrapolation.h` (~630 lines) | ⚠️ analysis-specific | Yield extrapolation across pT slices. May not generalise beyond ALICE pT spectra; revisit at port time. |
| `AAU_Style.h` | ❌ | ROOT-style boilerplate (`SetMarkerStyle` etc.). Better as a private helper per analysis than a published API. |
| `AliAnalysisUtility.h` top-level | ❌ partial | `fIsWorthFitting`, `uTestGoodParameters`, `fMakeMeTH1F`, `fPublishSpectrum`, `uCompareResultsTH1F` are useful in spirit but each needs a from-scratch design (the originals have precedence bugs, dead branches, and global side-effects). Port individually under `mist::hep::fit::*` or `mist::hep::plot::*`, not as a single dump. |

**Proposed order:**

1. **Breit-Wigner** factory in `mist::hep::fit` (immediate parity with the
   most-used non-Levy AAU function).
2. `mist::hep::histo` module: `sum_errors`, `efficiency_correction`,
   `randomize_points_symm`.
3. `mist::hep::graph::make_tge` (histogram → TGraphErrors), cleaned of the
   debug `cout`.
4. `mist::hep::resolution::eval` family.
5. `mist::hep::efficiency::get` family.
6. **Then** open a design discussion for the systematics Barlow-pipeline
   port — it's large enough to warrant `D-XX` of its own.

**Decision needed:** Confirm the order, or reshuffle?

---

### D-02 — RooFit as an optional component

**Observation:**
mist-hep's `CMakeLists.txt` links `ROOT::RooFit` and `ROOT::RooFitCore`
**unconditionally** as PUBLIC dependencies. Today nothing in the surface
actually calls RooFit — the Levy-Tsallis factory uses raw `TF1`, the stats
helpers are ROOT-free. RooFit was included anticipating future fitter ports
(e.g. the AAU Barlow pipeline does use it).

**Cost:** every consumer of `mist::hep` transitively pulls RooFit's full
include set and link surface (`RooFitCore` alone is ~3 MB on macOS,
non-trivial compile time on first include). Some downstreams (small
analysis macros) may not want this.

**Options:**

| Option | Mechanism | Trade-off |
|---|---|---|
| A — drop RooFit until something needs it | Remove `RooFit` / `RooFitCore` from `target_link_libraries` and from `mist-hepConfig.cmake.in`'s `find_dependency`. | Smallest surface; easy to re-add later. |
| B — keep as PUBLIC, document it | Status quo. | Simplest mental model; pays cost upfront. |
| C — `MIST_HEP_WITH_ROOFIT` CMake option | Gate the RooFit link behind an option (default ON), with `#ifdef MIST_HEP_HAS_ROOFIT` in headers that need it. | Most flexible; adds config-time complexity to every consumer. |

**Recommendation:** Option A for now. Nothing currently needs RooFit; carry
the dependency only once the first port actually uses it (likely with the
systematics Barlow-pipeline). Re-evaluate Option C if/when ports diverge
in their RooFit needs.

**Decision needed:** A, B, or C?

---

### D-03 — Version-pin policy with mist

**Observation:**
mist-hep's `find_package(mist 0.1 REQUIRED)` matches any 0.1.x — fine
while both repos are pre-1.0 and move together. Once mist hits 1.0 (or
introduces a breaking change), mist-hep needs a policy.

**Options:**

| Option | Behaviour | Trade-off |
|---|---|---|
| A — float (`find_package(mist REQUIRED)` no version) | Always grab whatever mist is installed. | Simplest; breaks silently when mist changes API. |
| B — pin to a major version (`find_package(mist 1 REQUIRED)`) | Compatible-within-major. Matches `SameMajorVersion` in mist's own version file. | Standard semver; matches mist's compatibility policy. |
| C — pin to a specific minor (`find_package(mist 1.2 REQUIRED)`) | Lock to that minor + patches. | Safest; requires bumping the pin every minor. |

**Recommendation:** Option B once both reach 1.0. Track mist's
`SameMajorVersion` exactly. Until then (pre-1.0), pin to the matching
minor.

**Decision needed:** Confirm B at 1.0?

---

### D-04 — Promote `test_fit` to a real ROOT roundtrip

**Observation:**
`test/tester_fit.cxx` is currently a smoke test: it builds the Levy-Tsallis
TF1, evaluates it at points, checks finiteness / positivity / integral
sanity, touches the `rng()` singleton. It does *not* exercise a real fit
loop — sampling from the function, building a histogram, fitting it back,
checking parameter recovery within errors.

**Proposed addition:**

```cpp
auto f_true = mist::hep::fit::make_levy_tsallis("truth");
TH1F h_sample("sample", "", 100, 0, 10);
h_sample.FillRandom("truth", 100000);

auto f_fit = mist::hep::fit::make_levy_tsallis("fit");
h_sample.Fit(f_fit.get(), "RMQ0");

assert(within_n_sigma(f_fit, f_true, 3));   // pulls within 3σ
```

**Trade-off:** Test runtime jumps from ~1.5s (current) to ~5–10s
(minimisation is the slow part). Acceptable for CI; would want a fast-mode
that skips the fit when iterating locally.

**Decision needed:** Add now, or defer until CI is in place
([T-01](#todos--concrete-fixes-in-the-queue))?

---

### ~~D-05 — ROOT ownership / memory-safety service~~ → RESOLVED & IMPLEMENTED

**Context.** ROOT's object model fights RAII: a `TH1` registers itself in the
current `gDirectory` on construction, so a `TFile` open at construction time
*also* owns the histogram and deletes it on close — double-freeing anything
that wrapped it in a `std::unique_ptr`. This surfaced as a **latent
double-free in mist-hep itself**: `make_th1_from_vector` and the histo
`scale`/`offset`/`absolute` (which `Clone()`) returned histograms attached to
`gDirectory`. It is also the home for beam-test-analysis's long-standing D-02
(the ~60 `new TH1F` sites in `lightdata_writer.cxx`).

**Decisions taken.**
- Module name: **`mist::hep::owned`** (ownership-honest — the guarantee is
  correct ownership, not full memory safety).
- Migrate existing factory return types **now** (pre-1.0, so free): fit,
  graph, and histo factories return `owned::root_ptr<T>` instead of
  `std::unique_ptr<T>`. This is the fix for the latent double-free.
- **Full** first cut: `root_ptr` + `root_deleter`, `make`, `clone`, `adopt`,
  `scoped_directory_off`.
- Deleter: a single runtime-`dynamic_cast` deleter (detaches `TH1` before
  delete; `TF1` self-deregisters; `TGraph` plain delete).

**Contract.** "Detach-on-create, own-via-smart-pointer": objects from
`make`/`clone`/`adopt` are detached from ROOT's registries; `Write()` on a
detached object serialises a *copy* (no ownership transfer), so the
`root_ptr` stays the sole owner — no double free.

**Outcome.** Implemented in `include/mist/hep/owned.h`; all fit/graph/histo
factories migrated; `tester_owned` covers detachment, clone/adopt,
`scoped_directory_off`, and the decisive open-`TFile` no-double-free case.
Cross-reference: this supersedes the "Option C `root_utils.h`" idea in
beam-test-analysis D-02 — that helper now lives here.

---

### D-06 — `make_qtof` requested but BLOCKED (unknown functional form)

**Context.** A `mist::hep::fit::make_qtof` factory was requested alongside the
other fit factories — the SiPM "qTOF" 5-parameter signal shape.

**Blocker.** The original qTOF source file was lost, and its functional form is
not recorded anywhere. The other factories (`make_gauss`, `make_qgauss`,
`make_exgauss`, `make_erf_step`, `make_expo`, `make_arrhenius`) have known,
standard definitions and were implemented; qTOF does not.

**Decision.** Do **not** implement it. Fabricating a 5-parameter shape would
produce a plausible-looking but wrong fit function — worse than its absence,
because callers would trust it. `make_qtof` stays unimplemented until the
original definition (formula + parameter meanings) is recovered.

**To unblock:** supply the qTOF formula and its five parameters; then it is a
~20-line factory mirroring `make_qgauss`.

---

## TODOs — concrete fixes in the queue

| ID | Item | Where | Source |
|---|---|---|---|
| T-01 | Add a CI workflow (build + ctest) separate from `docs.yml`. Should match mist's `.github/workflows/ci.yml` matrix (Linux / macOS, Release + Debug). Needs ROOT in the runner — easiest via [`root-project/root-image`](https://hub.docker.com/r/rootproject/root) container or `apt-get install root-system`. | `.github/workflows/ci.yml` (new). | Pending — scaffold only ships `docs.yml`. |
| T-02 | Add a `[![CI]](…)` badge to the README once T-01 lands and the Pages URL is live. | `README.md` line 7–8. | Same. |
| T-03 | Add `LICENSE` file (MIT, matching the SPDX headers already in source). | Repo root. | Noticed at README write time — the README points at it. |
| T-04 | The cross-link from `docs/CODING_CONVENTIONS.md` to mist's CODING_CONVENTIONS.md was removed because Doxygen can't resolve relative paths outside the input set. Once both Pages sites are live, replace with an absolute URL: `[mist](https://nikolajal.github.io/mist/CODING_CONVENTIONS.html)`. | `docs/CODING_CONVENTIONS.md` § header. | Doxygen warning fix during the docs-scaffold pass. |

---

## Attention points — latent issues to be careful about

### A-01 — `mist_DIR` development workflow

Until [mist's T-01](https://github.com/Nikolajal/mist/blob/main/DISCUSSION.md)
lands, you must install mist to a prefix before mist-hep can find it:

```bash
cmake --install /path/to/mist/build --prefix /tmp/mist-install
cmake -B build \
      -DMIST_HEP_BUILD_TESTS=ON \
      -Dmist_DIR=/tmp/mist-install/lib/cmake/mist
```

Pointing `mist_DIR` at mist's build tree directly fails because mist's
`CMakeLists.txt` only writes `mistTargets.cmake` to the install prefix
(via `install(EXPORT …)`), not to the build dir (would need an additional
`export(EXPORT …)` call).

### A-02 — ROOT is a PUBLIC transitive dependency

Every consumer of `mist::hep` automatically gets ROOT's include dirs and
link flags via the PUBLIC `target_link_libraries`. This is intentional —
mist-hep's headers expose `TH1F*`, `TF1`, etc. — but it means a downstream
that links `mist::hep` *cannot* opt out of ROOT, even if it only calls the
ROOT-free `mist::hep::stats` helpers. If that becomes a real friction
point, consider Option C of [D-02](#d-02--roofit-as-an-optional-component)
(extend the gating mechanism to all of ROOT, not just RooFit).

### A-03 — Output discipline allows `printf` in tests

The conventions doc carves tests out: `tester_stats.cxx` and
`tester_fit.cxx` use `std::printf` / `std::puts` for assertion macros
where pulling in `mist::logger` would obscure the test itself. This is
explicitly allowed. If new test files emerge that do non-trivial logging
(e.g. progress through a long fit loop), use `mist::logger` for the
progress and keep stdio only for assertion output. See
[`docs/CODING_CONVENTIONS.md`](docs/CODING_CONVENTIONS.md#output-never-stdcout--stdcerr--printf)
for the rule and its scope.

### A-04 — Meyers singleton destruction order

`mist::hep::rng()`, `benchmark()`, `latex()` are Meyers singletons:
destroyed at program exit in reverse order of first access. If a future
addition (say, a hypothetical `mist::hep::canvas()` that holds a TLatex
member) calls `mist::hep::latex()` in its destructor, you'll get
use-after-free unless `canvas()` calls `latex()` in its **constructor**
first (which registers `latex()`'s destructor *earlier*, so it runs
*later*). The current 3-singleton set has no interdependencies; this is a
heads-up for whoever adds the 4th.

---

## AAU port queue

Functions flagged for porting but not yet scheduled. The queue now draws
from three external sources:

- **AAU** — AliAnalysisUtility (2021–2022). Driven by
  [D-01](#d-01--aau-port-roadmap) for order and rationale.
- **graphutils** — a standalone `namespace graphutils` contribution
  reviewed for the `mist::hep::graph` module (F-11..F-16). Decisions
  taken at review time: three-file layout (`transforms.h` / `binning.h` /
  `smoothing.h`), `drop_partial = false` default for the block
  reductions, `relative_diff` over the misleading original name `diff`.
- **BLU** — Bologna Laboratory Utility (2022), authors N. Rubini and
  S. Strazzi. Audited 2026-06; salvageable graph/histogram primitives
  are F-17..F-22. The `sgn<T>` primitive is ROOT-free and goes to mist's
  own feature queue (`mist:F-05`), not here.
- **DHL** — a 2024 clean-slate TGraph-subclass experiment. Audited
  2026-06; the one salvageable transform is `swap_xy` (F-34). Its
  in-place member-function-pointer dispatch and the `#include "*.cpp"`
  header pattern were not carried over (mist-hep uses free,
  copy-returning functions). `scale_xy` was declared but never
  implemented and is already covered by F-17.

Out of scope (audited, nothing to port): the 2023 AliAnalysisUtility is a
later, larger edition of AAU — same categories already queued (F-01..F-10),
to be diffed against the newest copy at each module's port time rather than
re-catalogued now. AliAnalysisQA is an ALICE-coupled PID/track QA
*framework* — a future consumer of mist/mist-hep, not a primitive source.

The block reductions in F-14..F-16 are thin TGraph adapters over the
`mist::algo` primitives shipped in mist 1.0 (`block_mean`, `block_rms`,
`moving_mean`); they pull X/Y arrays from the TGraph, call `mist::algo`,
and pack the result back.

| ID | Source symbol | Target home in mist-hep | Status |
|----|---------------|--------------------------|--------|
| F-01 | (AAU) `fSetBreitWigner` | `mist::hep::fit::make_breit_wigner` | not started |
| F-02 | (AAU) `fSetBGBlastWave` | `mist::hep::fit::make_bg_blast_wave` | not started |
| F-03 | (AAU) `fSetGauss` / `fSetAsymmGauss` | `mist::hep::fit::make_gauss` / `make_asymm_gauss` | not started |
| F-04 | (AAU) `fSumErrors` (TH1 + TGraph variants) | `mist::hep::histo::sum_errors` | not started |
| F-05 | (AAU) `fEfficiencycorrection` (TH1 + TH2) | `mist::hep::histo::efficiency_correction` | not started |
| F-06 | (AAU) `uRandomizePointsSymm` | `mist::hep::histo::randomize_points_symm` | not started |
| F-07 | (AAU) `uMakeMeTGE` | `mist::hep::graph::make_tge` (clean of debug `cout`) | not started |
| F-08 | (AAU) `EvalResolution` family | `mist::hep::resolution::eval` | not started |
| F-09 | (AAU) `fGetEfficiency` family | `mist::hep::efficiency::get` | not started |
| F-10 | (AAU) Barlow-pipeline | `mist::hep::systematics::*` — needs its own D-XX at port time | deferred |
| F-11 | (graphutils) `diff` | `mist::hep::graph::relative_diff` (in `transforms.h`; out-of-range policy enum, zero-reference NaN guard) | **done** |
| F-12 | (graphutils) `fromZero` | `mist::hep::graph::shift_x_to_origin` (`transforms.h`) | **done** |
| F-13 | (graphutils) `invertY` | `mist::hep::graph::negate_y` (`transforms.h`) | **done** |
| F-14 | (graphutils) `average` | `mist::hep::graph::block_average` (`binning.h`; over `mist::algo::block_mean`, `drop_partial=false`) | **done** |
| F-15 | (graphutils) `rms` | `mist::hep::graph::block_rms` (`binning.h`; over `mist::algo::block_rms`) | **done** |
| F-16 | (graphutils) `moving_average` | `mist::hep::graph::moving_average` (`smoothing.h`; over `mist::algo::moving_mean`) | **done** |
| F-17 | (BLU) `uScale` (TGraph + TGraphErrors) | `mist::hep::graph::scale` (`transforms.h`; error-bar scaling in the TGraphErrors overload) | **done** |
| F-18 | (BLU) `uInvertX` / `uInvertXY` | `mist::hep::graph::negate_x` / `negate_xy` (extends the F-13 negate family) | **done** |
| F-19 | (BLU) `uDerivative` | `mist::hep::graph::central_difference_derivative` — **review error propagation before porting** (original mixes absolute/relative error at the source) | not started |
| F-20 | (BLU) `uGetTHDimension` | `mist::hep::histo::dimension` (runtime TH1/TH2/TH3 introspection) | **done** |
| F-21 | (BLU) `uGetTHPairDimension` | `mist::hep::histo::pair_dimension` | **done** |
| F-22 | (BLU) `uIsTHPairConsistent` | `mist::hep::histo::is_consistent` (a working `is_rebinnable` still to be re-derived; the BLU original has a compile-breaking `GetXmax()()` typo) | **done** |
| F-23 | (BLU) `uBuildTH1` | `mist::hep::histo::make_th1_from_vector` (auto-binning heuristic; fixed the duplicate `>= 1e3` branch at BLU lines 144-145) | **done** |
| F-24 | (BLU) `uRebin1D` + `uRebin` dispatcher | `mist::hep::histo::rebin` — **re-derive**: BLU original calls the non-existent `uGetPairDimension` and never compiled. 2D/3D were stubs; implement or leave 1D-only. | not started |
| F-25 | (BLU) `uGetUniformBinningArray` / `uUniformBinning` | `mist::hep::histo::uniform_binning` — returns `std::vector<double>`, not raw `new[]`. | **done** |
| F-26 | (BLU) `uOffset` / `uAbsolute` | `mist::hep::histo::offset` / `absolute` | **done** |
| F-27 | (BLU) `uScale` (TH) | `mist::hep::histo::scale` — distinct from the graph `scale` (F-17). Dropped the `-1`/`-2` magic sentinels; fixed the `GetBinContent(iBin)`→global-bin bug. | **done** |
| F-28 | (BLU) `uRandomisePoints` (1-arg) | `mist::hep::histo::randomize_points` — Gaussian bin-smear; **consolidate with AAU F-06** (`uRandomizePointsSymm`), same concept. The BLU 2-arg overload is broken; drop it. | not started |
| F-29 | (BLU) `uLoadHistograms` / `uInternalLoadHisto` (1D/2D/3D nested) | `mist::hep::histo::load` family — N-dimensional `TFile` loader via `Form()` name patterns into nested `std::vector`. Fix the `void`-return-with-`return fResult;` compile error in the scalar overload. | not started |
| F-30 | (BLU) `uAddSumHistogram` | `mist::hep::histo::add_sum` (prepend weighted-sum histogram to a vector; +2D overload) | not started |
| F-31 | (BLU) `uReverseStructure` | `mist::hep::histo::transpose` (transpose nested histogram vectors; fix the 3-D loop bound at BLU line 554) | not started |
| F-32 | (BLU) `uMakeMeTGraphErrors` | `mist::hep::graph::to_graph_errors` (TGraph → TGraphErrors with zero errors; from the newer Root-Analysis copy) | **done** |
| F-33 | (new) log-spaced binning | `mist::hep::histo::log_binning` — logarithmic bin edges; sibling of `uniform_binning`. Common for pT / energy spectra. | **done** |
| F-34 | (DHL) `DHLgraph::Swap_xy` | `mist::hep::graph::swap_xy` (`transforms.h`) — exchange x/y coordinates. Rewritten as a free, non-mutating function (DHL original was an in-place TGraph-subclass method). | **done** |

**BLU items deliberately skipped** (recorded so the deletion accounting is complete):

| BLU symbol | Reason |
|---|---|
| `uBuildTH2` / `uBuildTH3`, `uRebin2D` / `uRebin3D` | Stubs — return empty / no-op. |
| `uRandomisePoints` (2-arg) | Broken: smears then overwrites the result with sum-errors. |
| `uSumErrors` (TH) | Duplicate of AAU F-04. |
| `uMovingAverage` (TGraph / TGraphErrors) | Duplicate of graphutils F-16; TGraphErrors variant has broken error arithmetic. |
| `SDataStructure`, `uLoadDataFromFile` (LGAD ingestion) | Application code, not a primitive. |
| `BLU_BreakdownVoltage.h` (`fMeasureVbd`) | Entirely commented out; SiPM-specific; references undefined globals. Dead code. |
| `uRandomGen` / `uBenchmark` / `uLatex` globals, `uSquareSum`, `TNVector` | Already covered in mist-hep (`globals`, `stats::quadrature_sum`) or trivial to re-add. |

Once F-17..F-32 are implemented, BLU carries nothing salvageable and the
duplicate repositories (`bologna-laboratory-repository`,
`Root-Analysis-Utility-Laboratory`) are safe to delete (housekeeping).

---

## ePIC SiPM-characterisation sweep (2026-06)

`ePIC/sipm-characterisation` is the most mature utility trove audited. Its
`utils/graphutils.C` is the *superset* the earlier graphutils contribution
(F-11..F-16) was extracted from, so the `TGraph` transforms were already
done. The new yield, implemented this round:

| ID | Source | Target | Status |
|----|--------|--------|--------|
| F-19 | (graphutils/SiPM) `derivate` | `mist::hep::graph::derivative` (forward difference at midpoint; clean value-only TGraph form) | **done** |
| F-35 | (SiPM) `eval_with_errors` | `graph::eval` (interpolation WITH error) | **done** |
| F-36 | (SiPM) `add` | `graph::add` | **done** |
| F-37 | (SiPM) `power` | `graph::power` | **done** |
| F-38 | (SiPM) `scale` (uncertain factor) | `graph::scale_values` | **done** |
| F-39 | (SiPM) `log` / `log10` | `graph::log` / `log10` (fixed log10 error: `ey/(y ln10)`, original used `ey/y`) | **done** |
| F-40 | (SiPM) `ratio` | `graph::ratio` | **done** |
| F-41 | (SiPM) `multi` | `graph::product` | **done** |
| F-42 | (SiPM) `diff` (graph, TF1) | `graph::difference` (two overloads) | **done** |
| F-43 | (SiPM) `remove_points` | `graph::trim` (non-mutating) | **done** |
| F-44 | (SiPM) `average_graphs` | `graph::mean_of` | **done** |
| F-45 | (SiPM) `swapxy` (errors) | `graph::swap_xy(TGraphErrors)` overload | **done** |
| F-55 | (SiPM) `project` | `histo::project_y` | **done** |
| F-56 | (SiPM) `fill_profile` / `fill_persistance` / `fill_from_interval` | `histo::fill_profile` / `fill_persistence` / `fill_from_interval` | **done** |
| F-57 | (SiPM) `TProfile_to_TGraphErorrs` | `graph::from_profile` (fixed last-bin drop) | **done** |
| F-49 | (SiPM) `get_amplitude` / `get_graph_max` | `signal::amplitude` / `extremum` | **done** |
| F-50 | (SiPM) `get_transitions` / `find_first/last_above` | `signal::threshold_crossings` / `first_above` / `last_above` | **done** |
| F-51 | (SiPM) `maximum_filter` | `signal::maximum_filter` | **done** |
| F-52 | (SiPM) `gauss_filter` | `signal::gaussian_filter` (real Gaussian weights; original was a mislabelled plain average) | **done** |
| F-53 | (SiPM) `find_peaks` | `signal::find_peaks` (stripped the `new TCanvas()`/`Draw()` side-effect) | **done** |
| F-54 | (SiPM) `integrate` | `signal::integrate` | **done** |
| F-58 | (SiPM) `get_intercept` (two pol1) | `fit::line_intersection` — **deferred**: needs the `general_utility.h` error-propagation formula reviewed before porting. | not started |

New module: `mist::hep::signal` (`signal/waveform.h`).

**SiPM items deliberately skipped:** `measure_noisepower` (SiPM-specific magic
constants), `set_style` / `style` / `draw` / `standard_canvas` (cosmetic, per
policy), `write_all(TObjects)` (app aggregate), `make_log_th1` (covered by
`histo::log_binning`), `s_ts` / `swap_values` (trivial/niche), the
`internal_generation_counter` + `Form`-named-clone pattern (superseded by
`mist::hep::owned`). The `analysis_utils.h` ALCOR indexing
(`get_eochannel`, `get_global_index`) is detector-application code, not a
primitive.

**Consumer switch (pending):** point `sipm-characterisation`'s macros at
these functions. Gated on the integration model (it is ROOT-macro based, not
a compiled CMake project) — to be settled by inspecting its build.

---

## Coding conventions

Naming, output, file-layout, and include-order rules live in
[`docs/CODING_CONVENTIONS.md`](docs/CODING_CONVENTIONS.md). They are
inherited verbatim from
[mist](https://github.com/Nikolajal/mist/blob/main/docs/CODING_CONVENTIONS.md)
— only the macro prefix changes (`MIST_*` → `MIST_HEP_*`).

---

*This file is the canonical hub. Edits land in git, get published to the
Doxygen Pages site on the next push to `main`, and are reviewable like any
other source change.*
