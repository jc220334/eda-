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
// 1. 右侧画布类（带滚动条 + 颜色网格）
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

        // 画布底色设为非常浅的灰色偏白，让网格线能显现出来
        SetBackgroundColour(wxColour(252, 252, 252));

        Bind(wxEVT_PAINT, &DesignCanvas::OnPaint, this);
    }

private:
    void OnPaint(wxPaintEvent& event)
    {
        wxAutoBufferedPaintDC dc(this);
        PrepareDC(dc);

        dc.SetBackground(wxBrush(GetBackgroundColour()));
        dc.Clear();

        wxSize clientSize = GetClientSize();
        wxPoint viewStart = GetViewStart();

        // 定义网格尺寸
        const int minorGrid = 10;   // 小网格间距 (10个滚动单位/像素)
        const int majorGrid = 100;  // 大网格间距 (100个滚动单位/像素)

        // 计算当前可见区域的逻辑坐标边界
        int startX = (viewStart.x * 10) / minorGrid * minorGrid;
        int startY = (viewStart.y * 10) / minorGrid * minorGrid;
        int endX = startX + clientSize.x + minorGrid;
        int endY = startY + clientSize.y + minorGrid;

        // 1. 绘制次网格（浅色细线）
        dc.SetPen(wxPen(wxColour(235, 235, 235), 1));
        for (int x = startX; x < endX; x += minorGrid)
        {
            if (x % majorGrid != 0) // 避开主网格的位置
                dc.DrawLine(x, startY, x, endY);
        }
        for (int y = startY; y < endY; y += minorGrid)
        {
            if (y % majorGrid != 0) // 避开主网格的位置
                dc.DrawLine(startX, y, endX, y);
        }

        // 2. 绘制主网格（深色稍粗线，或者使用不同的颜色）
        dc.SetPen(wxPen(wxColour(210, 210, 210), 1));
        for (int x = startX; x < endX; x += minorGrid)
        {
            if (x % majorGrid == 0)
                dc.DrawLine(x, startY, x, endY);
        }
        for (int y = startY; y < endY; y += minorGrid)
        {
            if (y % majorGrid == 0)
                dc.DrawLine(startX, y, endX, y);
        }

        // 3. 绘制坐标原点 (可选，类似 EDA 软件的原点十字准星)
        dc.SetPen(wxPen(wxColour(150, 150, 150), 1, wxPENSTYLE_DOT));
        dc.DrawLine(-20, 0, 20, 0);
        dc.DrawLine(0, -20, 0, 20);
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
        SetBackgroundColour(wxColour(245, 245, 245));
        wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

        // --- 1. Layers ---
        wxStaticText* layerTitle = new wxStaticText(this, wxID_ANY, "Layers");
        wxFont titleFont = layerTitle->GetFont();
        titleFont.SetWeight(wxFONTWEIGHT_BOLD);
        layerTitle->SetFont(titleFont);
        mainSizer->Add(layerTitle, 0, wxALL | wxALIGN_LEFT, 8);
        mainSizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 5);

        wxArrayString layers;
        layers.Add("Component");
        layers.Add("solder");
        layers.Add("outline");
        layers.Add("spare");
        layers.Add("top silk");
        layers.Add("fat lines");
        layers.Add("pins/pads");
        layers.Add("vias");
        layers.Add("far side");
        layers.Add("solder mask");

        wxCheckListBox* layerList = new wxCheckListBox(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 250), layers);
        layerList->Check(0, true);
        layerList->Check(3, true);
        mainSizer->Add(layerList, 0, wxEXPAND | wxALL, 5);

        // --- 2. Route Style ---
        mainSizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxALL, 5);

        wxStaticText* styleTitle = new wxStaticText(this, wxID_ANY, "Route Style");
        styleTitle->SetFont(titleFont);
        mainSizer->Add(styleTitle, 0, wxALL | wxALIGN_LEFT, 8);
        mainSizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 5);

        wxArrayString styleChoices;
        styleChoices.Add("Signal");
        styleChoices.Add("Power");

        wxRadioBox* radioStyle = new wxRadioBox(this, wxID_ANY, "Style",
            wxDefaultPosition, wxDefaultSize, styleChoices, 1, wxRA_SPECIFY_COLS);
        mainSizer->Add(radioStyle, 0, wxEXPAND | wxALL, 10);

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
        menuFile->Append(wxID_EXIT, "E&xit");
        menuBar->Append(menuFile, "&File");

        wxMenu* menuEdit = new wxMenu();
        menuBar->Append(menuEdit, "&Edit");

        wxMenu* menuView = new wxMenu();
        menuBar->Append(menuView, "&View");

        SetMenuBar(menuBar);
    }

    void BuildToolBar()
    {
        wxToolBar* toolBar = CreateToolBar(wxTB_HORIZONTAL | wxTB_FLAT | wxTB_TEXT);

        toolBar->AddTool(wxID_ANY, "New", wxArtProvider::GetBitmap(wxART_NEW));
        toolBar->AddTool(wxID_ANY, "Open", wxArtProvider::GetBitmap(wxART_FILE_OPEN));
        toolBar->AddSeparator();
        toolBar->AddTool(wxID_ANY, "Zoom In", wxArtProvider::GetBitmap(wxART_PLUS));
        toolBar->AddTool(wxID_ANY, "Zoom Out", wxArtProvider::GetBitmap(wxART_MINUS));

        toolBar->Realize();
    }

    void BuildMainLayout()
    {
        wxSplitterWindow* splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D);
        splitter->SetMinimumPaneSize(200);

        LeftSidebarPanel* leftPanel = new LeftSidebarPanel(splitter);
        DesignCanvas* canvas = new DesignCanvas(splitter);

        splitter->SplitVertically(leftPanel, canvas, 220);
    }

    void BuildStatusBar()
    {
        CreateStatusBar(4);
        int widths[] = { 150, 150, 300, -1 };
        SetStatusWidths(4, widths);

        SetStatusText("Grid: 10.00", 0);
        SetStatusText("Unit: mil", 1);
        SetStatusText("Clearance=20.00 trace=10.00 buffer=8.00", 2);
        SetStatusText("Ready", 3);
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
        MainFrame* frame = new MainFrame("Untitled: (Untitled) - PCB Editor");
        frame->Show(true);
        return true;
    }
};

IMPLEMENT_APP(PCBEditorApp)