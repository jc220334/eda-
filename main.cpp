#pragma execution_character_set("utf-8")

#include <wx/wx.h>
#include <wx/splitter.h>
#include <wx/toolbar.h>
#include <wx/menu.h>
#include <wx/artprov.h>
#include <wx/statline.h>
#include <wx/dcbuffer.h>
#include <wx/checklst.h>
#include <wx/radiobox.h>

// ============================================================================
// 1. 右侧画布类（带滚动条 + 网格背景）
// ============================================================================
class DesignCanvas : public wxScrolledWindow
{
public:
    DesignCanvas(wxWindow* parent)
        : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL | wxVSCROLL)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetVirtualSize(3000, 3000);
        SetScrollRate(10, 10);

        // 设置背景为浅灰色，类似图片中的画布
        SetBackgroundColour(wxColour(240, 240, 240));

        Bind(wxEVT_PAINT, &DesignCanvas::OnPaint, this);
    }

private:
    void OnPaint(wxPaintEvent& event)
    {
        wxAutoBufferedPaintDC dc(this);
        PrepareDC(dc);

        dc.SetBackground(wxBrush(GetBackgroundColour()));
        dc.Clear();

        // 绘制网格点 (类似图片中的背景点阵)
        dc.SetPen(wxPen(wxColour(200, 200, 200), 1));
        wxSize clientSize = GetClientSize();
        wxPoint viewStart = GetViewStart();

        const int gridSize = 20; // 网格稍微放大一点
        int startX = (viewStart.x * 10) / gridSize * gridSize;
        int startY = (viewStart.y * 10) / gridSize * gridSize;
        int endX = startX + clientSize.x + gridSize;
        int endY = startY + clientSize.y + gridSize;

        for (int x = startX; x < endX; x += gridSize)
        {
            for (int y = startY; y < endY; y += gridSize)
            {
                dc.DrawPoint(x, y);
            }
        }
    }
};

// ============================================================================
// 2. 左侧层选择与连线样式面板
// ============================================================================
class LeftSidebarPanel : public wxPanel
{
public:
    LeftSidebarPanel(wxWindow* parent) : wxPanel(parent, wxID_ANY)
    {
        SetBackgroundColour(wxColour(245, 245, 245)); // 侧边栏背景色

        wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

        // --- 1. 层选择栏 ---
        wxStaticText* layerTitle = new wxStaticText(this, wxID_ANY, wxT("层选择栏"));
        wxFont titleFont = layerTitle->GetFont();
        titleFont.SetWeight(wxFONTWEIGHT_BOLD);
        layerTitle->SetFont(titleFont);
        mainSizer->Add(layerTitle, 0, wxALL | wxALIGN_LEFT, 8);
        mainSizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 5);

        wxArrayString layers;
        layers.Add(wxT("Component"));
        layers.Add(wxT("solder"));
        layers.Add(wxT("outline"));
        layers.Add(wxT("spare"));
        layers.Add(wxT("top silk"));
        layers.Add(wxT("fat lines"));
        layers.Add(wxT("pins/pads"));
        layers.Add(wxT("vias"));
        layers.Add(wxT("far side"));
        layers.Add(wxT("solder mask"));

        wxCheckListBox* layerList = new wxCheckListBox(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 250), layers);
        // 默认勾选前几个
        layerList->Check(0, true);
        layerList->Check(3, true);
        mainSizer->Add(layerList, 0, wxEXPAND | wxALL, 5);

        // --- 2. 连线样式栏 ---
        mainSizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxALL, 5);

        wxStaticText* styleTitle = new wxStaticText(this, wxID_ANY, wxT("连线样式栏"));
        styleTitle->SetFont(titleFont);
        mainSizer->Add(styleTitle, 0, wxALL | wxALIGN_LEFT, 8);
        mainSizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 5);

        wxArrayString styleChoices;
        styleChoices.Add(wxT("Signal"));
        styleChoices.Add(wxT("Power"));

        wxRadioBox* radioStyle = new wxRadioBox(this, wxID_ANY, wxT("Route Style"),
            wxDefaultPosition, wxDefaultSize, styleChoices, 1, wxRA_SPECIFY_COLS);
        mainSizer->Add(radioStyle, 0, wxEXPAND | wxALL, 10);

        // 撑满剩余空间
        mainSizer->AddStretchSpacer();
        SetSizer(mainSizer);
    }
};

// ============================================================================
// 3. 主窗口 (Frame)
// ============================================================================
class MainFrame : public wxFrame
{
public:
    MainFrame(const wxString& title)
        : wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition, wxSize(1200, 800))
    {
        BuildMenuBar();
        BuildToolBar();
        BuildMainLayout();
        BuildStatusBar();
    }

private:
    void BuildMenuBar()
    {
        wxMenuBar* menuBar = new wxMenuBar();
        wxMenu* menuFile = new wxMenu();
        menuFile->Append(wxID_EXIT, wxT("E&xit"));
        menuBar->Append(menuFile, wxT("&File"));

        wxMenu* menuEdit = new wxMenu();
        menuBar->Append(menuEdit, wxT("&Edit"));

        wxMenu* menuView = new wxMenu();
        menuBar->Append(menuView, wxT("&View"));

        SetMenuBar(menuBar);
    }

    void BuildToolBar()
    {
        wxToolBar* toolBar = CreateToolBar(wxTB_HORIZONTAL | wxTB_FLAT | wxTB_TEXT);

        // 图片里工具栏是纯图标，这里用文字代替以示意
        toolBar->AddTool(wxID_ANY, wxT("New"), wxArtProvider::GetBitmap(wxART_NEW));
        toolBar->AddTool(wxID_ANY, wxT("Open"), wxArtProvider::GetBitmap(wxART_FILE_OPEN));
        toolBar->AddSeparator();
        toolBar->AddTool(wxID_ANY, wxT("Zoom In"), wxArtProvider::GetBitmap(wxART_PLUS));
        toolBar->AddTool(wxID_ANY, wxT("Zoom Out"), wxArtProvider::GetBitmap(wxART_MINUS));

        toolBar->Realize();
    }

    void BuildMainLayout()
    {
        // 使用 Splitter 分割左右侧边栏和画布
        wxSplitterWindow* splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D);
        splitter->SetMinimumPaneSize(200);

        // 左侧面板
        LeftSidebarPanel* leftPanel = new LeftSidebarPanel(splitter);

        // 右侧画布
        DesignCanvas* canvas = new DesignCanvas(splitter);

        splitter->SplitVertically(leftPanel, canvas, 220); // 左侧宽 220
    }

    void BuildStatusBar()
    {
        // 图片底部状态栏有多段信息
        CreateStatusBar(4);
        int widths[] = { 150, 150, 300, -1 };
        SetStatusWidths(4, widths);

        SetStatusText(wxT("Grid: 10.00"), 0);
        SetStatusText(wxT("Unit: mil"), 1);
        SetStatusText(wxT("Clearance=20.00 trace=10.00 buffer=8.00"), 2);
        SetStatusText(wxT("Ready"), 3);
    }
};

// ============================================================================
// 4. 应用程序入口
// ============================================================================
class PCBEditorApp : public wxApp
{
public:
    virtual bool OnInit() override
    {
        MainFrame* frame = new MainFrame(wxT("Untitled: (Untitled) - PCB Editor"));
        frame->Show(true);
        return true;
    }
};

IMPLEMENT_APP(PCBEditorApp)