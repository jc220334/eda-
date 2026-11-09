// ============================================================================
// Footprint editor canvas (Task C)
// ============================================================================
enum FpTool {
    FP_SELECT = 0, FP_PAD_SMD, FP_PAD_THT, FP_VIA,
    FP_LINE, FP_RECT, FP_CIRCLE, FP_ARC, FP_POLY, FP_TEXT
};

class FootprintCanvas : public wxScrolledWindow
{
public:
    std::function<void()> OnModified;
    std::function<void()> OnSelectionChanged;
    std::function<void(const wxString&, const wxString&)> OnStatus;
    std::function<void(const wxString&)> OnCoords;

    FootprintCanvas(wxWindow* parent)
        : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL | wxVSCROLL | wxBORDER_NONE)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetBackgroundColour(wxColour(250, 250, 248));
        SetScrollRate(10, 10);
        SetVirtualSize(960, 960);
        Bind(wxEVT_PAINT, &FootprintCanvas::OnPaint, this);
        Bind(wxEVT_LEFT_DOWN, &FootprintCanvas::OnLeftDown, this);
        Bind(wxEVT_LEFT_UP, &FootprintCanvas::OnLeftUp, this);
        Bind(wxEVT_LEFT_DCLICK, &FootprintCanvas::OnLeftDClick, this);
        Bind(wxEVT_MOTION, &FootprintCanvas::OnMouseMove, this);
        Bind(wxEVT_MIDDLE_DOWN, &FootprintCanvas::OnMiddleDown, this);
        Bind(wxEVT_MIDDLE_UP, &FootprintCanvas::OnMiddleUp, this);
        Bind(wxEVT_RIGHT_DOWN, &FootprintCanvas::OnRightDown, this);
        Bind(wxEVT_MOUSEWHEEL, &FootprintCanvas::OnWheel, this);
        Bind(wxEVT_KEY_DOWN, &FootprintCanvas::OnKeyDown, this);
    }

    void SetFootprint(Footprint* fp)
    {
        m_fp = fp;
        Refresh();
    }

    void SetTool(FpTool tool)
    {
        CancelPending();
        m_tool = tool;
        SetCursor(wxCursor(tool == FP_SELECT ? wxCURSOR_ARROW : wxCURSOR_CROSS));
        SetFocus();
    }

    void SetGridSize(double g) { if (g > 0.001) { m_grid = g; Refresh(); } }
    void SetSnap(bool s) { m_snap = s; }
    void SetShowLabels(bool show) { m_showLabels = show; Refresh(); }
    bool GetShowLabels() const { return m_showLabels; }
    double GetScale() const { return m_scale; }

    int SelectionCount() const { return (int)(SelectedPads().size() + SelectedShapes().size()); }

    std::vector<Pad*> SelectedPads() const
    {
        std::vector<Pad*> result;
        if (!m_fp) return result;
        for (auto& p : m_fp->pads) if (p.selected) result.push_back(const_cast<Pad*>(&p));
        return result;
    }

    std::vector<Shape*> SelectedShapes() const
    {
        std::vector<Shape*> result;
        if (!m_fp) return result;
        for (auto& s : m_fp->shapes) if (s->selected) result.push_back(s.get());
        return result;
    }

    Pad* GetSelectedPad() const
    {
        std::vector<Pad*> pads = SelectedPads();
        return pads.size() == 1 ? pads.front() : nullptr;
    }

    Shape* GetSelectedShape() const
    {
        std::vector<Shape*> shapes = SelectedShapes();
        return (shapes.size() == 1 && SelectedPads().empty()) ? shapes.front() : nullptr;
    }

    void ClearSelection()
    {
        if (!m_fp) return;
        for (auto& p : m_fp->pads) p.selected = false;
        for (auto& s : m_fp->shapes) s->selected = false;
        if (OnSelectionChanged) OnSelectionChanged();
        Refresh();
    }

    void SelectAll()
    {
        if (!m_fp) return;
        for (auto& p : m_fp->pads) p.selected = true;
        for (auto& s : m_fp->shapes) s->selected = true;
        if (OnSelectionChanged) OnSelectionChanged();
        Refresh();
    }

    void DeleteSelection()
    {
        if (!m_fp) return;
        m_fp->pads.erase(std::remove_if(m_fp->pads.begin(), m_fp->pads.end(),
            [](const Pad& p) { return p.selected; }), m_fp->pads.end());
        m_fp->shapes.erase(std::remove_if(m_fp->shapes.begin(), m_fp->shapes.end(),
            [](const std::unique_ptr<Shape>& s) { return s->selected; }), m_fp->shapes.end());
        NotifyEdited();
    }

    void CopySelection()
    {
        m_clipPads.clear();
        m_clipShapes.clear();
        if (!m_fp) return;
        for (auto& p : m_fp->pads) if (p.selected) m_clipPads.push_back(p);
        for (auto& s : m_fp->shapes) if (s->selected) m_clipShapes.emplace_back(s->Clone());
        if (OnStatus) OnStatus(wxString::Format("Copied %d pad(s) and %d object(s)",
            (int)m_clipPads.size(), (int)m_clipShapes.size()), "");
    }

    void PasteSelection()
    {
        if (!m_fp) return;
        if (m_clipPads.empty() && m_clipShapes.empty()) return;
        ClearSelection();
        for (const Pad& src : m_clipPads) {
            Pad p = src;
            p.uid = NextPadUid();
            p.x += 2.0; p.y += 2.0;
            p.selected = true;
            if (p.type != PAD_VIA) p.number = m_fp->NextPadNumber();
            m_fp->pads.push_back(p);
        }
        for (const auto& src : m_clipShapes) {
            Shape* c = src->Clone();
            c->Move(2.0, 2.0);
            c->selected = true;
            m_fp->shapes.emplace_back(c);
        }
        NotifyEdited();
    }

    void DuplicateSelection()
    {
        if (!m_fp) return;
        std::vector<Pad> pads;
        std::vector<std::unique_ptr<Shape>> shapes;
        for (auto& p : m_fp->pads) if (p.selected) pads.push_back(p);
        for (auto& s : m_fp->shapes) if (s->selected) shapes.emplace_back(s->Clone());
        ClearSelection();
        for (const Pad& src : pads) {
            Pad p = src;
            p.uid = NextPadUid();
            p.x += 2.0; p.y += 2.0;
            p.selected = true;
            if (p.type != PAD_VIA) p.number = m_fp->NextPadNumber();
            m_fp->pads.push_back(p);
        }
        for (auto& src : shapes) {
            src->Move(2.0, 2.0);
            src->selected = true;
            m_fp->shapes.emplace_back(std::move(src));
        }
        NotifyEdited();
    }

    void RotateSelection(double deg)
    {
        if (!m_fp) return;
        for (auto& p : m_fp->pads) if (p.selected) p.rotation = NormalizeAngle(p.rotation + deg);
        for (auto& s : m_fp->shapes) if (s->selected) s->rotation = NormalizeAngle(s->rotation + deg);
        NotifyEdited();
    }

    void Align(AlignMode mode)
    {
        if (!m_fp) return;
        struct Item { Pad* pad; Shape* shape; wxRect2DDouble b; };
        std::vector<Item> items;
        for (auto& p : m_fp->pads) if (p.selected) items.push_back({ &p, nullptr, p.AABB() });
        for (auto& s : m_fp->shapes) if (s->selected) items.push_back({ nullptr, s.get(), s->WorldAABB() });
        if (items.size() < 2) { if (OnStatus) OnStatus("Align needs at least two selected objects", ""); return; }

        wxRect2DDouble ref = items[0].b;
        for (size_t i = 1; i < items.size(); ++i) ref = UnionRect2D(ref, items[i].b);

        auto moveItem = [](Item& it, double dx, double dy) {
            if (it.pad) { it.pad->x += dx; it.pad->y += dy; }
            else it.shape->Move(dx, dy);
        };

        if (mode == ALIGN_DISTRIBUTE_H || mode == ALIGN_DISTRIBUTE_V) {
            if (items.size() < 3) { if (OnStatus) OnStatus("Distribute needs at least three objects", ""); return; }
            bool horizontal = mode == ALIGN_DISTRIBUTE_H;
            std::sort(items.begin(), items.end(), [horizontal](const Item& a, const Item& b) {
                return horizontal ? (a.b.m_x < b.b.m_x) : (a.b.m_y < b.b.m_y);
            });
            double sum = 0;
            for (auto& it : items) sum += horizontal ? it.b.m_width : it.b.m_height;
            double gap = ((horizontal ? ref.m_width : ref.m_height) - sum) / (items.size() - 1);
            double cursor = horizontal ? ref.m_x : ref.m_y;
            for (auto& it : items) {
                double delta = cursor - (horizontal ? it.b.m_x : it.b.m_y);
                moveItem(it, horizontal ? delta : 0.0, horizontal ? 0.0 : delta);
                cursor += (horizontal ? it.b.m_width : it.b.m_height) + gap;
            }
        }
        else {
            for (auto& it : items) {
                double dx = 0, dy = 0;
                switch (mode) {
                case ALIGN_LEFT: dx = ref.m_x - it.b.m_x; break;
                case ALIGN_RIGHT: dx = (ref.m_x + ref.m_width) - (it.b.m_x + it.b.m_width); break;
                case ALIGN_TOP: dy = ref.m_y - it.b.m_y; break;
                case ALIGN_BOTTOM: dy = (ref.m_y + ref.m_height) - (it.b.m_y + it.b.m_height); break;
                case ALIGN_CENTER_H: dx = (ref.m_x + ref.m_width * 0.5) - (it.b.m_x + it.b.m_width * 0.5); break;
                case ALIGN_CENTER_V: dy = (ref.m_y + ref.m_height * 0.5) - (it.b.m_y + it.b.m_height * 0.5); break;
                default: break;
                }
                moveItem(it, dx, dy);
            }
        }
        NotifyEdited();
    }

    void ZoomFit()
    {
        if (!m_fp) return;
        wxRect2DDouble b = m_fp->Bounds();
        wxSize cs = GetClientSize();
        if (cs.x < 20 || cs.y < 20) return;
        double sx = (cs.x - 100) / std::max(1.0, b.m_width + 6.0);
        double sy = (cs.y - 100) / std::max(1.0, b.m_height + 6.0);
        m_scale = ClampD(std::min(sx, sy), 1.5, 40.0);
        SetVirtualSize((int)std::lround(120 * m_scale), (int)std::lround(120 * m_scale));
        double cx = b.m_x + b.m_width * 0.5;
        double cy = b.m_y + b.m_height * 0.5;
        double scrollX = (cx - WORLD_MIN) * m_scale - cs.x * 0.5;
        double scrollY = (cy - WORLD_MIN) * m_scale - cs.y * 0.5;
        Scroll((int)std::max(0.0, scrollX / 10), (int)std::max(0.0, scrollY / 10));
        Refresh();
    }

    wxString SelectionDescription() const
    {
        int n = SelectionCount();
        if (n == 0) return "Nothing selected";
        if (Pad* p = GetSelectedPad()) return "Pad " + (p->number.empty() ? wxString("(via)") : p->number);
        if (Shape* s = GetSelectedShape()) return s->TypeName();
        return wxString::Format("%d objects selected", n);
    }

    void NotifyEdited()
    {
        if (OnModified) OnModified();
        if (OnSelectionChanged) OnSelectionChanged();
        Refresh();
    }

private:
    Footprint* m_fp = nullptr;
    FpTool m_tool = FP_SELECT;
    double m_scale = 8.0;
    double m_grid = 0.5;
    bool m_snap = true;
    bool m_showLabels = true;

    enum Mode { MODE_IDLE, MODE_MOVE, MODE_RESIZE, MODE_ROTATE, MODE_BOX, MODE_DRAW, MODE_POLY, MODE_ARC, MODE_PAN };
    Mode m_mode = MODE_IDLE;
    Shape* m_activeShape = nullptr;
    int m_resizeHandle = 0;
    wxPoint2DDouble m_dragStart, m_dragLast;
    wxRect2DDouble m_box;
    bool m_boxAdd = false;
    bool m_moved = false;
    double m_rotateStart = 0.0, m_rotateBase = 0.0;
    std::vector<Pad> m_clipPads;
    std::vector<std::unique_ptr<Shape>> m_clipShapes;
    std::vector<wxPoint2DDouble> m_polyPts;
    wxPoint2DDouble m_arcCenter, m_arcStart, m_arcEnd;
    int m_arcStage = 0;
    wxPoint m_panStart;
    int m_panScrollX = 0, m_panScrollY = 0;

    static constexpr double WORLD_MIN = -60.0;
    static constexpr double WORLD_SIZE = 120.0;

    Xform CurrentXform() const
    {
        int sx = 0, sy = 0;
        GetViewStart(&sx, &sy);
        int px = 0, py = 0;
        GetScrollPixelsPerUnit(&px, &py);
        Xform xf;
        xf.scale = m_scale;
        xf.ox = -WORLD_MIN * m_scale - sx * px;
        xf.oy = -WORLD_MIN * m_scale - sy * py;
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

        if (m_fp) {
            DrawFootprint(gc.get(), xf, *m_fp, m_showLabels, wxString(), true);
            for (const auto& p : m_fp->pads) {
                if (p.selected) DrawPadShape(gc.get(), xf, p, true);
            }
            for (const auto& s : m_fp->shapes) {
                if (s->selected) s->DrawSelection(gc.get(), xf);
            }
        }
        DrawPending(gc.get(), xf);
    }

    void DrawGrid(wxDC& dc, const Xform& xf)
    {
        wxSize cs = GetClientSize();
        double left = xf.ToWorld(0, 0).m_x;
        double top = xf.ToWorld(0, 0).m_y;
        double right = xf.ToWorld(cs.x, 0).m_x;
        double bottom = xf.ToWorld(0, cs.y).m_y;
        double step = m_grid;
        double px = step * m_scale;
        if (px < 4.0) {
            double factor = std::ceil(4.0 / std::max(0.001, px));
            step *= factor;
        }
        dc.SetPen(wxPen(wxColour(233, 233, 229), 1));
        for (double x = std::floor(left / step) * step; x <= right; x += step) {
            wxPoint2DDouble a = xf.ToScreen(x, top);
            dc.DrawLine((int)std::lround(a.m_x), 0, (int)std::lround(a.m_x), cs.y);
        }
        for (double y = std::floor(top / step) * step; y <= bottom; y += step) {
            dc.DrawLine(0, (int)std::lround(xf.ToScreen(0, y).m_y), cs.x, (int)std::lround(xf.ToScreen(0, y).m_y));
        }
        wxPoint2DDouble o = xf.ToScreen(0, 0);
        dc.SetPen(wxPen(wxColour(200, 190, 180), 1));
        dc.DrawLine(0, (int)o.m_y, cs.x, (int)o.m_y);
        dc.DrawLine((int)o.m_x, 0, (int)o.m_x, cs.y);
    }

    void DrawPending(wxGraphicsContext* gc, const Xform& xf)
    {
        if (m_mode == MODE_BOX) {
            wxPoint2DDouble a = xf.ToScreen(m_box.m_x, m_box.m_y);
            gc->SetPen(wxPen(wxColour(30, 100, 230), 1, wxPENSTYLE_SHORT_DASH));
            gc->SetBrush(wxBrush(wxColour(30, 100, 230, 35)));
            gc->DrawRectangle(a.m_x, a.m_y, xf.Len(m_box.m_width), xf.Len(m_box.m_height));
        }
        else if (m_mode == MODE_DRAW) {
            gc->SetPen(wxPen(wxColour(30, 100, 230), 1, wxPENSTYLE_SHORT_DASH));
            gc->SetBrush(*wxTRANSPARENT_BRUSH);
            wxPoint2DDouble a = xf.ToScreen(m_dragStart.m_x, m_dragStart.m_y);
            wxPoint2DDouble b = xf.ToScreen(m_dragLast.m_x, m_dragLast.m_y);
            if (m_tool == FP_LINE) gc->StrokeLine(a.m_x, a.m_y, b.m_x, b.m_y);
            else if (m_tool == FP_RECT) gc->DrawRectangle(std::min(a.m_x, b.m_x), std::min(a.m_y, b.m_y), std::abs(b.m_x - a.m_x), std::abs(b.m_y - a.m_y));
            else if (m_tool == FP_CIRCLE) gc->DrawEllipse(std::min(a.m_x, b.m_x), std::min(a.m_y, b.m_y), std::abs(b.m_x - a.m_x), std::abs(b.m_y - a.m_y));
        }
        else if (m_mode == MODE_POLY) {
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
        }
        else if (m_mode == MODE_ARC && m_arcStage >= 1) {
            gc->SetPen(wxPen(wxColour(30, 100, 230), 1, wxPENSTYLE_SHORT_DASH));
            gc->SetBrush(*wxTRANSPARENT_BRUSH);
            wxPoint2DDouble c = xf.ToScreen(m_arcCenter.m_x, m_arcCenter.m_y);
            double r = Dist(m_arcCenter.m_x, m_arcCenter.m_y, m_dragLast.m_x, m_dragLast.m_y);
            gc->DrawEllipse(c.m_x - xf.Len(r), c.m_y - xf.Len(r), xf.Len(r) * 2, xf.Len(r) * 2);
            gc->SetPen(wxPen(wxColour(30, 100, 230), 1));
            wxPoint2DDouble e = xf.ToScreen(m_dragLast.m_x, m_dragLast.m_y);
            gc->StrokeLine(c.m_x, c.m_y, e.m_x, e.m_y);
        }
    }

    void AddPadAt(const wxPoint2DDouble& p, FpTool tool)
    {
        if (!m_fp) return;
        Pad pad;
        pad.uid = NextPadUid();
        pad.x = p.m_x;
        pad.y = p.m_y;
        pad.layer = LAYER_TOP_COPPER;
        if (tool == FP_PAD_SMD) {
            pad.number = m_fp->NextPadNumber();
            pad.type = PAD_SMD;
            pad.shape = PAD_SHAPE_RECT;
            pad.w = 1.5; pad.h = 0.8;
        }
        else if (tool == FP_PAD_THT) {
            pad.number = m_fp->NextPadNumber();
            pad.type = PAD_THT;
            pad.shape = PAD_SHAPE_ROUND;
            pad.w = 1.8; pad.h = 1.8; pad.drill = 0.9;
        }
        else {
            pad.number.clear();
            pad.type = PAD_VIA;
            pad.shape = PAD_SHAPE_ROUND;
            pad.w = 1.2; pad.h = 1.2; pad.drill = 0.6;
        }
        for (auto& q : m_fp->pads) q.selected = false;
        for (auto& s : m_fp->shapes) s->selected = false;
        pad.selected = true;
        m_fp->pads.push_back(pad);
        NotifyEdited();
        if (OnStatus) OnStatus(tool == FP_VIA ? wxString("Via placed") : ("Pad " + pad.number + " placed"), "");
    }

    void ApplyBoxSelection()
    {
        if (!m_fp) return;
        for (auto& p : m_fp->pads) if (m_box.Intersects(p.AABB())) p.selected = true;
        for (auto& s : m_fp->shapes) if (m_box.Intersects(s->WorldAABB())) s->selected = true;
        if (OnSelectionChanged) OnSelectionChanged();
    }

    void FinishPolygon()
    {
        if (m_fp && m_polyPts.size() >= 3) {
            auto poly = std::make_unique<PolygonShape>(m_polyPts);
            poly->layer = LAYER_SILK;
            poly->stroke = LayerColour(LAYER_SILK);
            poly->fill = wxNullColour;
            poly->uid = 100000 + (int)m_fp->shapes.size();
            for (auto& s : m_fp->shapes) s->selected = false;
            poly->selected = true;
            m_fp->shapes.emplace_back(std::move(poly));
            NotifyEdited();
        }
        m_polyPts.clear();
        m_mode = MODE_IDLE;
        if (HasCapture()) ReleaseMouse();
        Refresh();
    }

    void OnLeftDown(wxMouseEvent& e)
    {
        if (!m_fp) return;
        SetFocus();
        Xform xf = CurrentXform();
        wxPoint2DDouble raw = xf.ToWorld(e.GetX(), e.GetY());
        wxPoint2DDouble p = MouseWorld(e, true);
        if (m_mode != MODE_IDLE) return;

        if (m_tool == FP_SELECT) {
            Shape* sel = GetSelectedShape();
            if (sel && sel->selected) {
                int handle = sel->HitTestHandle(xf, e.GetX(), e.GetY());
                if (handle != 0) {
                    m_activeShape = sel;
                    m_resizeHandle = handle;
                    m_dragStart = p;
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

            bool add = e.ShiftDown() || e.ControlDown();
            Pad* hitPad = nullptr;
            for (int i = (int)m_fp->pads.size() - 1; i >= 0; --i) {
                if (m_fp->pads[i].HitTest(raw.m_x, raw.m_y, 0.2)) { hitPad = &m_fp->pads[i]; break; }
            }
            Shape* hitShape = nullptr;
            if (!hitPad) {
                for (int i = (int)m_fp->shapes.size() - 1; i >= 0; --i) {
                    if (m_fp->shapes[i]->HitTest(raw.m_x, raw.m_y, 0.25)) { hitShape = m_fp->shapes[i].get(); break; }
                }
            }

            if (hitPad || hitShape) {
                bool isSelected = hitPad ? hitPad->selected : hitShape->selected;
                if (add) {
                    if (hitPad) hitPad->selected = !hitPad->selected;
                    else hitShape->selected = !hitShape->selected;
                    if (!(hitPad ? hitPad->selected : hitShape->selected)) {
                        if (OnSelectionChanged) OnSelectionChanged();
                        Refresh();
                        return;
                    }
                }
                else if (!isSelected) {
                    for (auto& q : m_fp->pads) q.selected = false;
                    for (auto& s : m_fp->shapes) s->selected = false;
                    if (hitPad) hitPad->selected = true;
                    else hitShape->selected = true;
                }
                m_mode = MODE_MOVE;
                m_dragStart = p;
                m_dragLast = p;
                m_moved = false;
                CaptureMouse();
                if (OnSelectionChanged) OnSelectionChanged();
                Refresh();
                return;
            }

            if (!add) {
                for (auto& q : m_fp->pads) q.selected = false;
                for (auto& s : m_fp->shapes) s->selected = false;
            }
            m_mode = MODE_BOX;
            m_dragStart = raw;
            m_box = wxRect2DDouble(raw.m_x, raw.m_y, 0, 0);
            m_boxAdd = add;
            CaptureMouse();
            if (OnSelectionChanged) OnSelectionChanged();
            Refresh();
            return;
        }

        if (m_tool == FP_PAD_SMD || m_tool == FP_PAD_THT || m_tool == FP_VIA) {
            AddPadAt(p, m_tool);
            return;
        }

        if (m_tool == FP_TEXT) {
            wxString text = wxGetTextFromUser("Text to place:", "Silkscreen Text", "TEXT", this);
            if (!TrimStr(text).empty()) {
                auto t = std::make_unique<TextShape>(p.m_x, p.m_y, text, 1.5);
                t->layer = LAYER_SILK;
                t->stroke = LayerColour(LAYER_SILK);
                t->uid = 100000 + (int)m_fp->shapes.size();
                for (auto& s : m_fp->shapes) s->selected = false;
                t->selected = true;
                m_fp->shapes.emplace_back(std::move(t));
                NotifyEdited();
            }
            return;
        }

        if (m_tool == FP_POLY) {
            m_mode = MODE_POLY;
            m_polyPts.push_back(p);
            m_dragLast = p;
            Refresh();
            return;
        }

        if (m_tool == FP_ARC) {
            m_mode = MODE_ARC;
            if (m_arcStage == 0) { m_arcCenter = p; m_arcStage = 1; }
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
                    arc->layer = LAYER_SILK;
                    arc->stroke = LayerColour(LAYER_SILK);
                    arc->uid = 100000 + (int)m_fp->shapes.size();
                    for (auto& s : m_fp->shapes) s->selected = false;
                    arc->selected = true;
                    m_fp->shapes.emplace_back(std::move(arc));
                    NotifyEdited();
                }
                m_arcStage = 0;
                m_mode = MODE_IDLE;
                Refresh();
            }
            return;
        }

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
            wxPoint2DDouble d(p.m_x - m_dragLast.m_x, p.m_y - m_dragLast.m_y);
            if (d.m_x != 0.0 || d.m_y != 0.0) {
                if (m_fp) {
                    for (auto& pad : m_fp->pads) if (pad.selected) { pad.x += d.m_x; pad.y += d.m_y; }
                    for (auto& s : m_fp->shapes) if (s->selected) s->Move(d.m_x, d.m_y);
                }
                m_dragLast = p;
                m_moved = true;
                if (OnSelectionChanged) OnSelectionChanged();
                Refresh();
            }
            break;
        }
        case MODE_RESIZE:
            if (m_activeShape) {
                m_activeShape->ResizeHandle(m_resizeHandle, p);
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
                double dx = p.m_x - m_dragStart.m_x, dy = p.m_y - m_dragStart.m_y;
                if (m_tool == FP_LINE) {
                    double ang = std::round(std::atan2(dy, dx) / (PI_VAL / 4.0)) * (PI_VAL / 4.0);
                    double len = std::sqrt(dx * dx + dy * dy);
                    m_dragLast = wxPoint2DDouble(m_dragStart.m_x + std::cos(ang) * len, m_dragStart.m_y + std::sin(ang) * len);
                }
                else {
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
            int dx = pos.x - m_panStart.x;
            int dy = pos.y - m_panStart.y;
            int px = 0, py = 0;
            GetScrollPixelsPerUnit(&px, &py);
            Scroll(m_panScrollX - (int)(dx / std::max(1, px)), m_panScrollY - (int)(dy / std::max(1, py)));
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
            if (m_moved || m_mode != MODE_MOVE) NotifyEdited();
            else if (OnSelectionChanged) OnSelectionChanged();
            m_mode = MODE_IDLE;
            m_activeShape = nullptr;
            m_resizeHandle = 0;
            if (HasCapture()) ReleaseMouse();
            Refresh();
            return;
        }
        if (m_mode == MODE_BOX) {
            ApplyBoxSelection();
            m_mode = MODE_IDLE;
            if (HasCapture()) ReleaseMouse();
            Refresh();
            return;
        }
        if (m_mode == MODE_DRAW) {
            wxPoint2DDouble p = MouseWorld(e, true);
            if (e.ShiftDown() && m_tool != FP_LINE) {
                double dx = p.m_x - m_dragStart.m_x, dy = p.m_y - m_dragStart.m_y;
                double s = std::max(std::abs(dx), std::abs(dy));
                p = wxPoint2DDouble(m_dragStart.m_x + (dx < 0 ? -s : s), m_dragStart.m_y + (dy < 0 ? -s : s));
            }
            std::unique_ptr<Shape> shape;
            if (m_tool == FP_LINE && Dist(m_dragStart.m_x, m_dragStart.m_y, p.m_x, p.m_y) > 0.05)
                shape = std::make_unique<LineShape>(m_dragStart.m_x, m_dragStart.m_y, p.m_x, p.m_y);
            else if (m_tool == FP_RECT && (std::abs(p.m_x - m_dragStart.m_x) > 0.05 || std::abs(p.m_y - m_dragStart.m_y) > 0.05))
                shape = std::make_unique<RectShape>(NormalizeRect2D(m_dragStart.m_x, m_dragStart.m_y, p.m_x, p.m_y));
            else if (m_tool == FP_CIRCLE && (std::abs(p.m_x - m_dragStart.m_x) > 0.05 || std::abs(p.m_y - m_dragStart.m_y) > 0.05))
                shape = std::make_unique<CircleShape>(NormalizeRect2D(m_dragStart.m_x, m_dragStart.m_y, p.m_x, p.m_y));

            if (shape && m_fp) {
                shape->layer = LAYER_SILK;
                shape->stroke = LayerColour(LAYER_SILK);
                shape->uid = 100000 + (int)m_fp->shapes.size();
                for (auto& s : m_fp->shapes) s->selected = false;
                shape->selected = true;
                m_fp->shapes.emplace_back(std::move(shape));
                NotifyEdited();
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
                if (m_polyPts.size() > 1 &&
                    Dist(p.m_x, p.m_y, m_polyPts[m_polyPts.size() - 2].m_x, m_polyPts[m_polyPts.size() - 2].m_y) < 0.01)
                    m_polyPts.pop_back();
            }
            FinishPolygon();
        }
        else e.Skip();
    }

    void OnMiddleDown(wxMouseEvent& e)
    {
        if (m_mode != MODE_IDLE) return;
        m_mode = MODE_PAN;
        m_panStart = e.GetPosition();
        int sx = 0, sy = 0;
        GetViewStart(&sx, &sy);
        m_panScrollX = sx;
        m_panScrollY = sy;
        CaptureMouse();
    }

    void OnMiddleUp(wxMouseEvent&)
    {
        if (m_mode == MODE_PAN) {
            m_mode = MODE_IDLE;
            if (HasCapture()) ReleaseMouse();
            Refresh();
        }
    }

    void OnRightDown(wxMouseEvent& e)
    {
        if (m_mode == MODE_POLY) FinishPolygon();
        else if (m_mode == MODE_ARC) CancelPending();
        else e.Skip();
    }

    void OnWheel(wxMouseEvent& e)
    {
        if (e.ControlDown()) {
            Xform before = CurrentXform();
            wxPoint2DDouble world = before.ToWorld(e.GetX(), e.GetY());
            double nscale = ClampD(m_scale * (e.GetWheelRotation() > 0 ? 1.15 : 1.0 / 1.15), 1.0, 40.0);
            if (std::abs(nscale - m_scale) < 1e-9) return;
            m_scale = nscale;
            SetVirtualSize((int)std::lround(WORLD_SIZE * m_scale), (int)std::lround(WORLD_SIZE * m_scale));
            double scrollX = (world.m_x - WORLD_MIN) * m_scale - e.GetX();
            double scrollY = (world.m_y - WORLD_MIN) * m_scale - e.GetY();
            Scroll((int)std::max(0.0, scrollX / 10), (int)std::max(0.0, scrollY / 10));
            Refresh();
        }
        else {
            int sx = 0, sy = 0;
            GetViewStart(&sx, &sy);
            Scroll(sx, sy - e.GetWheelRotation() / 120 * 3);
            Refresh();
        }
    }

    void OnKeyDown(wxKeyEvent& e)
    {
        bool ctrl = e.ControlDown();
        int code = e.GetKeyCode();
        if (code == WXK_DELETE || code == WXK_BACK) {
            if (m_mode == MODE_POLY) { if (!m_polyPts.empty()) m_polyPts.pop_back(); Refresh(); }
            else DeleteSelection();
            return;
        }
        if (code == WXK_ESCAPE) { CancelPending(); ClearSelection(); return; }
        if (code == WXK_RETURN) { if (m_mode == MODE_POLY) FinishPolygon(); return; }
        if (ctrl && code == 'A') { SelectAll(); return; }
        if (ctrl && code == 'C') { CopySelection(); return; }
        if (ctrl && code == 'V') { PasteSelection(); return; }
        if (ctrl && code == 'D') { DuplicateSelection(); return; }
        if (ctrl && code == 'X') { CopySelection(); DeleteSelection(); return; }
        if (code == WXK_LEFT || code == WXK_RIGHT || code == WXK_UP || code == WXK_DOWN) {
            double step = m_snap ? m_grid : 0.1;
            if (e.ShiftDown()) step *= 10.0;
            double dx = 0, dy = 0;
            if (code == WXK_LEFT) dx = -step;
            if (code == WXK_RIGHT) dx = step;
            if (code == WXK_UP) dy = -step;
            if (code == WXK_DOWN) dy = step;
            if (m_fp) {
                for (auto& p : m_fp->pads) if (p.selected) { p.x += dx; p.y += dy; }
                for (auto& s : m_fp->shapes) if (s->selected) s->Move(dx, dy);
            }
            NotifyEdited();
            return;
        }
        e.Skip();
    }
};

// ============================================================================
// Footprint property panel (pad + silkscreen object)
// ============================================================================
class FpPropertyPanel : public wxPanel
{
public:
    wxPropertyGrid* pg = nullptr;
    std::function<void()> OnModified;
    std::function<void(const wxString&)> OnHint;

    explicit FpPropertyPanel(wxWindow* parent) : wxPanel(parent, wxID_ANY)
    {
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        pg = new wxPropertyGrid(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                wxPG_DEFAULT_STYLE | wxPG_SPLITTER_AUTO_CENTER);
        auto sel = new wxStringProperty("Selection", wxPG_LABEL, "Nothing selected");
        pg->Append(sel);
        pg->SetPropertyReadOnly("Selection", true);

        wxArrayString padTypes, padShapes, layers;
        padTypes.Add(PadTypeName(PAD_SMD)); padTypes.Add(PadTypeName(PAD_THT)); padTypes.Add(PadTypeName(PAD_VIA));
        padShapes.Add(PadShapeName(PAD_SHAPE_RECT)); padShapes.Add(PadShapeName(PAD_SHAPE_ROUND)); padShapes.Add(PadShapeName(PAD_SHAPE_OVAL));
        for (int i = 0; i < LAYER_COUNT; ++i) layers.Add(LayerName(i));

        pg->Append(new wxStringProperty("Pin number", wxPG_LABEL, ""));
        pg->Append(new wxEnumProperty("Pad type", wxPG_LABEL, padTypes));
        pg->Append(new wxEnumProperty("Pad shape", wxPG_LABEL, padShapes));
        pg->Append(new wxFloatProperty("Pad X", wxPG_LABEL, 0.0));
        pg->Append(new wxFloatProperty("Pad Y", wxPG_LABEL, 0.0));
        pg->Append(new wxFloatProperty("Pad width", wxPG_LABEL, 1.5));
        pg->Append(new wxFloatProperty("Pad height", wxPG_LABEL, 1.5));
        pg->Append(new wxFloatProperty("Drill", wxPG_LABEL, 0.0));
        pg->Append(new wxFloatProperty("Pad rotation", wxPG_LABEL, 0.0));
        pg->Append(new wxEnumProperty("Pad layer", wxPG_LABEL, layers));

        pg->Append(new wxStringProperty("Object type", wxPG_LABEL, ""));
        pg->SetPropertyReadOnly("Object type", true);
        pg->Append(new wxFloatProperty("X", wxPG_LABEL, 0.0));
        pg->Append(new wxFloatProperty("Y", wxPG_LABEL, 0.0));
        pg->Append(new wxFloatProperty("Width", wxPG_LABEL, 0.0));
        pg->Append(new wxFloatProperty("Height", wxPG_LABEL, 0.0));
        pg->Append(new wxFloatProperty("Rotation", wxPG_LABEL, 0.0));
        pg->Append(new wxEnumProperty("Layer", wxPG_LABEL, layers));
        pg->Append(new wxColourProperty("Stroke colour", wxPG_LABEL, *wxBLACK));
        pg->Append(new wxFloatProperty("Line width", wxPG_LABEL, 0.2));
        pg->Append(new wxBoolProperty("Filled", wxPG_LABEL, false));
        pg->Append(new wxColourProperty("Fill colour", wxPG_LABEL, wxColour(244, 220, 216)));
        pg->Append(new wxStringProperty("Text", wxPG_LABEL, ""));
        pg->Append(new wxFloatProperty("Start angle", wxPG_LABEL, 0.0));
        pg->Append(new wxFloatProperty("Sweep angle", wxPG_LABEL, 90.0));

        for (const char* n : { "Pad X", "Pad Y", "Pad width", "Pad height", "Drill", "Pad rotation" }) {
            if (wxPGProperty* p = pg->GetProperty(n)) p->SetAttribute("Precision", 3);
        }
        for (const char* n : { "X", "Y", "Width", "Height", "Rotation", "Line width" }) {
            if (wxPGProperty* p = pg->GetProperty(n)) p->SetAttribute("Precision", 3);
        }

        sizer->Add(pg, 1, wxEXPAND);
        SetSizer(sizer);
        pg->Bind(wxEVT_PG_CHANGED, &FpPropertyPanel::OnPropertyChanged, this);
        Update(nullptr, nullptr, 0);
    }

    void Update(Pad* pad, Shape* shape, int count)
    {
        m_updating = true;
        m_pad = (count == 1) ? pad : nullptr;
        m_shape = (count == 1 && !pad) ? shape : nullptr;

        auto hide = [this](const wxString& n, bool h) {
            if (wxPGProperty* p = pg->GetProperty(n)) p->Hide(h);
        };
        for (const char* n : { "Pin number", "Pad type", "Pad shape", "Pad X", "Pad Y", "Pad width",
                               "Pad height", "Drill", "Pad rotation", "Pad layer" })
            hide(n, true);
        for (const char* n : { "Object type", "X", "Y", "Width", "Height", "Rotation", "Layer",
                               "Stroke colour", "Line width", "Filled", "Fill colour", "Text",
                               "Start angle", "Sweep angle" })
            hide(n, true);

        if (count == 0) {
            pg->Disable();
            pg->SetPropertyValue("Selection", "Nothing selected");
            m_updating = false;
            return;
        }
        pg->Enable();
        pg->SetPropertyValue("Selection", wxString::Format("%d object(s) selected", count));
        if (count > 1 || (!pad && !shape)) {
            pg->Disable();
            m_updating = false;
            return;
        }

        if (pad) {
            for (const char* n : { "Pin number", "Pad type", "Pad shape", "Pad X", "Pad Y", "Pad width",
                                   "Pad height", "Drill", "Pad rotation", "Pad layer" })
                hide(n, false);
            pg->SetPropertyValue("Selection", pad->type == PAD_VIA ? wxString("Via") : ("Pad " + pad->number));
            pg->SetPropertyValue("Pin number", pad->number);
            pg->SetPropertyValue("Pad type", (long)pad->type);
            pg->SetPropertyValue("Pad shape", (long)pad->shape);
            pg->SetPropertyValue("Pad X", pad->x);
            pg->SetPropertyValue("Pad Y", pad->y);
            pg->SetPropertyValue("Pad width", pad->w);
            pg->SetPropertyValue("Pad height", pad->h);
            pg->SetPropertyValue("Drill", pad->drill);
            pg->SetPropertyValue("Pad rotation", pad->rotation);
            pg->SetPropertyValue("Pad layer", (long)pad->layer);
            hide("Pin number", pad->type == PAD_VIA);
            if (wxPGProperty* p = pg->GetProperty("Drill")) p->Enable(pad->type != PAD_SMD);
        }
        else if (shape) {
            wxString type = shape->TypeName();
            bool canFill = (type == "Rectangle" || type == "Circle" || type == "Polygon");
            bool isText = (type == "Text");
            bool isArc = (type == "Arc");
            for (const char* n : { "Object type", "X", "Y", "Width", "Height", "Rotation", "Layer",
                                   "Stroke colour", "Line width" })
                hide(n, false);
            hide("Filled", !canFill);
            hide("Fill colour", !canFill);
            hide("Text", !isText);
            hide("Start angle", !isArc);
            hide("Sweep angle", !isArc);

            wxRect2DDouble b = shape->LocalBounds();
            pg->SetPropertyValue("Selection", type);
            pg->SetPropertyValue("Object type", type);
            pg->SetPropertyValue("X", b.m_x);
            pg->SetPropertyValue("Y", b.m_y);
            pg->SetPropertyValue("Width", b.m_width);
            pg->SetPropertyValue("Height", b.m_height);
            pg->SetPropertyValue("Rotation", shape->rotation);
            pg->SetPropertyValue("Layer", (long)shape->layer);
            SetColour("Stroke colour", shape->stroke);
            pg->SetPropertyValue("Line width", shape->lineWidth);
            pg->SetPropertyValue("Filled", shape->fill.IsOk());
            SetColour("Fill colour", shape->fill.IsOk() ? shape->fill : wxColour(244, 220, 216));
            if (isText) pg->SetPropertyValue("Text", shape->GetText());
            if (isArc) {
                ArcShape* a = static_cast<ArcShape*>(shape);
                pg->SetPropertyValue("Start angle", a->startAngle);
                pg->SetPropertyValue("Sweep angle", a->sweepAngle);
            }
        }
        m_updating = false;
    }

private:
    Pad* m_pad = nullptr;
    Shape* m_shape = nullptr;
    bool m_updating = false;

    void SetColour(const wxString& name, const wxColour& c)
    {
        wxVariant var;
        var << wxColourPropertyValue(c);
        pg->SetPropertyValue(name, var);
    }

    wxColour GetColour(const wxString& name) const
    {
        wxVariant var = pg->GetPropertyValue(name);
        if (var.GetType() == wxT("wxColourPropertyValue")) {
            wxColourPropertyValue cpv;
            cpv << var;
            return cpv.m_colour;
        }
        return *wxBLACK;
    }

    void OnPropertyChanged(wxPropertyGridEvent& e)
    {
        if (m_updating) return;
        wxString name = e.GetPropertyName();
        if (name == "Selection" || name == "Object type") return;

        if (m_pad) {
            if (name == "Pin number") m_pad->number = pg->GetPropertyValueAsString("Pin number");
            else if (name == "Pad type") { m_pad->type = (PadType)pg->GetPropertyValueAsLong("Pad type"); if (m_pad->type != PAD_SMD && m_pad->drill <= 0.0) m_pad->drill = 0.8; }
            else if (name == "Pad shape") m_pad->shape = (PadShape)pg->GetPropertyValueAsLong("Pad shape");
            else if (name == "Pad X") m_pad->x = pg->GetPropertyValueAsDouble("Pad X");
            else if (name == "Pad Y") m_pad->y = pg->GetPropertyValueAsDouble("Pad Y");
            else if (name == "Pad width") m_pad->w = std::max(0.05, pg->GetPropertyValueAsDouble("Pad width"));
            else if (name == "Pad height") m_pad->h = std::max(0.05, pg->GetPropertyValueAsDouble("Pad height"));
            else if (name == "Drill") m_pad->drill = std::max(0.0, pg->GetPropertyValueAsDouble("Drill"));
            else if (name == "Pad rotation") m_pad->rotation = NormalizeAngle(pg->GetPropertyValueAsDouble("Pad rotation"));
            else if (name == "Pad layer") m_pad->layer = pg->GetPropertyValueAsLong("Pad layer");
            if (OnModified) OnModified();
            if (name == "Pad type") Update(m_pad, nullptr, 1);
            return;
        }
        if (m_shape) {
            wxRect2DDouble b = m_shape->LocalBounds();
            if (name == "X" || name == "Y" || name == "Width" || name == "Height") {
                b.m_x = pg->GetPropertyValueAsDouble("X");
                b.m_y = pg->GetPropertyValueAsDouble("Y");
                b.m_width = std::max(0.1, pg->GetPropertyValueAsDouble("Width"));
                b.m_height = std::max(0.1, pg->GetPropertyValueAsDouble("Height"));
                m_shape->SetLocalBounds(b);
            }
            else if (name == "Rotation") m_shape->rotation = NormalizeAngle(pg->GetPropertyValueAsDouble("Rotation"));
            else if (name == "Layer") m_shape->layer = pg->GetPropertyValueAsLong("Layer");
            else if (name == "Stroke colour") m_shape->stroke = GetColour("Stroke colour");
            else if (name == "Line width") m_shape->lineWidth = std::max(0.01, pg->GetPropertyValueAsDouble("Line width"));
            else if (name == "Text") m_shape->SetText(pg->GetPropertyValueAsString("Text"));
            else if (name == "Filled") {
                bool filled = pg->GetPropertyValueAsBool("Filled");
                m_shape->fill = filled ? GetColour("Fill colour") : wxNullColour;
            }
            else if (name == "Fill colour") {
                m_shape->fill = GetColour("Fill colour");
                m_updating = true;
                pg->SetPropertyValue("Filled", true);
                m_updating = false;
            }
            else if (name == "Start angle" || name == "Sweep angle") {
                if (ArcShape* a = dynamic_cast<ArcShape*>(m_shape)) {
                    a->startAngle = pg->GetPropertyValueAsDouble("Start angle");
                    a->sweepAngle = pg->GetPropertyValueAsDouble("Sweep angle");
                }
            }
            if (OnModified) OnModified();
        }
    }
};


