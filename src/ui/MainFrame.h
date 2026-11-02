#pragma once

#include "core/Project.h"
#include "ui/BoardCanvas.h"

#include <wx/wx.h>

#include <filesystem>

class MainFrame : public wxFrame {
public:
    explicit MainFrame(const std::filesystem::path& repositoryRoot);

private:
    void BuildInterface();
    void Log(const wxString& message);
    void LoadSample();
    void ImportNetlist();
    void OpenProject();
    void SaveProject();
    void AutoPlace();
    void AutoRoute();
    void AddVia();
    void AddGroundPour();
    void RotateSelected();
    void ValidateBoard();
    void FitBoard();
    bool LoadNetlistFiles(const std::string& netlistPath, const std::string& libraryPath);

    std::filesystem::path m_repositoryRoot;
    pcb::PcbProject m_project;
    BoardCanvas* m_canvas = nullptr;
    wxTextCtrl* m_log = nullptr;
    wxChoice* m_layerChoice = nullptr;
};
