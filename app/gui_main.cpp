#include "ui/MainFrame.h"

#include <wx/wx.h>
#include <wx/stdpaths.h>
#include <wx/strconv.h>

#include <filesystem>

namespace {

std::filesystem::path SearchUpwards(std::filesystem::path current)
{
    for (int depth = 0; depth < 32; ++depth) {
        if (std::filesystem::exists(
                current / "examples" / "netlist-basic" / "sample.net.json") &&
            std::filesystem::exists(current / "CMakeLists.txt")) {
            return current;
        }
        if (!current.has_parent_path() || current.parent_path() == current) {
            break;
        }
        current = current.parent_path();
    }
    return {};
}

std::filesystem::path FindRepositoryRoot()
{
    std::filesystem::path found = SearchUpwards(std::filesystem::current_path());
    if (!found.empty()) {
        return found;
    }
    const wxString executable = wxStandardPaths::Get().GetExecutablePath();
    found = SearchUpwards(
        std::filesystem::path(executable.ToStdWstring()).parent_path());
    return found.empty() ? std::filesystem::current_path() : found;
}

class EdaApp : public wxApp {
public:
    bool OnInit() override
    {
        wxConvCurrent = &wxConvUTF8;
        auto* frame = new MainFrame(FindRepositoryRoot());
        frame->Show();
        return true;
    }
};

} // namespace

wxIMPLEMENT_APP(EdaApp);
