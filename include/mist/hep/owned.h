// SPDX-License-Identifier: MIT
//
// mist/hep/owned.h — correct ownership for ROOT objects.
//
// ROOT's object model fights RAII: a TH1 registers itself in the current
// gDirectory on construction, so a TFile that is open at construction time
// will *also* own the histogram and delete it on close — double-freeing
// anything that wrapped it in a std::unique_ptr. This module encodes ROOT's
// ownership rules in one place so callers do not have to.
//
// The contract is "detach-on-create, own-via-smart-pointer":
//   - Objects produced by make()/clone()/adopt() are detached from ROOT's
//     global registries (histograms: SetDirectory(nullptr)).
//   - A detached object is owned solely by its root_ptr; the root_deleter
//     deletes it normally.
//   - Persisting is still safe: TObject::Write() on a detached object
//     serialises a *copy* into the file — it does not transfer ownership —
//     so the root_ptr remains the sole owner and there is no double free.
//
// Scope of the guarantee: this makes *ownership* safe (no double-free, no
// leak). It does NOT prevent use-after-free of a dangling TObject* or any
// thread-safety issue — hence "owned", not "memory".
//
#pragma once

#include <memory>
#include <type_traits>
#include <utility>

#include <TObject.h>
#include <TH1.h>

namespace mist::hep::owned {

// ---------------------------------------------------------------------------
// root_deleter: ownership-correct deletion for any TObject-derived type.
//
// A single runtime-dispatch deleter (rather than per-type specialisations):
// the per-call dynamic_cast cost is irrelevant for analysis-lifetime objects,
// and one deleter keeps root_ptr<T> a single coherent type family.
//
// Histograms are detached from any directory before deletion as a safety net
// (make/clone/adopt already detach at creation; this covers root_ptr<T>
// built directly from a raw pointer). TF1 removes itself from
// gROOT's function list in its own destructor, and TGraph is not registered
// anywhere, so both need only a plain delete.
// ---------------------------------------------------------------------------
struct root_deleter {
    void operator()(TObject* object) const noexcept
    {
        if (!object) return;
        if (auto* histogram = dynamic_cast<TH1*>(object))
            histogram->SetDirectory(nullptr);
        delete object;
    }
};

// Owning handle for a ROOT object with ownership-correct deletion.
template <typename T>
using root_ptr = std::unique_ptr<T, root_deleter>;

// ---------------------------------------------------------------------------
// make: construct a T from the given arguments, detach it if it is a
// histogram, and return sole ownership.
// ---------------------------------------------------------------------------
template <typename T, typename... Args>
[[nodiscard]] root_ptr<T> make(Args&&... args)
{
    static_assert(std::is_base_of_v<TObject, T>,
                  "mist::hep::owned::make requires a TObject-derived type");
    auto* raw = new T(std::forward<Args>(args)...);
    if constexpr (std::is_base_of_v<TH1, T>) raw->SetDirectory(nullptr);
    return root_ptr<T>(raw);
}

// ---------------------------------------------------------------------------
// clone: deep-copy an existing object via TObject::Clone, detach the copy if
// it is a histogram, and return sole ownership of the copy.
// ---------------------------------------------------------------------------
template <typename T>
[[nodiscard]] root_ptr<T> clone(const T& source)
{
    static_assert(std::is_base_of_v<TObject, T>,
                  "mist::hep::owned::clone requires a TObject-derived type");
    auto* raw = static_cast<T*>(source.Clone());
    if constexpr (std::is_base_of_v<TH1, T>) raw->SetDirectory(nullptr);
    return root_ptr<T>(raw);
}

// ---------------------------------------------------------------------------
// adopt: take ownership of an existing raw pointer (e.g. one returned by a
// ROOT API), detaching it if it is a histogram. Pass a pointer you would
// otherwise have to `delete` yourself; afterwards the root_ptr owns it.
// ---------------------------------------------------------------------------
template <typename T>
[[nodiscard]] root_ptr<T> adopt(T* raw)
{
    static_assert(std::is_base_of_v<TObject, T>,
                  "mist::hep::owned::adopt requires a TObject-derived type");
    if (raw) {
        if constexpr (std::is_base_of_v<TH1, T>) raw->SetDirectory(nullptr);
    }
    return root_ptr<T>(raw);
}

// ---------------------------------------------------------------------------
// scoped_directory_off: RAII guard that disables TH1 directory auto-attachment
// for its lifetime and restores the previous setting on exit. Use around a
// block that creates many histograms through ROOT APIs you do not control, so
// none of them attach to the current gDirectory:
//
//   {
//       mist::hep::owned::scoped_directory_off guard;
//       // every TH1 created here is born detached
//   }
// ---------------------------------------------------------------------------
class scoped_directory_off {
public:
    scoped_directory_off() : previous_(TH1::AddDirectoryStatus())
    {
        TH1::AddDirectory(false);
    }
    ~scoped_directory_off() { TH1::AddDirectory(previous_); }

    scoped_directory_off(const scoped_directory_off&) = delete;
    scoped_directory_off& operator=(const scoped_directory_off&) = delete;
    scoped_directory_off(scoped_directory_off&&) = delete;
    scoped_directory_off& operator=(scoped_directory_off&&) = delete;

private:
    bool previous_;
};

}  // namespace mist::hep::owned
