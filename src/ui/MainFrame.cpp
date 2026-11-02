#include "ui/MainFrame.h"

#include "io/ProjectIO.h"
#include "pcb/LayoutService.h"
#include "pcb/NetlistImporter.h"
#include "pcb/RoutingService.h"

#include <wx/filedlg.h>
#include <wx/artprov.h>
#include <wx/checklst.h>
#include <wx/radiobox.h>
#include <wx/splitter.h>
#include <wx/statline.h>
#include <wx/toolbar.h>

#include <algorithm>
#include <string>

namespace {

enum {
    ID_LOAD_SAMPLE = wxID_HIGHEST + 100,
    ID_SAMPLE_LAYOUT,
    ID_SAMPLE_MANUAL,
    ID_SAMPLE_ROUTED,
    ID_SAMPLE_COMPLETE,
    ID_SAMPLE_CORE,
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
    : wxFrame(nullptr, wxID_ANY, U8("未命名: (未命名) - PCB 编辑器")),
      m_repositoryRoot(repositoryRoot)
{
    SetMinSize(wxSize(980, 680));
    BuildInterface();
    SetSize(wxSize(1200, 800));
    Centre();
    LoadSample();
}

void MainFrame::BuildInterface()
{
    wxPanel* root = new wxPanel(this);
    wxBoxSizer* rootSizer = new wxBoxSizer(wxVERTICAL);

    wxSplitterWindow* splitter = new wxSplitterWindow(
        root, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D);
    splitter->SetMinimumPaneSize(190);

    wxPanel* sidebar = new wxPanel(splitter);
    sidebar->SetBackgroundColour(wxColour(245, 245, 245));
    wxBoxSizer* sidebarSizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText* layerTitle = new wxStaticText(sidebar, wxID_ANY, U8("图层"));
    wxFont titleFont = layerTitle->GetFont();
    titleFont.SetWeight(wxFONTWEIGHT_BOLD);
    layerTitle->SetFont(titleFont);
    sidebarSizer->Add(layerTitle, 0, wxALL | wxALIGN_LEFT, 8);
    sidebarSizer->Add(new wxStaticLine(sidebar), 0, wxEXPAND | wxLEFT | wxRIGHT, 5);

    wxArrayString layers;
    layers.Add(U8("元件"));
    layers.Add(U8("焊接层"));
    layers.Add(U8("板框"));
    layers.Add(U8("备用层"));
    layers.Add(U8("顶层丝印"));
    layers.Add(U8("粗线层"));
    layers.Add(U8("引脚/焊盘"));
    layers.Add(U8("过孔"));
    layers.Add(U8("背面"));
    layers.Add(U8("阻焊层"));
    m_layerList = new wxCheckListBox(
        sidebar, wxID_ANY, wxDefaultPosition, wxSize(-1, 250), layers);
    m_layerList->Check(0, true);
    m_layerList->Check(1, true);
    m_layerList->Check(2, true);
    m_layerList->Check(4, true);
    m_layerList->Check(5, true);
    m_layerList->Check(6, true);
    m_layerList->Check(7, true);
    m_layerList->Check(8, true);
    sidebarSizer->Add(m_layerList, 0, wxEXPAND | wxALL, 5);

    sidebarSizer->Add(new wxStaticLine(sidebar), 0, wxEXPAND | wxALL, 5);
    wxStaticText* routeTitle = new wxStaticText(sidebar, wxID_ANY, U8("布线样式"));
    routeTitle->SetFont(titleFont);
    sidebarSizer->Add(routeTitle, 0, wxALL | wxALIGN_LEFT, 8);
    sidebarSizer->Add(new wxStaticLine(sidebar), 0, wxEXPAND | wxLEFT | wxRIGHT, 5);
    wxArrayString routeChoices;
    routeChoices.Add(U8("信号"));
    routeChoices.Add(U8("电源"));
    m_routeStyle = new wxRadioBox(
        sidebar, wxID_ANY, U8("样式"), wxDefaultPosition, wxDefaultSize,
        routeChoices, 1, wxRA_SPECIFY_COLS);
    m_routeStyle->SetSelection(0);
    sidebarSizer->Add(m_routeStyle, 0, wxEXPAND | wxALL, 10);
    sidebarSizer->AddStretchSpacer();
    sidebar->SetSizer(sidebarSizer);

    wxPanel* rightPane = new wxPanel(splitter);
    wxBoxSizer* rightSizer = new wxBoxSizer(wxVERTICAL);
    m_canvas = new BoardCanvas(rightPane);
    m_canvas->SetStatusCallback([this](const wxString& message) { Log(message); });

    rightSizer->Add(m_canvas, 1, wxEXPAND);
    rightPane->SetSizer(rightSizer);
    splitter->SplitVertically(sidebar, rightPane, 220);
    rootSizer->Add(splitter, 1, wxEXPAND);
    root->SetSizer(rootSizer);

    CreateStatusBar(4);
    const int statusWidths[] = {150, 150, 300, -1};
    SetStatusWidths(4, statusWidths);
    SetStatusText(U8("网格: 10.00"), 0);
    SetStatusText(U8("单位: mil"), 1);
    SetStatusText(U8("间距=20.00 走线=10.00 缓冲=8.00"), 2);
    SetStatusText(U8("就绪"), 3);

    wxToolBar* toolBar = CreateToolBar(wxTB_HORIZONTAL | wxTB_FLAT | wxTB_TEXT);
    toolBar->AddTool(wxID_NEW, U8("新建"), wxArtProvider::GetBitmap(wxART_NEW));
    toolBar->AddTool(wxID_OPEN, U8("打开"), wxArtProvider::GetBitmap(wxART_FILE_OPEN));
    toolBar->AddSeparator();
    toolBar->AddTool(wxID_ZOOM_IN, U8("放大"), wxArtProvider::GetBitmap(wxART_PLUS));
    toolBar->AddTool(wxID_ZOOM_OUT, U8("缩小"), wxArtProvider::GetBitmap(wxART_MINUS));
    toolBar->Realize();

    Bind(wxEVT_TOOL, [this](wxCommandEvent&) { LoadSample(); }, wxID_NEW);
    Bind(wxEVT_TOOL, [this](wxCommandEvent&) { OpenProject(); }, wxID_OPEN);
    Bind(wxEVT_TOOL, [this](wxCommandEvent&) { m_canvas->ZoomIn(); }, wxID_ZOOM_IN);
    Bind(wxEVT_TOOL, [this](wxCommandEvent&) { m_canvas->ZoomOut(); }, wxID_ZOOM_OUT);
    Bind(wxEVT_CHECKLISTBOX, &MainFrame::OnLayerToggled, this, m_layerList->GetId());
    Bind(wxEVT_LISTBOX, &MainFrame::OnLayerSelected, this, m_layerList->GetId());
    Bind(wxEVT_RADIOBOX, &MainFrame::OnRouteStyleChanged, this, m_routeStyle->GetId());

    wxMenu* fileMenu = new wxMenu();
    fileMenu->Append(wxID_NEW, U8("新建/载入样例\tCtrl+N"));
    fileMenu->Append(ID_IMPORT_NETLIST, U8("导入网表\tCtrl+I"));
    fileMenu->Append(wxID_OPEN, U8("打开工程\tCtrl+O"));
    fileMenu->Append(wxID_SAVE, U8("保存工程\tCtrl+S"));
    wxMenu* sampleMenu = new wxMenu();
    sampleMenu->Append(ID_SAMPLE_LAYOUT, U8("01 - 导入与布局"));
    sampleMenu->Append(ID_SAMPLE_MANUAL, U8("02 - 手动布线与过孔"));
    sampleMenu->Append(ID_SAMPLE_ROUTED, U8("03 - 自动布线"));
    sampleMenu->Append(ID_SAMPLE_COMPLETE, U8("04 - 完整敷铜与验证"));
    sampleMenu->Append(ID_SAMPLE_CORE, U8("05 - 核心模型演示"));
    fileMenu->AppendSubMenu(sampleMenu, U8("样例"));
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT, U8("退出"));

    wxMenu* editMenu = new wxMenu();
    editMenu->Append(ID_AUTO_PLACE, U8("自动布局"));
    editMenu->Append(ID_AUTO_ROUTE, U8("自动布线"));
    editMenu->Append(ID_ADD_VIA, U8("放置过孔"));
    editMenu->Append(ID_ADD_POUR, U8("创建 GND 敷铜"));
    editMenu->Append(ID_ROTATE, U8("旋转选中元件"));
    editMenu->Append(ID_VALIDATE, U8("连接与间距检查"));

    wxMenu* viewMenu = new wxMenu();
    viewMenu->Append(ID_FIT_BOARD, U8("适合窗口"));

    wxMenuBar* menuBar = new wxMenuBar();
    menuBar->Append(fileMenu, U8("文件"));
    menuBar->Append(editMenu, U8("编辑"));
    menuBar->Append(viewMenu, U8("视图"));
    SetMenuBar(menuBar);

    Bind(wxEVT_MENU, [this](wxCommandEvent&) {
        LoadSampleProject("01-layout.pcbproj", U8("导入与自动布局样例"));
    }, ID_SAMPLE_LAYOUT);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {
        LoadSampleProject("02-manual-routing.pcbproj", U8("手动布线与过孔样例"));
    }, ID_SAMPLE_MANUAL);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {
        LoadSampleProject("03-auto-routing.pcbproj", U8("自动布线样例"));
    }, ID_SAMPLE_ROUTED);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {
        LoadSampleProject("04-complete.pcbproj", U8("完整敷铜与连接验证样例"));
    }, ID_SAMPLE_COMPLETE);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {
        LoadSampleProject("demo.pcbproj", U8("核心模型与持久化样例"));
    }, ID_SAMPLE_CORE);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { LoadSample(); }, wxID_NEW);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(true); }, wxID_EXIT);
}

void MainFrame::Log(const wxString& message)
{
    SetStatusText(message, 3);
}

void MainFrame::RefreshSidebar()
{
    if (!m_layerList) {
        return;
    }
    m_layerList->Check(0, true);
    m_layerList->Check(1, true);
    m_layerList->Check(2, true);
    m_layerList->Check(4, true);
    m_layerList->Check(5, true);
    m_layerList->Check(6, true);
    m_layerList->Check(7, true);
    m_layerList->Check(8, true);
    m_canvas->SetActiveLayer("F.Cu");
}

void MainFrame::OnLayerToggled(wxCommandEvent& event)
{
    const int index = static_cast<int>(event.GetSelection());
    const bool visible = index >= 0 && m_layerList->IsChecked(static_cast<unsigned int>(index));
    switch (index) {
    case 0:
        m_canvas->SetComponentsVisible(visible);
        break;
    case 1:
    case 8:
        if (pcb::Layer* layer = m_project.board.layers.findLayer("B.Cu")) {
            layer->visible = visible;
        }
        break;
    case 2:
        if (pcb::Layer* layer = m_project.board.layers.findLayer("Edge")) {
            layer->visible = visible;
        }
        break;
    case 4:
        if (pcb::Layer* layer = m_project.board.layers.findLayer("F.Silk")) {
            layer->visible = visible;
        }
        break;
    case 5:
        if (pcb::Layer* layer = m_project.board.layers.findLayer("F.Cu")) {
            layer->visible = visible;
        }
        break;
    case 6:
        m_canvas->SetPadsVisible(visible);
        break;
    case 7:
        m_canvas->SetViasVisible(visible);
        break;
    default:
        break;
    }
    m_canvas->Refresh();
    SetStatusText(visible ? U8("图层已显示") : U8("图层已隐藏"), 3);
}

void MainFrame::OnLayerSelected(wxCommandEvent& event)
{
    const int index = event.GetSelection();
    if (index == 1 || index == 8) {
        m_canvas->SetActiveLayer("B.Cu");
    } else if (index == 5) {
        m_canvas->SetActiveLayer("F.Cu");
    }
}

void MainFrame::OnRouteStyleChanged(wxCommandEvent&)
{
    const double width = m_routeStyle->GetSelection() == 1 ? 0.50 : 0.25;
    m_canvas->SetRouteWidth(width);
    Log(wxString::Format(U8("手动布线线宽：%.2f mm"), width));
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
    RefreshSidebar();
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
        SetTitle(U8("未命名: (已导入 PCB) - PCB 编辑器"));
        Log(U8("已载入内置样例。左键点击焊盘开始手动布线，右键旋转元件。"));
    }
}

void MainFrame::LoadSampleProject(
    const std::string& fileName,
    const wxString& description)
{
    const std::filesystem::path path = m_repositoryRoot / "samples" / fileName;
    pcb::PcbProject loadedProject;
    const pcb::LoadResult result =
        pcb::ProjectIO::loadFromFile(path.string(), loadedProject);
    if (!result.ok) {
        Log(U8("样例打开失败：") + wxString::FromUTF8(result.error));
        return;
    }
    m_project = std::move(loadedProject);
    if (m_canvas) {
        m_canvas->SetProject(&m_project);
    }
    RefreshSidebar();
    SetTitle(U8(fileName.c_str()) + U8(" - PCB 编辑器"));
    Log(U8("已打开样例：") + description);
    if (m_canvas) {
        m_canvas->Refresh();
    }
}

void MainFrame::ImportNetlist()
{
    wxFileDialog dialog(
        this, U8("选择网表文件"), wxString::FromUTF8(m_repositoryRoot.u8string()), "",
        U8("网表 JSON (*.net.json;*.json)|*.json"), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }
    const std::filesystem::path library =
        m_repositoryRoot / "examples" / "netlist-basic" / "sample.lib.json";
    if (LoadNetlistFiles(dialog.GetPath().ToStdString(), library.string())) {
        SetTitle(dialog.GetFilename() + U8(" - PCB 编辑器"));
    }
}

void MainFrame::OpenProject()
{
    wxFileDialog dialog(
        this, U8("打开 PCB 工程"), wxString::FromUTF8(m_repositoryRoot.u8string()), "",
        U8("PCB 工程 (*.pcbproj)|*.pcbproj|JSON 文件 (*.json)|*.json"),
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
    RefreshSidebar();
    SetTitle(dialog.GetFilename() + U8(" - PCB 编辑器"));
    Log(U8("工程已打开：") + dialog.GetPath());
}

void MainFrame::SaveProject()
{
    wxFileDialog dialog(
        this, U8("保存 PCB 工程"), wxString::FromUTF8(m_repositoryRoot.u8string()), "project.pcbproj",
        U8("PCB 工程 (*.pcbproj)|*.pcbproj"),
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
        result.ok ? U8("自动布线完成：新增走线 %zu，过孔 %zu")
                  : U8("自动布线未全部完成：新增走线 %zu，过孔 %zu"),
        result.traceCount, result.viaCount));
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
