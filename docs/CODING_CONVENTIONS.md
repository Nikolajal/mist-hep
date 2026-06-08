# Coding conventions

Naming and structural conventions followed across mist-hep. Reference
material for new contributors and code review — not a discussion log. These
conventions are inherited verbatim from mist; only the macro prefix and
example pool change.

## Naming

| Kind                                       | Style                       | Examples                                                  |
|--------------------------------------------|-----------------------------|-----------------------------------------------------------|
| Variables, free functions, methods         | `snake_case`                | `n_sigma`, `quadrature_sum`, `barlow_parameter`, `rng`    |
| Private members                            | `snake_case_`               | `params_`, `formula_`                                     |
| Classes, structs, type aliases             | `PascalCase`                | (none yet — add as the surface grows)                     |
| Enum values (incl. legacy plain enums)     | `PascalCase`                |                                                           |
| Project macros                             | `MIST_HEP_` + `ALL_CAPS`    | `MIST_HEP_BUILD_TESTS`                                    |
| Namespaces                                 | `lowercase`                 | `mist::hep`, `mist::hep::stats`, `mist::hep::fit`         |
| Filenames (incl. macro entry-points)       | `snake_case`                | `stats.h`, `functions.cxx`, `globals.h`                   |

## Local-vs-type collision rule

When a local variable would otherwise be spelled identically to its type
(`TF1 tf1`, `TH1F th1f`), break the collision with one of:

1. **`current_` prefix** when the variable is simply "the one being processed
   right now": `TF1 current_fit;`, `TH1F current_spectrum;`.
2. **A semantic role name** when the context warrants more specificity:
   `TF1 levy_tsallis_fit`, `TH1F reference_spectrum`, `TH1F numerator`.

Prefer the semantic role when several instances of the same type appear in
the same scope; reach for `current_` when no better name is available.

## Output: never `std::cout` / `std::cerr` / `printf`

All textual output goes through `mist::logger`. mist-hep links `mist::mist`
publicly, so the logger is always available.

```cpp
// no
std::cout << "fitted " << n << " spectra\n";
std::cerr << "bad chi2 at point " << i << "\n";
std::printf("done\n");

// yes
mist::logger::info("fitted {} spectra", n);
mist::logger::warning("bad chi2 at point {}", i);
mist::logger::done("done");
```

**ROOT-specific exception.** ROOT's minimiser, RooFit, and many `TF1`/`TH1`
methods print to `std::cout` directly. Wrap those calls in
`mist::logger::ScopedCoutToMist` so their output is routed through the
logger's anchor protocol instead of corrupting in-flight progress bars:

```cpp
{
    mist::logger::ScopedCoutToMist redirect;
    spectrum->Fit(levy_tsallis_fit.get(), "RMQ");   // ROOT chatter -> logger
}
```

Tests may use `std::cout` / `std::cerr` / `std::printf` for terse assertion
macros where pulling in the logger would obscure the test itself (see
`test/tester_stats.cxx`). Keep this minimal and prefer the logger where it
reads cleanly.

## File layout

Public headers live under `include/mist/hep/<module>/`; implementation TUs
under `src/hep/<module>/`. The directory under `include/mist/hep/` is the
namespace under `mist::hep::` — `include/mist/hep/fit/functions.h` declares
symbols in `mist::hep::fit`. Don't break this mapping.

Note that headers go under `include/mist/hep/` (not `include/mist-hep/`) so
consumers see one `mist/` include root regardless of which sibling library a
header came from.

## Include order

Inside each TU, group includes top-to-bottom and separate groups with a
blank line:

1. The header this TU implements (for `.cxx` files only)
2. C / C++ standard library
3. ROOT headers
4. Other `mist::*` modules
5. Project-internal headers from the same module
