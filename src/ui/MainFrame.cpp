#include "ui/MainFrame.h"

#include "io/ProjectIO.h"
#include "pcb/LayoutService.h"
#include "pcb/NetlistImporter.h"
#include "pcb/RoutingService.h"

#include <wx/filedlg.h>

#include <algorithm>
#include <string>

namespace {

enum {
    ID_LOAD_SAMPLE = wxID_HIGHEST + 100,
    ID_IMPORT_NETLIST,
    ID_AUTO_PLACE,
    ID_AUTO_ROUTE,
    ID_ADD_VIA,
    ID_ADD_POUR,
    ID_ROTATE,
    ID_VALIDATE,
    ID_FIT_BOARD,
};

wxString U8(const char* text)
{
    return wxString::FromUTF8(text);
}

} // namespace

MainFrame::MainFrame(const std::filesystem::path& repositoryRoot)
    : wxFrame(nullptr, wxID_ANY, U8("PCB EDA - 课程设计")),
      m_repositoryRoot(repositoryRoot)
{
    SetMinSize(wxSize(980, 680));
    BuildInterface();
    SetSize(wxSize(1260, 820));
    Centre();
    LoadSample();
}

void MainFrame::BuildInterface()
{
    wxPanel* root = new wxPanel(this);
    wxBoxSizer* rootSizer = new wxBoxSizer(wxVERTICAL);

    wxPanel* toolbar = new wxPanel(root);
    wxBoxSizer* toolbarSizer = new wxBoxSizer(wxHORIZONTAL);
    const auto addButton = [&](const wxString& label, int id) {
        wxButton* button = new wxButton(toolbar, id, label, wxDefaultPosition, wxSize(-1, 32));
        toolbarSizer->Add(button, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 6);
    };

    addButton(U8("载入样例"), ID_LOAD_SAMPLE);
    addButton(U8("导入网表"), ID_IMPORT_NETLIST);
    addButton(U8("打开工程"), wxID_OPEN);
    addButton(U8("保存工程"), wxID_SAVE);
    toolbarSizer->AddSpacer(10);
    addButton(U8("自动布局"), ID_AUTO_PLACE);
    addButton(U8("自动布线"), ID_AUTO_ROUTE);
    addButton(U8("放置过孔"), ID_ADD_VIA);
    addButton(U8("GND 敷铜"), ID_ADD_POUR);
    addButton(U8("旋转元件"), ID_ROTATE);
    addButton(U8("连接检查"), ID_VALIDATE);
    addButton(U8("适合窗口"), ID_FIT_BOARD);

    toolbarSizer->AddStretchSpacer();
    toolbarSizer->Add(new wxStaticText(toolbar, wxID_ANY, U8("当前层")), 0,
                      wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    m_layerChoice = new wxChoice(toolbar, wxID_ANY);
    m_layerChoice->Append("F.Cu");
    m_layerChoice->Append("B.Cu");
    m_layerChoice->SetSelection(0);
    toolbarSizer->Add(m_layerChoice, 0, wxALIGN_CENTER_VERTICAL);
    toolbar->SetSizer(toolbarSizer);
    toolbarSizer->SetMinSize(-1, 40);

    m_canvas = new BoardCanvas(root);
    m_canvas->SetStatusCallback([this](const wxString& message) { Log(message); });

    m_log = new wxTextCtrl(
        root, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(-1, 145),
        wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    m_log->SetBackgroundColour(wxColour(24, 29, 26));
    m_log->SetForegroundColour(wxColour(210, 222, 214));

    rootSizer->Add(toolbar, 0, wxEXPAND | wxALL, 7);
    rootSizer->Add(m_canvas, 1, wxEXPAND | wxLEFT | wxRIGHT, 7);
    rootSizer->Add(m_log, 0, wxEXPAND | wxALL, 7);
    root->SetSizer(rootSizer);

    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { LoadSample(); }, ID_LOAD_SAMPLE);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ImportNetlist(); }, ID_IMPORT_NETLIST);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OpenProject(); }, wxID_OPEN);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { SaveProject(); }, wxID_SAVE);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { AutoPlace(); }, ID_AUTO_PLACE);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { AutoRoute(); }, ID_AUTO_ROUTE);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { AddVia(); }, ID_ADD_VIA);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { AddGroundPour(); }, ID_ADD_POUR);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { RotateSelected(); }, ID_ROTATE);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ValidateBoard(); }, ID_VALIDATE);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { FitBoard(); }, ID_FIT_BOARD);
    Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        m_canvas->SetActiveLayer(m_layerChoice->GetSelection() == 1 ? "B.Cu" : "F.Cu");
    }, m_layerChoice->GetId());

    wxMenu* fileMenu = new wxMenu();
    fileMenu->Append(ID_LOAD_SAMPLE, U8("载入样例\tCtrl+L"));
    fileMenu->Append(ID_IMPORT_NETLIST, U8("导入网表\tCtrl+I"));
    fileMenu->Append(wxID_OPEN, U8("打开工程\tCtrl+O"));
    fileMenu->Append(wxID_SAVE, U8("保存工程\tCtrl+S"));
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT, U8("退出"));

    wxMenu* editMenu = new wxMenu();
    editMenu->Append(ID_AUTO_PLACE, U8("自动布局"));
    editMenu->Append(ID_AUTO_ROUTE, U8("自动布线"));
    editMenu->Append(ID_ADD_VIA, U8("放置过孔"));
    editMenu->Append(ID_ADD_POUR, U8("创建 GND 敷铜"));
    editMenu->Append(ID_ROTATE, U8("旋转选中元件"));
    editMenu->Append(ID_VALIDATE, U8("连接与间距检查"));
    editMenu->Append(ID_FIT_BOARD, U8("适合窗口"));

    wxMenuBar* menuBar = new wxMenuBar();
    menuBar->Append(fileMenu, U8("文件"));
    menuBar->Append(editMenu, "PCB");
    SetMenuBar(menuBar);

    Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(true); }, wxID_EXIT);
}

void MainFrame::Log(const wxString& message)
{
    if (!m_log) {
        return;
    }
    m_log->AppendText(message + "\n");
    m_log->ShowPosition(m_log->GetLastPosition());
}

bool MainFrame::LoadNetlistFiles(
    const std::string& netlistPath,
    const std::string& libraryPath)
{
    pcb::PcbProject imported;
    const pcb::NetlistImportResult result =
        pcb::NetlistImporter::importFiles(netlistPath, libraryPath, imported);
    if (!result.ok) {
        Log(U8("网表导入失败："));
        Log(wxString::FromUTF8(result.summary()));
        return false;
    }

    m_project = std::move(imported);
    const pcb::LayoutResult layout =
        pcb::LayoutService::autoPlace(m_project.board, 3.0, 2.0, 0.5);
    Log(U8("网表导入成功：") + wxString::FromUTF8(
        "元件 " + std::to_string(result.componentCount) +
        "，网络 " + std::to_string(result.netCount) +
        "，匹配引脚 " + std::to_string(result.matchedPinCount)));
    Log(U8("自动布局：") + wxString::FromUTF8(layout.message));
    m_canvas->SetProject(&m_project);
    m_canvas->FitBoard();
    return true;
}

void MainFrame::LoadSample()
{
    const std::filesystem::path netlist =
        m_repositoryRoot / "examples" / "netlist-basic" / "sample.net.json";
    const std::filesystem::path library =
        m_repositoryRoot / "examples" / "netlist-basic" / "sample.lib.json";
    if (LoadNetlistFiles(netlist.string(), library.string())) {
        Log(U8("已载入内置样例。左键点击焊盘开始手动布线，右键旋转元件。"));
    }
}

void MainFrame::ImportNetlist()
{
    wxFileDialog dialog(
        this, U8("选择网表文件"), wxString::FromUTF8(m_repositoryRoot.u8string()), "",
        "Netlist JSON (*.net.json;*.json)|*.json", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }
    const std::filesystem::path library =
        m_repositoryRoot / "examples" / "netlist-basic" / "sample.lib.json";
    LoadNetlistFiles(dialog.GetPath().ToStdString(), library.string());
}

void MainFrame::OpenProject()
{
    wxFileDialog dialog(
        this, U8("打开 PCB 工程"), wxString::FromUTF8(m_repositoryRoot.u8string()), "",
        "PCB Project (*.pcbproj)|*.pcbproj|JSON files (*.json)|*.json",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }
    pcb::PcbProject loadedProject;
    const pcb::LoadResult result =
        pcb::ProjectIO::loadFromFile(dialog.GetPath().ToStdString(), loadedProject);
    if (!result.ok) {
        Log(U8("工程打开失败：") + wxString::FromUTF8(result.error));
        return;
    }
    m_project = std::move(loadedProject);
    m_canvas->SetProject(&m_project);
    Log(U8("工程已打开：") + dialog.GetPath());
}

void MainFrame::SaveProject()
{
    wxFileDialog dialog(
        this, U8("保存 PCB 工程"), wxString::FromUTF8(m_repositoryRoot.u8string()), "project.pcbproj",
        "PCB Project (*.pcbproj)|*.pcbproj",
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }
    std::string error;
    if (!pcb::ProjectIO::saveToFile(
            m_project, dialog.GetPath().ToStdString(), error)) {
        Log(U8("保存失败：") + wxString::FromUTF8(error));
        return;
    }
    Log(U8("工程已保存：") + dialog.GetPath());
}

void MainFrame::AutoPlace()
{
    const pcb::LayoutResult result =
        pcb::LayoutService::autoPlace(m_project.board, 3.0, 2.0, 0.5);
    Log(U8("自动布局：") + wxString::FromUTF8(result.message));
    m_canvas->Refresh();
}

void MainFrame::AutoRoute()
{
    const pcb::RouteResult result =
        pcb::RoutingService::autoRoute(m_project.board, 0.25);
    Log(wxString::Format(
        U8("自动布线：%s；新增走线 %zu，过孔 %zu"),
        wxString::FromUTF8(result.message), result.traceCount, result.viaCount));
    if (!result.unresolvedNets.empty()) {
        wxString names;
        for (const std::string& name : result.unresolvedNets) {
            if (!names.empty()) {
                names += ", ";
            }
            names += wxString::FromUTF8(name);
        }
        Log(U8("未布通网络：") + names);
    }
    m_canvas->Refresh();
}

void MainFrame::AddVia()
{
    std::string netId = m_canvas->LastPadNetId();
    if (netId.empty() && !m_project.board.nets.nets().empty()) {
        netId = m_project.board.nets.nets().front().id;
    }
    if (netId.empty()) {
        Log(U8("请先载入网表或点击一个焊盘。"));
        return;
    }
    std::string error;
    if (!pcb::RoutingService::placeVia(
            m_project.board, netId, m_canvas->LastMousePoint(),
            "F.Cu", "B.Cu", 0.6, 0.3, error)) {
        Log(U8("过孔放置失败：") + wxString::FromUTF8(error));
        return;
    }
    Log(U8("已在鼠标位置放置过孔。"));
    m_canvas->Refresh();
}

void MainFrame::AddGroundPour()
{
    const pcb::Net* ground = m_project.board.findNetByName("GND");
    if (!ground) {
        Log(U8("当前工程没有 GND 网络。"));
        return;
    }
    std::string error;
    if (!pcb::RoutingService::addGroundPour(
            m_project.board, ground->id, "B.Cu", 1.0, 0.30, error)) {
        Log(U8("敷铜失败：") + wxString::FromUTF8(error));
        return;
    }
    Log(U8("已创建 GND 底层敷铜，并生成不同网络对象的避让孔。"));
    m_canvas->Refresh();
}

void MainFrame::RotateSelected()
{
    if (m_canvas->SelectedComponentId().empty()) {
        Log(U8("请先左键点击一个元件。"));
        return;
    }
    if (!pcb::LayoutService::rotateComponent(
            m_project.board, m_canvas->SelectedComponentId(), 90.0)) {
        Log(U8("元件旋转失败。"));
        return;
    }
    Log(U8("选中元件已旋转 90 度。"));
    m_canvas->Refresh();
}

void MainFrame::ValidateBoard()
{
    const pcb::ValidationResult result =
        pcb::RoutingService::validate(m_project.board, 0.25);
    Log(U8("连接与间距检查："));
    Log(wxString::FromUTF8(result.summary()));
    if (result.ok) {
        Log(U8("验证通过：所有网络已连接，未发现间距冲突。"));
    }
}

void MainFrame::FitBoard()
{
    m_canvas->FitBoard();
}
