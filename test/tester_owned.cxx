// tester_owned.cxx — exercises mist::hep::owned ownership helpers.
//
// The central guarantee is that objects produced by the module are detached
// from gDirectory, so they are not double-owned when a TFile is open. The
// decisive test creates histograms while a TFile is open and confirms (a)
// they are detached and (b) closing the file does not delete them out from
// under the owning root_ptr (no double-free / no use-after-free at scope end).

#include "mist/hep/owned.h"

#include <cstdio>
#include <cstdlib>

#include <TF1.h>
#include <TFile.h>
#include <TGraph.h>
#include <TH1.h>
#include <TH1F.h>

namespace o = mist::hep::owned;

namespace {

int failures = 0;

void check(bool cond, const char* what) {
    if (!cond) { std::printf("  FAIL: %s\n", what); ++failures; }
}

}  // namespace

int main() {
    // ------------------------------------------------------------------
    std::puts("[tester_owned] make / detachment");

    {
        auto h = o::make<TH1F>("om_h", "om_h", 10, 0, 10);
        check(h != nullptr, "make returns non-null");
        check(h->GetDirectory() == nullptr, "make<TH1F> is detached from gDirectory");
    }

    // TGraph is not a histogram — make still works, nothing to detach.
    {
        auto g = o::make<TGraph>();
        check(g != nullptr, "make<TGraph> non-null");
        g->SetPoint(0, 1.0, 2.0);
        check(g->GetN() == 1, "make<TGraph> usable");
    }

    // ------------------------------------------------------------------
    std::puts("[tester_owned] clone / adopt");

    {
        TH1F src("om_src", "om_src", 5, 0, 5);
        src.SetBinContent(1, 3.0);
        auto c = o::clone(src);
        check(c->GetDirectory() == nullptr, "clone is detached");
        check(c->GetBinContent(1) == 3.0, "clone copies content");
        check(c->GetName() != nullptr, "clone has a name");
    }
    {
        // adopt a raw histogram (as if returned by a ROOT API).
        auto* raw = new TH1F("om_adopt", "om_adopt", 4, 0, 4);
        auto a = o::adopt(raw);
        check(a.get() == raw, "adopt takes the same pointer");
        check(a->GetDirectory() == nullptr, "adopt detaches the histogram");
    }

    // ------------------------------------------------------------------
    std::puts("[tester_owned] scoped_directory_off");

    {
        const bool before = TH1::AddDirectoryStatus();
        {
            o::scoped_directory_off guard;
            check(TH1::AddDirectoryStatus() == false, "guard disables AddDirectory");
            // A bare TH1 created here is born detached.
            TH1F local("om_local", "om_local", 3, 0, 3);
            check(local.GetDirectory() == nullptr, "bare TH1 detached inside guard");
        }
        check(TH1::AddDirectoryStatus() == before, "guard restores previous status");
    }

    // ------------------------------------------------------------------
    std::puts("[tester_owned] no double-free with an open TFile (the core case)");

    {
        TFile f("tester_owned_tmp.root", "RECREATE");
        // Created while the file is open. Without detachment, the file would
        // also own these and delete them on Close(), double-freeing the
        // root_ptr-owned objects at scope exit.
        auto h = o::make<TH1F>("om_infile", "om_infile", 10, 0, 10);
        h->FillRandom("gaus", 100);
        check(h->GetDirectory() == nullptr, "histogram detached despite open file");
        h->Write();          // serialises a copy into the file; ownership stays with root_ptr
        f.Close();           // file deletes only what it owns — not our detached histogram
        check(h != nullptr && h->GetEntries() == 100,
              "histogram still valid after file Close()");
        // h's root_ptr deletes the (detached) histogram here — no double free.
    }
    std::remove("tester_owned_tmp.root");

    // ------------------------------------------------------------------
    std::puts("[tester_owned] TF1 ownership");

    {
        auto f = o::make<TF1>("om_f", "[0]*x", 0, 1);
        f->SetParameter(0, 2.0);
        check(std::abs(f->Eval(3.0) - 6.0) < 1e-9, "TF1 owned and usable");
        // root_ptr deletes; TF1 dtor de-registers from gROOT's function list.
    }

    // ------------------------------------------------------------------
    if (failures) {
        std::printf("[tester_owned] %d failure(s)\n", failures);
        return EXIT_FAILURE;
    }
    std::puts("[tester_owned] OK");
    return EXIT_SUCCESS;
}
