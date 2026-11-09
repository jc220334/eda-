// ============================================================================
// Component instance placed on the board (links to a library footprint)
// ============================================================================
class ComponentShape : public Shape
{
public:
    wxString footprintName;
    wxString designator;
    wxString side = "Top";
    wxPoint2DDouble position;  // footprint origin in board millimetres

    ComponentShape(const wxString& fpName, const wxString& ref, double x, double y)
        : footprintName(fpName), designator(ref), position(x, y)
    {
        layer = LAYER_TOP_COPPER;
        stroke = wxColour(40, 90, 190);
        lineWidth = 0.2;
    }

    wxString TypeName() const override { return "Component"; }
    Shape* Clone() const override { return new ComponentShape(*this); }
    bool CanResize() const override { return false; }
    wxString GetText() const override { return designator; }
    void SetText(const wxString& t) override { designator = t; }

    wxRect2DDouble LocalBounds() const override
    {
        Footprint* fp = FindLibraryFootprint(footprintName);
        if (!fp) return wxRect2DDouble(position.m_x - 3.0, position.m_y - 3.0, 6.0, 6.0);
        wxRect2DDouble b = fp->Bounds();
        b.m_x += position.m_x;
        b.m_y += position.m_y;
        return b;
    }

    void SetLocalBounds(const wxRect2DDouble& r) override
    {
        wxRect2DDouble b = LocalBounds();
        position.m_x += r.m_x - b.m_x;
        position.m_y += r.m_y - b.m_y;
    }

    void Move(double dx, double dy) override { position.m_x += dx; position.m_y += dy; }

    bool HitTest(double x, double y, double tol) const override
    {
        wxPoint2DDouble local = ToLocal(wxPoint2DDouble(x, y));
        wxRect2DDouble b = LocalBounds();
        b.m_x -= tol; b.m_y -= tol; b.m_width += tol * 2; b.m_height += tol * 2;
        return b.Contains(local);
    }

    void Draw(wxGraphicsContext* gc, const Xform& xf) const override
    {
        Footprint* fp = FindLibraryFootprint(footprintName);
        gc->PushState();
        ApplyRotation(gc, xf);
        if (fp) {
            Xform sub = xf;
            sub.ox = xf.ox + position.m_x * xf.scale;
            sub.oy = xf.oy + position.m_y * xf.scale;
            DrawFootprint(gc, sub, *fp, xf.scale >= 2.0, designator, false);
        }
        else {
            wxPoint2DDouble s = xf.ToScreen(position.m_x, position.m_y);
            gc->SetPen(wxPen(*wxRED, 1, wxPENSTYLE_SHORT_DASH));
            gc->SetBrush(*wxTRANSPARENT_BRUSH);
            gc->DrawRectangle(s.m_x - 15, s.m_y - 15, 30, 30);
            gc->SetFont(wxFont(wxFontInfo(wxSize(9, 9))), *wxRED);
            gc->DrawText("MISSING: " + footprintName, s.m_x - 15, s.m_y - 26);
        }
        gc->PopState();
    }
};

// ============================================================================
// Board canvas with full drawing / editing tools (Task B)
// ============================================================================
enum ToolType {
    TOOL_SELECT = 0, TOOL_LINE, TOOL_RECT, TOOL_CIRCLE, TOOL_ARC, TOOL_POLY, TOOL_TEXT
};

enum AlignMode {
    ALIGN_LEFT = 0, ALIGN_RIGHT, ALIGN_TOP, ALIGN_BOTTOM,
    ALIGN_CENTER_H, ALIGN_CENTER_V, ALIGN_DISTRIBUTE_H, ALIGN_DISTRIBUTE_V
};

class DesignCanvas : public wxScrolledWindow
{
public:
    std::function<void()> OnSelectionChanged;
    std::function<void()> OnContentChanged;
    std::function<void(const wxString&, const wxString&)> OnStatus;
    std::function<void(const wxString&)> OnCoords;
    std::function<void()> OnViewChanged;

    DesignCanvas(wxWindow* parent)
        : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL | wxVSCROLL | wxBORDER_NONE)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetBackgroundColour(wxColour(250, 250, 248));
        SetScrollRate(10, 10);
        SetVirtualSize(960, 720);
        Bind(wxEVT_PAINT, &DesignCanvas::OnPaint, this);
        Bind(wxEVT_LEFT_DOWN, &DesignCanvas::OnLeftDown, this);
        Bind(wxEVT_LEFT_UP, &DesignCanvas::OnLeftUp, this);
        Bind(wxEVT_LEFT_DCLICK, &DesignCanvas::OnLeftDClick, this);
        Bind(wxEVT_MOTION, &DesignCanvas::OnMouseMove, this);
        Bind(wxEVT_MIDDLE_DOWN, &DesignCanvas::OnMiddleDown, this);
        Bind(wxEVT_MIDDLE_UP, &DesignCanvas::OnMiddleUp, this);
        Bind(wxEVT_RIGHT_DOWN, &DesignCanvas::OnRightDown, this);
        Bind(wxEVT_MOUSEWHEEL, &DesignCanvas::OnWheel, this);
        Bind(wxEVT_KEY_DOWN, &DesignCanvas::OnKeyDown, this);
        Bind(wxEVT_SIZE, [this](wxSizeEvent& e) { e.Skip(); });
    }

    ~DesignCanvas() override = default;

    // ---- configuration ----------------------------------------------------
    void SetTool(ToolType tool)
    {
        CancelPending();
        m_tool = tool;
        SetCursor(wxCursor(tool == TOOL_SELECT ? wxCURSOR_ARROW : wxCURSOR_CROSS));
        SetFocus();
    }
    ToolType GetTool() const { return m_tool; }
    void SetGridSize(double g) { if (g > 0.001) { m_grid = g; Refresh(); if (OnViewChanged) OnViewChanged(); } }
    double GetGridSize() const { return m_grid; }
    void SetSnap(bool snap) { m_snap = snap; Refresh(); if (OnViewChanged) OnViewChanged(); }
    bool GetSnap() const { return m_snap; }
    void SetShowGrid(bool show) { m_showGrid = show; Refresh(); if (OnViewChanged) OnViewChanged(); }
    bool GetShowGrid() const { return m_showGrid; }
    void SetActiveLayer(int layer) { m_activeLayer = layer; Refresh(); }
    int GetActiveLayer() const { return m_activeLayer; }

    void SetLayerVisible(int layer, bool visible)
    {
        if (layer < 0 || layer >= LAYER_COUNT) return;
        m_layerVisible[layer] = visible;
        Refresh();
    }
    bool GetLayerVisible(int layer) const { return layer >= 0 && layer < LAYER_COUNT ? m_layerVisible[layer] : true; }

    // ---- selection --------------------------------------------------------
    std::vector<Shape*> SelectedShapes() const
    {
        std::vector<Shape*> result;
        for (const auto& s : m_shapes) if (s->selected && m_layerVisible[s->layer]) result.push_back(s.get());
        return result;
    }

    Shape* GetSelectedShape() const
    {
        std::vector<Shape*> sel = SelectedShapes();
        return sel.empty() ? nullptr : sel.front();
    }

    int GetSelectionCount() const { return (int)SelectedShapes().size(); }

    void ClearSelection()
    {
        for (auto& s : m_shapes) s->selected = false;
        NotifySelection();
        Refresh();
    }

    void SelectShape(Shape* target, bool add = false)
    {
        if (!add) for (auto& s : m_shapes) s->selected = false;
        if (target) target->selected = true;
        NotifySelection();
        Refresh();
    }

    void SelectShapeByUid(int uid)
    {
        for (auto& s : m_shapes) {
            if (s->uid == uid) { SelectShape(s.get(), false); return; }
        }
    }

    void SelectAll()
    {
        for (auto& s : m_shapes) if (m_layerVisible[s->layer]) s->selected = true;
        NotifySelection();
        Refresh();
    }

    // ---- editing commands -------------------------------------------------
    void DeleteSelection()
    {
        m_shapes.erase(std::remove_if(m_shapes.begin(), m_shapes.end(),
            [](const std::unique_ptr<Shape>& s) { return s->selected; }), m_shapes.end());
        NotifySelection();
        NotifyContent();
        Refresh();
    }

    void CopySelection()
    {
        m_clipboard.clear();
        for (Shape* s : SelectedShapes()) m_clipboard.emplace_back(s->Clone());
        if (OnStatus) OnStatus(wxString::Format("Copied %d object(s)", (int)m_clipboard.size()), "");
    }

    void CutSelection()
    {
        CopySelection();
        DeleteSelection();
    }

    void PasteClipboard()
    {
        if (m_clipboard.empty()) return;
        for (auto& s : m_shapes) s->selected = false;
        for (auto& s : m_clipboard) {
            Shape* copy = s->Clone();
            copy->Move(5.0, 5.0);
            copy->selected = true;
            copy->uid = m_nextUid++;
            m_shapes.emplace_back(copy);
        }
        // Keep pasting offset from the previous paste.
        for (auto& s : m_clipboard) s->Move(5.0, 5.0);
        NotifySelection();
        NotifyContent();
        Refresh();
    }

    void DuplicateSelection()
    {
        std::vector<Shape*> sel = SelectedShapes();
        if (sel.empty()) return;
        for (auto& s : m_shapes) s->selected = false;
        for (Shape* s : sel) {
            Shape* copy = s->Clone();
            copy->Move(5.0, 5.0);
            copy->selected = true;
            copy->uid = m_nextUid++;
            m_shapes.emplace_back(copy);
        }
        NotifySelection();
        NotifyContent();
        Refresh();
    }

    void RotateSelection(double degrees)
    {
        std::vector<Shape*> sel = SelectedShapes();
        for (Shape* s : sel) s->rotation = NormalizeAngle(s->rotation + degrees);
        if (!sel.empty()) {
            NotifySelection();
            Refresh();
        }
    }

    void Align(AlignMode mode)
    {
        std::vector<Shape*> sel = SelectedShapes();
        if (sel.size() < 2) { if (OnStatus) OnStatus("Align needs at least two selected objects", ""); return; }
        wxRect2DDouble ref(0, 0, 0, 0);
        for (size_t i = 0; i < sel.size(); ++i) ref = (i == 0) ? sel[i]->WorldAABB() : UnionRect2D(ref, sel[i]->WorldAABB());

        if (mode == ALIGN_DISTRIBUTE_H || mode == ALIGN_DISTRIBUTE_V) {
            if (sel.size() < 3) { if (OnStatus) OnStatus("Distribute needs at least three objects", ""); return; }
            bool horizontal = mode == ALIGN_DISTRIBUTE_H;
            std::sort(sel.begin(), sel.end(), [horizontal](Shape* a, Shape* b) {
                wxRect2DDouble ba = a->WorldAABB(), bb = b->WorldAABB();
                return horizontal ? (ba.m_x < bb.m_x) : (ba.m_y < bb.m_y);
            });
            double sum = 0.0;
            for (Shape* s : sel) { wxRect2DDouble b = s->WorldAABB(); sum += horizontal ? b.m_width : b.m_height; }
            double total = horizontal ? ref.m_width : ref.m_height;
            double gap = (total - sum) / (double)(sel.size() - 1);
            double cursor = horizontal ? ref.m_x : ref.m_y;
            for (Shape* s : sel) {
                wxRect2DDouble b = s->WorldAABB();
                double delta = cursor - (horizontal ? b.m_x : b.m_y);
                s->Move(horizontal ? delta : 0.0, horizontal ? 0.0 : delta);
                cursor += (horizontal ? b.m_width : b.m_height) + gap;
            }
        }
        else {
            for (Shape* s : sel) {
                wxRect2DDouble b = s->WorldAABB();
                double dx = 0.0, dy = 0.0;
                switch (mode) {
                case ALIGN_LEFT: dx = ref.m_x - b.m_x; break;
                case ALIGN_RIGHT: dx = (ref.m_x + ref.m_width) - (b.m_x + b.m_width); break;
                case ALIGN_TOP: dy = ref.m_y - b.m_y; break;
                case ALIGN_BOTTOM: dy = (ref.m_y + ref.m_height) - (b.m_y + b.m_height); break;
                case ALIGN_CENTER_H: dx = (ref.m_x + ref.m_width * 0.5) - (b.m_x + b.m_width * 0.5); break;
                case ALIGN_CENTER_V: dy = (ref.m_y + ref.m_height * 0.5) - (b.m_y + b.m_height * 0.5); break;
                default: break;
                }
                s->Move(dx, dy);
            }
        }
        NotifySelection();
        Refresh();
    }

    void BringToFront()
    {
        std::vector<std::unique_ptr<Shape>> moved;
        for (int i = (int)m_shapes.size() - 1; i >= 0; --i) {
            if (m_shapes[i]->selected) {
                moved.emplace_back(std::move(m_shapes[i]));
                m_shapes.erase(m_shapes.begin() + i);
            }
        }
        for (int i = (int)moved.size() - 1; i >= 0; --i) m_shapes.emplace_back(std::move(moved[i]));
        NotifyContent();
        Refresh();
    }

    void SendToBack()
    {
        std::vector<std::unique_ptr<Shape>> moved;
        for (int i = 0; i < (int)m_shapes.size(); ++i) {
            if (m_shapes[i]->selected) {
                moved.emplace_back(std::move(m_shapes[i]));
                m_shapes.erase(m_shapes.begin() + i);
                --i;
            }
        }
        for (int i = (int)moved.size() - 1; i >= 0; --i) m_shapes.insert(m_shapes.begin(), std::move(moved[i]));
        NotifyContent();
        Refresh();
    }

    void SelectByLayer(int layer)
    {
        for (auto& s : m_shapes) s->selected = (s->layer == layer) && m_layerVisible[layer];
        NotifySelection();
        Refresh();
    }

    // ---- component placement ---------------------------------------------
    bool PlaceComponent(const wxString& fpName)
    {
        Footprint* fp = FindLibraryFootprint(fpName);
        if (!fp) return false;
        wxString ref = NextDesignator(fp);
        wxPoint2DDouble center = VisibleCentre();
        auto comp = std::make_unique<ComponentShape>(fpName, ref, center.m_x, center.m_y);
        for (auto& s : m_shapes) s->selected = false;
        comp->selected = true;
        comp->uid = m_nextUid++;
        m_shapes.emplace_back(std::move(comp));
        NotifySelection();
        NotifyContent();
        Refresh();
        if (OnStatus) OnStatus("Placed " + ref + " (" + fpName + ")", "");
        return true;
    }

    int CountComponents(const wxString& fpName) const
    {
        int n = 0;
        for (const auto& s : m_shapes) {
            if (s->TypeName() != "Component") continue;
            const ComponentShape* c = static_cast<const ComponentShape*>(s.get());
            if (fpName.empty() || c->footprintName.IsSameAs(fpName, false)) ++n;
        }
        return n;
    }

    void RenameComponentFootprint(const wxString& oldName, const wxString& newName)
    {
        for (auto& s : m_shapes) {
            if (s->TypeName() != "Component") continue;
            ComponentShape* c = static_cast<ComponentShape*>(s.get());
            if (c->footprintName.IsSameAs(oldName, false)) c->footprintName = newName;
        }
        Refresh();
    }

    void OnLibraryChanged()
    {
        // Footprint geometry may have changed; redraw live references.
        Refresh();
        NotifySelection();
        NotifyContent();
    }

    void NewDocument()
    {
        m_shapes.clear();
        m_clipboard.clear();
        m_nextUid = 1;
        CancelPending();
        NotifySelection();
        NotifyContent();
        Refresh();
    }

    // ---- view -------------------------------------------------------------
    void ZoomIn() { ZoomAt(1.25, wxPoint(GetClientSize().x / 2, GetClientSize().y / 2)); }
    void ZoomOut() { ZoomAt(0.8, wxPoint(GetClientSize().x / 2, GetClientSize().y / 2)); }

    void ZoomFit()
    {
        wxRect2DDouble bounds(0, 0, 0, 0);
        bool first = true;
        for (const auto& s : m_shapes) {
            if (!m_layerVisible[s->layer]) continue;
            bounds = first ? s->WorldAABB() : UnionRect2D(bounds, s->WorldAABB());
            first = false;
        }
        if (first) bounds = wxRect2DDouble(0, 0, 160, 100);
        wxSize cs = GetClientSize();
        double sx = (cs.x - 80) / std::max(1.0, bounds.m_width);
        double sy = (cs.y - 80) / std::max(1.0, bounds.m_height);
        m_scale = ClampD(std::min(sx, sy), 0.8, 30.0);
        UpdateVirtualSize();
        CenterOnWorld(bounds.m_x + bounds.m_width * 0.5, bounds.m_y + bounds.m_height * 0.5);
    }

    void ShowGridChanged(bool show) { SetShowGrid(show); }

    // ---- info -------------------------------------------------------------
    int ShapeCount() const { return (int)m_shapes.size(); }
    int ComponentCount() const { return CountComponents(""); }

    wxString SelectionDescription() const
    {
        std::vector<Shape*> sel = SelectedShapes();
        if (sel.empty()) return "Nothing selected";
        if (sel.size() == 1) return sel[0]->TypeName();
        return wxString::Format("%d objects selected", (int)sel.size());
    }

    std::vector<ObjectInfo> GetObjectInfos() const
    {
        std::vector<ObjectInfo> infos;
        for (const auto& s : m_shapes) {
            ObjectInfo info;
            info.uid = s->uid;
            info.type = s->TypeName();
            info.layer = LayerName(s->layer);
            if (s->TypeName() == "Component") {
                const ComponentShape* c = static_cast<const ComponentShape*>(s.get());
                info.name = c->designator + "  " + c->footprintName;
            }
            else if (s->TypeName() == "Text") {
                info.name = "Text: " + s->GetText();
            }
            else {
                info.name = s->TypeName();
            }
            infos.push_back(info);
        }
        return infos;
    }

    void ShapeEditedExternally()
    {
        NotifyContent();
        NotifySelection();
        Refresh();
    }

    void NotifyExternalSelection() { NotifySelection(); }

    // Populates the board with representative Task B / Task D content. Used by
    // File > Load demo design (and by the --demo command line switch).
    void BuildDemo()
    {
        m_shapes.clear();
        m_nextUid = 1;
        auto add = [this](std::unique_ptr<Shape> s, int layer, const wxColour& colour, double width) {
            s->layer = layer;
            s->stroke = colour;
            s->lineWidth = width;
            s->uid = m_nextUid++;
            m_shapes.emplace_back(std::move(s));
        };

        auto outline = std::make_unique<RectShape>(wxRect2DDouble(0, 0, 160, 100));
        add(std::move(outline), LAYER_OUTLINE, LayerColour(LAYER_OUTLINE), 0.5);

        auto title = std::make_unique<TextShape>(8, 4, "EDA DEMO BOARD", 4.5);
        add(std::move(title), LAYER_SILK, LayerColour(LAYER_SILK), 0.2);

        auto trace1 = std::make_unique<LineShape>(20, 82, 65, 82);
        add(std::move(trace1), LAYER_TOP_COPPER, LayerColour(LAYER_TOP_COPPER), 0.6);
        auto trace2 = std::make_unique<LineShape>(65, 82, 75, 72);
        add(std::move(trace2), LAYER_TOP_COPPER, LayerColour(LAYER_TOP_COPPER), 0.6);

        auto silkCircle = std::make_unique<CircleShape>(wxRect2DDouble(72, 78, 12, 12));
        add(std::move(silkCircle), LAYER_SILK, LayerColour(LAYER_SILK), 0.2);

        auto arc = std::make_unique<ArcShape>(105, 86, 9.0, 0, 240);
        add(std::move(arc), LAYER_SILK, LayerColour(LAYER_SILK), 0.25);

        std::vector<wxPoint2DDouble> tri = { {135, 90}, {150, 90}, {142.5, 78} };
        auto polygon = std::make_unique<PolygonShape>(tri);
        polygon->fill = wxColour(226, 236, 248);
        add(std::move(polygon), LAYER_SILK, LayerColour(LAYER_SILK), 0.2);

        auto body = std::make_unique<RectShape>(wxRect2DDouble(18, 55, 26, 16));
        body->fill = wxColour(215, 230, 248);
        add(std::move(body), LAYER_BOTTOM_COPPER, LayerColour(LAYER_BOTTOM_COPPER), 0.3);

        ComponentShape* selectedComponent = nullptr;
        struct Placement { const char* fp; const char* ref; double x, y, rot; };
        Placement placements[] = {
            { "SOIC-8", "U1", 40.0, 35.0, 0.0 },
            { "R_0603", "R1", 78.0, 24.0, 90.0 },
            { "Header-1x4_2.54mm", "J1", 120.0, 55.0, 90.0 },
            { "SOT-23", "Q1", 78.0, 62.0, 0.0 },
            { "TestPoint_1.0mm", "TP1", 132.0, 40.0, 0.0 }
        };
        for (const auto& p : placements) {
            if (!FindLibraryFootprint(p.fp)) continue;
            auto comp = std::make_unique<ComponentShape>(p.fp, p.ref, p.x, p.y);
            comp->rotation = p.rot;
            comp->uid = m_nextUid++;
            comp->selected = false;
            if (!selectedComponent) selectedComponent = comp.get();
            m_shapes.emplace_back(std::move(comp));
        }

        if (selectedComponent) {
            for (auto& s : m_shapes) s->selected = (s.get() == selectedComponent);
        }
        CancelPending();
        NotifyContent();
        NotifySelection();
        ZoomFit();
    }

private:
    std::vector<std::unique_ptr<Shape>> m_shapes;
    std::vector<std::unique_ptr<Shape>> m_clipboard;
    ToolType m_tool = TOOL_SELECT;
    int m_activeLayer = LAYER_TOP_COPPER;
    bool m_layerVisible[LAYER_COUNT] = { true, true, true, true };
    double m_scale = 4.0;
    double m_grid = 0.5;
    bool m_snap = true;
    bool m_showGrid = true;
    int m_nextUid = 1;

    enum Mode { MODE_IDLE, MODE_MOVE, MODE_RESIZE, MODE_ROTATE, MODE_BOX, MODE_DRAW, MODE_PAN, MODE_POLY, MODE_ARC };
    Mode m_mode = MODE_IDLE;
    Shape* m_activeShape = nullptr;
    int m_resizeHandle = 0;
    wxPoint2DDouble m_dragStart, m_dragLast, m_boxCur;
    wxRect2DDouble m_box;
    bool m_boxAdd = false;
    bool m_moved = false;
    wxPoint m_panMouseStart;
    int m_panScrollX = 0, m_panScrollY = 0;
    double m_rotateStart = 0.0, m_rotateBase = 0.0;

    // polygon / arc in-progress state
    std::vector<wxPoint2DDouble> m_polyPts;
    wxPoint2DDouble m_arcCenter, m_arcStart, m_arcEnd;
    int m_arcStage = 0;

    static constexpr double WORLD_MIN_X = -20.0;
    static constexpr double WORLD_MIN_Y = -20.0;
    static constexpr double WORLD_W = 240.0;
    static constexpr double WORLD_H = 180.0;

    Xform CurrentXform() const
    {
        int sx = 0, sy = 0;
        GetViewStart(&sx, &sy);
        int px = 0, py = 0;
        GetScrollPixelsPerUnit(&px, &py);
        double scrollX = sx * px;
        double scrollY = sy * py;
        Xform xf;
        xf.scale = m_scale;
        xf.ox = -WORLD_MIN_X * m_scale - scrollX;
        xf.oy = -WORLD_MIN_Y * m_scale - scrollY;
        return xf;
    }

    wxPoint2DDouble MouseWorld(const wxMouseEvent& e, bool snap) const
    {
        Xform xf = CurrentXform();
        wxPoint2DDouble w = xf.ToWorld(e.GetX(), e.GetY());
        if (snap && m_snap) {
            w.m_x = SnapTo(w.m_x, m_grid);
            w.m_y = SnapTo(w.m_y, m_grid);
        }
        return w;
    }

    void UpdateVirtualSize()
    {
        SetVirtualSize((int)std::lround(WORLD_W * m_scale), (int)std::lround(WORLD_H * m_scale));
        Refresh();
    }

    wxPoint2DDouble VisibleCentre() const
    {
        Xform xf = CurrentXform();
        wxSize cs = GetClientSize();
        return xf.ToWorld(cs.x * 0.5, cs.y * 0.5);
    }

    void CenterOnWorld(double wx_, double wy_)
    {
        wxSize cs = GetClientSize();
        double scrollX = (wx_ - WORLD_MIN_X) * m_scale - cs.x * 0.5;
        double scrollY = (wy_ - WORLD_MIN_Y) * m_scale - cs.y * 0.5;
        int px = 0, py = 0;
        GetScrollPixelsPerUnit(&px, &py);
        Scroll((int)std::max(0.0, scrollX / std::max(1, px)),
               (int)std::max(0.0, scrollY / std::max(1, py)));
        Refresh();
    }

    void ZoomAt(double factor, const wxPoint& anchor)
    {
        Xform before = CurrentXform();
        wxPoint2DDouble world = before.ToWorld(anchor.x, anchor.y);
        double newScale = ClampD(m_scale * factor, 0.8, 40.0);
        if (std::abs(newScale - m_scale) < 1e-9) return;
        m_scale = newScale;
        SetVirtualSize((int)std::lround(WORLD_W * m_scale), (int)std::lround(WORLD_H * m_scale));
        double scrollX = (world.m_x - WORLD_MIN_X) * m_scale - anchor.x;
        double scrollY = (world.m_y - WORLD_MIN_Y) * m_scale - anchor.y;
        int px = 0, py = 0;
        GetScrollPixelsPerUnit(&px, &py);
        Scroll((int)std::max(0.0, scrollX / std::max(1, px)),
               (int)std::max(0.0, scrollY / std::max(1, py)));
        Refresh();
    }

    void NotifySelection() { if (OnSelectionChanged) OnSelectionChanged(); }
    void NotifyContent() { if (OnContentChanged) OnContentChanged(); }

    void CancelPending()
    {
        m_mode = MODE_IDLE;
        m_polyPts.clear();
        m_arcStage = 0;
        m_activeShape = nullptr;
        m_resizeHandle = 0;
        if (HasCapture()) ReleaseMouse();
        Refresh();
    }

    wxString NextDesignator(const Footprint* fp) const
    {
        wxString prefix = "U";
        wxString n = fp ? fp->name.Lower() : wxString();
        wxString cat = fp ? fp->category.Lower() : wxString();
        if (n.StartsWith("r_") || n.StartsWith("res") || cat.Contains("passive")) prefix = "R";
        else if (n.StartsWith("c_") || n.StartsWith("cap")) prefix = "C";
        else if (n.Contains("led")) prefix = "D";
        else if (n.Contains("sot")) prefix = "Q";
        else if (n.Contains("header") || cat.Contains("connector")) prefix = "J";
        else if (n.StartsWith("testpoint") || n.StartsWith("tp")) prefix = "TP";
        int index = 1;
        for (;;) {
            wxString candidate = wxString::Format("%s%d", prefix, index);
            bool exists = false;
            for (const auto& s : m_shapes) {
                if (s->TypeName() == "Component" && static_cast<ComponentShape*>(s.get())->designator == candidate) { exists = true; break; }
            }
            if (!exists) return candidate;
            ++index;
        }
    }

    // ---- painting ---------------------------------------------------------
    void OnPaint(wxPaintEvent&)
    {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(GetBackgroundColour()));
        dc.Clear();
        Xform xf = CurrentXform();
        DrawGrid(dc, xf);

        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        if (!gc) return;
        gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);

        for (const auto& s : m_shapes) {
            if (!m_layerVisible[s->layer]) continue;
            s->Draw(gc.get(), xf);
        }
        for (const auto& s : m_shapes) {
            if (s->selected && m_layerVisible[s->layer]) s->DrawSelection(gc.get(), xf);
        }

        DrawPending(gc.get(), xf);
    }

    void DrawGrid(wxDC& dc, const Xform& xf)
    {
        if (!m_showGrid) return;
        wxSize cs = GetClientSize();
        double left = xf.ToWorld(0, 0).m_x;
        double top = xf.ToWorld(0, 0).m_y;
        double right = xf.ToWorld(cs.x, 0).m_x;
        double bottom = xf.ToWorld(0, cs.y).m_y;
        double spacing = m_grid * xf.scale;
        if (spacing < 4.0) {
            // avoid drawing an unreadable grid
            double factor = std::ceil(4.0 / std::max(0.001, spacing));
            spacing *= factor;
        }
        double step = spacing / xf.scale;

        dc.SetPen(wxPen(wxColour(232, 232, 228), 1));
        int maxLines = 4000;
        int lines = 0;
        for (double x = std::floor(left / step) * step; x <= right && lines < maxLines; x += step, ++lines) {
            wxPoint2DDouble a = xf.ToScreen(x, top);
            wxPoint2DDouble b = xf.ToScreen(x, bottom);
            dc.DrawLine((int)std::lround(a.m_x), 0, (int)std::lround(a.m_x), cs.y);
        }
        for (double y = std::floor(top / step) * step; y <= bottom && lines < maxLines; y += step, ++lines) {
            dc.DrawLine(0, (int)std::lround(xf.ToScreen(0, y).m_y), cs.x, (int)std::lround(xf.ToScreen(0, y).m_y));
        }

        // Axes / origin
        wxPoint2DDouble o = xf.ToScreen(0, 0);
        dc.SetPen(wxPen(wxColour(205, 205, 200), 1));
        if (o.m_x >= 0 && o.m_x <= cs.x) dc.DrawLine((int)o.m_x, 0, (int)o.m_x, cs.y);
        if (o.m_y >= 0 && o.m_y <= cs.y) dc.DrawLine(0, (int)o.m_y, cs.x, (int)o.m_y);

        // Board working area outline
        dc.SetPen(wxPen(wxColour(190, 190, 185), 1, wxPENSTYLE_DOT));
        wxPoint2DDouble a = xf.ToScreen(0, 0);
        wxPoint2DDouble b = xf.ToScreen(160, 100);
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawRectangle((int)a.m_x, (int)a.m_y, (int)(b.m_x - a.m_x), (int)(b.m_y - a.m_y));
    }

    void DrawPending(wxGraphicsContext* gc, const Xform& xf)
    {
        if (m_mode == MODE_BOX) {
            wxPoint2DDouble a = xf.ToScreen(m_box.m_x, m_box.m_y);
            gc->SetPen(wxPen(wxColour(30, 100, 230), 1, wxPENSTYLE_SHORT_DASH));
            gc->SetBrush(wxBrush(wxColour(30, 100, 230, 35)));
            gc->DrawRectangle(a.m_x, a.m_y, xf.Len(m_box.m_width), xf.Len(m_box.m_height));
            return;
        }
        if (m_mode == MODE_DRAW) {
            gc->SetPen(wxPen(wxColour(30, 100, 230), 1, wxPENSTYLE_SHORT_DASH));
            gc->SetBrush(*wxTRANSPARENT_BRUSH);
            wxPoint2DDouble a = xf.ToScreen(m_dragStart.m_x, m_dragStart.m_y);
            wxPoint2DDouble b = xf.ToScreen(m_dragLast.m_x, m_dragLast.m_y);
            if (m_tool == TOOL_LINE) {
                gc->StrokeLine(a.m_x, a.m_y, b.m_x, b.m_y);
            }
            else if (m_tool == TOOL_RECT) {
                gc->DrawRectangle(std::min(a.m_x, b.m_x), std::min(a.m_y, b.m_y), std::abs(b.m_x - a.m_x), std::abs(b.m_y - a.m_y));
            }
            else if (m_tool == TOOL_CIRCLE) {
                gc->DrawEllipse(std::min(a.m_x, b.m_x), std::min(a.m_y, b.m_y), std::abs(b.m_x - a.m_x), std::abs(b.m_y - a.m_y));
            }
            return;
        }
        if (m_mode == MODE_POLY) {
            if (m_polyPts.empty()) return;
            gc->SetPen(wxPen(wxColour(30, 100, 230), 1, wxPENSTYLE_SHORT_DASH));
            gc->SetBrush(*wxTRANSPARENT_BRUSH);
            wxGraphicsPath path = gc->CreatePath();
            wxPoint2DDouble s = xf.ToScreen(m_polyPts[0].m_x, m_polyPts[0].m_y);
            path.MoveToPoint(s.m_x, s.m_y);
            for (size_t i = 1; i < m_polyPts.size(); ++i) {
                s = xf.ToScreen(m_polyPts[i].m_x, m_polyPts[i].m_y);
                path.AddLineToPoint(s.m_x, s.m_y);
            }
            wxPoint2DDouble cur = xf.ToScreen(m_dragLast.m_x, m_dragLast.m_y);
            path.AddLineToPoint(cur.m_x, cur.m_y);
            gc->StrokePath(path);
            for (const auto& p : m_polyPts) {
                wxPoint2DDouble sp = xf.ToScreen(p.m_x, p.m_y);
                gc->SetBrush(*wxWHITE_BRUSH);
                gc->DrawEllipse(sp.m_x - 3, sp.m_y - 3, 6, 6);
            }
            return;
        }
        if (m_mode == MODE_ARC && m_arcStage >= 1) {
            gc->SetPen(wxPen(wxColour(30, 100, 230), 1, wxPENSTYLE_SHORT_DASH));
            gc->SetBrush(*wxTRANSPARENT_BRUSH);
            wxPoint2DDouble c = xf.ToScreen(m_arcCenter.m_x, m_arcCenter.m_y);
            wxPoint2DDouble e = xf.ToScreen(m_dragLast.m_x, m_dragLast.m_y);
            double radius = Dist(m_arcCenter.m_x, m_arcCenter.m_y, m_dragLast.m_x, m_dragLast.m_y);
            gc->DrawEllipse(c.m_x - xf.Len(radius), c.m_y - xf.Len(radius), xf.Len(radius) * 2, xf.Len(radius) * 2);
            gc->SetPen(wxPen(wxColour(30, 100, 230), 1));
            gc->StrokeLine(c.m_x, c.m_y, e.m_x, e.m_y);
            if (m_arcStage >= 2) {
                double start = Rad2Deg(std::atan2(m_arcStart.m_y - m_arcCenter.m_y, m_arcStart.m_x - m_arcCenter.m_x));
                double end = Rad2Deg(std::atan2(m_dragLast.m_y - m_arcCenter.m_y, m_dragLast.m_x - m_arcCenter.m_x));
                double sweep = NormalizeAngle(end - start);
                if (sweep < 0.5) sweep = 360.0;
                double r = Dist(m_arcCenter.m_x, m_arcCenter.m_y, m_arcStart.m_x, m_arcStart.m_y);
                wxGraphicsPath path = gc->CreatePath();
                int segments = std::max(8, (int)(sweep / 4));
                for (int i = 0; i <= segments; ++i) {
                    double ang = Deg2Rad(start + sweep * i / segments);
                    wxPoint2DDouble p = xf.ToScreen(m_arcCenter.m_x + r * std::cos(ang), m_arcCenter.m_y + r * std::sin(ang));
                    if (i == 0) path.MoveToPoint(p.m_x, p.m_y);
                    else path.AddLineToPoint(p.m_x, p.m_y);
                }
                gc->StrokePath(path);
            }
        }
    }

    // ---- mouse events -----------------------------------------------------
    void OnLeftDown(wxMouseEvent& e)
    {
        SetFocus();
        Xform xf = CurrentXform();
        wxPoint2DDouble raw = xf.ToWorld(e.GetX(), e.GetY());
        wxPoint2DDouble p = MouseWorld(e, true);
        if (m_mode != MODE_IDLE) return;

        if (m_tool == TOOL_SELECT) {
            Shape* sel = GetSelectedShape();
            if (sel && sel->selected) {
                int handle = sel->HitTestHandle(xf, e.GetX(), e.GetY());
                if (handle != 0) {
                    m_activeShape = sel;
                    m_resizeHandle = handle;
                    m_dragStart = p;
                    m_dragLast = p;
                    if (handle == 9) {
                        m_mode = MODE_ROTATE;
                        m_rotateBase = sel->rotation;
                        wxPoint2DDouble c = sel->Center();
                        m_rotateStart = Rad2Deg(std::atan2(raw.m_y - c.m_y, raw.m_x - c.m_x));
                    }
                    else {
                        m_mode = MODE_RESIZE;
                    }
                    CaptureMouse();
                    return;
                }
            }

            Shape* hit = nullptr;
            for (int i = (int)m_shapes.size() - 1; i >= 0; --i) {
                if (!m_layerVisible[m_shapes[i]->layer]) continue;
                if (m_shapes[i]->HitTest(raw.m_x, raw.m_y, 0.3)) { hit = m_shapes[i].get(); break; }
            }
            bool add = e.ShiftDown() || e.ControlDown();
            if (hit) {
                if (add) {
                    hit->selected = !hit->selected;
                    if (!hit->selected) { NotifySelection(); Refresh(); return; }
                }
                else if (!hit->selected) {
                    for (auto& s : m_shapes) s->selected = false;
                    hit->selected = true;
                }
                m_mode = MODE_MOVE;
                m_dragStart = p;
                m_dragLast = p;
                m_moved = false;
                CaptureMouse();
                NotifySelection();
                Refresh();
                return;
            }

            if (!add) for (auto& s : m_shapes) s->selected = false;
            m_boxAdd = add;
            m_mode = MODE_BOX;
            m_dragStart = raw;
            m_box = wxRect2DDouble(raw.m_x, raw.m_y, 0, 0);
            CaptureMouse();
            NotifySelection();
            Refresh();
            return;
        }

        if (m_tool == TOOL_TEXT) {
            wxString text = wxGetTextFromUser("Text to place:", "Text", "TEXT", this);
            if (!TrimStr(text).empty()) {
                auto t = std::make_unique<TextShape>(p.m_x, p.m_y, text, 2.0);
                t->layer = m_activeLayer;
                t->stroke = LayerColour(m_activeLayer);
                t->uid = m_nextUid++;
                for (auto& s : m_shapes) s->selected = false;
                t->selected = true;
                m_shapes.emplace_back(std::move(t));
                NotifySelection();
                NotifyContent();
                Refresh();
            }
            return;
        }

        if (m_tool == TOOL_POLY) {
            m_mode = MODE_POLY;
            m_polyPts.push_back(p);
            m_dragLast = p;
            Refresh();
            return;
        }

        if (m_tool == TOOL_ARC) {
            m_mode = MODE_ARC;
            if (m_arcStage == 0) {
                m_arcCenter = p;
                m_arcStage = 1;
            }
            else if (m_arcStage == 1) {
                if (Dist(m_arcCenter.m_x, m_arcCenter.m_y, p.m_x, p.m_y) > 0.2) {
                    m_arcStart = p;
                    m_arcStage = 2;
                }
            }
            else {
                m_arcEnd = p;
                double r = Dist(m_arcCenter.m_x, m_arcCenter.m_y, m_arcStart.m_x, m_arcStart.m_y);
                double start = Rad2Deg(std::atan2(m_arcStart.m_y - m_arcCenter.m_y, m_arcStart.m_x - m_arcCenter.m_x));
                double end = Rad2Deg(std::atan2(m_arcEnd.m_y - m_arcCenter.m_y, m_arcEnd.m_x - m_arcCenter.m_x));
                double sweep = NormalizeAngle(end - start);
                if (sweep < 0.5) sweep = 360.0;
                if (r > 0.1) {
                    auto arc = std::make_unique<ArcShape>(m_arcCenter.m_x, m_arcCenter.m_y, r, start, sweep);
                    arc->layer = m_activeLayer;
                    arc->stroke = LayerColour(m_activeLayer);
                    arc->uid = m_nextUid++;
                    for (auto& s : m_shapes) s->selected = false;
                    arc->selected = true;
                    m_shapes.emplace_back(std::move(arc));
                    NotifySelection();
                    NotifyContent();
                }
                m_arcStage = 0;
                m_mode = MODE_IDLE;
                Refresh();
            }
            return;
        }

        // line / rect / circle: click-drag
        m_mode = MODE_DRAW;
        m_dragStart = p;
        m_dragLast = p;
        CaptureMouse();
        Refresh();
    }

    void OnMouseMove(wxMouseEvent& e)
    {
        Xform xf = CurrentXform();
        wxPoint2DDouble raw = xf.ToWorld(e.GetX(), e.GetY());
        wxPoint2DDouble p = MouseWorld(e, true);
        if (OnCoords) OnCoords(wxString::Format("%.3f, %.3f mm", raw.m_x, raw.m_y));

        switch (m_mode) {
        case MODE_MOVE: {
            wxPoint2DDouble delta(p.m_x - m_dragLast.m_x, p.m_y - m_dragLast.m_y);
            if (delta.m_x != 0.0 || delta.m_y != 0.0) {
                for (Shape* s : SelectedShapes()) s->Move(delta.m_x, delta.m_y);
                m_dragLast = p;
                m_moved = true;
                NotifySelection();
                Refresh();
            }
            break;
        }
        case MODE_RESIZE:
            if (m_activeShape) {
                m_activeShape->ResizeHandle(m_resizeHandle, p);
                NotifySelection();
                Refresh();
            }
            break;
        case MODE_ROTATE:
            if (m_activeShape) {
                wxPoint2DDouble c = m_activeShape->Center();
                double ang = Rad2Deg(std::atan2(raw.m_y - c.m_y, raw.m_x - c.m_x));
                double delta = ang - m_rotateStart;
                if (m_snap) delta = std::round(delta / 15.0) * 15.0;
                m_activeShape->rotation = NormalizeAngle(m_rotateBase + delta);
                NotifySelection();
                Refresh();
            }
            break;
        case MODE_BOX:
            m_box = NormalizeRect2D(m_dragStart.m_x, m_dragStart.m_y, raw.m_x, raw.m_y);
            Refresh();
            break;
        case MODE_DRAW:
            m_dragLast = p;
            if (e.ShiftDown()) {
                if (m_tool == TOOL_LINE) {
                    double dx = p.m_x - m_dragStart.m_x, dy = p.m_y - m_dragStart.m_y;
                    double ang = std::round(std::atan2(dy, dx) / (PI_VAL / 4.0)) * (PI_VAL / 4.0);
                    double len = std::sqrt(dx * dx + dy * dy);
                    m_dragLast = wxPoint2DDouble(m_dragStart.m_x + std::cos(ang) * len, m_dragStart.m_y + std::sin(ang) * len);
                }
                else {
                    double dx = p.m_x - m_dragStart.m_x, dy = p.m_y - m_dragStart.m_y;
                    double s = std::max(std::abs(dx), std::abs(dy));
                    m_dragLast = wxPoint2DDouble(m_dragStart.m_x + (dx < 0 ? -s : s), m_dragStart.m_y + (dy < 0 ? -s : s));
                }
            }
            Refresh();
            break;
        case MODE_POLY:
        case MODE_ARC:
            m_dragLast = p;
            Refresh();
            break;
        case MODE_PAN: {
            wxPoint pos = e.GetPosition();
            int dx = pos.x - m_panMouseStart.x;
            int dy = pos.y - m_panMouseStart.y;
            int px = 0, py = 0;
            GetScrollPixelsPerUnit(&px, &py);
            Scroll(m_panScrollX - (int)(dx / std::max(1, px)),
                   m_panScrollY - (int)(dy / std::max(1, py)));
            Refresh();
            break;
        }
        default:
            break;
        }
    }

    void OnLeftUp(wxMouseEvent& e)
    {
        if (m_mode == MODE_MOVE || m_mode == MODE_RESIZE || m_mode == MODE_ROTATE) {
            if (m_moved || m_mode != MODE_MOVE) NotifyContent();
            m_mode = MODE_IDLE;
            m_activeShape = nullptr;
            m_resizeHandle = 0;
            if (HasCapture()) ReleaseMouse();
            Refresh();
            return;
        }
        if (m_mode == MODE_BOX) {
            for (const auto& s : m_shapes) {
                if (!m_layerVisible[s->layer]) continue;
                if (m_box.Intersects(s->WorldAABB())) {
                    if (m_boxAdd) s->selected = true;
                    else s->selected = true;
                }
                else if (!m_boxAdd) {
                    // selection was already cleared on mouse-down
                }
            }
            m_mode = MODE_IDLE;
            if (HasCapture()) ReleaseMouse();
            NotifySelection();
            Refresh();
            return;
        }
        if (m_mode == MODE_DRAW) {
            wxPoint2DDouble p = MouseWorld(e, true);
            if (e.ShiftDown() && m_tool != TOOL_LINE) {
                double dx = p.m_x - m_dragStart.m_x, dy = p.m_y - m_dragStart.m_y;
                double s = std::max(std::abs(dx), std::abs(dy));
                p = wxPoint2DDouble(m_dragStart.m_x + (dx < 0 ? -s : s), m_dragStart.m_y + (dy < 0 ? -s : s));
            }
            std::unique_ptr<Shape> shape;
            if (m_tool == TOOL_LINE) {
                if (Dist(m_dragStart.m_x, m_dragStart.m_y, p.m_x, p.m_y) > 0.05)
                    shape = std::make_unique<LineShape>(m_dragStart.m_x, m_dragStart.m_y, p.m_x, p.m_y);
            }
            else if (m_tool == TOOL_RECT) {
                if (std::abs(p.m_x - m_dragStart.m_x) > 0.05 || std::abs(p.m_y - m_dragStart.m_y) > 0.05)
                    shape = std::make_unique<RectShape>(NormalizeRect2D(m_dragStart.m_x, m_dragStart.m_y, p.m_x, p.m_y));
            }
            else if (m_tool == TOOL_CIRCLE) {
                if (std::abs(p.m_x - m_dragStart.m_x) > 0.05 || std::abs(p.m_y - m_dragStart.m_y) > 0.05)
                    shape = std::make_unique<CircleShape>(NormalizeRect2D(m_dragStart.m_x, m_dragStart.m_y, p.m_x, p.m_y));
            }
            if (shape) {
                shape->layer = m_activeLayer;
                shape->stroke = LayerColour(m_activeLayer);
                shape->uid = m_nextUid++;
                for (auto& s : m_shapes) s->selected = false;
                shape->selected = true;
                m_shapes.emplace_back(std::move(shape));
                NotifySelection();
                NotifyContent();
            }
            m_mode = MODE_IDLE;
            if (HasCapture()) ReleaseMouse();
            Refresh();
        }
    }

    void OnLeftDClick(wxMouseEvent& e)
    {
        if (m_mode == MODE_POLY) {
            if (m_polyPts.size() >= 2) {
                wxPoint2DDouble p = m_polyPts.back();
                if (m_polyPts.size() > 1 && Dist(p.m_x, p.m_y, m_polyPts[m_polyPts.size() - 2].m_x, m_polyPts[m_polyPts.size() - 2].m_y) < 0.01)
                    m_polyPts.pop_back();
            }
            FinishPolygon();
        }
        else {
            e.Skip();
        }
    }

    void FinishPolygon()
    {
        if (m_polyPts.size() >= 3) {
            auto poly = std::make_unique<PolygonShape>(m_polyPts);
            poly->layer = m_activeLayer;
            poly->stroke = LayerColour(m_activeLayer);
            poly->fill = wxColour(242, 235, 225);
            poly->uid = m_nextUid++;
            for (auto& s : m_shapes) s->selected = false;
            poly->selected = true;
            m_shapes.emplace_back(std::move(poly));
            NotifySelection();
            NotifyContent();
        }
        m_polyPts.clear();
        m_mode = MODE_IDLE;
        if (HasCapture()) ReleaseMouse();
        Refresh();
    }

    void OnMiddleDown(wxMouseEvent& e)
    {
        if (m_mode != MODE_IDLE) return;
        m_mode = MODE_PAN;
        m_panMouseStart = e.GetPosition();
        int sx, sy;
        GetViewStart(&sx, &sy);
        m_panScrollX = sx;
        m_panScrollY = sy;
        SetCursor(wxCursor(wxCURSOR_SIZING));
        CaptureMouse();
    }

    void OnMiddleUp(wxMouseEvent&)
    {
        if (m_mode == MODE_PAN) {
            m_mode = MODE_IDLE;
            if (HasCapture()) ReleaseMouse();
            SetCursor(wxCursor(m_tool == TOOL_SELECT ? wxCURSOR_ARROW : wxCURSOR_CROSS));
            Refresh();
        }
    }

    void OnRightDown(wxMouseEvent& e)
    {
        if (m_mode == MODE_POLY) {
            FinishPolygon();
        }
        else if (m_mode == MODE_ARC) {
            CancelPending();
        }
        else {
            e.Skip();
        }
    }

    void OnWheel(wxMouseEvent& e)
    {
        if (e.ControlDown()) {
            ZoomAt(e.GetWheelRotation() > 0 ? 1.15 : 1.0 / 1.15, e.GetPosition());
        }
        else if (e.ShiftDown()) {
            int sx, sy;
            GetViewStart(&sx, &sy);
            Scroll(sx - e.GetWheelRotation() / 120 * 3, sy);
            Refresh();
        }
        else {
            int sx, sy;
            GetViewStart(&sx, &sy);
            Scroll(sx, sy - e.GetWheelRotation() / 120 * 3);
            Refresh();
        }
    }

    void OnKeyDown(wxKeyEvent& e)
    {
        bool ctrl = e.ControlDown();
        bool shift = e.ShiftDown();
        int code = e.GetKeyCode();
        if (code == WXK_DELETE || code == WXK_BACK) {
            if (m_mode == MODE_POLY) {
                if (!m_polyPts.empty()) m_polyPts.pop_back();
                Refresh();
            }
            else DeleteSelection();
            return;
        }
        if (code == WXK_ESCAPE) {
            CancelPending();
            ClearSelection();
            return;
        }
        if (code == WXK_RETURN) {
            if (m_mode == MODE_POLY) FinishPolygon();
            return;
        }
        if (ctrl && code == 'A') { SelectAll(); return; }
        if (ctrl && code == 'C') { CopySelection(); return; }
        if (ctrl && code == 'X') { CutSelection(); return; }
        if (ctrl && code == 'V') { PasteClipboard(); return; }
        if (ctrl && code == 'D') { DuplicateSelection(); return; }
        if (ctrl && code == 'R') { RotateSelection(90.0); return; }
        if (ctrl && (code == '=' || code == WXK_ADD || code == WXK_NUMPAD_ADD)) { ZoomIn(); return; }
        if (ctrl && (code == '-' || code == WXK_SUBTRACT || code == WXK_NUMPAD_SUBTRACT)) { ZoomOut(); return; }
        if (ctrl && code == '0') { ZoomFit(); return; }
        if (ctrl) { e.Skip(); return; }

        if (code == 'R') {
            RotateSelection(shift ? -90.0 : 90.0);
            return;
        }
        if (code == 'G') { SetShowGrid(!m_showGrid); return; }
        if (code == 'S') { SetSnap(!m_snap); return; }
        if (code == WXK_LEFT || code == WXK_RIGHT || code == WXK_UP || code == WXK_DOWN) {
            double step = m_snap ? m_grid : 0.1;
            if (shift) step *= 10.0;
            double dx = 0, dy = 0;
            if (code == WXK_LEFT) dx = -step;
            if (code == WXK_RIGHT) dx = step;
            if (code == WXK_UP) dy = -step;
            if (code == WXK_DOWN) dy = step;
            for (Shape* s : SelectedShapes()) s->Move(dx, dy);
            NotifySelection();
            Refresh();
            return;
        }
        e.Skip();
    }
};

