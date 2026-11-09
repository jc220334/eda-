
#pragma execution_character_set("utf-8")

#include <wx/wx.h>
#include <wx/splitter.h>
#include <wx/toolbar.h>
#include <wx/dcbuffer.h>
#include <wx/propgrid/propgrid.h>
#include <wx/propgrid/advprops.h>
#include <wx/checklst.h>
#include <wx/statline.h>
#include <wx/graphics.h>
#include <wx/listctrl.h>
#include <wx/treectrl.h>
#include <wx/dialog.h>
#include <vector>
#include <memory>
#include <algorithm>
#include <functional>
#include <cmath>

// 菜单和工具栏 ID 定义
enum {
    ID_T_SEL = wxID_HIGHEST + 1, ID_T_LINE, ID_T_RECT, ID_T_CIRCLE, ID_T_TEXT,
    ID_TOOLS_LIB_MANAGER, ID_TOOLS_FP_EDITOR,
    ID_FP_T_SEL, ID_FP_T_PAD
};

enum ToolType { TOOL_SELECT, TOOL_LINE, TOOL_RECT, TOOL_CIRCLE, TOOL_TEXT };
enum ResizeHandle {
    HANDLE_NONE = 0, HANDLE_TOP_LEFT, HANDLE_TOP, HANDLE_TOP_RIGHT,
    HANDLE_RIGHT, HANDLE_BOTTOM_RIGHT, HANDLE_BOTTOM, HANDLE_BOTTOM_LEFT, HANDLE_LEFT
};

// ============================================================================
// 1. 任务 D：元件库管理器 (带缩略图预览交互)
// ============================================================================
class PreviewPanel : public wxPanel {
public:
    int m_previewType = 0; // 0: Empty, 1: SOIC-8, 2: 0603

    PreviewPanel(wxWindow* parent) : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE) {
        // ★ 修复：声明使用自定义背景绘制
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetBackgroundColour(*wxWHITE);
        Bind(wxEVT_PAINT, &PreviewPanel::OnPaint, this);
    }

    void SetPreview(int type) { m_previewType = type; Refresh(); }

private:
    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        // ★ 修复：显式清理背景
        dc.SetBackground(wxBrush(GetBackgroundColour()));
        dc.Clear();

        dc.SetPen(wxPen(*wxBLACK, 2));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);

        wxSize sz = GetClientSize();
        int cx = sz.x / 2, cy = sz.y / 2;

        if (m_previewType == 1) { // SOIC-8
            dc.DrawRectangle(cx - 40, cy - 30, 80, 60);
            dc.SetBrush(*wxLIGHT_GREY_BRUSH);
            for (int i = 0; i < 4; ++i) {
                dc.DrawRectangle(cx - 50, cy - 25 + i * 15, 10, 8);
                dc.DrawRectangle(cx + 40, cy - 25 + i * 15, 10, 8);
            }
            dc.SetPen(*wxRED_PEN); dc.DrawCircle(cx - 30, cy, 4);
        }
        else if (m_previewType == 2) { // 0603
            dc.SetBrush(wxBrush(wxColour(200, 200, 200)));
            dc.DrawRectangle(cx - 30, cy - 15, 20, 30);
            dc.DrawRectangle(cx + 10, cy - 15, 20, 30);
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.DrawRectangle(cx - 15, cy - 10, 30, 20);
        }
        else {
            dc.DrawText("No Preview", cx - 30, cy - 10);
        }
    }
};

class LibraryManagerDialog : public wxDialog
{
public:
    LibraryManagerDialog(wxWindow* parent)
        : wxDialog(parent, wxID_ANY, "Component Library Manager", wxDefaultPosition, wxSize(900, 600))
    {
        wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
        wxBoxSizer* searchSizer = new wxBoxSizer(wxHORIZONTAL);
        searchSizer->Add(new wxTextCtrl(this, wxID_ANY, ""), 1, wxEXPAND | wxALL, 5);
        mainSizer->Add(searchSizer, 0, wxEXPAND);

        wxSplitterWindow* splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D | wxSP_LIVE_UPDATE);
        wxTreeCtrl* tree = new wxTreeCtrl(splitter, wxID_ANY);
        wxTreeItemId root = tree->AddRoot("My Components");
        tree->AppendItem(root, "SOIC-8");
        tree->AppendItem(root, "0603");
        tree->ExpandAll();

        wxSplitterWindow* rightSplitter = new wxSplitterWindow(splitter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D | wxSP_LIVE_UPDATE);

        wxListCtrl* list = new wxListCtrl(rightSplitter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
        list->AppendColumn("Footprint Name", wxLIST_FORMAT_LEFT, 200);
        long idx1 = list->InsertItem(0, "SOIC-8_3.9x4.9mm");
        long idx2 = list->InsertItem(1, "R_0603_1608Metric");

        wxPanel* prevContainer = new wxPanel(rightSplitter, wxID_ANY);
        wxBoxSizer* prevSizer = new wxBoxSizer(wxVERTICAL);
        prevSizer->Add(new wxStaticText(prevContainer, wxID_ANY, "Footprint Preview"), 0, wxALL | wxALIGN_CENTER_HORIZONTAL, 10);
        PreviewPanel* imgBox = new PreviewPanel(prevContainer);
        prevSizer->Add(imgBox, 1, wxEXPAND | wxALL, 10);
        prevContainer->SetSizer(prevSizer);

        rightSplitter->SplitVertically(list, prevContainer, -300);
        splitter->SplitVertically(tree, rightSplitter, 200);
        mainSizer->Add(splitter, 1, wxEXPAND | wxALL, 5);
        mainSizer->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxALIGN_RIGHT | wxALL, 10);
        SetSizer(mainSizer);

        // 绑定列表选中事件，联动预览图
        list->Bind(wxEVT_LIST_ITEM_SELECTED, [imgBox](wxListEvent& e) {
            if (e.GetIndex() == 0) imgBox->SetPreview(1); // SOIC
            else if (e.GetIndex() == 1) imgBox->SetPreview(2); // 0603
            });
        CenterOnParent();
    }
};

// ============================================================================
// 2. 任务 C：封装编辑器 (可放置焊盘)
// ============================================================================
struct Pad {
    wxPoint pos;
    int w = 20, h = 10;
    bool selected = false;
};

class FootprintCanvas : public wxScrolledWindow {
public:
    FootprintCanvas(wxWindow* parent) : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL | wxVSCROLL) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetVirtualSize(2000, 2000);
        SetScrollRate(10, 10);
        SetBackgroundColour(wxColour(252, 252, 252));
        Bind(wxEVT_PAINT, &FootprintCanvas::OnPaint, this);
        Bind(wxEVT_LEFT_DOWN, &FootprintCanvas::OnLeftDown, this);
    }

    bool m_placeMode = false;
    std::function<void(Pad*)> OnPadSelected;

    void SelectPad(Pad* p) {
        for (auto& pad : m_pads) pad.selected = false;
        if (p) p->selected = true;
        if (OnPadSelected) OnPadSelected(p);
        Refresh();
    }

private:
    std::vector<Pad> m_pads;

    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        PrepareDC(dc);
        dc.SetBackground(wxBrush(GetBackgroundColour()));
        dc.Clear();
        wxSize sz = GetClientSize();
        wxPoint vs = GetViewStart();
        dc.SetPen(wxPen(wxColour(235, 235, 235), 1));
        for (int x = vs.x * 10; x < vs.x * 10 + sz.x; x += 10) dc.DrawLine(x, vs.y * 10, x, vs.y * 10 + sz.y);
        for (int y = vs.y * 10; y < vs.y * 10 + sz.y; y += 10) dc.DrawLine(vs.x * 10, y, vs.x * 10 + sz.x, y);
        dc.SetPen(wxPen(*wxRED, 1, wxPENSTYLE_SHORT_DASH));
        dc.DrawLine(-30, 0, 30, 0); dc.DrawLine(0, -30, 0, 30);

        for (auto& p : m_pads) {
            if (p.selected) { dc.SetPen(wxPen(*wxBLUE, 2)); dc.SetBrush(*wxYELLOW_BRUSH); }
            else { dc.SetPen(*wxTRANSPARENT_PEN); dc.SetBrush(*wxBLACK_BRUSH); }
            dc.DrawRectangle(p.pos.x - p.w / 2, p.pos.y - p.h / 2, p.w, p.h);
        }
    }

    void OnLeftDown(wxMouseEvent& e) {
        wxPoint p = CalcUnscrolledPosition(e.GetPosition());
        p.x = (p.x + 5) / 10 * 10; p.y = (p.y + 5) / 10 * 10; // 吸附

        if (m_placeMode) {
            Pad newPad; newPad.pos = p;
            m_pads.push_back(newPad);
            SelectPad(&m_pads.back());
            Refresh();
        }
        else {
            Pad* found = nullptr;
            for (auto& pad : m_pads) {
                if (wxRect(pad.pos.x - pad.w / 2, pad.pos.y - pad.h / 2, pad.w, pad.h).Contains(p)) { found = &pad; break; }
            }
            SelectPad(found);
        }
    }
};

class FootprintEditorFrame : public wxFrame {
public:
    FootprintEditorFrame(wxWindow* parent)
        : wxFrame(parent, wxID_ANY, "Footprint Editor", wxDefaultPosition, wxSize(1000, 700))
    {
        wxBitmap dummyBmp(16, 16);
        { wxMemoryDC mem(dummyBmp); mem.SetBackground(*wxWHITE_BRUSH); mem.Clear(); mem.SetPen(*wxBLACK_PEN); mem.DrawRectangle(1, 1, 14, 14); }

        wxToolBar* tb = CreateToolBar(wxTB_HORIZONTAL | wxTB_FLAT | wxTB_TEXT);
        tb->AddRadioTool(ID_FP_T_SEL, "Select", dummyBmp);
        tb->AddRadioTool(ID_FP_T_PAD, "Place Pad", dummyBmp);
        tb->Realize();

        wxSplitterWindow* mainSplitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D | wxSP_LIVE_UPDATE);
        mainSplitter->SetMinimumPaneSize(200);
        wxSplitterWindow* rightSplitter = new wxSplitterWindow(mainSplitter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D | wxSP_LIVE_UPDATE);
        rightSplitter->SetMinimumPaneSize(200);

        m_canvas = new FootprintCanvas(rightSplitter);

        wxPanel* propPanel = new wxPanel(rightSplitter, wxID_ANY);
        wxBoxSizer* propSizer = new wxBoxSizer(wxVERTICAL);
        m_pg = new wxPropertyGrid(propPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxPG_DEFAULT_STYLE);
        m_pg->Append(new wxPropertyCategory("Pad Properties"));
        m_pg->Append(new wxIntProperty("X", wxPG_LABEL, 0));
        m_pg->Append(new wxIntProperty("Y", wxPG_LABEL, 0));
        m_pg->Disable();
        propSizer->Add(m_pg, 1, wxEXPAND);
        propPanel->SetSizer(propSizer);

        rightSplitter->SplitVertically(m_canvas, propPanel, -300);
        mainSplitter->SplitVertically(new wxPanel(mainSplitter), rightSplitter, 150);

        // 联动逻辑
        tb->Bind(wxEVT_TOOL, [this](wxCommandEvent& e) {
            if (e.GetId() == ID_FP_T_SEL) m_canvas->m_placeMode = false;
            else if (e.GetId() == ID_FP_T_PAD) m_canvas->m_placeMode = true;
            });

        m_canvas->OnPadSelected = [this](Pad* p) {
            if (!p) { m_pg->Disable(); return; }
            m_pg->Enable();
            m_pg->SetPropertyValue("X", p->pos.x);
            m_pg->SetPropertyValue("Y", p->pos.y);
            };

        CreateStatusBar();
        SetStatusText("Select 'Place Pad' tool and click on grid to place pads.");
        CenterOnParent();
    }
private:
    FootprintCanvas* m_canvas;
    wxPropertyGrid* m_pg;
};

// ============================================================================
// 3. 主界面图形基类 (任务 B)
// ============================================================================
class Shape
{
public:
    wxRect rect; wxColour color = *wxBLACK; int lineWidth = 2; double rotation = 0.0; bool selected = false; wxString text = "TEXT";
    Shape(const wxRect& r) : rect(r) { Normalize(); }
    virtual ~Shape() {}
    void Normalize() { if (rect.width < 0) { rect.x += rect.width; rect.width = -rect.width; } if (rect.height < 0) { rect.y += rect.height; rect.height = -rect.height; } }
    virtual Shape* Clone() const = 0;
    virtual void Draw(wxDC& dc) = 0;
    virtual wxString GetTypeName() const = 0;
    virtual void Move(const wxPoint& delta) { rect.x += delta.x; rect.y += delta.y; }
    virtual bool HitTest(const wxPoint& pt) const { wxRect r = rect; r.Inflate(2); return r.Contains(pt); }
    virtual void DrawSelection(wxDC& dc) const {
        dc.SetPen(wxPen(*wxBLUE, 1, wxPENSTYLE_DOT)); dc.SetBrush(*wxTRANSPARENT_BRUSH); dc.DrawRectangle(rect);
        dc.SetPen(wxPen(*wxBLUE, 1)); dc.SetBrush(*wxWHITE_BRUSH);
        for (int i = 1; i <= 8; ++i) { wxPoint p = GetHandlePoint(i); dc.DrawRectangle(p.x - 3, p.y - 3, 6, 6); }
    }
    virtual wxPoint GetHandlePoint(int handle) const {
        wxRect r = rect;
        switch (handle) {
        case HANDLE_TOP_LEFT: return r.GetTopLeft(); case HANDLE_TOP: return wxPoint(r.x + r.width / 2, r.y);
        case HANDLE_TOP_RIGHT: return r.GetTopRight(); case HANDLE_RIGHT: return wxPoint(r.x + r.width, r.y + r.height / 2);
        case HANDLE_BOTTOM_RIGHT: return r.GetBottomRight(); case HANDLE_BOTTOM: return wxPoint(r.x + r.width / 2, r.y + r.height);
        case HANDLE_BOTTOM_LEFT: return r.GetBottomLeft(); case HANDLE_LEFT: return wxPoint(r.x, r.y + r.height / 2);
        } return wxPoint(0, 0);
    }
    virtual int HitTestHandle(const wxPoint& pt) const {
        if (!selected) return HANDLE_NONE;
        for (int i = 1; i <= 8; ++i) { wxPoint p = GetHandlePoint(i); if (std::abs(pt.x - p.x) <= 8 && std::abs(pt.y - p.y) <= 8) return i; }
        return HANDLE_NONE;
    }
    virtual void Resize(int handle, const wxPoint& pt) {
        wxRect r = rect;
        switch (handle) {
        case HANDLE_TOP_LEFT: r.SetTopLeft(pt); break; case HANDLE_TOP: r.SetTop(pt.y); break;
        case HANDLE_TOP_RIGHT: r.SetTopRight(pt); break; case HANDLE_RIGHT: r.SetRight(pt.x); break;
        case HANDLE_BOTTOM_RIGHT: r.SetBottomRight(pt); break; case HANDLE_BOTTOM: r.SetBottom(pt.y); break;
        case HANDLE_BOTTOM_LEFT: r.SetBottomLeft(pt); break; case HANDLE_LEFT: r.SetLeft(pt.x); break;
        }
        if (r.width < 5) r.width = 5; if (r.height < 5) r.height = 5; rect = r;
    }
};

class LineShape : public Shape {
public:
    wxPoint m_p1, m_p2;
    LineShape(const wxPoint& p1, const wxPoint& p2) : Shape(wxRect(std::min(p1.x, p2.x), std::min(p1.y, p2.y), std::abs(p1.x - p2.x), std::abs(p1.y - p2.y))), m_p1(p1), m_p2(p2) {}
    Shape* Clone() const override { return new LineShape(*this); }
    void Move(const wxPoint& delta) override { Shape::Move(delta); m_p1 += delta; m_p2 += delta; }
    void Resize(int handle, const wxPoint& pt) override {
        wxRect oldRect = rect; Shape::Resize(handle, pt);
        if (oldRect.width == 0 || oldRect.height == 0) return;
        double rx1 = (double)(m_p1.x - oldRect.x) / oldRect.width; double ry1 = (double)(m_p1.y - oldRect.y) / oldRect.height;
        double rx2 = (double)(m_p2.x - oldRect.x) / oldRect.width; double ry2 = (double)(m_p2.y - oldRect.y) / oldRect.height;
        m_p1.x = rect.x + rx1 * rect.width; m_p1.y = rect.y + ry1 * rect.height;
        m_p2.x = rect.x + rx2 * rect.width; m_p2.y = rect.y + ry2 * rect.height;
    }
    void Draw(wxDC& dc) override {
        if (rotation != 0.0) {
            wxGraphicsContext* gc = wxGraphicsContext::Create(dc.GetWindow());
            if (gc) { gc->SetPen(wxPen(color, lineWidth)); gc->Translate(rect.x + rect.width / 2.0, rect.y + rect.height / 2.0); gc->Rotate(rotation * 3.1415926535 / 180.0); gc->Translate(-(rect.x + rect.width / 2.0), -(rect.y + rect.height / 2.0)); gc->StrokeLine(m_p1.x, m_p1.y, m_p2.x, m_p2.y); delete gc; }
        }
        else { dc.SetPen(wxPen(color, lineWidth)); dc.DrawLine(m_p1, m_p2); }
    }
    wxString GetTypeName() const override { return "Line"; }
};

class RectShape : public Shape {
public:
    RectShape(const wxRect& r) : Shape(r) {}
    Shape* Clone() const override { return new RectShape(*this); }
    void Draw(wxDC& dc) override {
        if (rotation != 0.0) {
            wxGraphicsContext* gc = wxGraphicsContext::Create(dc.GetWindow());
            if (gc) { gc->SetPen(wxPen(color, lineWidth)); gc->SetBrush(*wxTRANSPARENT_BRUSH); gc->Translate(rect.x + rect.width / 2.0, rect.y + rect.height / 2.0); gc->Rotate(rotation * 3.1415926535 / 180.0); gc->Translate(-rect.width / 2.0, -rect.height / 2.0); gc->DrawRectangle(0, 0, rect.width, rect.height); delete gc; }
        }
        else { dc.SetPen(wxPen(color, lineWidth)); dc.SetBrush(*wxTRANSPARENT_BRUSH); dc.DrawRectangle(rect); }
    }
    wxString GetTypeName() const override { return "Rectangle"; }
};

class CircleShape : public Shape {
public:
    CircleShape(const wxRect& r) : Shape(r) {}
    Shape* Clone() const override { return new CircleShape(*this); }
    void Draw(wxDC& dc) override {
        if (rotation != 0.0) {
            wxGraphicsContext* gc = wxGraphicsContext::Create(dc.GetWindow());
            if (gc) { gc->SetPen(wxPen(color, lineWidth)); gc->SetBrush(*wxTRANSPARENT_BRUSH); gc->Translate(rect.x + rect.width / 2.0, rect.y + rect.height / 2.0); gc->Rotate(rotation * 3.1415926535 / 180.0); gc->DrawEllipse(-rect.width / 2.0, -rect.height / 2.0, rect.width, rect.height); delete gc; }
        }
        else { dc.SetPen(wxPen(color, lineWidth)); dc.SetBrush(*wxTRANSPARENT_BRUSH); dc.DrawEllipse(rect); }
    }
    wxString GetTypeName() const override { return "Circle"; }
};

class TextShape : public Shape {
public:
    TextShape(const wxRect& r) : Shape(r) {}
    Shape* Clone() const override { return new TextShape(*this); }
    void Draw(wxDC& dc) override {
        if (rotation != 0.0) {
            wxGraphicsContext* gc = wxGraphicsContext::Create(dc.GetWindow());
            if (gc) { gc->SetFont(wxFont(12 + lineWidth, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL), color); gc->Translate(rect.x + rect.width / 2.0, rect.y + rect.height / 2.0); gc->Rotate(rotation * 3.1415926535 / 180.0); gc->Translate(-rect.width / 2.0, -rect.height / 2.0); gc->DrawText(text, 0, 0); delete gc; }
        }
        else { dc.SetTextForeground(color); dc.SetFont(wxFont(12 + lineWidth, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL)); dc.DrawText(text, rect.x, rect.y); }
    }
    wxString GetTypeName() const override { return "Text"; }
};

// ============================================================================
// 4. 主画布
// ============================================================================
class DesignCanvas : public wxScrolledWindow
{
public:
    DesignCanvas(wxWindow* parent) : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL | wxVSCROLL)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT); SetVirtualSize(2000, 2000); SetScrollRate(10, 10);
        SetBackgroundColour(wxColour(252, 252, 252)); SetFocus();
        Bind(wxEVT_PAINT, &DesignCanvas::OnPaint, this); Bind(wxEVT_LEFT_DOWN, &DesignCanvas::OnLeftDown, this);
        Bind(wxEVT_LEFT_UP, &DesignCanvas::OnLeftUp, this); Bind(wxEVT_MOTION, &DesignCanvas::OnMouseMove, this);
        Bind(wxEVT_KEY_DOWN, &DesignCanvas::OnKeyDown, this);
    }
    void SetTool(ToolType tool) { m_currentTool = tool; SetCursor(wxCursor(tool == TOOL_SELECT ? wxCURSOR_ARROW : wxCURSOR_CROSS)); }
    void SetActiveLayer(const wxString& layer) { m_activeLayer = layer; Refresh(); }
    Shape* GetSelectedShape() { for (auto& s : m_shapes) if (s->selected) return s.get(); return nullptr; }
    std::function<void()> OnSelectionChanged;
private:
    std::vector<std::unique_ptr<Shape>> m_shapes; std::unique_ptr<Shape> m_clipboard;
    ToolType m_currentTool = TOOL_SELECT; wxString m_activeLayer = "Component";
    bool m_isDrawing = false, m_isDragging = false, m_isBoxSelecting = false;
    wxPoint m_startPt, m_lastPt; wxRect m_selectBox;
    Shape* m_draggingShape = nullptr; Shape* m_resizingShape = nullptr; int m_resizeHandle = HANDLE_NONE;

    int Snap(int val) { return (val >= 0) ? ((val + 5) / 10) * 10 : -(((-val) + 5) / 10) * 10; }
    wxPoint GetSnappedPos(wxMouseEvent& event) { wxPoint p = CalcUnscrolledPosition(event.GetPosition()); return wxPoint(Snap(p.x), Snap(p.y)); }

    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this); PrepareDC(dc); dc.SetBackground(wxBrush(GetBackgroundColour())); dc.Clear();
        wxSize sz = GetClientSize(); wxPoint vs = GetViewStart();
        dc.SetPen(wxPen(wxColour(235, 235, 235), 1));
        for (int x = vs.x * 10; x < vs.x * 10 + sz.x; x += 10) dc.DrawLine(x, vs.y * 10, x, vs.y * 10 + sz.y);
        for (int y = vs.y * 10; y < vs.y * 10 + sz.y; y += 10) dc.DrawLine(vs.x * 10, y, vs.x * 10 + sz.x, y);
        for (auto& s : m_shapes) { s->Draw(dc); if (s->selected) s->DrawSelection(dc); }
        if (m_isDrawing) { dc.SetPen(wxPen(*wxBLACK, 1, wxPENSTYLE_SHORT_DASH)); dc.DrawLine(m_startPt, m_lastPt); }
        if (m_isBoxSelecting) { dc.SetPen(wxPen(*wxBLUE, 1, wxPENSTYLE_DOT)); dc.SetBrush(wxBrush(wxColour(0, 0, 255, 30))); dc.DrawRectangle(m_selectBox); }
    }
    void OnLeftDown(wxMouseEvent& e) {
        SetFocus(); wxPoint rawP = CalcUnscrolledPosition(e.GetPosition()); wxPoint p = GetSnappedPos(e);
        if (m_currentTool == TOOL_SELECT) {
            Shape* sel = GetSelectedShape(); if (sel) { int handle = sel->HitTestHandle(rawP); if (handle != HANDLE_NONE) { m_resizingShape = sel; m_resizeHandle = handle; return; } }
            m_draggingShape = nullptr;
            for (int i = m_shapes.size() - 1; i >= 0; --i) {
                if (m_shapes[i]->HitTest(rawP)) {
                    for (auto& s : m_shapes) if (s.get() != m_shapes[i].get()) s->selected = false;
                    m_shapes[i]->selected = true; m_draggingShape = m_shapes[i].get(); m_startPt = p; m_isDragging = true;
                    if (OnSelectionChanged) OnSelectionChanged(); Refresh(); return;
                }
            }
            for (auto& s : m_shapes) s->selected = false; if (OnSelectionChanged) OnSelectionChanged();
            m_isBoxSelecting = true; m_startPt = p; m_selectBox = wxRect(p, p); Refresh();
        }
        else { m_isDrawing = true; m_startPt = p; m_lastPt = p; }
    }
    void OnMouseMove(wxMouseEvent& e) {
        wxPoint rawP = CalcUnscrolledPosition(e.GetPosition()); wxPoint p = GetSnappedPos(e);
        if (m_resizingShape) { m_resizingShape->Resize(m_resizeHandle, rawP); if (OnSelectionChanged) OnSelectionChanged(); Refresh(); }
        else if (m_isDragging && m_draggingShape) { wxPoint delta = p - m_startPt; m_draggingShape->Move(delta); m_startPt = p; if (OnSelectionChanged) OnSelectionChanged(); Refresh(); }
        else if (m_isBoxSelecting) { m_selectBox = wxRect(m_startPt, p); Refresh(); }
        else if (m_isDrawing) { m_lastPt = p; Refresh(); }
    }
    void OnLeftUp(wxMouseEvent& e) {
        if (m_resizingShape) { m_resizingShape = nullptr; m_resizeHandle = HANDLE_NONE; }
        else if (m_isDragging) { m_isDragging = false; m_draggingShape = nullptr; }
        else if (m_isBoxSelecting) {
            m_isBoxSelecting = false;
            for (auto& s : m_shapes) { if (m_selectBox.Intersects(s->rect)) s->selected = true; }
            if (OnSelectionChanged) OnSelectionChanged(); Refresh();
        }
        else if (m_isDrawing) {
            m_isDrawing = false; wxPoint p = GetSnappedPos(e);
            if (m_startPt != p) {
                std::unique_ptr<Shape> newShape;
                if (m_currentTool == TOOL_LINE) newShape.reset(new LineShape(m_startPt, p));
                else if (m_currentTool == TOOL_RECT) newShape.reset(new RectShape(wxRect(m_startPt, p)));
                else if (m_currentTool == TOOL_CIRCLE) newShape.reset(new CircleShape(wxRect(m_startPt, p)));
                else if (m_currentTool == TOOL_TEXT) newShape.reset(new TextShape(wxRect(m_startPt, p)));
                if (newShape) m_shapes.push_back(std::move(newShape));
            } Refresh();
        }
    }
    void OnKeyDown(wxKeyEvent& e) {
        if (e.GetKeyCode() == WXK_DELETE || e.GetKeyCode() == WXK_BACK) {
            m_shapes.erase(std::remove_if(m_shapes.begin(), m_shapes.end(), [](const std::unique_ptr<Shape>& s) { return s->selected; }), m_shapes.end());
            if (OnSelectionChanged) OnSelectionChanged(); Refresh();
        }
        else if (e.GetKeyCode() == 'C' && e.ControlDown()) {
            Shape* sel = GetSelectedShape(); if (sel) m_clipboard.reset(sel->Clone());
        }
        else if (e.GetKeyCode() == 'V' && e.ControlDown()) {
            if (m_clipboard) {
                for (auto& s : m_shapes) s->selected = false;
                Shape* copy = m_clipboard->Clone(); copy->Move(wxPoint(20, 20)); copy->selected = true;
                m_shapes.push_back(std::unique_ptr<Shape>(copy)); m_clipboard.reset(copy->Clone());
                if (OnSelectionChanged) OnSelectionChanged(); Refresh();
            }
        }
    }
};

// ============================================================================
// 5. 侧边栏与属性面板
// ============================================================================
class LeftSidebarPanel : public wxPanel {
public:
    LeftSidebarPanel(wxWindow* parent) : wxPanel(parent, wxID_ANY) {
        SetBackgroundColour(wxColour(245, 245, 245)); wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
        wxStaticText* layerTitle = new wxStaticText(this, wxID_ANY, "Layers");
        wxFont titleFont = layerTitle->GetFont(); titleFont.SetWeight(wxFONTWEIGHT_BOLD); layerTitle->SetFont(titleFont);
        mainSizer->Add(layerTitle, 0, wxALL | wxALIGN_LEFT, 8); mainSizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 5);
        wxArrayString layers; layers.Add("Component"); layers.Add("solder"); layers.Add("outline");
        m_layerList = new wxCheckListBox(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 200), layers);
        m_layerList->Check(0, true); m_layerList->SetSelection(0);
        m_layerList->Bind(wxEVT_LISTBOX, &LeftSidebarPanel::OnLayerSelected, this);
        mainSizer->Add(m_layerList, 0, wxEXPAND | wxALL, 5); SetSizer(mainSizer);
    }
    std::function<void(wxString)> OnLayerChangedCallback;
private:
    wxCheckListBox* m_layerList;
    void OnLayerSelected(wxCommandEvent& event) { if (OnLayerChangedCallback) OnLayerChangedCallback(m_layerList->GetStringSelection()); }
};

class RightSidebarPanel : public wxPanel {
public:
    wxPropertyGrid* pg; bool m_ignoreChange = false;
    RightSidebarPanel(wxWindow* parent) : wxPanel(parent, wxID_ANY) {
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        pg = new wxPropertyGrid(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxPG_DEFAULT_STYLE);
        pg->Append(new wxPropertyCategory("Geometry"));
        pg->Append(new wxIntProperty("X", wxPG_LABEL, 0)); pg->Append(new wxIntProperty("Y", wxPG_LABEL, 0));
        pg->Append(new wxIntProperty("Width", wxPG_LABEL, 0)); pg->Append(new wxIntProperty("Height", wxPG_LABEL, 0));
        pg->Append(new wxFloatProperty("Rotation", wxPG_LABEL, 0.0));
        pg->Append(new wxPropertyCategory("Appearance"));
        pg->Append(new wxColourProperty("Color", wxPG_LABEL, *wxBLACK)); pg->Append(new wxIntProperty("Line Width", wxPG_LABEL, 2));
        sizer->Add(pg, 1, wxEXPAND | wxALL, 0); SetSizer(sizer);
        pg->Bind(wxEVT_PG_CHANGED, &RightSidebarPanel::OnPropertyChanged, this);
    }
    void UpdateFromShape(Shape* shape) {
        m_ignoreChange = true;
        if (!shape) pg->Disable();
        else {
            pg->Enable();
            pg->SetPropertyValue("X", shape->rect.x); pg->SetPropertyValue("Y", shape->rect.y);
            pg->SetPropertyValue("Width", shape->rect.width); pg->SetPropertyValue("Height", shape->rect.height);
            pg->SetPropertyValue("Rotation", shape->rotation); pg->SetPropertyValue("Line Width", shape->lineWidth);
            wxVariant var; var << wxColourPropertyValue(shape->color); pg->SetPropertyValue("Color", var);
        }
        m_ignoreChange = false;
    }
    std::function<void()> OnShapeModified;
private:
    void OnPropertyChanged(wxPropertyGridEvent& e) { if (m_ignoreChange) return; if (OnShapeModified) OnShapeModified(); }
};

// ============================================================================
// 6. 主窗口
// ============================================================================
class MainFrame : public wxFrame {
public:
    MainFrame() : wxFrame(nullptr, wxID_ANY, "EDA Editor", wxDefaultPosition, wxSize(1280, 800)) {
        CreateMenuBar(); CreateToolbar();
        wxSplitterWindow* mainSplitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D | wxSP_LIVE_UPDATE);
        mainSplitter->SetMinimumPaneSize(200);
        wxSplitterWindow* rightSplitter = new wxSplitterWindow(mainSplitter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D | wxSP_LIVE_UPDATE);
        rightSplitter->SetMinimumPaneSize(200);
        m_leftPanel = new LeftSidebarPanel(mainSplitter); m_canvas = new DesignCanvas(rightSplitter); m_propPanel = new RightSidebarPanel(rightSplitter);
        rightSplitter->SplitVertically(m_canvas, m_propPanel, -280); mainSplitter->SplitVertically(m_leftPanel, rightSplitter, 200);

        m_leftPanel->OnLayerChangedCallback = [this](wxString layer) { m_canvas->SetActiveLayer(layer); SetStatusText("Layer: " + layer, 0); };
        m_propPanel->OnShapeModified = [this]() {
            Shape* s = m_canvas->GetSelectedShape(); if (!s) return; auto pg = m_propPanel->pg;
            s->rect.x = pg->GetPropertyValueAsInt("X"); s->rect.y = pg->GetPropertyValueAsInt("Y");
            s->rect.width = pg->GetPropertyValueAsInt("Width"); s->rect.height = pg->GetPropertyValueAsInt("Height");
            s->rotation = pg->GetPropertyValueAsDouble("Rotation"); s->lineWidth = pg->GetPropertyValueAsInt("Line Width");
            if (s->GetTypeName() == "Line") s->Resize(HANDLE_NONE, wxPoint(0, 0));
            wxVariant var = pg->GetPropertyValue("Color");
            if (var.GetType() == wxT("wxColourPropertyValue")) { wxColourPropertyValue cpv; cpv << var; s->color = cpv.m_colour; }
            m_canvas->Refresh();
            };
        m_canvas->OnSelectionChanged = [this]() { m_propPanel->UpdateFromShape(m_canvas->GetSelectedShape()); };
        m_propPanel->UpdateFromShape(nullptr);

        CreateStatusBar(2); int* widths = new int[2] {200, -1}; SetStatusWidths(2, widths); delete[] widths;
        SetStatusText("Layer: Component", 0); SetStatusText("Ready", 1);
    }
private:
    LeftSidebarPanel* m_leftPanel; DesignCanvas* m_canvas; RightSidebarPanel* m_propPanel;
    void CreateMenuBar() {
        wxMenuBar* mb = new wxMenuBar(); wxMenu* fileMenu = new wxMenu(); fileMenu->Append(wxID_EXIT, "E&xit"); mb->Append(fileMenu, "&File");
        wxMenu* toolsMenu = new wxMenu(); toolsMenu->Append(ID_TOOLS_LIB_MANAGER, "Library Manager..."); toolsMenu->Append(ID_TOOLS_FP_EDITOR, "Footprint Editor...");
        mb->Append(toolsMenu, "&Tools"); SetMenuBar(mb);
    }
    void CreateToolbar() {
        wxBitmap dummyBmp(16, 16); { wxMemoryDC mem(dummyBmp); mem.SetBackground(*wxWHITE_BRUSH); mem.Clear(); mem.SetPen(*wxBLACK_PEN); mem.DrawRectangle(2, 2, 12, 12); }
        wxToolBar* tb = CreateToolBar(wxTB_HORIZONTAL | wxTB_FLAT | wxTB_TEXT);
        tb->AddRadioTool(ID_T_SEL, "Select", dummyBmp); tb->AddRadioTool(ID_T_LINE, "Line", dummyBmp); tb->AddRadioTool(ID_T_RECT, "Rect", dummyBmp);
        tb->AddRadioTool(ID_T_CIRCLE, "Circle", dummyBmp); tb->AddRadioTool(ID_T_TEXT, "Text", dummyBmp); tb->Realize();

        Bind(wxEVT_TOOL, [this](wxCommandEvent& e) {
            if (e.GetId() == ID_T_SEL) m_canvas->SetTool(TOOL_SELECT); else if (e.GetId() == ID_T_LINE) m_canvas->SetTool(TOOL_LINE);
            else if (e.GetId() == ID_T_RECT) m_canvas->SetTool(TOOL_RECT); else if (e.GetId() == ID_T_CIRCLE) m_canvas->SetTool(TOOL_CIRCLE);
            else if (e.GetId() == ID_T_TEXT) m_canvas->SetTool(TOOL_TEXT);
            });
        Bind(wxEVT_MENU, [this](wxCommandEvent&) { LibraryManagerDialog dlg(this); dlg.ShowModal(); }, ID_TOOLS_LIB_MANAGER);
        Bind(wxEVT_MENU, [this](wxCommandEvent&) { FootprintEditorFrame* f = new FootprintEditorFrame(this); f->Show(); }, ID_TOOLS_FP_EDITOR);
        Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(true); }, wxID_EXIT);
    }
};

class MyApp : public wxApp { public: virtual bool OnInit() override { (new MainFrame())->Show(); return true; } };
IMPLEMENT_APP(MyApp)