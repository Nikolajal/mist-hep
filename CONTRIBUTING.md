# Contributing to mist-hep

This document defines the conventions and workflow expected for changes to
mist-hep. The substance — design decisions, open questions, AAU port queue —
lives in [`DISCUSSION.md`](DISCUSSION.md); the code style lives in
[`docs/CODING_CONVENTIONS.md`](docs/CODING_CONVENTIONS.md); this file
covers process.

mist-hep inherits its conventions and branching model verbatim from the
parent [mist](https://github.com/Nikolajal/mist) repository. Only the
macro prefix differs (`MIST_HEP_*` instead of `MIST_*`).

## Versioning and stability contract

mist-hep follows [Semantic Versioning](https://semver.org/) with the
`SameMajorVersion` compatibility policy declared in
[`CMakeLists.txt`](CMakeLists.txt).

| Phase | Contract |
|---|---|
| Pre-1.0 (current) | No backward-compatibility guarantees. Any release may break any consumer. Active development happens on `main` directly until M-2 (0.2.0); from M-6 (1.0.0) the branching model below activates. |
| Post-1.0 | Breaking changes require a major-version bump. Additive changes within `1.x` are permitted. Removal of any symbol requires at least one minor release with a `[[deprecated]]` notice first. |

mist-hep's own version trajectory is independent of mist's. The dependency
on mist is declared via `find_package(mist X REQUIRED)` and follows
`SameMajorVersion`; bumping the mist major version is a coordinated change
that justifies a mist-hep minor (pre-1.0) or major (post-1.0) bump.

## Branching model (effective at v1.0.0)

| Branch | Role |
|---|---|
| `main` | Delivered software. Default GitHub branch. Docs published from here. |
| `dev` | Integration branch (created at v1.0.0). |
| `dev_patch_<name>` | Short-lived bug-fix branch off `dev`. |
| `dev_<feature_name>` | Short-lived feature branch off `dev`. |

Release procedure mirrors mist's: substantive work on `dev` via topic
branches, `dev → main` merge at release time, tag `main`, docs workflow
auto-fires on the `main` push.

## Build and test

mist-hep requires mist to be findable by CMake. Until mist's build-tree
export lands (mist `T-01`), install mist to a prefix first:

```bash
cmake --install /path/to/mist/build --prefix /tmp/mist-install
```

Then:

```bash
cmake -B build \
      -DMIST_HEP_BUILD_TESTS=ON \
      -Dmist_DIR=/tmp/mist-install/lib/cmake/mist
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Tests must pass on the working branch before any merge.

## Toolchain baseline

mist-hep targets **C++20** from v0.2.0 onwards, matching mist v1.0.0.
Minimum supported compilers as for mist (GCC ≥ 10, Clang ≥ 10, AppleClang
Xcode 12.5+, MSVC ≥ 19.29).

Additional runtime requirement:

| Dependency | Minimum version | Components used |
|---|---|---|
| [ROOT](https://root.cern/) | 6.24 | `Core`, `Hist`, `Graf`, `Gpad`, `RIO`, `Tree`, `MathCore`, `Matrix`, `Minuit` |
| [mist](https://github.com/Nikolajal/mist) | per `find_package(mist X REQUIRED)` in `CMakeLists.txt` | All |

RooFit is intentionally not linked at v0.2.0; the dependency will be
re-introduced when the first port requires it (likely the systematics
Barlow pipeline at M-5).

## Coding conventions

Defined in [`docs/CODING_CONVENTIONS.md`](docs/CODING_CONVENTIONS.md),
inherited from mist with the macro-prefix substitution.

## Sibling repository

mist-hep is a sibling of [mist](https://github.com/Nikolajal/mist); changes
that affect the family conventions or that touch both repositories should
be reviewed in coordination.

## AAU port queue

mist-hep is a clean-room rewrite of the salvageable parts of
AliAnalysisUtility (AAU). The port queue lives in
[`DISCUSSION.md`](DISCUSSION.md#aau-port-queue); each entry includes the
source AAU symbol and the target mist-hep symbol. New ports should:

1. Pick the next entry from the queue (or open a discussion to insert one).
2. Fix bugs and convention violations in the port rather than carrying
   them forward.
3. Update the queue entry with the commit SHA when landed.
