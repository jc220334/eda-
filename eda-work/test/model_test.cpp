// Console test harness for the model layer (shapes, footprints, library XML).
#include "../src/parts/01_head.cpp"
#include "../src/parts/02_footprint.cpp"

#include <wx/init.h>
#include <wx/filename.h>
#include <wx/log.h>

#include <cstdio>
#include <map>

static int g_fail = 0;
static int g_pass = 0;
static void Trace(const wxString& s) { fprintf(stderr, "[trace] %s\n", (const char*)s.utf8_str()); fflush(stderr); }

static void Check(bool cond, const wxString& what)
{
    if (cond) { ++g_pass; }
    else { ++g_fail; fprintf(stderr, "FAIL: %s\n", (const char*)what.utf8_str()); fflush(stderr); }
}

static std::map<wxString, int> ShapeTypes(const Footprint& fp)
{
    std::map<wxString, int> m;
    for (const auto& s : fp.shapes) m[s->TypeName()]++;
    return m;
}

int main(int argc, char** argv)
{
    wxInitializer initializer(argc, argv);
    if (!initializer.IsOk()) { wxPrintf("wx init failed\n"); return 2; }

    Trace("create sample");
    auto lib = Library::CreateSample();
    Trace(wxString::Format("sample items %d", (int)lib->items.size()));
    Check(lib->items.size() >= 8, "sample library has at least 8 footprints");
    Check(lib->Find("SOIC-8") != nullptr, "sample lookup is case-insensitive");
    Check(lib->NameExists("soic-8"), "name uniqueness is case-insensitive");
    Check(!lib->NameExists("soic-8", lib->Find("SOIC-8")->uid), "name uniqueness can exclude self");
    Check(lib->MakeUniqueName("SOIC-8") != "SOIC-8", "MakeUniqueName avoids duplicates");

    Trace("validation loop");
    for (const auto& fp : lib->items) {
        wxArrayString errors;
        bool ok = fp->Validate(errors);
        wxString detail;
        for (const auto& e : errors) detail += "  " + e + "\n";
        Check(ok, "sample validates: " + fp->name + (ok ? wxString() : "\n" + detail));
    }

    Trace("validation failures");
    {
        Footprint fp;
        fp.name = "Bad";
        wxArrayString errors;
        Check(!fp.Validate(errors), "empty footprint fails validation");
        bool found = false;
        for (const auto& e : errors) if (e.Contains("at least one pad")) found = true;
        Check(found, "missing pads reported");

        Pad p; p.number = "1"; p.x = 0; p.y = 0; p.w = 1; p.h = 1;
        fp.pads.push_back(p);
        Pad q = p; q.number = "1"; fp.pads.push_back(q);
        errors.Clear();
        Check(!fp.Validate(errors), "duplicate pin numbers fail validation");
        found = false;
        for (const auto& e : errors) if (e.Contains("Duplicate pin number")) found = true;
        Check(found, "duplicate pin reported");

        fp.pads[1].number = "2";
        fp.pads[1].w = 0.0;
        errors.Clear();
        Check(!fp.Validate(errors), "zero pad width fails validation");

        fp.pads[1].w = 1.0;
        fp.pads[0].type = PAD_THT;
        fp.pads[0].drill = 2.0;
        errors.Clear();
        Check(!fp.Validate(errors), "oversized drill fails validation");

        fp.pads[0].drill = 0.5;
        fp.name.clear();
        errors.Clear();
        Check(!fp.Validate(errors), "empty name fails validation");
    }

    Trace("xml save");
    wxString dir = "E:\\202691\\eda-work\\testout";
    wxFileName::Mkdir(dir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    wxString path = dir + "\\roundtrip.elib";

    wxString err;
    Check(lib->Save(path, err), "library save succeeds: " + err);
    Check(wxFileName(path).FileExists(), "library file exists after save");

    Trace("xml load");
    Library loaded;
    wxString warnings;
    Check(loaded.Load(path, warnings), "library load succeeds");
    Check(loaded.items.size() == lib->items.size(), "loaded footprint count matches");

    Trace("compare loop");
    for (const auto& original : lib->items) {
        Trace("compare " + original->name);
        Footprint* copy = loaded.Find(original->name);
        Check(copy != nullptr, "loaded footprint present: " + original->name);
        if (!copy) continue;
        Check(copy->uid == original->uid, "uid preserved: " + original->name);
        Check(copy->category == original->category, "category preserved: " + original->name);
        Check(copy->unit == original->unit, "unit preserved: " + original->name);
        Check(copy->description == original->description, "description preserved: " + original->name);
        Check(copy->pads.size() == original->pads.size(), "pad count preserved: " + original->name);
        Check(copy->shapes.size() == original->shapes.size(), "object count preserved: " + original->name);
        Check(ShapeTypes(*copy) == ShapeTypes(*original), "object types preserved: " + original->name);

        wxRect2DDouble a = original->Bounds();
        wxRect2DDouble b = copy->Bounds();
        Check(std::abs(a.m_width - b.m_width) < 0.002 && std::abs(a.m_height - b.m_height) < 0.002,
              "bounds preserved: " + original->name);

        for (size_t i = 0; i < std::min(copy->pads.size(), original->pads.size()); ++i) {
            const Pad& p = original->pads[i];
            const Pad& q = copy->pads[i];
            Check(p.number == q.number && std::abs(p.x - q.x) < 0.002 && std::abs(p.w - q.w) < 0.002 &&
                  p.type == q.type && p.shape == q.shape && p.layer == q.layer,
                  wxString::Format("pad %s preserved: %s", p.number, original->name));
        }
    }

    Trace("second round trip");
    Footprint* soic = loaded.Find("SOIC-8");
    if (soic) {
        Trace(wxString::Format("soic pads=%d shapes=%d", (int)soic->pads.size(), (int)soic->shapes.size()));
        if (soic->pads.empty() || soic->shapes.empty()) { fprintf(stderr, "SOIC CONTENTS MISSING\n"); fflush(stderr); return 3; }
        soic->name = "SOIC-8_Renamed";
        soic->pads[0].number = "A1";
        soic->shapes[0]->rotation = 15.0;
        wxString err2;
        Check(loaded.Save(path, err2), "edited library saves");
        Library again;
        wxString warn2;
        Check(again.Load(path, warn2), "edited library reloads");
        Footprint* againFp = again.Find("SOIC-8_Renamed");
        Check(againFp != nullptr, "renamed footprint reloads");
        if (againFp) {
            Trace(wxString::Format("again pads=%d shapes=%d", (int)againFp->pads.size(), (int)againFp->shapes.size()));
            if (againFp->pads.empty() || againFp->shapes.empty()) { fprintf(stderr, "RELOADED CONTENTS MISSING\n"); fflush(stderr); return 4; }
            Check(againFp->pads[0].number == "A1", "edited pad number persists");
            Check(std::abs(againFp->shapes[0]->rotation - 15.0) < 0.002, "edited rotation persists");
        }
    }

    Trace("deep copy");
    if (soic) {
        Footprint copy = *soic;
        copy.name = "COPY";
        copy.shapes[0]->rotation = 90.0;
        copy.pads[0].number = "Z9";
        Check(soic->shapes[0]->rotation != 90.0, "deep copy does not share shapes");
        Check(soic->pads[0].number != "Z9", "deep copy does not share pads");
    }

    Trace("helpers");
    {
        Footprint f;
        f.name = "Nums";
        Pad a; a.number = "1"; a.w = 1; a.h = 1;
        Pad b; b.number = "3"; b.w = 1; b.h = 1;
        f.AddPad(a); f.AddPad(b);
        Check(f.NextPadNumber() == "4", "NextPadNumber increments highest number");
        Check(f.HasPadNumber("3"), "HasPadNumber finds existing number");
        Check(!f.HasPadNumber("9"), "HasPadNumber rejects missing number");
    }

    wxPrintf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}


