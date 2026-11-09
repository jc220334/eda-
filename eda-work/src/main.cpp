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
#include <wx/xml/xml.h>
#include <wx/filename.h>
#include <wx/textdlg.h>
#include <wx/numdlg.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/scrolwin.h>
#include <wx/spinctrl.h>
#include <wx/combobox.h>
#include <wx/font.h>
#include <wx/datetime.h>
#include <wx/stdpaths.h>
#include <wx/tokenzr.h>
#include <wx/regex.h>
#include <wx/statbox.h>
#include <wx/radiobut.h>

#include <vector>
#include <memory>
#include <algorithm>
#include <functional>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <map>
#include <set>
#include <string>
#include <utility>

// ============================================================================
// Global constants and small helpers
// ============================================================================
static const double PI_VAL = 3.14159265358979323846;

inline double Deg2Rad(double d) { return d * PI_VAL / 180.0; }
inline double Rad2Deg(double r) { return r * 180.0 / PI_VAL; }
inline double ClampD(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline double SnapTo(double v, double step) { return step <= 0.0 ? v : std::round(v / step) * step; }
inline double NormalizeAngle(double a) { a = std::fmod(a, 360.0); if (a < 0.0) a += 360.0; return a; }
inline double Dist(double x1, double y1, double x2, double y2) { return std::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1)); }

inline wxString FmtNum(double v, int decimals = 3) { return wxString::Format("%.*f", decimals, v); }
inline wxString FmtMm(double v) { return wxString::Format("%.3f mm", v); }

inline wxString TrimStr(const wxString& s) { wxString t = s; t.Trim(true); t.Trim(false); return t; }

inline wxString ColourToHex(const wxColour& c)
{
    if (!c.IsOk()) return "none";
    return wxString::Format("#%02X%02X%02X", c.Red(), c.Green(), c.Blue());
}

inline wxColour HexToColour(const wxString& s, const wxColour& def = wxNullColour)
{
    if (s.IsEmpty() || s == "none") return def;
    wxColour c;
    if (!c.Set(s)) return def;
    return c;
}

inline bool IsDarkColour(const wxColour& c)
{
    return (c.Red() * 299 + c.Green() * 587 + c.Blue() * 114) / 1000 < 145;
}

inline double DistPointSegment(double px, double py, double ax, double ay, double bx, double by)
{
    double dx = bx - ax, dy = by - ay;
    double len2 = dx * dx + dy * dy;
    if (len2 < 1e-12) return Dist(px, py, ax, ay);
    double t = ((px - ax) * dx + (py - ay) * dy) / len2;
    t = ClampD(t, 0.0, 1.0);
    return Dist(px, py, ax + t * dx, ay + t * dy);
}

inline bool PointInPolygon(double x, double y, const std::vector<wxPoint2DDouble>& pts)
{
    if (pts.size() < 3) return false;
    bool inside = false;
    size_t n = pts.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const wxPoint2DDouble& a = pts[i];
        const wxPoint2DDouble& b = pts[j];
        if (((a.m_y > y) != (b.m_y > y)) &&
            (x < (b.m_x - a.m_x) * (y - a.m_y) / (b.m_y - a.m_y + 1e-300) + a.m_x))
            inside = !inside;
    }
    return inside;
}

inline wxRect2DDouble NormalizeRect2D(double x1, double y1, double x2, double y2)
{
    wxRect2DDouble r;
    r.m_x = std::min(x1, x2);
    r.m_y = std::min(y1, y2);
    r.m_width = std::abs(x2 - x1);
    r.m_height = std::abs(y2 - y1);
    return r;
}

inline wxRect2DDouble UnionRect2D(const wxRect2DDouble& a, const wxRect2DDouble& b)
{
    if (a.m_width <= 0 && a.m_height <= 0) return b;
    if (b.m_width <= 0 && b.m_height <= 0) return a;
    double x1 = std::min(a.m_x, b.m_x);
    double y1 = std::min(a.m_y, b.m_y);
    double x2 = std::max(a.m_x + a.m_width, b.m_x + b.m_width);
    double y2 = std::max(a.m_y + a.m_height, b.m_y + b.m_height);
    return wxRect2DDouble(x1, y1, x2 - x1, y2 - y1);
}

inline wxPoint2DDouble RotatePoint(const wxPoint2DDouble& p, double deg)
{
    double a = Deg2Rad(deg), c = std::cos(a), s = std::sin(a);
    return wxPoint2DDouble(p.m_x * c - p.m_y * s, p.m_x * s + p.m_y * c);
}

inline wxPoint2DDouble LocalToWorld(const wxPoint2DDouble& center, double rotDeg, const wxPoint2DDouble& local)
{
    wxPoint2DDouble d(local.m_x - center.m_x, local.m_y - center.m_y);
    wxPoint2DDouble r = RotatePoint(d, rotDeg);
    return wxPoint2DDouble(center.m_x + r.m_x, center.m_y + r.m_y);
}

inline wxPoint2DDouble WorldToLocal(const wxPoint2DDouble& center, double rotDeg, const wxPoint2DDouble& world)
{
    wxPoint2DDouble d(world.m_x - center.m_x, world.m_y - center.m_y);
    wxPoint2DDouble r = RotatePoint(d, -rotDeg);
    return wxPoint2DDouble(center.m_x + r.m_x, center.m_y + r.m_y);
}

// ============================================================================
// Layers
// ============================================================================
enum LayerId {
    LAYER_TOP_COPPER = 0,
    LAYER_BOTTOM_COPPER,
    LAYER_SILK,
    LAYER_OUTLINE,
    LAYER_COUNT
};

inline wxString LayerName(int layer)
{
    switch (layer) {
    case LAYER_TOP_COPPER: return "Top Copper";
    case LAYER_BOTTOM_COPPER: return "Bottom Copper";
    case LAYER_SILK: return "Silkscreen";
    case LAYER_OUTLINE: return "Board Outline";
    }
    return "Unknown";
}

inline wxColour LayerColour(int layer)
{
    switch (layer) {
    case LAYER_TOP_COPPER: return wxColour(196, 48, 43);
    case LAYER_BOTTOM_COPPER: return wxColour(46, 88, 190);
    case LAYER_SILK: return wxColour(150, 110, 10);
    case LAYER_OUTLINE: return wxColour(70, 70, 70);
    }
    return *wxBLACK;
}

inline wxString LayerStorageName(int layer)
{
    switch (layer) {
    case LAYER_TOP_COPPER: return "top";
    case LAYER_BOTTOM_COPPER: return "bottom";
    case LAYER_SILK: return "silk";
    case LAYER_OUTLINE: return "outline";
    }
    return "top";
}

inline int LayerFromStorageName(const wxString& s)
{
    if (s == "bottom") return LAYER_BOTTOM_COPPER;
    if (s == "silk") return LAYER_SILK;
    if (s == "outline") return LAYER_OUTLINE;
    return LAYER_TOP_COPPER;
}

// ============================================================================
// 2D view transform: millimetres <-> screen pixels
// ============================================================================
struct Xform
{
    double scale = 4.0;   // pixels per millimetre
    double ox = 0.0;      // screen x of world 0,0
    double oy = 0.0;      // screen y of world 0,0

    wxPoint2DDouble ToScreen(double x, double y) const { return wxPoint2DDouble(x * scale + ox, y * scale + oy); }
    wxPoint2DDouble ToWorld(double sx, double sy) const { return wxPoint2DDouble((sx - ox) / scale, (sy - oy) / scale); }
    double Len(double mm) const { return mm * scale; }
};

// Lightweight description of a canvas object, used by the object list.
struct ObjectInfo
{
    int uid = 0;
    wxString name;
    wxString type;
    wxString layer;
};

// ============================================================================
// Shape hierarchy (Task B drawing engine)
// ============================================================================
class Shape
{
public:
    int uid = 0;
    int layer = LAYER_TOP_COPPER;
    wxColour stroke = LayerColour(LAYER_TOP_COPPER);
    wxColour fill = wxNullColour;      // invalid means "no fill"
    double lineWidth = 0.2;            // millimetres
    double rotation = 0.0;             // degrees, around the local-bounds centre
    bool selected = false;

    virtual ~Shape() = default;

    virtual Shape* Clone() const = 0;
    virtual wxString TypeName() const = 0;
    virtual wxRect2DDouble LocalBounds() const = 0;
    virtual void SetLocalBounds(const wxRect2DDouble& r) = 0;
    virtual void Draw(wxGraphicsContext* gc, const Xform& xf) const = 0;
    virtual bool HitTest(double x, double y, double tol) const = 0;
    virtual void Move(double dx, double dy) = 0;
    virtual bool CanResize() const { return true; }
    virtual wxString GetText() const { return wxString(); }
    virtual void SetText(const wxString&) {}
    virtual wxString Describe() const { return TypeName(); }

    wxPoint2DDouble Center() const
    {
        wxRect2DDouble b = LocalBounds();
        return wxPoint2DDouble(b.m_x + b.m_width * 0.5, b.m_y + b.m_height * 0.5);
    }

    wxPoint2DDouble ToWorld(const wxPoint2DDouble& local) const
    {
        return LocalToWorld(Center(), rotation, local);
    }

    wxPoint2DDouble ToLocal(const wxPoint2DDouble& world) const
    {
        return WorldToLocal(Center(), rotation, world);
    }

    // Axis-aligned bounds after rotation.
    wxRect2DDouble WorldAABB() const
    {
        wxRect2DDouble b = LocalBounds();
        wxPoint2DDouble c = Center();
        wxPoint2DDouble pts[4] = {
            ToWorld(wxPoint2DDouble(b.m_x, b.m_y)),
            ToWorld(wxPoint2DDouble(b.m_x + b.m_width, b.m_y)),
            ToWorld(wxPoint2DDouble(b.m_x + b.m_width, b.m_y + b.m_height)),
            ToWorld(wxPoint2DDouble(b.m_x, b.m_y + b.m_height))
        };
        double minx = pts[0].m_x, maxx = pts[0].m_x, miny = pts[0].m_y, maxy = pts[0].m_y;
        for (int i = 1; i < 4; ++i) {
            minx = std::min(minx, pts[i].m_x); maxx = std::max(maxx, pts[i].m_x);
            miny = std::min(miny, pts[i].m_y); maxy = std::max(maxy, pts[i].m_y);
        }
        return wxRect2DDouble(minx, miny, maxx - minx, maxy - miny);
    }

    // Handles 1..8 are resize handles, 9 is the rotation handle.
    wxPoint2DDouble HandleWorld(int handle) const
    {
        wxRect2DDouble b = LocalBounds();
        wxPoint2DDouble local;
        switch (handle) {
        case 1: local = wxPoint2DDouble(b.m_x, b.m_y); break;
        case 2: local = wxPoint2DDouble(b.m_x + b.m_width * 0.5, b.m_y); break;
        case 3: local = wxPoint2DDouble(b.m_x + b.m_width, b.m_y); break;
        case 4: local = wxPoint2DDouble(b.m_x + b.m_width, b.m_y + b.m_height * 0.5); break;
        case 5: local = wxPoint2DDouble(b.m_x + b.m_width, b.m_y + b.m_height); break;
        case 6: local = wxPoint2DDouble(b.m_x + b.m_width * 0.5, b.m_y + b.m_height); break;
        case 7: local = wxPoint2DDouble(b.m_x, b.m_y + b.m_height); break;
        case 8: local = wxPoint2DDouble(b.m_x, b.m_y + b.m_height * 0.5); break;
        case 9: local = wxPoint2DDouble(b.m_x + b.m_width * 0.5, b.m_y - 5.0); break;
        default: return ToWorld(wxPoint2DDouble(b.m_x, b.m_y));
        }
        return ToWorld(local);
    }

    int HitTestHandle(const Xform& xf, double sx, double sy) const
    {
        if (!selected) return 0;
        int last = CanResize() ? 9 : 9;
        for (int i = 1; i <= last; ++i) {
            if (i >= 1 && i <= 8 && !CanResize()) continue;
            wxPoint2DDouble p = HandleWorld(i);
            wxPoint2DDouble s = xf.ToScreen(p.m_x, p.m_y);
            if (std::abs(s.m_x - sx) <= 7.0 && std::abs(s.m_y - sy) <= 7.0) return i;
        }
        return 0;
    }

    void ResizeHandle(int handle, const wxPoint2DDouble& world)
    {
        if (!CanResize() || handle < 1 || handle > 8) return;
        wxPoint2DDouble center = Center();
        wxPoint2DDouble local = WorldToLocal(center, rotation, world);
        wxRect2DDouble b = LocalBounds();
        double right = b.m_x + b.m_width;
        double bottom = b.m_y + b.m_height;
        switch (handle) {
        case 1: b.m_x = local.m_x; b.m_y = local.m_y; b.m_width = right - b.m_x; b.m_height = bottom - b.m_y; break;
        case 2: b.m_y = local.m_y; b.m_height = bottom - b.m_y; break;
        case 3: b.m_y = local.m_y; b.m_width = local.m_x - b.m_x; b.m_height = bottom - b.m_y; break;
        case 4: b.m_width = local.m_x - b.m_x; break;
        case 5: b.m_width = local.m_x - b.m_x; b.m_height = local.m_y - b.m_y; break;
        case 6: b.m_height = local.m_y - b.m_y; break;
        case 7: b.m_x = local.m_x; b.m_width = right - b.m_x; b.m_height = local.m_y - b.m_y; break;
        case 8: b.m_x = local.m_x; b.m_width = right - b.m_x; break;
        }
        if (b.m_width < 0.1) b.m_width = 0.1;
        if (b.m_height < 0.1) b.m_height = 0.1;
        if (handle == 1 || handle == 7 || handle == 8) {
            // keep the right edge anchored for left-side handles
            if (b.m_x > right - 0.1) b.m_x = right - 0.1;
        }
        SetLocalBounds(b);
    }

    void ApplyRotation(wxGraphicsContext* gc, const Xform& xf) const
    {
        if (std::abs(rotation) < 1e-9) return;
        wxPoint2DDouble c = xf.ToScreen(Center().m_x, Center().m_y);
        gc->Translate(c.m_x, c.m_y);
        gc->Rotate(Deg2Rad(rotation));
        gc->Translate(-c.m_x, -c.m_y);
    }

    void SetStrokeAndBrush(wxGraphicsContext* gc, const Xform& xf, bool allowFill = true) const
    {
        int w = std::max(1, (int)std::lround(lineWidth * xf.scale));
        gc->SetPen(wxPen(stroke.IsOk() ? stroke : *wxBLACK, w));
        if (allowFill && fill.IsOk()) gc->SetBrush(wxBrush(fill));
        else gc->SetBrush(*wxTRANSPARENT_BRUSH);
    }

    void DrawSelection(wxGraphicsContext* gc, const Xform& xf) const
    {
        wxRect2DDouble b = LocalBounds();
        wxPoint2DDouble corners[4] = {
            ToWorld(wxPoint2DDouble(b.m_x, b.m_y)),
            ToWorld(wxPoint2DDouble(b.m_x + b.m_width, b.m_y)),
            ToWorld(wxPoint2DDouble(b.m_x + b.m_width, b.m_y + b.m_height)),
            ToWorld(wxPoint2DDouble(b.m_x, b.m_y + b.m_height))
        };
        wxGraphicsPath path = gc->CreatePath();
        wxPoint2DDouble s0 = xf.ToScreen(corners[0].m_x, corners[0].m_y);
        path.MoveToPoint(s0.m_x, s0.m_y);
        for (int i = 1; i < 4; ++i) {
            wxPoint2DDouble s = xf.ToScreen(corners[i].m_x, corners[i].m_y);
            path.AddLineToPoint(s.m_x, s.m_y);
        }
        path.CloseSubpath();
        gc->SetPen(wxPen(wxColour(35, 105, 220), 1, wxPENSTYLE_SHORT_DASH));
        gc->SetBrush(*wxTRANSPARENT_BRUSH);
        gc->StrokePath(path);

        // Rotation handle
        wxPoint2DDouble topMid = ToWorld(wxPoint2DDouble(b.m_x + b.m_width * 0.5, b.m_y));
        wxPoint2DDouble rot = HandleWorld(9);
        wxPoint2DDouble ts = xf.ToScreen(topMid.m_x, topMid.m_y);
        wxPoint2DDouble rs = xf.ToScreen(rot.m_x, rot.m_y);
        gc->SetPen(wxPen(wxColour(35, 105, 220), 1));
        gc->StrokeLine(ts.m_x, ts.m_y, rs.m_x, rs.m_y);
        gc->SetBrush(*wxWHITE_BRUSH);
        gc->DrawEllipse(rs.m_x - 4.5, rs.m_y - 4.5, 9.0, 9.0);

        if (CanResize()) {
            for (int i = 1; i <= 8; ++i) {
                wxPoint2DDouble p = HandleWorld(i);
                wxPoint2DDouble s = xf.ToScreen(p.m_x, p.m_y);
                gc->SetPen(wxPen(wxColour(35, 105, 220), 1));
                gc->SetBrush(*wxWHITE_BRUSH);
                gc->DrawRectangle(s.m_x - 3.5, s.m_y - 3.5, 7.0, 7.0);
            }
        }
    }
};

// ---------------------------------------------------------------------------
class LineShape : public Shape
{
public:
    wxPoint2DDouble p1, p2;

    LineShape(double x1, double y1, double x2, double y2) : p1(x1, y1), p2(x2, y2)
    {
        stroke = LayerColour(LAYER_TOP_COPPER);
    }

    wxString TypeName() const override { return "Line"; }
    Shape* Clone() const override { return new LineShape(*this); }

    wxRect2DDouble LocalBounds() const override
    {
        return NormalizeRect2D(p1.m_x, p1.m_y, p2.m_x, p2.m_y);
    }

    void SetLocalBounds(const wxRect2DDouble& r) override
    {
        wxRect2DDouble b = LocalBounds();
        auto mapX = [&](double x) {
            if (b.m_width < 1e-9) return r.m_x;
            return r.m_x + (x - b.m_x) / b.m_width * r.m_width;
        };
        auto mapY = [&](double y) {
            if (b.m_height < 1e-9) return r.m_y;
            return r.m_y + (y - b.m_y) / b.m_height * r.m_height;
        };
        p1 = wxPoint2DDouble(mapX(p1.m_x), mapY(p1.m_y));
        p2 = wxPoint2DDouble(mapX(p2.m_x), mapY(p2.m_y));
    }

    void Move(double dx, double dy) override { p1.m_x += dx; p1.m_y += dy; p2.m_x += dx; p2.m_y += dy; }

    bool HitTest(double x, double y, double tol) const override
    {
        return DistPointSegment(x, y, p1.m_x, p1.m_y, p2.m_x, p2.m_y) <= tol + lineWidth * 0.5;
    }

    void Draw(wxGraphicsContext* gc, const Xform& xf) const override
    {
        gc->PushState();
        ApplyRotation(gc, xf);
        SetStrokeAndBrush(gc, xf, false);
        wxPoint2DDouble a = xf.ToScreen(p1.m_x, p1.m_y);
        wxPoint2DDouble b = xf.ToScreen(p2.m_x, p2.m_y);
        gc->StrokeLine(a.m_x, a.m_y, b.m_x, b.m_y);
        gc->PopState();
    }
};

// ---------------------------------------------------------------------------
class RectShape : public Shape
{
public:
    wxRect2DDouble r;

    explicit RectShape(const wxRect2DDouble& rect) : r(rect) { stroke = LayerColour(LAYER_TOP_COPPER); }

    wxString TypeName() const override { return "Rectangle"; }
    Shape* Clone() const override { return new RectShape(*this); }
    wxRect2DDouble LocalBounds() const override { return r; }
    void SetLocalBounds(const wxRect2DDouble& b) override { r = b; }
    void Move(double dx, double dy) override { r.m_x += dx; r.m_y += dy; }

    bool HitTest(double x, double y, double tol) const override
    {
        wxRect2DDouble rr = r;
        rr.m_x -= tol; rr.m_y -= tol; rr.m_width += tol * 2; rr.m_height += tol * 2;
        return rr.Contains(wxPoint2DDouble(x, y));
    }

    void Draw(wxGraphicsContext* gc, const Xform& xf) const override
    {
        gc->PushState();
        ApplyRotation(gc, xf);
        SetStrokeAndBrush(gc, xf);
        wxPoint2DDouble a = xf.ToScreen(r.m_x, r.m_y);
        gc->DrawRectangle(a.m_x, a.m_y, xf.Len(r.m_width), xf.Len(r.m_height));
        gc->PopState();
    }
};

// ---------------------------------------------------------------------------
class CircleShape : public Shape
{
public:
    wxRect2DDouble r;

    explicit CircleShape(const wxRect2DDouble& rect) : r(rect) { stroke = LayerColour(LAYER_TOP_COPPER); }

    wxString TypeName() const override { return "Circle"; }
    Shape* Clone() const override { return new CircleShape(*this); }
    wxRect2DDouble LocalBounds() const override { return r; }
    void SetLocalBounds(const wxRect2DDouble& b) override { r = b; }
    void Move(double dx, double dy) override { r.m_x += dx; r.m_y += dy; }

    bool HitTest(double x, double y, double tol) const override
    {
        double rx = r.m_width * 0.5, ry = r.m_height * 0.5;
        if (rx < 1e-9 || ry < 1e-9) return false;
        double cx = r.m_x + rx, cy = r.m_y + ry;
        double nx = (x - cx) / (rx + tol), ny = (y - cy) / (ry + tol);
        return nx * nx + ny * ny <= 1.0;
    }

    void Draw(wxGraphicsContext* gc, const Xform& xf) const override
    {
        gc->PushState();
        ApplyRotation(gc, xf);
        SetStrokeAndBrush(gc, xf);
        wxPoint2DDouble a = xf.ToScreen(r.m_x, r.m_y);
        gc->DrawEllipse(a.m_x, a.m_y, xf.Len(r.m_width), xf.Len(r.m_height));
        gc->PopState();
    }
};

// ---------------------------------------------------------------------------
class ArcShape : public Shape
{
public:
    wxPoint2DDouble center;
    double radius = 1.0;
    double startAngle = 0.0;   // degrees, 0 = +X
    double sweepAngle = 90.0;   // degrees, positive = clockwise on screen (Y down)

    ArcShape(double cx, double cy, double r, double start, double sweep)
        : center(cx, cy), radius(std::max(0.1, r)), startAngle(start), sweepAngle(sweep)
    {
        stroke = LayerColour(LAYER_SILK);
    }

    wxString TypeName() const override { return "Arc"; }
    Shape* Clone() const override { return new ArcShape(*this); }

    wxRect2DDouble LocalBounds() const override
    {
        return wxRect2DDouble(center.m_x - radius, center.m_y - radius, radius * 2, radius * 2);
    }

    void SetLocalBounds(const wxRect2DDouble& b) override
    {
        center = wxPoint2DDouble(b.m_x + b.m_width * 0.5, b.m_y + b.m_height * 0.5);
        radius = std::max(0.1, std::min(b.m_width, b.m_height) * 0.5);
    }

    void Move(double dx, double dy) override { center.m_x += dx; center.m_y += dy; }

    bool HitTest(double x, double y, double tol) const override
    {
        double d = Dist(x, y, center.m_x, center.m_y);
        if (std::abs(d - radius) > tol + lineWidth * 0.5) return false;
        double ang = Rad2Deg(std::atan2(y - center.m_y, x - center.m_x));
        double delta = NormalizeAngle(ang - startAngle);
        bool pos = sweepAngle >= 0;
        double sweep = std::abs(sweepAngle);
        if (sweep >= 359.9) return true;
        if (pos) return delta <= sweep + 1e-6;
        return NormalizeAngle(startAngle - ang) <= sweep + 1e-6;
    }

    void Draw(wxGraphicsContext* gc, const Xform& xf) const override
    {
        gc->PushState();
        ApplyRotation(gc, xf);
        SetStrokeAndBrush(gc, xf, false);
        const int segments = std::max(8, (int)(std::abs(sweepAngle) / 4.0));
        wxGraphicsPath path = gc->CreatePath();
        for (int i = 0; i <= segments; ++i) {
            double a = Deg2Rad(startAngle + sweepAngle * (double)i / segments);
            double px = center.m_x + radius * std::cos(a);
            double py = center.m_y + radius * std::sin(a);
            wxPoint2DDouble s = xf.ToScreen(px, py);
            if (i == 0) path.MoveToPoint(s.m_x, s.m_y);
            else path.AddLineToPoint(s.m_x, s.m_y);
        }
        gc->StrokePath(path);
        gc->PopState();
    }
};

// ---------------------------------------------------------------------------
class PolygonShape : public Shape
{
public:
    std::vector<wxPoint2DDouble> pts;

    explicit PolygonShape(const std::vector<wxPoint2DDouble>& points) : pts(points)
    {
        stroke = LayerColour(LAYER_TOP_COPPER);
        fill = wxColour(244, 220, 216);
    }

    wxString TypeName() const override { return "Polygon"; }
    Shape* Clone() const override { return new PolygonShape(*this); }

    wxRect2DDouble LocalBounds() const override
    {
        if (pts.empty()) return wxRect2DDouble(0, 0, 0, 0);
        double minx = pts[0].m_x, maxx = minx, miny = pts[0].m_y, maxy = miny;
        for (auto& p : pts) {
            minx = std::min(minx, p.m_x); maxx = std::max(maxx, p.m_x);
            miny = std::min(miny, p.m_y); maxy = std::max(maxy, p.m_y);
        }
        return wxRect2DDouble(minx, miny, maxx - minx, maxy - miny);
    }

    void SetLocalBounds(const wxRect2DDouble& b) override
    {
        wxRect2DDouble old = LocalBounds();
        for (auto& p : pts) {
            p.m_x = old.m_width < 1e-9 ? b.m_x : b.m_x + (p.m_x - old.m_x) / old.m_width * b.m_width;
            p.m_y = old.m_height < 1e-9 ? b.m_y : b.m_y + (p.m_y - old.m_y) / old.m_height * b.m_height;
        }
    }

    void Move(double dx, double dy) override { for (auto& p : pts) { p.m_x += dx; p.m_y += dy; } }

    bool HitTest(double x, double y, double tol) const override
    {
        if (PointInPolygon(x, y, pts)) return true;
        for (size_t i = 0; i < pts.size(); ++i) {
            const auto& a = pts[i];
            const auto& b = pts[(i + 1) % pts.size()];
            if (DistPointSegment(x, y, a.m_x, a.m_y, b.m_x, b.m_y) <= tol + lineWidth * 0.5) return true;
        }
        return false;
    }

    void Draw(wxGraphicsContext* gc, const Xform& xf) const override
    {
        if (pts.size() < 2) return;
        gc->PushState();
        ApplyRotation(gc, xf);
        SetStrokeAndBrush(gc, xf);
        wxGraphicsPath path = gc->CreatePath();
        wxPoint2DDouble s = xf.ToScreen(pts[0].m_x, pts[0].m_y);
        path.MoveToPoint(s.m_x, s.m_y);
        for (size_t i = 1; i < pts.size(); ++i) {
            s = xf.ToScreen(pts[i].m_x, pts[i].m_y);
            path.AddLineToPoint(s.m_x, s.m_y);
        }
        path.CloseSubpath();
        gc->DrawPath(path);
        gc->PopState();
    }
};

// ---------------------------------------------------------------------------
class TextShape : public Shape
{
public:
    wxString text = "TEXT";
    double fontSize = 2.0;       // millimetres (cap height approximation)
    wxPoint2DDouble pos;         // top-left in world coordinates

    TextShape(double x, double y, const wxString& t, double size)
        : text(t), fontSize(std::max(0.5, size)), pos(x, y)
    {
        stroke = LayerColour(LAYER_SILK);
    }

    wxString TypeName() const override { return "Text"; }
    Shape* Clone() const override { return new TextShape(*this); }
    wxString GetText() const override { return text; }
    void SetText(const wxString& t) override { text = t; }

    wxRect2DDouble LocalBounds() const override
    {
        double w = 0.0;
        for (wxUniChar ch : text) {
            w += (ch.GetValue() < 128) ? 0.62 : 1.05;
        }
        if (text.empty()) w = 0.5;
        return wxRect2DDouble(pos.m_x, pos.m_y, w * fontSize, fontSize * 1.35);
    }

    void SetLocalBounds(const wxRect2DDouble& b) override
    {
        pos = wxPoint2DDouble(b.m_x, b.m_y);
        fontSize = std::max(0.4, b.m_height / 1.35);
    }

    void Move(double dx, double dy) override { pos.m_x += dx; pos.m_y += dy; }

    bool HitTest(double x, double y, double tol) const override
    {
        wxRect2DDouble b = LocalBounds();
        b.m_x -= tol; b.m_y -= tol; b.m_width += tol * 2; b.m_height += tol * 2;
        return b.Contains(wxPoint2DDouble(x, y));
    }

    void Draw(wxGraphicsContext* gc, const Xform& xf) const override
    {
        gc->PushState();
        ApplyRotation(gc, xf);
        int px = std::max(6, (int)std::lround(fontSize * xf.scale));
        wxFont font(wxFontInfo(wxSize(px, px)).Family(wxFONTFAMILY_SWISS).FaceName("Segoe UI"));
        gc->SetFont(font, stroke.IsOk() ? stroke : *wxBLACK);
        wxPoint2DDouble s = xf.ToScreen(pos.m_x, pos.m_y);
        gc->DrawText(text, s.m_x, s.m_y);
        gc->PopState();
    }
};


// ============================================================================
// Footprint model (Tasks C and D)
// ============================================================================
enum PadType { PAD_SMD = 0, PAD_THT, PAD_VIA };
enum PadShape { PAD_SHAPE_RECT = 0, PAD_SHAPE_ROUND, PAD_SHAPE_OVAL };

inline wxString PadTypeName(PadType t)
{
    switch (t) {
    case PAD_SMD: return "SMD";
    case PAD_THT: return "Through-hole";
    case PAD_VIA: return "Via";
    }
    return "SMD";
}

inline wxString PadShapeName(PadShape s)
{
    switch (s) {
    case PAD_SHAPE_RECT: return "Rectangular";
    case PAD_SHAPE_ROUND: return "Round";
    case PAD_SHAPE_OVAL: return "Oval";
    }
    return "Rectangular";
}

inline wxString PadShapeStorage(PadShape s)
{
    switch (s) {
    case PAD_SHAPE_RECT: return "rect";
    case PAD_SHAPE_ROUND: return "round";
    case PAD_SHAPE_OVAL: return "oval";
    }
    return "rect";
}

inline PadShape PadShapeFromStorage(const wxString& s)
{
    if (s == "round") return PAD_SHAPE_ROUND;
    if (s == "oval") return PAD_SHAPE_OVAL;
    return PAD_SHAPE_RECT;
}

struct Pad
{
    int uid = 0;
    wxString number;
    double x = 0.0, y = 0.0;       // centre, footprint millimetres
    double w = 1.0, h = 1.0;       // pad size
    double rotation = 0.0;
    double drill = 0.0;
    PadType type = PAD_SMD;
    PadShape shape = PAD_SHAPE_RECT;
    int layer = LAYER_TOP_COPPER;
    wxString net;
    bool selected = false;

    wxRect2DDouble LocalBounds() const { return wxRect2DDouble(x - w * 0.5, y - h * 0.5, w, h); }

    wxRect2DDouble AABB() const
    {
        double hw = w * 0.5, hh = h * 0.5;
        wxPoint2DDouble pts[4] = {
            RotatePoint(wxPoint2DDouble(-hw, -hh), rotation),
            RotatePoint(wxPoint2DDouble(hw, -hh), rotation),
            RotatePoint(wxPoint2DDouble(hw, hh), rotation),
            RotatePoint(wxPoint2DDouble(-hw, hh), rotation)
        };
        double minx = pts[0].m_x, maxx = minx, miny = pts[0].m_y, maxy = miny;
        for (int i = 1; i < 4; ++i) {
            minx = std::min(minx, pts[i].m_x); maxx = std::max(maxx, pts[i].m_x);
            miny = std::min(miny, pts[i].m_y); maxy = std::max(maxy, pts[i].m_y);
        }
        return wxRect2DDouble(x + minx, y + miny, maxx - minx, maxy - miny);
    }

    bool HitTest(double px, double py, double tol) const
    {
        wxPoint2DDouble d = RotatePoint(wxPoint2DDouble(px - x, py - y), -rotation);
        double hw = w * 0.5 + tol, hh = h * 0.5 + tol;
        if (shape == PAD_SHAPE_ROUND) {
            double nx = d.m_x / hw, ny = d.m_y / hh;
            return nx * nx + ny * ny <= 1.0;
        }
        return std::abs(d.m_x) <= hw && std::abs(d.m_y) <= hh;
    }
};

inline int NextPadUid()
{
    static int counter = 1;
    return counter++;
}

// ---------------------------------------------------------------------------
class Footprint
{
public:
    wxString name;
    wxString uid;
    wxString category;
    wxString description;
    wxString unit = "mm";
    wxString keywords;
    std::vector<Pad> pads;
    std::vector<std::unique_ptr<Shape>> shapes;

    Footprint() = default;

    Footprint(const Footprint& other) { CopyFrom(other); }

    Footprint& operator=(const Footprint& other)
    {
        if (this != &other) CopyFrom(other);
        return *this;
    }

    void CopyFrom(const Footprint& other)
    {
        name = other.name; uid = other.uid; category = other.category;
        description = other.description; unit = other.unit; keywords = other.keywords;
        pads = other.pads;
        shapes.clear();
        for (const auto& s : other.shapes) shapes.emplace_back(s->Clone());
    }

    std::unique_ptr<Footprint> Clone() const { return std::make_unique<Footprint>(*this); }

    Pad* AddPad(const Pad& p)
    {
        pads.push_back(p);
        if (pads.back().uid == 0) pads.back().uid = NextPadUid();
        return &pads.back();
    }

    Pad* FindPadByUid(int id)
    {
        for (auto& p : pads) if (p.uid == id) return &p;
        return nullptr;
    }

    wxString NextPadNumber() const
    {
        int maxNum = 0;
        for (const auto& p : pads) {
            long v = 0;
            wxString n = TrimStr(p.number);
            if (!n.empty() && n.ToLong(&v) && v > maxNum) maxNum = (int)v;
        }
        return wxString::Format("%d", maxNum + 1);
    }

    bool HasPadNumber(const wxString& number, int exceptUid = -1) const
    {
        for (const auto& p : pads) {
            if (p.uid == exceptUid) continue;
            if (TrimStr(p.number) == TrimStr(number)) return true;
        }
        return false;
    }

    wxRect2DDouble Bounds() const
    {
        bool first = true;
        wxRect2DDouble b(0, 0, 0, 0);
        for (const auto& p : pads) {
            wxRect2DDouble r = p.AABB();
            b = first ? r : UnionRect2D(b, r);
            first = false;
        }
        for (const auto& s : shapes) {
            wxRect2DDouble r = s->WorldAABB();
            b = first ? r : UnionRect2D(b, r);
            first = false;
        }
        if (first) return wxRect2DDouble(-1, -1, 2, 2);
        return b;
    }

    wxString SizeText() const
    {
        wxRect2DDouble b = Bounds();
        return wxString::Format("%.2f x %.2f mm", b.m_width, b.m_height);
    }

    wxString Summary() const
    {
        wxString cat = category.empty() ? wxString("Uncategorised") : category;
        return wxString::Format("%s  |  %d pads  |  %s", cat, (int)pads.size(), SizeText());
    }

    // Full validation used before saving (Task C).
    bool Validate(wxArrayString& errors) const
    {
        errors.Clear();
        if (TrimStr(name).empty())
            errors.Add("Footprint name must not be empty.");
        if (unit != "mm" && unit != "mil")
            errors.Add("Default unit must be either mm or mil.");
        if (pads.empty())
            errors.Add("Footprint must contain at least one pad.");
        if (pads.size() >= 1000)
            errors.Add("Too many pads (limit is 999).");

        std::map<wxString, int> numbers;
        for (size_t i = 0; i < pads.size(); ++i) {
            const Pad& p = pads[i];
            wxString tag = wxString::Format("Pad %d", (int)i + 1);
            if (!p.number.empty()) tag += " (" + p.number + ")";
            else if (p.type == PAD_VIA) tag += " (via)";

            if (p.w < 0.05 || p.h < 0.05 || p.w > 500.0 || p.h > 500.0) {
                errors.Add(tag + ": invalid pad size (must be between 0.05 and 500 mm).");
            }
            if (p.drill < 0.0) {
                errors.Add(tag + ": drill diameter must not be negative.");
            }
            if (p.type == PAD_THT || p.type == PAD_VIA) {
                if (p.drill <= 0.0) errors.Add(tag + ": through-hole pad / via needs a drill diameter.");
                else if (p.drill >= std::min(p.w, p.h)) errors.Add(tag + ": drill is larger than the pad.");
            }
            if (p.type != PAD_VIA && TrimStr(p.number).empty()) {
                errors.Add(tag + ": pin number is required.");
            }
            if (p.type != PAD_VIA && !p.number.empty()) {
                wxString n = TrimStr(p.number);
                numbers[n]++;
            }
        }
        for (const auto& kv : numbers) {
            if (kv.second > 1)
                errors.Add(wxString::Format("Duplicate pin number '%s' (%d pads).", kv.first, kv.second));
        }
        for (size_t i = 0; i < shapes.size(); ++i) {
            const Shape* s = shapes[i].get();
            wxRect2DDouble b = s->LocalBounds();
            if (b.m_width <= 0.0 && b.m_height <= 0.0)
                errors.Add(wxString::Format("%s object %d has no usable size.", s->TypeName(), (int)i + 1));
            if (s->TypeName() == "Text" && TrimStr(s->GetText()).empty())
                errors.Add(wxString::Format("Text object %d is empty.", (int)i + 1));
        }
        return errors.empty();
    }
};

// ---------------------------------------------------------------------------
// Drawing helpers shared by editor, board and preview
// ---------------------------------------------------------------------------
inline void DrawPadShape(wxGraphicsContext* gc, const Xform& xf, const Pad& pad, bool selectedHighlight)
{
    wxColour copper = LayerColour(pad.layer);
    wxPoint2DDouble c = xf.ToScreen(pad.x, pad.y);
    gc->PushState();
    gc->Translate(c.m_x, c.m_y);
    gc->Rotate(Deg2Rad(pad.rotation));
    gc->Translate(-c.m_x, -c.m_y);

    double w = std::max(1.0, xf.Len(pad.w));
    double h = std::max(1.0, xf.Len(pad.h));
    gc->SetPen(wxPen(wxColour(95, 55, 45), 1));
    gc->SetBrush(wxBrush(copper));

    if (pad.shape == PAD_SHAPE_ROUND) {
        gc->DrawEllipse(c.m_x - w * 0.5, c.m_y - h * 0.5, w, h);
    }
    else if (pad.shape == PAD_SHAPE_OVAL) {
        double rad = std::min(w, h) * 0.5;
        gc->DrawRoundedRectangle(c.m_x - w * 0.5, c.m_y - h * 0.5, w, h, rad);
    }
    else {
        gc->DrawRectangle(c.m_x - w * 0.5, c.m_y - h * 0.5, w, h);
    }

    if (pad.drill > 0.0) {
        double d = std::max(1.0, xf.Len(pad.drill));
        gc->SetPen(wxPen(wxColour(80, 80, 80), 1));
        gc->SetBrush(*wxWHITE_BRUSH);
        gc->DrawEllipse(c.m_x - d * 0.5, c.m_y - d * 0.5, d, d);
    }
    if (selectedHighlight) {
        gc->SetPen(wxPen(wxColour(30, 100, 230), 2));
        gc->SetBrush(*wxTRANSPARENT_BRUSH);
        gc->DrawRectangle(c.m_x - w * 0.5 - 2, c.m_y - h * 0.5 - 2, w + 4, h + 4);
    }
    gc->PopState();
}

inline void DrawFootprint(wxGraphicsContext* gc, const Xform& xf, const Footprint& fp,
                          bool showLabels = true, const wxString& designator = wxString(),
                          bool drawOrigin = false)
{
    // Silkscreen / graphics first.
    for (const auto& s : fp.shapes) s->Draw(gc, xf);

    // Copper pads.
    for (const auto& p : fp.pads) DrawPadShape(gc, xf, p, false);

    // Reference designator (board view).
    if (!designator.empty()) {
        wxRect2DDouble b = fp.Bounds();
        int px = std::max(8, (int)std::lround(xf.scale * 1.1));
        wxFont f(wxFontInfo(wxSize(px, px)).Family(wxFONTFAMILY_SWISS).Bold());
        gc->SetFont(f, wxColour(35, 90, 190));
        wxPoint2DDouble s = xf.ToScreen(b.m_x, b.m_y);
        gc->DrawText(designator, s.m_x, s.m_y - px);
    }

    // Pad numbers, placed to avoid overlap (Task C: identifiers must stay visible).
    if (showLabels) {
        double labelPx = ClampD(xf.scale * 0.95, 7.0, 15.0);
        double labelHmm = labelPx / xf.scale;
        wxFont f(wxFontInfo(wxSize((int)std::lround(labelPx), (int)std::lround(labelPx))).Family(wxFONTFAMILY_SWISS));
        gc->SetFont(f, *wxBLACK);

        std::vector<wxRect2DDouble> used;
        for (const auto& p : fp.pads) used.push_back(p.AABB());

        for (size_t i = 0; i < fp.pads.size(); ++i) {
            const Pad& p = fp.pads[i];
            wxString num = TrimStr(p.number);
            if (num.empty()) continue;
            wxDouble tw = 0, th = 0, desc = 0, lead = 0;
            gc->GetTextExtent(num, &tw, &th, &desc, &lead);
            if (tw <= 0 || th <= 0) { tw = labelPx * 0.6 * num.length(); th = labelPx * 1.2; }
            double twmm = tw / xf.scale + 0.25;
            double thmm = labelHmm + 0.15;
            wxRect2DDouble padb = p.AABB();
            double cx = padb.m_x + padb.m_width * 0.5;
            double cy = padb.m_y + padb.m_height * 0.5;

            struct Cand { double cx, cy; bool inside; };
            std::vector<Cand> cands;
            cands.push_back({ cx, cy, true });
            cands.push_back({ cx, padb.m_y - thmm * 0.5 - 0.15, false });
            cands.push_back({ cx, padb.m_y + padb.m_height + thmm * 0.5 + 0.15, false });
            cands.push_back({ padb.m_x - twmm * 0.5 - 0.15, cy, false });
            cands.push_back({ padb.m_x + padb.m_width + twmm * 0.5 + 0.15, cy, false });

            wxRect2DDouble chosen(cx - twmm * 0.5, cy - thmm * 0.5, twmm, thmm);
            bool found = false;
            for (const Cand& cand : cands) {
                wxRect2DDouble r(cand.cx - twmm * 0.5, cand.cy - thmm * 0.5, twmm, thmm);
                bool clash = false;
                if (cand.inside) {
                    if (r.m_width > padb.m_width - 0.05 || r.m_height > padb.m_height - 0.05) clash = true;
                }
                if (!clash) {
                    for (size_t u = 0; u < used.size(); ++u) {
                        if (u == i) continue;
                        if (used[u].Intersects(r)) { clash = true; break; }
                    }
                }
                if (!clash) { chosen = r; found = true; break; }
            }
            if (!found) {
                // fall back to a spot above the pad, accepting the overlap
                chosen = wxRect2DDouble(cx - twmm * 0.5, padb.m_y - thmm - 0.15, twmm, thmm);
            }
            used.push_back(chosen);

            wxColour textColour = *wxBLACK;
            bool inside = chosen.m_x >= padb.m_x && chosen.m_y >= padb.m_y &&
                          chosen.m_x + chosen.m_width <= padb.m_x + padb.m_width &&
                          chosen.m_y + chosen.m_height <= padb.m_y + padb.m_height;
            if (inside) textColour = IsDarkColour(LayerColour(p.layer)) ? *wxWHITE : *wxBLACK;
            gc->SetFont(f, textColour);
            wxPoint2DDouble sp = xf.ToScreen(chosen.m_x + chosen.m_width * 0.5 - twmm * 0.5 + 0.125,
                                             chosen.m_y + chosen.m_height * 0.5 - thmm * 0.5 + 0.075);
            gc->DrawText(num, sp.m_x, sp.m_y);
        }
    }

    if (drawOrigin) {
        wxPoint2DDouble o = xf.ToScreen(0, 0);
        gc->SetPen(wxPen(wxColour(200, 40, 40), 1, wxPENSTYLE_SHORT_DASH));
        gc->SetBrush(*wxTRANSPARENT_BRUSH);
        double arm = xf.Len(2.0);
        gc->StrokeLine(o.m_x - arm, o.m_y, o.m_x + arm, o.m_y);
        gc->StrokeLine(o.m_x, o.m_y - arm, o.m_x, o.m_y + arm);
        gc->DrawEllipse(o.m_x - 5, o.m_y - 5, 10, 10);
    }
}

// ============================================================================
// XML serialization for footprints and libraries
// ============================================================================
inline void ShapeToXml(wxXmlNode* parent, const Shape& s)
{
    wxXmlNode* n = new wxXmlNode(wxXML_ELEMENT_NODE, "shape");
    wxString type = s.TypeName();
    n->AddAttribute("type", type);
    n->AddAttribute("layer", LayerStorageName(s.layer));
    n->AddAttribute("stroke", ColourToHex(s.stroke));
    n->AddAttribute("fill", ColourToHex(s.fill));
    n->AddAttribute("width", wxString::Format("%.4f", s.lineWidth));
    n->AddAttribute("rotation", wxString::Format("%.4f", s.rotation));

    if (type == "Line") {
        const LineShape* l = static_cast<const LineShape*>(&s);
        n->AddAttribute("x1", wxString::Format("%.4f", l->p1.m_x));
        n->AddAttribute("y1", wxString::Format("%.4f", l->p1.m_y));
        n->AddAttribute("x2", wxString::Format("%.4f", l->p2.m_x));
        n->AddAttribute("y2", wxString::Format("%.4f", l->p2.m_y));
    }
    else if (type == "Rectangle" || type == "Circle") {
        const Shape* sh = &s;
        wxRect2DDouble b = sh->LocalBounds();
        n->AddAttribute("x", wxString::Format("%.4f", b.m_x));
        n->AddAttribute("y", wxString::Format("%.4f", b.m_y));
        n->AddAttribute("w", wxString::Format("%.4f", b.m_width));
        n->AddAttribute("h", wxString::Format("%.4f", b.m_height));
    }
    else if (type == "Arc") {
        const ArcShape* a = static_cast<const ArcShape*>(&s);
        n->AddAttribute("cx", wxString::Format("%.4f", a->center.m_x));
        n->AddAttribute("cy", wxString::Format("%.4f", a->center.m_y));
        n->AddAttribute("radius", wxString::Format("%.4f", a->radius));
        n->AddAttribute("start", wxString::Format("%.4f", a->startAngle));
        n->AddAttribute("sweep", wxString::Format("%.4f", a->sweepAngle));
    }
    else if (type == "Polygon") {
        const PolygonShape* p = static_cast<const PolygonShape*>(&s);
        wxString pts;
        for (size_t i = 0; i < p->pts.size(); ++i) {
            if (i) pts += ";";
            pts += wxString::Format("%.4f,%.4f", p->pts[i].m_x, p->pts[i].m_y);
        }
        n->AddAttribute("points", pts);
    }
    else if (type == "Text") {
        const TextShape* t = static_cast<const TextShape*>(&s);
        n->AddAttribute("x", wxString::Format("%.4f", t->pos.m_x));
        n->AddAttribute("y", wxString::Format("%.4f", t->pos.m_y));
        n->AddAttribute("size", wxString::Format("%.4f", t->fontSize));
        n->AddAttribute("text", t->text);
    }
    parent->AddChild(n);
}

inline std::unique_ptr<Shape> ShapeFromXml(const wxXmlNode* n)
{
    wxString type = n->GetAttribute("type", "Line");
    wxString stroke = n->GetAttribute("stroke", "#000000");
    wxString fill = n->GetAttribute("fill", "none");
    double width = 0.2, rot = 0.0;
    n->GetAttribute("width", "0.2").ToDouble(&width);
    n->GetAttribute("rotation", "0").ToDouble(&rot);

    std::unique_ptr<Shape> s;
    auto getD = [&](const wxString& key, double def) {
        double v = def;
        wxString str = n->GetAttribute(key, wxString::Format("%.4f", def));
        str.ToDouble(&v);
        return v;
    };

    if (type == "Line") {
        s = std::make_unique<LineShape>(getD("x1", 0), getD("y1", 0), getD("x2", 1), getD("y2", 1));
    }
    else if (type == "Rectangle") {
        s = std::make_unique<RectShape>(wxRect2DDouble(getD("x", 0), getD("y", 0), getD("w", 1), getD("h", 1)));
    }
    else if (type == "Circle") {
        s = std::make_unique<CircleShape>(wxRect2DDouble(getD("x", 0), getD("y", 0), getD("w", 1), getD("h", 1)));
    }
    else if (type == "Arc") {
        s = std::make_unique<ArcShape>(getD("cx", 0), getD("cy", 0), getD("radius", 1), getD("start", 0), getD("sweep", 90));
    }
    else if (type == "Polygon") {
        std::vector<wxPoint2DDouble> pts;
        wxString all = n->GetAttribute("points", "");
        wxStringTokenizer tok(all, ";");
        while (tok.HasMoreTokens()) {
            wxString pair = tok.GetNextToken();
            wxStringTokenizer p2(pair, ",");
            if (!p2.HasMoreTokens()) continue;
            double x = 0, y = 0;
            p2.GetNextToken().ToDouble(&x);
            if (p2.HasMoreTokens()) p2.GetNextToken().ToDouble(&y);
            pts.push_back(wxPoint2DDouble(x, y));
        }
        if (pts.size() < 3) return nullptr;
        s = std::make_unique<PolygonShape>(pts);
    }
    else if (type == "Text") {
        s = std::make_unique<TextShape>(getD("x", 0), getD("y", 0), n->GetAttribute("text", "TEXT"), getD("size", 2.0));
    }
    if (!s) return nullptr;
    s->stroke = HexToColour(stroke, LayerColour(LAYER_SILK));
    s->fill = HexToColour(fill, wxNullColour);
    s->lineWidth = width;
    s->rotation = rot;
    s->layer = LayerFromStorageName(n->GetAttribute("layer", "top"));
    return s;
}

inline wxXmlNode* FootprintToXml(const Footprint& fp)
{
    wxXmlNode* n = new wxXmlNode(nullptr, wxXML_ELEMENT_NODE, "footprint");
    n->AddAttribute("name", fp.name);
    n->AddAttribute("uid", fp.uid);
    n->AddAttribute("category", fp.category);
    n->AddAttribute("description", fp.description);
    n->AddAttribute("unit", fp.unit);
    n->AddAttribute("keywords", fp.keywords);

    for (const auto& p : fp.pads) {
        wxXmlNode* pn = new wxXmlNode(wxXML_ELEMENT_NODE, "pad");
        pn->AddAttribute("number", p.number);
        pn->AddAttribute("x", wxString::Format("%.4f", p.x));
        pn->AddAttribute("y", wxString::Format("%.4f", p.y));
        pn->AddAttribute("w", wxString::Format("%.4f", p.w));
        pn->AddAttribute("h", wxString::Format("%.4f", p.h));
        pn->AddAttribute("rotation", wxString::Format("%.4f", p.rotation));
        pn->AddAttribute("drill", wxString::Format("%.4f", p.drill));
        pn->AddAttribute("type", p.type == PAD_THT ? "tht" : (p.type == PAD_VIA ? "via" : "smd"));
        pn->AddAttribute("shape", PadShapeStorage(p.shape));
        pn->AddAttribute("layer", LayerStorageName(p.layer));
        if (!p.net.empty()) pn->AddAttribute("net", p.net);
        n->AddChild(pn);
    }
    for (const auto& s : fp.shapes) ShapeToXml(n, *s);
    return n;
}

inline std::shared_ptr<Footprint> FootprintFromXml(const wxXmlNode* n, wxString& err)
{
    auto fp = std::make_shared<Footprint>();
    fp->name = n->GetAttribute("name", "");
    fp->uid = n->GetAttribute("uid", "");
    fp->category = n->GetAttribute("category", "");
    fp->description = n->GetAttribute("description", "");
    fp->unit = n->GetAttribute("unit", "mm");
    fp->keywords = n->GetAttribute("keywords", "");
    if (fp->uid.empty()) fp->uid = wxString::Format("fp-%ld-%d", (long)time(nullptr), rand());

    for (wxXmlNode* c = n->GetChildren(); c; c = c->GetNext()) {
        if (c->GetName() == "pad") {
            Pad p;
            p.number = c->GetAttribute("number", "");
            c->GetAttribute("x", "0").ToDouble(&p.x);
            c->GetAttribute("y", "0").ToDouble(&p.y);
            c->GetAttribute("w", "1").ToDouble(&p.w);
            c->GetAttribute("h", "1").ToDouble(&p.h);
            c->GetAttribute("rotation", "0").ToDouble(&p.rotation);
            c->GetAttribute("drill", "0").ToDouble(&p.drill);
            wxString t = c->GetAttribute("type", "smd");
            p.type = t == "tht" ? PAD_THT : (t == "via" ? PAD_VIA : PAD_SMD);
            p.shape = PadShapeFromStorage(c->GetAttribute("shape", "rect"));
            p.layer = LayerFromStorageName(c->GetAttribute("layer", "top"));
            p.net = c->GetAttribute("net", "");
            p.uid = NextPadUid();
            fp->pads.push_back(p);
        }
        else if (c->GetName() == "shape") {
            std::unique_ptr<Shape> s = ShapeFromXml(c);
            if (s) fp->shapes.push_back(std::move(s));
            else err += "Skipped an unreadable shape in footprint '" + fp->name + "'.\n";
        }
    }
    return fp;
}

// ============================================================================
// Component library (Task D)
// ============================================================================
class Library
{
public:
    wxString name = "Untitled Library";
    wxString filePath;
    bool modified = false;
    std::vector<std::shared_ptr<Footprint>> items;

    static wxString NewUid()
    {
        static int counter = 0;
        ++counter;
        return wxString::Format("fp-%08lx-%04d-%04d", (unsigned long)time(nullptr), counter, rand() % 10000);
    }

    int IndexOfUid(const wxString& id) const
    {
        if (id.empty()) return -1;
        for (size_t i = 0; i < items.size(); ++i) if (items[i]->uid == id) return (int)i;
        return -1;
    }

    Footprint* Find(const wxString& fpName) const
    {
        for (const auto& fp : items) {
            if (fp->name.IsSameAs(fpName, false)) return fp.get();
        }
        return nullptr;
    }

    bool NameExists(const wxString& fpName, const wxString& exceptUid = wxString()) const
    {
        for (const auto& fp : items) {
            if (!exceptUid.empty() && fp->uid == exceptUid) continue;
            if (fp->name.IsSameAs(TrimStr(fpName), false)) return true;
        }
        return false;
    }

    wxString MakeUniqueName(const wxString& base, const wxString& exceptUid = wxString()) const
    {
        wxString root = TrimStr(base);
        if (root.empty()) root = "Footprint";
        if (!NameExists(root, exceptUid)) return root;
        for (int i = 2; i < 1000; ++i) {
            wxString candidate = wxString::Format("%s_%d", root, i);
            if (!NameExists(candidate, exceptUid)) return candidate;
        }
        return root + "_copy";
    }

    wxArrayString Categories() const
    {
        wxArrayString result;
        for (const auto& fp : items) {
            wxString cat = TrimStr(fp->category);
            if (cat.empty()) cat = "Uncategorised";
            if (result.Index(cat) == wxNOT_FOUND) result.Add(cat);
        }
        result.Sort();
        return result;
    }

    bool Save(const wxString& path, wxString& err)
    {
        wxXmlDocument doc;
        wxXmlNode* root = new wxXmlNode(nullptr, wxXML_ELEMENT_NODE, "eda-library");
        root->AddAttribute("version", "1.0");
        root->AddAttribute("name", name);
        for (const auto& fp : items) root->AddChild(FootprintToXml(*fp));
        doc.SetRoot(root);
        if (!doc.Save(path)) {
            err = "Could not write library file:\n" + path;
            return false;
        }
        filePath = path;
        modified = false;
        if (name.empty() || name == "Untitled Library") {
            name = wxFileName(path).GetName();
        }
        return true;
    }

    bool Load(const wxString& path, wxString& err)
    {
        wxXmlDocument doc;
        if (!doc.Load(path)) {
            err = "Could not read library file (invalid XML):\n" + path;
            return false;
        }
        wxXmlNode* root = doc.GetRoot();
        if (!root || root->GetName() != "eda-library") {
            err = "The selected file is not an EDA component library.";
            return false;
        }

        std::vector<std::shared_ptr<Footprint>> loaded;
        std::set<wxString> names;
        for (wxXmlNode* c = root->GetChildren(); c; c = c->GetNext()) {
            if (c->GetName() != "footprint") continue;
            wxString parseErr;
            auto fp = FootprintFromXml(c, parseErr);
            if (!fp) continue;
            if (fp->name.empty()) {
                err += "Skipped a footprint without a name.\n";
                continue;
            }
            wxString lower = fp->name.Lower();
            if (names.count(lower)) {
                err += "Skipped duplicate footprint name '" + fp->name + "'.\n";
                continue;
            }
            names.insert(lower);
            if (fp->uid.empty()) fp->uid = NewUid();
            loaded.push_back(fp);
        }

        items.swap(loaded);
        name = root->GetAttribute("name", wxFileName(path).GetName());
        filePath = path;
        modified = false;
        return true;
    }

    static std::shared_ptr<Library> CreateSample()
    {
        auto lib = std::make_shared<Library>();
        lib->name = "Sample Library";
        lib->filePath.clear();

        // ---- SOIC-8: SMD, rectangular pads, 8 pins --------------------------
        {
            auto fp = std::make_shared<Footprint>();
            fp->name = "SOIC-8";
            fp->uid = NewUid();
            fp->category = "IC / SMD";
            fp->description = "8-pin SOIC, 1.27 mm pitch, 3.9 x 4.9 mm body";
            fp->keywords = "soic sop smd 8pin 1.27";
            double ys[4] = { -1.905, -0.635, 0.635, 1.905 };
            for (int i = 0; i < 4; ++i) {
                Pad p; p.number = wxString::Format("%d", i + 1);
                p.x = -2.7; p.y = ys[i]; p.w = 1.5; p.h = 0.6; p.shape = PAD_SHAPE_RECT; p.type = PAD_SMD;
                fp->AddPad(p);
            }
            for (int i = 0; i < 4; ++i) {
                Pad p; p.number = wxString::Format("%d", 8 - i);
                p.x = 2.7; p.y = ys[i]; p.w = 1.5; p.h = 0.6; p.shape = PAD_SHAPE_RECT; p.type = PAD_SMD;
                fp->AddPad(p);
            }
            auto rect = std::make_unique<RectShape>(wxRect2DDouble(-1.95, -2.45, 3.9, 4.9));
            rect->layer = LAYER_SILK; rect->stroke = LayerColour(LAYER_SILK); rect->lineWidth = 0.15;
            fp->shapes.push_back(std::move(rect));
            auto dot = std::make_unique<CircleShape>(wxRect2DDouble(-1.55, -2.2, 0.5, 0.5));
            dot->layer = LAYER_SILK; dot->stroke = LayerColour(LAYER_SILK); dot->fill = LayerColour(LAYER_SILK); dot->lineWidth = 0.1;
            fp->shapes.push_back(std::move(dot));
            auto txt = std::make_unique<TextShape>(-1.8, 2.6, "SOIC-8", 0.9);
            txt->layer = LAYER_SILK; txt->stroke = LayerColour(LAYER_SILK);
            fp->shapes.push_back(std::move(txt));
            lib->items.push_back(fp);
        }

        // ---- 0603 resistor ------------------------------------------------
        {
            auto fp = std::make_shared<Footprint>();
            fp->name = "R_0603";
            fp->uid = NewUid();
            fp->category = "Passive / SMD";
            fp->description = "0603 chip resistor / capacitor, 1.6 x 0.8 mm";
            fp->keywords = "0603 resistor capacitor passive smd chip";
            Pad p1; p1.number = "1"; p1.x = -0.8; p1.y = 0; p1.w = 0.8; p1.h = 0.95; p1.shape = PAD_SHAPE_RECT;
            Pad p2 = p1; p2.number = "2"; p2.x = 0.8;
            fp->AddPad(p1); fp->AddPad(p2);
            auto rect = std::make_unique<RectShape>(wxRect2DDouble(-0.8, -0.45, 1.6, 0.9));
            rect->layer = LAYER_SILK; rect->stroke = LayerColour(LAYER_SILK); rect->lineWidth = 0.12;
            fp->shapes.push_back(std::move(rect));
            auto txt = std::make_unique<TextShape>(-0.45, -1.35, "R?", 0.8);
            txt->layer = LAYER_SILK; txt->stroke = LayerColour(LAYER_SILK);
            fp->shapes.push_back(std::move(txt));
            lib->items.push_back(fp);
        }

        // ---- DIP-8: through-hole, round pads ------------------------------
        {
            auto fp = std::make_shared<Footprint>();
            fp->name = "DIP-8_W7.62mm";
            fp->uid = NewUid();
            fp->category = "IC / THT";
            fp->description = "8-pin DIP, 2.54 mm pitch, 7.62 mm row spacing";
            fp->keywords = "dip dip8 through hole tht 2.54 8pin";
            double ys[4] = { -3.81, -1.27, 1.27, 3.81 };
            for (int i = 0; i < 4; ++i) {
                Pad p; p.number = wxString::Format("%d", i + 1);
                p.x = -3.81; p.y = ys[i]; p.w = 1.8; p.h = 1.8; p.drill = 0.9;
                p.shape = PAD_SHAPE_ROUND; p.type = PAD_THT;
                fp->AddPad(p);
            }
            for (int i = 0; i < 4; ++i) {
                Pad p; p.number = wxString::Format("%d", 8 - i);
                p.x = 3.81; p.y = ys[i]; p.w = 1.8; p.h = 1.8; p.drill = 0.9;
                p.shape = PAD_SHAPE_ROUND; p.type = PAD_THT;
                fp->AddPad(p);
            }
            auto rect = std::make_unique<RectShape>(wxRect2DDouble(-3.175, -4.635, 6.35, 9.27));
            rect->layer = LAYER_SILK; rect->stroke = LayerColour(LAYER_SILK); rect->lineWidth = 0.15;
            fp->shapes.push_back(std::move(rect));
            auto notch = std::make_unique<ArcShape>(0, -4.635, 0.75, 0, 180);
            notch->layer = LAYER_SILK; notch->stroke = LayerColour(LAYER_SILK); notch->lineWidth = 0.15;
            fp->shapes.push_back(std::move(notch));
            auto dot = std::make_unique<CircleShape>(wxRect2DDouble(-3.2, -4.1, 0.45, 0.45));
            dot->layer = LAYER_SILK; dot->stroke = LayerColour(LAYER_SILK); dot->fill = LayerColour(LAYER_SILK); dot->lineWidth = 0.1;
            fp->shapes.push_back(std::move(dot));
            auto txt = std::make_unique<TextShape>(-2.1, 5.0, "DIP-8", 1.0);
            txt->layer = LAYER_SILK; txt->stroke = LayerColour(LAYER_SILK);
            fp->shapes.push_back(std::move(txt));
            lib->items.push_back(fp);
        }

        // ---- QFP-32: dense SMD, four sides --------------------------------
        {
            auto fp = std::make_shared<Footprint>();
            fp->name = "QFP-32_7x7mm";
            fp->uid = NewUid();
            fp->category = "IC / SMD";
            fp->description = "32-pin QFP, 0.8 mm pitch, 7 x 7 mm body";
            fp->keywords = "qfp qfp32 smd 0.8 32pin";
            int num = 1;
            for (int i = 0; i < 8; ++i) {
                Pad p; p.number = wxString::Format("%d", num++);
                p.x = -3.65; p.y = -2.8 + i * 0.8; p.w = 1.5; p.h = 0.3; p.shape = PAD_SHAPE_RECT;
                fp->AddPad(p);
            }
            for (int i = 0; i < 8; ++i) {
                Pad p; p.number = wxString::Format("%d", num++);
                p.x = -2.8 + i * 0.8; p.y = 3.65; p.w = 0.3; p.h = 1.5; p.shape = PAD_SHAPE_RECT;
                fp->AddPad(p);
            }
            for (int i = 0; i < 8; ++i) {
                Pad p; p.number = wxString::Format("%d", num++);
                p.x = 3.65; p.y = 2.8 - i * 0.8; p.w = 1.5; p.h = 0.3; p.shape = PAD_SHAPE_RECT;
                fp->AddPad(p);
            }
            for (int i = 0; i < 8; ++i) {
                Pad p; p.number = wxString::Format("%d", num++);
                p.x = 2.8 - i * 0.8; p.y = -3.65; p.w = 0.3; p.h = 1.5; p.shape = PAD_SHAPE_RECT;
                fp->AddPad(p);
            }
            auto rect = std::make_unique<RectShape>(wxRect2DDouble(-2.9, -2.9, 5.8, 5.8));
            rect->layer = LAYER_SILK; rect->stroke = LayerColour(LAYER_SILK); rect->lineWidth = 0.12;
            fp->shapes.push_back(std::move(rect));
            auto dot = std::make_unique<CircleShape>(wxRect2DDouble(-2.4, -2.4, 0.45, 0.45));
            dot->layer = LAYER_SILK; dot->stroke = LayerColour(LAYER_SILK); dot->fill = LayerColour(LAYER_SILK); dot->lineWidth = 0.1;
            fp->shapes.push_back(std::move(dot));
            auto txt = std::make_unique<TextShape>(-1.5, -4.9, "QFP-32", 1.0);
            txt->layer = LAYER_SILK; txt->stroke = LayerColour(LAYER_SILK);
            fp->shapes.push_back(std::move(txt));
            lib->items.push_back(fp);
        }

        // ---- SOT-23 -------------------------------------------------------
        {
            auto fp = std::make_shared<Footprint>();
            fp->name = "SOT-23";
            fp->uid = NewUid();
            fp->category = "Transistor / SMD";
            fp->description = "3-pin SOT-23 transistor, 0.95 mm pitch";
            fp->keywords = "sot23 sot-23 transistor smd 3pin";
            Pad p1; p1.number = "1"; p1.x = -1.0; p1.y = -0.95; p1.w = 1.0; p1.h = 0.8; p1.shape = PAD_SHAPE_RECT;
            Pad p2; p2.number = "2"; p2.x = -1.0; p2.y = 0.95; p2.w = 1.0; p2.h = 0.8; p2.shape = PAD_SHAPE_RECT;
            Pad p3; p3.number = "3"; p3.x = 1.0; p3.y = 0.0; p3.w = 1.0; p3.h = 0.8; p3.shape = PAD_SHAPE_RECT;
            fp->AddPad(p1); fp->AddPad(p2); fp->AddPad(p3);
            auto rect = std::make_unique<RectShape>(wxRect2DDouble(-0.7, -1.4, 1.4, 2.8));
            rect->layer = LAYER_SILK; rect->stroke = LayerColour(LAYER_SILK); rect->lineWidth = 0.12;
            fp->shapes.push_back(std::move(rect));
            auto txt = std::make_unique<TextShape>(-0.7, -2.4, "Q?", 0.8);
            txt->layer = LAYER_SILK; txt->stroke = LayerColour(LAYER_SILK);
            fp->shapes.push_back(std::move(txt));
            lib->items.push_back(fp);
        }

        // ---- 1x4 pin header: THT oval pads --------------------------------
        {
            auto fp = std::make_shared<Footprint>();
            fp->name = "Header-1x4_2.54mm";
            fp->uid = NewUid();
            fp->category = "Connector / THT";
            fp->description = "4-pin 2.54 mm vertical pin header, oval pads";
            fp->keywords = "header connector 2.54 4pin tht pin";
            for (int i = 0; i < 4; ++i) {
                Pad p; p.number = wxString::Format("%d", i + 1);
                p.x = 0; p.y = -3.81 + i * 2.54; p.w = 1.7; p.h = 1.7; p.drill = 1.0;
                p.shape = PAD_SHAPE_OVAL; p.type = PAD_THT;
                fp->AddPad(p);
            }
            auto rect = std::make_unique<RectShape>(wxRect2DDouble(-0.85, -4.75, 1.7, 9.5));
            rect->layer = LAYER_SILK; rect->stroke = LayerColour(LAYER_SILK); rect->lineWidth = 0.12;
            fp->shapes.push_back(std::move(rect));
            auto txt = std::make_unique<TextShape>(-1.6, 5.1, "J?", 0.9);
            txt->layer = LAYER_SILK; txt->stroke = LayerColour(LAYER_SILK);
            fp->shapes.push_back(std::move(txt));
            lib->items.push_back(fp);
        }

        // ---- LED 0805: oval pads with polarity mark -----------------------
        {
            auto fp = std::make_shared<Footprint>();
            fp->name = "LED-0805";
            fp->uid = NewUid();
            fp->category = "LED / SMD";
            fp->description = "0805 LED, oval pads, cathode marked on silkscreen";
            fp->keywords = "led 0805 smd diode light";
            Pad p1; p1.number = "1"; p1.x = -0.9; p1.y = 0; p1.w = 1.0; p1.h = 1.4; p1.shape = PAD_SHAPE_OVAL;
            Pad p2 = p1; p2.number = "2"; p2.x = 0.9;
            fp->AddPad(p1); fp->AddPad(p2);
            auto rect = std::make_unique<RectShape>(wxRect2DDouble(-0.7, -0.6, 1.4, 1.2));
            rect->layer = LAYER_SILK; rect->stroke = LayerColour(LAYER_SILK); rect->lineWidth = 0.12;
            fp->shapes.push_back(std::move(rect));
            auto line = std::make_unique<LineShape>(-0.7, -0.75, -0.7, 0.75);
            line->layer = LAYER_SILK; line->stroke = LayerColour(LAYER_SILK); line->lineWidth = 0.2;
            fp->shapes.push_back(std::move(line));
            auto txt = std::make_unique<TextShape>(-0.6, -1.6, "D?", 0.8);
            txt->layer = LAYER_SILK; txt->stroke = LayerColour(LAYER_SILK);
            fp->shapes.push_back(std::move(txt));
            lib->items.push_back(fp);
        }

        // ---- Test point ---------------------------------------------------
        {
            auto fp = std::make_shared<Footprint>();
            fp->name = "TestPoint_1.0mm";
            fp->uid = NewUid();
            fp->category = "Test Point";
            fp->description = "Single through-hole test point, 1.0 mm drill";
            fp->keywords = "test point probe tht 1pin";
            Pad p; p.number = "1"; p.x = 0; p.y = 0; p.w = 2.0; p.h = 2.0; p.drill = 1.0;
            p.shape = PAD_SHAPE_ROUND; p.type = PAD_THT;
            fp->AddPad(p);
            auto circle = std::make_unique<CircleShape>(wxRect2DDouble(-1.5, -1.5, 3.0, 3.0));
            circle->layer = LAYER_SILK; circle->stroke = LayerColour(LAYER_SILK); circle->lineWidth = 0.15;
            fp->shapes.push_back(std::move(circle));
            auto txt = std::make_unique<TextShape>(-1.2, 1.8, "TP?", 0.9);
            txt->layer = LAYER_SILK; txt->stroke = LayerColour(LAYER_SILK);
            fp->shapes.push_back(std::move(txt));
            lib->items.push_back(fp);
        }

        lib->modified = false;
        return lib;
    }
};

// Global pointer used by ComponentShape to render the live library footprint.
static Library* g_library = nullptr;

inline Footprint* FindLibraryFootprint(const wxString& name)
{
    if (!g_library) return nullptr;
    return g_library->Find(name);
}



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


// ============================================================================
// Small generated toolbar icons (no external resources required)
// ============================================================================
inline wxBitmap MakeToolIcon(const wxString& kind, int size = 20)
{
    wxBitmap bmp(size, size, 24);
    wxMemoryDC dc(bmp);
    dc.SetBackground(wxBrush(wxColour(247, 247, 247)));
    dc.Clear();
    dc.SetPen(wxPen(wxColour(45, 45, 45), 2));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.SetTextForeground(wxColour(30, 30, 30));
    int s = size;

    if (kind == "select") {
        wxPoint pts[7] = {
            wxPoint(4, 2), wxPoint(4, s - 4), wxPoint(8, s - 8),
            wxPoint(11, s - 2), wxPoint(14, s - 3), wxPoint(11, s - 9), wxPoint(16, s - 9)
        };
        dc.SetBrush(wxBrush(wxColour(250, 250, 250)));
        dc.DrawPolygon(7, pts);
    }
    else if (kind == "line") {
        dc.DrawLine(4, s - 4, s - 4, 4);
    }
    else if (kind == "rect") {
        dc.DrawRectangle(4, 5, s - 8, s - 10);
    }
    else if (kind == "circle") {
        dc.DrawEllipse(3, 3, s - 6, s - 6);
    }
    else if (kind == "arc") {
        wxPoint pts[10];
        for (int i = 0; i < 10; ++i) {
            double a = (180.0 + 90.0 * i / 9.0) * PI_VAL / 180.0;
            pts[i] = wxPoint((int)(s / 2 + (s / 2 - 4) * std::cos(a)), (int)(s / 2 + (s / 2 - 4) * std::sin(a)));
        }
        dc.DrawLines(10, pts);
    }
    else if (kind == "poly") {
        wxPoint pts[5] = { wxPoint(10, 2), wxPoint(17, 7), wxPoint(14, 17), wxPoint(5, 16), wxPoint(3, 7) };
        dc.DrawPolygon(5, pts);
    }
    else if (kind == "text") {
        dc.SetFont(wxFont(wxFontInfo(wxSize(13, 13)).Bold().Family(wxFONTFAMILY_SWISS)));
        dc.DrawText("T", 4, 2);
    }
    else if (kind == "grid") {
        dc.SetPen(wxPen(wxColour(70, 70, 70), 1));
        for (int i = 4; i <= s - 4; i += 4) {
            dc.DrawLine(4, i, s - 4, i);
            dc.DrawLine(i, 4, i, s - 4);
        }
    }
    else if (kind == "snap") {
        dc.SetPen(wxPen(wxColour(45, 45, 45), 2));
        dc.DrawLine(5, 4, 5, 11);
        dc.DrawLine(s - 5, 4, s - 5, 11);
        dc.DrawArc(5, 7, s - 5, 7, s / 2, 11);
        dc.DrawLine(3, 4, 7, 4);
        dc.DrawLine(s - 7, 4, s - 3, 4);
    }
    else if (kind == "rotate") {
        wxPoint pts[9];
        for (int i = 0; i < 9; ++i) {
            double a = (-40.0 + 260.0 * i / 8.0) * PI_VAL / 180.0;
            pts[i] = wxPoint((int)(s / 2 + (s / 2 - 5) * std::cos(a)), (int)(s / 2 + (s / 2 - 5) * std::sin(a)));
        }
        dc.DrawLines(9, pts);
        dc.SetBrush(wxBrush(wxColour(45, 45, 45)));
        wxPoint arrow[3] = { wxPoint(s - 8, 3), wxPoint(s - 2, 7), wxPoint(s - 5, 11) };
        dc.DrawPolygon(3, arrow);
    }
    else if (kind == "delete") {
        dc.SetPen(wxPen(wxColour(170, 40, 40), 2));
        dc.DrawLine(5, 5, s - 5, s - 5);
        dc.DrawLine(s - 5, 5, 5, s - 5);
    }
    else if (kind == "library") {
        dc.SetBrush(wxBrush(wxColour(70, 120, 200)));
        dc.DrawRectangle(4, 3, 12, 14);
        dc.SetBrush(wxBrush(wxColour(250, 250, 250)));
        dc.DrawRectangle(7, 6, 6, 2);
        dc.DrawRectangle(7, 10, 6, 2);
        dc.DrawRectangle(7, 14, 4, 2);
    }
    else if (kind == "pad") {
        dc.SetBrush(wxBrush(wxColour(196, 48, 43)));
        dc.SetPen(wxPen(wxColour(90, 40, 30), 1));
        dc.DrawRectangle(3, 6, 14, 8);
        dc.SetBrush(*wxWHITE_BRUSH);
        dc.DrawEllipse(8, 8, 4, 4);
    }
    else if (kind == "via") {
        dc.SetBrush(wxBrush(wxColour(196, 48, 43)));
        dc.SetPen(wxPen(wxColour(90, 40, 30), 1));
        dc.DrawEllipse(3, 3, 14, 14);
        dc.SetBrush(*wxWHITE_BRUSH);
        dc.DrawEllipse(7, 7, 6, 6);
    }
    else if (kind == "silk") {
        dc.SetPen(wxPen(wxColour(150, 110, 10), 2));
        dc.DrawLine(3, 7, 17, 7);
        dc.DrawLine(3, 13, 17, 13);
        dc.DrawLine(3, 7, 3, 13);
        dc.DrawLine(17, 7, 17, 13);
    }
    else if (kind == "check") {
        dc.SetPen(wxPen(wxColour(30, 130, 60), 3));
        dc.DrawLine(4, 10, 8, 15);
        dc.DrawLine(8, 15, 17, 4);
    }
    else if (kind == "save") {
        dc.SetBrush(wxBrush(wxColour(70, 110, 180)));
        dc.SetPen(wxPen(wxColour(40, 50, 70), 1));
        dc.DrawRectangle(3, 3, 14, 14);
        dc.SetBrush(wxBrush(wxColour(250, 250, 250)));
        dc.DrawRectangle(6, 4, 8, 5);
        dc.DrawRectangle(6, 12, 8, 5);
    }
    else if (kind == "fit") {
        dc.SetPen(wxPen(wxColour(50, 80, 150), 2));
        dc.DrawLine(3, 8, 3, 3); dc.DrawLine(3, 3, 8, 3);
        dc.DrawLine(s - 3, 8, s - 3, 3); dc.DrawLine(s - 3, 3, s - 8, 3);
        dc.DrawLine(3, s - 8, 3, s - 3); dc.DrawLine(3, s - 3, 8, s - 3);
        dc.DrawLine(s - 3, s - 8, s - 3, s - 3); dc.DrawLine(s - 3, s - 3, s - 8, s - 3);
        dc.SetBrush(wxBrush(wxColour(196, 48, 43)));
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawCircle(s / 2, s / 2, 3);
    }
    else if (kind == "new") {
        dc.SetBrush(*wxWHITE_BRUSH);
        dc.DrawRectangle(5, 2, 10, 16);
        dc.SetPen(wxPen(wxColour(50, 80, 150), 1));
        dc.DrawLine(8, 7, 13, 7); dc.DrawLine(8, 10, 13, 10); dc.DrawLine(8, 13, 12, 13);
    }
    else if (kind == "open") {
        dc.SetBrush(wxBrush(wxColour(220, 180, 80)));
        dc.SetPen(wxPen(wxColour(90, 70, 30), 1));
        dc.DrawRectangle(3, 6, 8, 3);
        dc.DrawRectangle(3, 8, 14, 8);
    }
    else {
        dc.SetFont(wxFont(wxFontInfo(wxSize(9, 9)).Bold()));
        dc.DrawText("?", 7, 4);
    }

    dc.SelectObject(wxNullBitmap);
    return bmp;
}

// ============================================================================
// Library footprint preview
// ============================================================================
class FootprintPreviewPanel : public wxPanel
{
public:
    explicit FootprintPreviewPanel(wxWindow* parent)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetBackgroundColour(*wxWHITE);
        Bind(wxEVT_PAINT, &FootprintPreviewPanel::OnPaint, this);
    }

    void SetFootprint(std::shared_ptr<Footprint> fp)
    {
        m_fp = std::move(fp);
        Refresh();
    }

private:
    std::shared_ptr<Footprint> m_fp;

    void OnPaint(wxPaintEvent&)
    {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(*wxWHITE));
        dc.Clear();
        wxSize sz = GetClientSize();

        // A subtle measuring grid.
        dc.SetPen(wxPen(wxColour(238, 238, 234), 1));
        for (int x = 0; x < sz.x; x += 10) dc.DrawLine(x, 0, x, sz.y);
        for (int y = 0; y < sz.y; y += 10) dc.DrawLine(0, y, sz.x, y);

        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        if (!gc) return;
        gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);

        if (!m_fp) {
            gc->SetFont(wxFont(wxFontInfo(wxSize(10, 10))), wxColour(120, 120, 120));
            gc->DrawText("No footprint selected", 14, 14);
            return;
        }

        wxRect2DDouble b = m_fp->Bounds();
        double pad = 26.0;
        double scale = std::min((sz.x - pad * 2) / std::max(0.1, b.m_width),
                                (sz.y - pad * 2) / std::max(0.1, b.m_height));
        scale = ClampD(scale, 0.5, 80.0);
        Xform xf;
        xf.scale = scale;
        xf.ox = sz.x * 0.5 - (b.m_x + b.m_width * 0.5) * scale;
        xf.oy = sz.y * 0.5 - (b.m_y + b.m_height * 0.5) * scale;
        DrawFootprint(gc.get(), xf, *m_fp, true, wxString(), true);
    }
};

// ============================================================================
// Layer panel
// ============================================================================
class LayerPanel : public wxPanel
{
public:
    std::function<void(int)> OnActivate;
    std::function<void(int, bool)> OnVisibility;
    std::function<void(int)> OnSelectLayerObjects;

    explicit LayerPanel(wxWindow* parent) : wxPanel(parent, wxID_ANY)
    {
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        for (int i = 0; i < LAYER_COUNT; ++i) {
            wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
            wxPanel* swatch = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(14, 14), wxBORDER_SIMPLE);
            swatch->SetBackgroundColour(LayerColour(i));
            row->Add(swatch, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);

            m_radio[i] = new wxRadioButton(this, wxID_ANY, LayerName(i), wxDefaultPosition, wxDefaultSize,
                                           i == 0 ? wxRB_GROUP : 0);
            m_radio[i]->SetValue(i == 0);
            row->Add(m_radio[i], 1, wxALIGN_CENTER_VERTICAL);

            m_show[i] = new wxCheckBox(this, wxID_ANY, "Show");
            m_show[i]->SetValue(true);
            row->Add(m_show[i], 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);

            sizer->Add(row, 0, wxEXPAND | wxTOP | wxBOTTOM, 2);
            int layer = i;
            m_radio[i]->Bind(wxEVT_RADIOBUTTON, [this, layer](wxCommandEvent&) {
                if (!m_ignore && OnActivate) OnActivate(layer);
            });
            m_show[i]->Bind(wxEVT_CHECKBOX, [this, layer](wxCommandEvent& e) {
                if (!m_ignore && OnVisibility) OnVisibility(layer, e.IsChecked());
            });
            m_radio[i]->Bind(wxEVT_RIGHT_DOWN, [this, layer](wxMouseEvent&) {
                if (OnSelectLayerObjects) OnSelectLayerObjects(layer);
            });
        }
        SetSizer(sizer);
    }

    void SetActive(int layer)
    {
        if (layer < 0 || layer >= LAYER_COUNT) return;
        m_ignore = true;
        for (int i = 0; i < LAYER_COUNT; ++i) m_radio[i]->SetValue(i == layer);
        m_ignore = false;
    }

    void SetVisibleState(int layer, bool visible)
    {
        if (layer < 0 || layer >= LAYER_COUNT) return;
        m_ignore = true;
        m_show[layer]->SetValue(visible);
        m_ignore = false;
    }

private:
    wxRadioButton* m_radio[LAYER_COUNT] = { nullptr, nullptr, nullptr, nullptr };
    wxCheckBox* m_show[LAYER_COUNT] = { nullptr, nullptr, nullptr, nullptr };
    bool m_ignore = false;
};

// ============================================================================
// Board shape property panel
// ============================================================================
class BoardPropertyPanel : public wxPanel
{
public:
    wxPropertyGrid* pg = nullptr;
    std::function<void()> OnModified;

    explicit BoardPropertyPanel(wxWindow* parent) : wxPanel(parent, wxID_ANY)
    {
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        pg = new wxPropertyGrid(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                wxPG_DEFAULT_STYLE | wxPG_SPLITTER_AUTO_CENTER);
        pg->Append(new wxPropertyCategory("Object"));
        auto typeProp = new wxStringProperty("Type", wxPG_LABEL, "Nothing selected");
        pg->Append(typeProp);
        pg->SetPropertyReadOnly("Type", true);
        auto refProp = new wxStringProperty("Designator", wxPG_LABEL, "");
        pg->Append(refProp);
        auto fpProp = new wxStringProperty("Footprint", wxPG_LABEL, "");
        pg->Append(fpProp);
        pg->SetPropertyReadOnly("Footprint", true);

        pg->Append(new wxPropertyCategory("Geometry"));
        pg->Append(new wxFloatProperty("X", wxPG_LABEL, 0.0));
        pg->Append(new wxFloatProperty("Y", wxPG_LABEL, 0.0));
        pg->Append(new wxFloatProperty("Width", wxPG_LABEL, 0.0));
        pg->Append(new wxFloatProperty("Height", wxPG_LABEL, 0.0));
        pg->Append(new wxFloatProperty("Rotation", wxPG_LABEL, 0.0));
        pg->Append(new wxFloatProperty("Start angle", wxPG_LABEL, 0.0));
        pg->Append(new wxFloatProperty("Sweep angle", wxPG_LABEL, 90.0));

        pg->Append(new wxPropertyCategory("Appearance"));
        wxArrayString layerNames;
        for (int i = 0; i < LAYER_COUNT; ++i) layerNames.Add(LayerName(i));
        pg->Append(new wxEnumProperty("Layer", wxPG_LABEL, layerNames));
        pg->Append(new wxColourProperty("Stroke colour", wxPG_LABEL, *wxBLACK));
        pg->Append(new wxFloatProperty("Line width", wxPG_LABEL, 0.2));
        pg->Append(new wxBoolProperty("Filled", wxPG_LABEL, false));
        pg->Append(new wxColourProperty("Fill colour", wxPG_LABEL, wxColour(244, 220, 216)));
        pg->Append(new wxStringProperty("Text", wxPG_LABEL, ""));

        wxPGProperty* p = pg->GetProperty("Rotation"); if (p) p->SetAttribute("Precision", 2);
        p = pg->GetProperty("X"); if (p) p->SetAttribute("Precision", 3);
        p = pg->GetProperty("Y"); if (p) p->SetAttribute("Precision", 3);
        p = pg->GetProperty("Width"); if (p) p->SetAttribute("Precision", 3);
        p = pg->GetProperty("Height"); if (p) p->SetAttribute("Precision", 3);
        p = pg->GetProperty("Line width"); if (p) p->SetAttribute("Precision", 3);

        sizer->Add(pg, 1, wxEXPAND);
        SetSizer(sizer);
        pg->Bind(wxEVT_PG_CHANGED, &BoardPropertyPanel::OnPropertyChanged, this);
        UpdateFromShape(nullptr, 0);
    }

    void UpdateFromShape(Shape* shape, int count)
    {
        m_updating = true;
        m_shape = (count == 1) ? shape : nullptr;
        m_count = count;

        auto hide = [this](const wxString& name, bool hidden) {
            wxPGProperty* p = pg->GetProperty(name);
            if (p) p->Hide(hidden);
        };

        if (count == 0 || !shape) {
            pg->Disable();
            pg->SetPropertyValue("Type", count == 0 ? wxString("Nothing selected")
                                                    : wxString::Format("%d objects selected", count));
            m_updating = false;
            return;
        }

        pg->Enable();
        wxString type = shape->TypeName();
        pg->SetPropertyValue("Type", type);

        bool isComponent = type == "Component";
        bool isText = type == "Text";
        bool isArc = type == "Arc";
        bool canFill = (type == "Rectangle" || type == "Circle" || type == "Polygon");

        hide("Designator", !isComponent);
        hide("Footprint", !isComponent);
        hide("Start angle", !isArc);
        hide("Sweep angle", !isArc);
        hide("Text", !isText);
        hide("Filled", !canFill);
        hide("Fill colour", !canFill);

        wxRect2DDouble b = shape->LocalBounds();
        pg->SetPropertyValue("X", b.m_x);
        pg->SetPropertyValue("Y", b.m_y);
        pg->SetPropertyValue("Width", b.m_width);
        pg->SetPropertyValue("Height", b.m_height);
        pg->SetPropertyValue("Rotation", shape->rotation);
        SetColour("Stroke colour", shape->stroke);
        pg->SetPropertyValue("Line width", shape->lineWidth);
        pg->SetPropertyValue("Filled", shape->fill.IsOk());
        SetColour("Fill colour", shape->fill.IsOk() ? shape->fill : wxColour(244, 220, 216));
        pg->SetPropertyValue("Layer", (long)shape->layer);

        if (isComponent) {
            ComponentShape* c = static_cast<ComponentShape*>(shape);
            pg->SetPropertyValue("Designator", c->designator);
            pg->SetPropertyValue("Footprint", c->footprintName);
        }
        if (isArc) {
            ArcShape* a = static_cast<ArcShape*>(shape);
            pg->SetPropertyValue("Start angle", a->startAngle);
            pg->SetPropertyValue("Sweep angle", a->sweepAngle);
        }
        if (isText) pg->SetPropertyValue("Text", shape->GetText());

        if (wxPGProperty* p = pg->GetProperty("Width")) p->Enable(!isComponent);
        if (wxPGProperty* p = pg->GetProperty("Height")) p->Enable(!isComponent);
        m_updating = false;
    }

private:
    Shape* m_shape = nullptr;
    int m_count = 0;
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
        if (m_updating || !m_shape) return;
        wxString name = e.GetPropertyName();
        wxRect2DDouble b = m_shape->LocalBounds();

        if (name == "Type" || name == "Footprint") return;

        if (name == "Layer") {
            m_shape->layer = pg->GetPropertyValueAsLong("Layer");
        }
        else if (name == "Stroke colour") {
            m_shape->stroke = GetColour("Stroke colour");
        }
        else if (name == "Fill colour") {
            m_shape->fill = GetColour("Fill colour");
            m_updating = true;
            pg->SetPropertyValue("Filled", true);
            m_updating = false;
        }
        else if (name == "Filled") {
            bool filled = pg->GetPropertyValueAsBool("Filled");
            if (filled) m_shape->fill = GetColour("Fill colour");
            else m_shape->fill = wxNullColour;
        }
        else if (name == "Line width") {
            m_shape->lineWidth = std::max(0.01, pg->GetPropertyValueAsDouble("Line width"));
        }
        else if (name == "Text") {
            m_shape->SetText(pg->GetPropertyValueAsString("Text"));
        }
        else if (name == "Designator") {
            if (m_shape->TypeName() == "Component")
                static_cast<ComponentShape*>(m_shape)->designator = pg->GetPropertyValueAsString("Designator");
        }
        else if (name == "Start angle" || name == "Sweep angle") {
            if (ArcShape* arc = dynamic_cast<ArcShape*>(m_shape)) {
                arc->startAngle = pg->GetPropertyValueAsDouble("Start angle");
                arc->sweepAngle = pg->GetPropertyValueAsDouble("Sweep angle");
            }
        }
        else if (name == "Rotation") {
            m_shape->rotation = NormalizeAngle(pg->GetPropertyValueAsDouble("Rotation"));
        }
        else if (name == "X" || name == "Y" || name == "Width" || name == "Height") {
            if (m_shape->TypeName() == "Component") {
                ComponentShape* c = static_cast<ComponentShape*>(m_shape);
                double nx = pg->GetPropertyValueAsDouble("X");
                double ny = pg->GetPropertyValueAsDouble("Y");
                c->Move(nx - b.m_x, ny - b.m_y);
            }
            else {
                b.m_x = pg->GetPropertyValueAsDouble("X");
                b.m_y = pg->GetPropertyValueAsDouble("Y");
                b.m_width = std::max(0.1, pg->GetPropertyValueAsDouble("Width"));
                b.m_height = std::max(0.1, pg->GetPropertyValueAsDouble("Height"));
                m_shape->SetLocalBounds(b);
            }
        }

        if (OnModified) OnModified();
    }
};

// ============================================================================
// Left sidebar: layers, grid and object list
// ============================================================================
class LeftSidebarPanel : public wxPanel
{
public:
    std::function<void(int)> OnLayerActivated;
    std::function<void(int, bool)> OnLayerVisibility;
    std::function<void(double)> OnGridChanged;
    std::function<void(bool)> OnSnapChanged;
    std::function<void(bool)> OnShowGridChanged;
    std::function<void(int)> OnObjectSelected;
    std::function<void()> OnFitRequested;

    explicit LeftSidebarPanel(wxWindow* parent) : wxPanel(parent, wxID_ANY)
    {
        SetBackgroundColour(wxColour(245, 245, 243));
        wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

        wxStaticText* layerTitle = new wxStaticText(this, wxID_ANY, "Layers");
        wxFont titleFont = layerTitle->GetFont();
        titleFont.SetWeight(wxFONTWEIGHT_BOLD);
        layerTitle->SetFont(titleFont);
        mainSizer->Add(layerTitle, 0, wxALL | wxALIGN_LEFT, 8);
        mainSizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 5);

        m_layerPanel = new LayerPanel(this);
        mainSizer->Add(m_layerPanel, 0, wxEXPAND | wxALL, 6);

        mainSizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 5);

        wxStaticText* gridTitle = new wxStaticText(this, wxID_ANY, "Grid && Snapping");
        gridTitle->SetFont(titleFont);
        mainSizer->Add(gridTitle, 0, wxALL | wxALIGN_LEFT, 8);

        wxBoxSizer* gridRow = new wxBoxSizer(wxHORIZONTAL);
        gridRow->Add(new wxStaticText(this, wxID_ANY, "Step"), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
        wxArrayString grids;
        grids.Add("0.1 mm"); grids.Add("0.25 mm"); grids.Add("0.5 mm");
        grids.Add("1.0 mm"); grids.Add("2.54 mm"); grids.Add("5.0 mm");
        m_gridChoice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, grids);
        m_gridChoice->SetSelection(2);
        gridRow->Add(m_gridChoice, 1, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 6);
        mainSizer->Add(gridRow, 0, wxEXPAND | wxTOP, 2);

        m_snapCheck = new wxCheckBox(this, wxID_ANY, "Snap to grid (S)");
        m_snapCheck->SetValue(true);
        mainSizer->Add(m_snapCheck, 0, wxLEFT | wxTOP, 8);
        m_gridCheck = new wxCheckBox(this, wxID_ANY, "Show grid (G)");
        m_gridCheck->SetValue(true);
        mainSizer->Add(m_gridCheck, 0, wxLEFT | wxTOP | wxBOTTOM, 8);

        wxButton* fitBtn = new wxButton(this, wxID_ANY, "Zoom to fit");
        mainSizer->Add(fitBtn, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

        mainSizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 5);

        wxBoxSizer* objHeader = new wxBoxSizer(wxHORIZONTAL);
        wxStaticText* objTitle = new wxStaticText(this, wxID_ANY, "Objects");
        objTitle->SetFont(titleFont);
        objHeader->Add(objTitle, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
        m_countLabel = new wxStaticText(this, wxID_ANY, "0");
        objHeader->Add(m_countLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
        mainSizer->Add(objHeader, 0, wxEXPAND | wxALL, 4);

        m_objects = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                   wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_SIMPLE);
        m_objects->AppendColumn("Object", wxLIST_FORMAT_LEFT, 140);
        m_objects->AppendColumn("Layer", wxLIST_FORMAT_LEFT, 80);
        mainSizer->Add(m_objects, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

        SetSizer(mainSizer);

        m_layerPanel->OnActivate = [this](int layer) { if (OnLayerActivated) OnLayerActivated(layer); };
        m_layerPanel->OnVisibility = [this](int layer, bool v) { if (OnLayerVisibility) OnLayerVisibility(layer, v); };
        m_layerPanel->OnSelectLayerObjects = [this](int layer) {
            if (OnLayerActivated) OnLayerActivated(layer);
        };
        m_gridChoice->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
            if (OnGridChanged) OnGridChanged(GridValue());
        });
        m_snapCheck->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent& e) {
            if (OnSnapChanged) OnSnapChanged(e.IsChecked());
        });
        m_gridCheck->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent& e) {
            if (OnShowGridChanged) OnShowGridChanged(e.IsChecked());
        });
        fitBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { if (OnFitRequested) OnFitRequested(); });
        m_objects->Bind(wxEVT_LIST_ITEM_SELECTED, [this](wxListEvent& e) {
            long data = m_objects->GetItemData(e.GetIndex());
            if (OnObjectSelected) OnObjectSelected((int)data);
        });
    }

    double GridValue() const
    {
        switch (m_gridChoice->GetSelection()) {
        case 0: return 0.1;
        case 1: return 0.25;
        case 2: return 0.5;
        case 3: return 1.0;
        case 4: return 2.54;
        case 5: return 5.0;
        }
        return 0.5;
    }

    void SyncGrid(double g)
    {
        int sel = 2;
        if (g == 0.1) sel = 0;
        else if (g == 0.25) sel = 1;
        else if (g == 0.5) sel = 2;
        else if (g == 1.0) sel = 3;
        else if (g == 2.54) sel = 4;
        else if (g == 5.0) sel = 5;
        m_gridChoice->SetSelection(sel);
    }

    void SyncChecks(bool snap, bool grid)
    {
        m_snapCheck->SetValue(snap);
        m_gridCheck->SetValue(grid);
    }

    void SetActiveLayer(int layer) { m_layerPanel->SetActive(layer); }
    void SetLayerVisible(int layer, bool visible) { m_layerPanel->SetVisibleState(layer, visible); }

    void SetObjects(const std::vector<ObjectInfo>& infos, int selectedUid = -1)
    {
        m_objects->Freeze();
        m_objects->DeleteAllItems();
        for (size_t i = 0; i < infos.size(); ++i) {
            long idx = m_objects->InsertItem((long)i, infos[i].name);
            m_objects->SetItem(idx, 1, infos[i].layer);
            m_objects->SetItemData(idx, (long)infos[i].uid);
            if (infos[i].uid == selectedUid) {
                m_objects->SetItemState(idx, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
                m_objects->EnsureVisible(idx);
            }
        }
        m_countLabel->SetLabel(wxString::Format("%d", (int)infos.size()));
        m_objects->Thaw();
    }

private:
    LayerPanel* m_layerPanel = nullptr;
    wxChoice* m_gridChoice = nullptr;
    wxCheckBox* m_snapCheck = nullptr;
    wxCheckBox* m_gridCheck = nullptr;
    wxListCtrl* m_objects = nullptr;
    wxStaticText* m_countLabel = nullptr;
};



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



// ============================================================================
// Footprint metadata dialog
// ============================================================================
class FootprintPropertiesDialog : public wxDialog
{
public:
    FootprintPropertiesDialog(wxWindow* parent, const wxString& title, const Footprint* fp)
        : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxSize(480, 430), wxDEFAULT_DIALOG_STYLE)
    {
        wxBoxSizer* main = new wxBoxSizer(wxVERTICAL);

        wxFlexGridSizer* grid = new wxFlexGridSizer(2, 8, 8);
        grid->AddGrowableCol(1, 1);

        grid->Add(new wxStaticText(this, wxID_ANY, "Name:"), 0, wxALIGN_CENTER_VERTICAL);
        wxString initialName = fp ? fp->name : wxString();
        m_name = new wxTextCtrl(this, wxID_ANY, initialName);
        m_name->SetHint("e.g. SOIC-8_3.9x4.9mm");
        grid->Add(m_name, 1, wxEXPAND);

        grid->Add(new wxStaticText(this, wxID_ANY, "Category:"), 0, wxALIGN_CENTER_VERTICAL);
        wxArrayString cats;
        cats.Add("IC / SMD"); cats.Add("IC / THT"); cats.Add("Passive / SMD");
        cats.Add("Transistor / SMD"); cats.Add("Connector / THT"); cats.Add("LED / SMD");
        cats.Add("Test Point"); cats.Add("Mechanical");
        wxString initialCategory = fp ? fp->category : wxString("IC / SMD");
        m_category = new wxComboBox(this, wxID_ANY, initialCategory,
                                    wxDefaultPosition, wxDefaultSize, cats, wxCB_DROPDOWN);
        grid->Add(m_category, 1, wxEXPAND);

        grid->Add(new wxStaticText(this, wxID_ANY, "Default unit:"), 0, wxALIGN_CENTER_VERTICAL);
        wxArrayString units;
        units.Add("mm"); units.Add("mil");
        m_unit = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, units);
        m_unit->SetSelection(fp && fp->unit == "mil" ? 1 : 0);
        grid->Add(m_unit, 0);

        grid->Add(new wxStaticText(this, wxID_ANY, "Keywords:"), 0, wxALIGN_TOP | wxTOP, 4);
        wxString initialKeywords = fp ? fp->keywords : wxString();
        m_keywords = new wxTextCtrl(this, wxID_ANY, initialKeywords);
        m_keywords->SetHint("space separated search terms");
        grid->Add(m_keywords, 1, wxEXPAND);

        main->Add(grid, 0, wxEXPAND | wxALL, 14);

        main->Add(new wxStaticText(this, wxID_ANY, "Description:"), 0, wxLEFT | wxRIGHT, 14);
        wxString initialDescription = fp ? fp->description : wxString();
        m_description = new wxTextCtrl(this, wxID_ANY, initialDescription,
                                       wxDefaultPosition, wxSize(-1, 90), wxTE_MULTILINE);
        main->Add(m_description, 1, wxEXPAND | wxALL, 14);

        wxStaticText* tip = new wxStaticText(this, wxID_ANY,
            "Names must be unique in the library. Default unit is stored as metadata.");
        tip->SetForegroundColour(wxColour(110, 110, 110));
        main->Add(tip, 0, wxLEFT | wxRIGHT | wxBOTTOM, 14);

        main->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxALIGN_RIGHT | wxALL, 12);
        SetSizer(main);
        CenterOnParent();
        m_name->SetFocus();

        Bind(wxEVT_BUTTON, [this](wxCommandEvent& e) {
            if (e.GetId() == wxID_OK) {
                if (TrimStr(m_name->GetValue()).empty()) {
                    wxMessageBox("Footprint name must not be empty.", "Invalid name", wxOK | wxICON_WARNING, this);
                    m_name->SetFocus();
                    return;
                }
            }
            e.Skip();
        }, wxID_OK);
    }

    wxString GetName() const { return TrimStr(m_name->GetValue()); }
    wxString GetCategory() const { return TrimStr(m_category->GetValue()); }
    wxString GetUnit() const { return m_unit->GetSelection() == 1 ? "mil" : "mm"; }
    wxString GetKeywords() const { return TrimStr(m_keywords->GetValue()); }
    wxString GetDescription() const { return TrimStr(m_description->GetValue()); }

    void ApplyTo(Footprint& fp) const
    {
        fp.name = GetName();
        fp.category = GetCategory();
        fp.unit = GetUnit();
        fp.keywords = GetKeywords();
        fp.description = GetDescription();
    }

private:
    wxTextCtrl* m_name = nullptr;
    wxComboBox* m_category = nullptr;
    wxChoice* m_unit = nullptr;
    wxTextCtrl* m_keywords = nullptr;
    wxTextCtrl* m_description = nullptr;
};

// ============================================================================
// Footprint editor frame
// ============================================================================
enum {
    ID_FPE_SELECT = wxID_HIGHEST + 500, ID_FPE_PAD_SMD, ID_FPE_PAD_THT, ID_FPE_VIA,
    ID_FPE_LINE, ID_FPE_RECT, ID_FPE_CIRCLE, ID_FPE_ARC, ID_FPE_POLY, ID_FPE_TEXT,
    ID_FPE_CHECK, ID_FPE_SAVE, ID_FPE_PROPERTIES, ID_FPE_LABELS, ID_FPE_SNAP, ID_FPE_ZOOMFIT,
    ID_FPE_ALIGN_LEFT, ID_FPE_ALIGN_RIGHT, ID_FPE_ALIGN_TOP, ID_FPE_ALIGN_BOTTOM,
    ID_FPE_ALIGN_CENTER_H, ID_FPE_ALIGN_CENTER_V, ID_FPE_ALIGN_DIST_H, ID_FPE_ALIGN_DIST_V,
    ID_FPE_ROT_CW, ID_FPE_ROT_CCW
};

class FootprintEditorFrame : public wxFrame
{
public:
    FootprintEditorFrame(wxWindow* parent, std::unique_ptr<Footprint> fp,
                         std::function<bool(Footprint*, wxString&)> saveCb,
                         std::function<void()> onSaved,
                         std::function<void()> onClosed,
                         bool isNew = false)
        : wxFrame(parent, wxID_ANY, "Footprint Editor", wxDefaultPosition, wxSize(1060, 650)),
          m_fp(std::move(fp)), m_saveCb(std::move(saveCb)), m_onSaved(std::move(onSaved)),
          m_onClosed(std::move(onClosed)), m_dirty(isNew)
    {
        CreateMenu();
        CreateToolbarAndStatus();

        wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);

        wxPanel* meta = new wxPanel(this, wxID_ANY);
        meta->SetBackgroundColour(wxColour(246, 246, 243));
        wxBoxSizer* metaSizer = new wxBoxSizer(wxHORIZONTAL);
        m_metaText = new wxStaticText(meta, wxID_ANY, "");
        wxFont metaFont = m_metaText->GetFont();
        metaFont.SetWeight(wxFONTWEIGHT_BOLD);
        m_metaText->SetFont(metaFont);
        metaSizer->Add(m_metaText, 1, wxALIGN_CENTER_VERTICAL | wxALL, 8);
        wxButton* propBtn = new wxButton(meta, ID_FPE_PROPERTIES, "Edit footprint properties...");
        metaSizer->Add(propBtn, 0, wxALIGN_CENTER_VERTICAL | wxALL, 6);
        meta->SetSizer(metaSizer);
        root->Add(meta, 0, wxEXPAND);

        m_canvas = new FootprintCanvas(this);
        m_props = new FpPropertyPanel(this);

        wxBoxSizer* content = new wxBoxSizer(wxHORIZONTAL);
        content->Add(m_canvas, 70, wxEXPAND);
        content->Add(m_props, 30, wxEXPAND);
        root->Add(content, 1, wxEXPAND);
        SetSizer(root);

        m_canvas->SetFootprint(m_fp.get());
        m_canvas->OnModified = [this]() { MarkDirty(); RefreshSelection(); };
        m_canvas->OnSelectionChanged = [this]() { RefreshSelection(); };
        m_canvas->OnStatus = [this](const wxString& text, const wxString&) { SetStatusText(text, 0); };
        m_canvas->OnCoords = [this](const wxString& c) { SetStatusText(c, 1); };
        m_props->OnModified = [this]() {
            MarkDirty();
            if (m_canvas) { m_canvas->Refresh(); }
            RefreshSelection();
        };

        Bind(wxEVT_TOOL, &FootprintEditorFrame::OnTool, this);
        Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EditProperties(); }, ID_FPE_PROPERTIES);
        Bind(wxEVT_CLOSE_WINDOW, &FootprintEditorFrame::OnClose, this);

        UpdateMetaText();
        UpdateTitle();
        RefreshSelection();
        CallAfter([this]() { if (m_canvas) m_canvas->ZoomFit(); });
        SetStatusText("Ready. Place pads with the SMD/THT pad tools, draw silkscreen, then save.", 0);
        CenterOnParent();
    }

    void MarkDirty()
    {
        if (!m_dirty) {
            m_dirty = true;
            UpdateTitle();
        }
        SetStatusText("Modified", 2);
    }

    bool SaveFootprint()
    {
        wxArrayString errors;
        if (!m_fp->Validate(errors)) {
            ShowErrors("Footprint validation failed", errors);
            return false;
        }
        wxString err;
        if (!m_saveCb || !m_saveCb(m_fp.get(), err)) {
            wxMessageBox(err.empty() ? wxString("The footprint could not be saved.") : err,
                         "Save failed", wxOK | wxICON_ERROR, this);
            return false;
        }
        m_dirty = false;
        UpdateTitle();
        UpdateMetaText();
        SetStatusText("Saved to library", 0);
        SetStatusText("", 2);
        if (m_onSaved) m_onSaved();
        return true;
    }

    bool CheckFootprint()
    {
        wxArrayString errors;
        bool ok = m_fp->Validate(errors);
        if (ok) {
            wxMessageBox("Validation passed. No errors found.", "Footprint check",
                         wxOK | wxICON_INFORMATION, this);
            SetStatusText("Validation passed", 0);
        }
        else {
            ShowErrors("Footprint check found problems", errors);
            SetStatusText(wxString::Format("Validation failed: %d problem(s)", (int)errors.size()), 0);
        }
        return ok;
    }

private:
    std::unique_ptr<Footprint> m_fp;
    FootprintCanvas* m_canvas = nullptr;
    FpPropertyPanel* m_props = nullptr;
    wxStaticText* m_metaText = nullptr;
    std::function<bool(Footprint*, wxString&)> m_saveCb;
    std::function<void()> m_onSaved;
    std::function<void()> m_onClosed;
    bool m_dirty = false;
    bool m_closedNotified = false;

    void CreateMenu()
    {
        wxMenuBar* mb = new wxMenuBar();
        wxMenu* fileMenu = new wxMenu();
        fileMenu->Append(ID_FPE_SAVE, "&Save\tCtrl+S");
        fileMenu->Append(ID_FPE_CHECK, "&Check footprint\tF7");
        fileMenu->AppendSeparator();
        fileMenu->Append(wxID_CLOSE, "&Close\tCtrl+W");
        mb->Append(fileMenu, "&File");

        wxMenu* editMenu = new wxMenu();
        editMenu->Append(wxID_COPY, "&Copy\tCtrl+C");
        editMenu->Append(wxID_PASTE, "&Paste\tCtrl+V");
        editMenu->Append(wxID_DUPLICATE, "D&uplicate\tCtrl+D");
        editMenu->Append(wxID_DELETE, "&Delete\tDel");
        editMenu->Append(wxID_SELECTALL, "Select &all\tCtrl+A");
        editMenu->AppendSeparator();
        wxMenu* align = new wxMenu();
        align->Append(ID_FPE_ALIGN_LEFT, "Align left");
        align->Append(ID_FPE_ALIGN_RIGHT, "Align right");
        align->Append(ID_FPE_ALIGN_TOP, "Align top");
        align->Append(ID_FPE_ALIGN_BOTTOM, "Align bottom");
        align->Append(ID_FPE_ALIGN_CENTER_H, "Center horizontally");
        align->Append(ID_FPE_ALIGN_CENTER_V, "Center vertically");
        align->AppendSeparator();
        align->Append(ID_FPE_ALIGN_DIST_H, "Distribute horizontally");
        align->Append(ID_FPE_ALIGN_DIST_V, "Distribute vertically");
        editMenu->AppendSubMenu(align, "&Align");
        editMenu->Append(ID_FPE_ROT_CW, "Rotate 90 deg clockwise");
        editMenu->Append(ID_FPE_ROT_CCW, "Rotate 90 deg counter-clockwise");
        mb->Append(editMenu, "&Edit");

        wxMenu* viewMenu = new wxMenu();
        viewMenu->AppendCheckItem(ID_FPE_LABELS, "Show pad numbers");
        viewMenu->Check(ID_FPE_LABELS, true);
        viewMenu->AppendCheckItem(ID_FPE_SNAP, "Snap to grid");
        viewMenu->Check(ID_FPE_SNAP, true);
        viewMenu->Append(ID_FPE_ZOOMFIT, "Zoom to fit\tCtrl+0");
        mb->Append(viewMenu, "&View");

        wxMenu* fpMenu = new wxMenu();
        fpMenu->Append(ID_FPE_PROPERTIES, "&Properties...");
        fpMenu->AppendSeparator();
        fpMenu->Append(ID_FPE_CHECK, "&Check footprint\tF7");
        mb->Append(fpMenu, "&Footprint");

        SetMenuBar(mb);
    }

    void CreateToolbarAndStatus()
    {
        wxToolBar* tb = CreateToolBar(wxTB_HORIZONTAL | wxTB_FLAT | wxTB_TEXT);
        tb->AddRadioTool(ID_FPE_SELECT, "Select", MakeToolIcon("select"));
        tb->AddSeparator();
        tb->AddRadioTool(ID_FPE_PAD_SMD, "SMD Pad", MakeToolIcon("pad"));
        tb->AddRadioTool(ID_FPE_PAD_THT, "THT Pad", MakeToolIcon("via"));
        tb->AddRadioTool(ID_FPE_VIA, "Via", MakeToolIcon("via"));
        tb->AddSeparator();
        tb->AddRadioTool(ID_FPE_LINE, "Line", MakeToolIcon("line"));
        tb->AddRadioTool(ID_FPE_RECT, "Rect", MakeToolIcon("rect"));
        tb->AddRadioTool(ID_FPE_CIRCLE, "Circle", MakeToolIcon("circle"));
        tb->AddRadioTool(ID_FPE_ARC, "Arc", MakeToolIcon("arc"));
        tb->AddRadioTool(ID_FPE_POLY, "Polygon", MakeToolIcon("poly"));
        tb->AddRadioTool(ID_FPE_TEXT, "Text", MakeToolIcon("text"));
        tb->AddSeparator();
        tb->AddTool(ID_FPE_CHECK, "Check", MakeToolIcon("check"));
        tb->AddTool(ID_FPE_SAVE, "Save", MakeToolIcon("save"));
        tb->Realize();
        tb->ToggleTool(ID_FPE_SELECT, true);

        CreateStatusBar(3);
        int widths[3] = { 520, 180, -1 };
        SetStatusWidths(3, widths);
        SetStatusText("Ready", 0);
    }

    void UpdateTitle()
    {
        wxString title = "Footprint Editor - " + (m_fp ? m_fp->name : wxString("(unnamed)"));
        if (m_dirty) title += " *";
        SetTitle(title);
    }

    void UpdateMetaText()
    {
        if (!m_metaText || !m_fp) return;
        wxString text = m_fp->name;
        if (!m_fp->category.empty()) text += "   |   " + m_fp->category;
        text += "   |   unit: " + m_fp->unit;
        text += wxString::Format("   |   %d pads", (int)m_fp->pads.size());
        if (!m_fp->description.empty()) text += "   |   " + m_fp->description;
        m_metaText->SetLabel(text);
    }

    void RefreshSelection()
    {
        if (!m_props || !m_canvas) return;
        Pad* pad = m_canvas->GetSelectedPad();
        Shape* shape = m_canvas->GetSelectedShape();
        m_props->Update(pad, shape, m_canvas->SelectionCount());
        SetStatusText(m_canvas->SelectionDescription(), 2);
    }

    void ShowErrors(const wxString& title, const wxArrayString& errors)
    {
        wxString text;
        for (size_t i = 0; i < errors.size() && i < 12; ++i) text += "- " + errors[i] + "\n";
        if (errors.size() > 12) text += wxString::Format("... and %d more.\n", (int)errors.size() - 12);
        wxMessageDialog dlg(this, text, title, wxOK | wxICON_WARNING);
        dlg.ShowModal();
    }

    void EditProperties()
    {
        FootprintPropertiesDialog dlg(this, "Footprint Properties", m_fp.get());
        if (dlg.ShowModal() == wxID_OK) {
            dlg.ApplyTo(*m_fp);
            MarkDirty();
            UpdateMetaText();
            UpdateTitle();
            RefreshSelection();
            m_canvas->Refresh();
        }
    }

    void OnTool(wxCommandEvent& e)
    {
        int id = e.GetId();
        if (id == ID_FPE_CHECK) { CheckFootprint(); return; }
        if (id == ID_FPE_SAVE) { SaveFootprint(); return; }
        if (id == ID_FPE_SELECT) m_canvas->SetTool(FP_SELECT);
        else if (id == ID_FPE_PAD_SMD) m_canvas->SetTool(FP_PAD_SMD);
        else if (id == ID_FPE_PAD_THT) m_canvas->SetTool(FP_PAD_THT);
        else if (id == ID_FPE_VIA) m_canvas->SetTool(FP_VIA);
        else if (id == ID_FPE_LINE) m_canvas->SetTool(FP_LINE);
        else if (id == ID_FPE_RECT) m_canvas->SetTool(FP_RECT);
        else if (id == ID_FPE_CIRCLE) m_canvas->SetTool(FP_CIRCLE);
        else if (id == ID_FPE_ARC) m_canvas->SetTool(FP_ARC);
        else if (id == ID_FPE_POLY) m_canvas->SetTool(FP_POLY);
        else if (id == ID_FPE_TEXT) m_canvas->SetTool(FP_TEXT);
        else if (id == ID_FPE_LABELS) { m_canvas->SetShowLabels(e.IsChecked()); return; }
        else if (id == ID_FPE_SNAP) { m_canvas->SetSnap(e.IsChecked()); return; }
        else if (id == ID_FPE_ZOOMFIT) { m_canvas->ZoomFit(); return; }

        wxString hints[] = {
            "Select and edit objects", "Click to place an SMD pad", "Click to place a through-hole pad",
            "Click to place a via", "Drag to draw a silkscreen line", "Drag to draw a silkscreen rectangle",
            "Drag to draw a silkscreen circle", "Click centre, edge, then end point",
            "Click to add points; double-click or Enter to finish", "Click to place text"
        };
        SetStatusText(hints[id - ID_FPE_SELECT], 0);
    }

    void OnClose(wxCloseEvent& e)
    {
        if (m_dirty) {
            int answer = wxMessageBox("Save changes to footprint '" + m_fp->name + "'?",
                                      "Close footprint editor", wxYES_NO | wxCANCEL | wxICON_QUESTION, this);
            if (answer == wxCANCEL) { e.Veto(); return; }
            if (answer == wxYES && !SaveFootprint()) { e.Veto(); return; }
        }
        if (!m_closedNotified && m_onClosed) {
            m_closedNotified = true;
            m_onClosed();
        }
        Destroy();
    }
};








// ============================================================================
// Component library manager (Task D)
// ============================================================================
enum {
    ID_LM_NEW_LIB = wxID_HIGHEST + 700, ID_LM_OPEN_LIB, ID_LM_SAVE_LIB, ID_LM_SAVEAS_LIB,
    ID_LM_NEW_FP, ID_LM_EDIT_FP, ID_LM_RENAME_FP, ID_LM_COPY_FP, ID_LM_DELETE_FP,
    ID_LM_CHECK_FP, ID_LM_PLACE_FP, ID_LM_REFRESH
};

class LibraryManagerFrame : public wxFrame
{
public:
    LibraryManagerFrame(wxWindow* parent, std::shared_ptr<Library> lib,
                        std::function<bool(Footprint*, wxString&)> commitCb,
                        std::function<int(const wxString&)> refCountCb,
                        std::function<bool(const wxString&)> placeCb,
                        std::function<void()> onLibraryChanged,
                        std::function<void()> onClosed)
        : wxFrame(parent, wxID_ANY, "Component Library Manager", wxDefaultPosition, wxSize(1120, 650)),
          m_lib(std::move(lib)), m_commitCb(std::move(commitCb)), m_refCountCb(std::move(refCountCb)),
          m_placeCb(std::move(placeCb)), m_onChanged(std::move(onLibraryChanged)),
          m_onClosed(std::move(onClosed))
    {
        CreateMenu();
        CreateToolbarAndStatus();
        BuildUi();
        Bind(wxEVT_TOOL, &LibraryManagerFrame::OnTool, this);
        Bind(wxEVT_MENU, &LibraryManagerFrame::OnMenu, this);
        Bind(wxEVT_CLOSE_WINDOW, &LibraryManagerFrame::OnClose, this);
        RefreshAll();
        CenterOnParent();
    }

    void RefreshAll()
    {
        UpdateTitle();
        RefreshCategories();
        RefreshList();
        UpdatePreview();
        UpdateStatus();
    }

    bool HasEditor() const { return m_editor != nullptr; }

    bool SaveLibraryNow() { return SaveLibrary(false); }

    void SelectFootprintByName(const wxString& name)
    {
        if (!m_list) return;
        for (long i = 0; i < m_list->GetItemCount(); ++i) {
            long libIndex = m_list->GetItemData(i);
            if (libIndex >= 0 && libIndex < (long)m_lib->items.size() &&
                m_lib->items[libIndex]->name.IsSameAs(name, false)) {
                m_list->SetItemState(i, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                                     wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
                m_list->EnsureVisible(i);
                UpdatePreview();
                return;
            }
        }
    }

    void EditByName(const wxString& name)
    {
        if (m_editor) { m_editor->Raise(); return; }
        SelectFootprintByName(name);
        EditSelected();
    }

private:
    std::shared_ptr<Library> m_lib;
    std::function<bool(Footprint*, wxString&)> m_commitCb;
    std::function<int(const wxString&)> m_refCountCb;
    std::function<bool(const wxString&)> m_placeCb;
    std::function<void()> m_onChanged;
    std::function<void()> m_onClosed;

    wxTextCtrl* m_search = nullptr;
    wxChoice* m_categoryFilter = nullptr;
    wxListCtrl* m_list = nullptr;
    FootprintPreviewPanel* m_preview = nullptr;
    wxTextCtrl* m_info = nullptr;
    wxButton* m_editBtn = nullptr;
    wxButton* m_placeBtn = nullptr;
    FootprintEditorFrame* m_editor = nullptr;
    long m_selectedLibIndex = -1;
    bool m_refreshingCategories = false;

    void CreateMenu()
    {
        wxMenuBar* mb = new wxMenuBar();
        wxMenu* fileMenu = new wxMenu();
        fileMenu->Append(ID_LM_NEW_LIB, "&New library\tCtrl+N");
        fileMenu->Append(ID_LM_OPEN_LIB, "&Open library...\tCtrl+O");
        fileMenu->AppendSeparator();
        fileMenu->Append(ID_LM_SAVE_LIB, "&Save\tCtrl+S");
        fileMenu->Append(ID_LM_SAVEAS_LIB, "Save &as...");
        fileMenu->AppendSeparator();
        fileMenu->Append(wxID_CLOSE, "&Close\tCtrl+W");
        mb->Append(fileMenu, "&File");

        wxMenu* editMenu = new wxMenu();
        editMenu->Append(ID_LM_NEW_FP, "New &footprint...\tCtrl+Shift+N");
        editMenu->Append(ID_LM_EDIT_FP, "&Edit footprint...\tEnter");
        editMenu->Append(ID_LM_RENAME_FP, "&Rename...\tF2");
        editMenu->Append(ID_LM_COPY_FP, "&Copy...\tCtrl+D");
        editMenu->Append(ID_LM_DELETE_FP, "De&lete\tDel");
        editMenu->AppendSeparator();
        editMenu->Append(ID_LM_CHECK_FP, "&Check footprint\tF7");
        editMenu->Append(ID_LM_PLACE_FP, "&Place on board\tCtrl+P");
        mb->Append(editMenu, "&Edit");

        wxMenu* helpMenu = new wxMenu();
        helpMenu->Append(wxID_ABOUT, "&About...");
        mb->Append(helpMenu, "&Help");
        SetMenuBar(mb);
    }

    void CreateToolbarAndStatus()
    {
        wxToolBar* tb = CreateToolBar(wxTB_HORIZONTAL | wxTB_FLAT | wxTB_TEXT);
        tb->AddTool(ID_LM_NEW_LIB, "New Library", MakeToolIcon("new"));
        tb->AddTool(ID_LM_OPEN_LIB, "Open", MakeToolIcon("open"));
        tb->AddTool(ID_LM_SAVE_LIB, "Save", MakeToolIcon("save"));
        tb->AddTool(ID_LM_SAVEAS_LIB, "Save As", MakeToolIcon("save"));
        tb->AddSeparator();
        tb->AddTool(ID_LM_NEW_FP, "New Footprint", MakeToolIcon("pad"));
        tb->AddTool(ID_LM_EDIT_FP, "Edit", MakeToolIcon("select"));
        tb->AddTool(ID_LM_RENAME_FP, "Rename", MakeToolIcon("text"));
        tb->AddTool(ID_LM_COPY_FP, "Copy", MakeToolIcon("library"));
        tb->AddTool(ID_LM_DELETE_FP, "Delete", MakeToolIcon("delete"));
        tb->AddTool(ID_LM_CHECK_FP, "Check", MakeToolIcon("check"));
        tb->AddSeparator();
        tb->AddTool(ID_LM_PLACE_FP, "Place on Board", MakeToolIcon("fit"));
        tb->Realize();

        CreateStatusBar(2);
        int widths[2] = { -1, 420 };
        SetStatusWidths(2, widths);
    }

    void BuildUi()
    {
        wxPanel* panel = new wxPanel(this, wxID_ANY);
        wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);

        wxPanel* searchPanel = new wxPanel(panel, wxID_ANY);
        wxBoxSizer* searchSizer = new wxBoxSizer(wxHORIZONTAL);
        searchSizer->Add(new wxStaticText(searchPanel, wxID_ANY, "Search:"), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
        m_search = new wxTextCtrl(searchPanel, wxID_ANY, "");
        m_search->SetHint("name, category, keyword or description");
        searchSizer->Add(m_search, 1, wxEXPAND | wxALL, 6);
        searchSizer->Add(new wxStaticText(searchPanel, wxID_ANY, "Category:"), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 6);
        m_categoryFilter = new wxChoice(searchPanel, wxID_ANY);
        m_categoryFilter->Append("All categories");
        m_categoryFilter->SetSelection(0);
        searchSizer->Add(m_categoryFilter, 0, wxALIGN_CENTER_VERTICAL | wxALL, 6);
        searchPanel->SetSizer(searchSizer);
        root->Add(searchPanel, 0, wxEXPAND);

        wxSplitterWindow* splitter = new wxSplitterWindow(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                                          wxSP_LIVE_UPDATE | wxSP_3D);
        splitter->SetMinimumPaneSize(350);

        m_list = new wxListCtrl(splitter, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_NONE);
        m_list->AppendColumn("Footprint", wxLIST_FORMAT_LEFT, 170);
        m_list->AppendColumn("Category", wxLIST_FORMAT_LEFT, 120);
        m_list->AppendColumn("Pads", wxLIST_FORMAT_RIGHT, 45);
        m_list->AppendColumn("Summary", wxLIST_FORMAT_LEFT, 230);

        wxPanel* right = new wxPanel(splitter, wxID_ANY);
        wxBoxSizer* rightSizer = new wxBoxSizer(wxVERTICAL);
        m_preview = new FootprintPreviewPanel(right);
        rightSizer->Add(m_preview, 1, wxEXPAND | wxALL, 8);

        m_info = new wxTextCtrl(right, wxID_ANY, "", wxDefaultPosition, wxSize(-1, 150),
                                wxTE_MULTILINE | wxTE_READONLY | wxBORDER_SIMPLE);
        rightSizer->Add(m_info, 0, wxEXPAND | wxLEFT | wxRIGHT, 8);

        wxBoxSizer* buttonRow = new wxBoxSizer(wxHORIZONTAL);
        m_editBtn = new wxButton(right, ID_LM_EDIT_FP, "Edit footprint");
        m_placeBtn = new wxButton(right, ID_LM_PLACE_FP, "Place on board");
        buttonRow->Add(m_editBtn, 1, wxRIGHT, 6);
        buttonRow->Add(m_placeBtn, 1);
        rightSizer->Add(buttonRow, 0, wxEXPAND | wxALL, 8);
        right->SetSizer(rightSizer);

        splitter->SplitVertically(m_list, right, 580);
        root->Add(splitter, 1, wxEXPAND | wxALL, 4);
        panel->SetSizer(root);

        m_search->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { RefreshList(); });
        m_categoryFilter->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { RefreshList(); });
        m_list->Bind(wxEVT_LIST_ITEM_SELECTED, [this](wxListEvent&) { UpdatePreview(); });
        m_list->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent&) { EditSelected(); });
        m_list->Bind(wxEVT_LIST_KEY_DOWN, [this](wxListEvent& e) {
            if (e.GetKeyCode() == WXK_DELETE) DeleteSelected();
            else if (e.GetKeyCode() == WXK_F2) RenameSelected();
            else e.Skip();
        });
        m_editBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EditSelected(); });
        m_placeBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { PlaceSelected(); });
    }

    void UpdateTitle()
    {
        wxString title = "Component Library Manager - " + m_lib->name;
        if (m_lib->modified) title += " *";
        SetTitle(title);
    }

    void UpdateStatus()
    {
        int shown = (int)m_list->GetItemCount();
        SetStatusText(wxString::Format("%d of %d footprints  |  library: %s",
            shown, (int)m_lib->items.size(), m_lib->name), 0);
        SetStatusText(m_lib->filePath.empty() ? wxString("Not saved yet") : m_lib->filePath, 1);
    }

    void RefreshCategories()
    {
        m_refreshingCategories = true;
        wxString current = m_categoryFilter->GetSelection() > 0 ? m_categoryFilter->GetStringSelection() : wxString();
        m_categoryFilter->Clear();
        m_categoryFilter->Append("All categories");
        wxArrayString cats = m_lib->Categories();
        for (const auto& c : cats) m_categoryFilter->Append(c);
        int sel = 0;
        if (!current.empty()) {
            int idx = m_categoryFilter->FindString(current);
            if (idx != wxNOT_FOUND) sel = idx;
        }
        m_categoryFilter->SetSelection(sel);
        m_refreshingCategories = false;
    }

    bool MatchesFilter(const Footprint& fp, const wxString& query, const wxString& category) const
    {
        if (!category.empty() && !fp.category.IsSameAs(category, false)) return false;
        if (query.empty()) return true;
        wxString haystack = fp.name + " " + fp.category + " " + fp.keywords + " " + fp.description;
        haystack.MakeLower();
        return haystack.Contains(query);
    }

    void RefreshList()
    {
        if (!m_list) return;
        wxString query = TrimStr(m_search->GetValue()).Lower();
        wxString category;
        if (m_categoryFilter->GetSelection() > 0) category = m_categoryFilter->GetStringSelection();

        wxString selectedUid;
        if (m_selectedLibIndex >= 0 && m_selectedLibIndex < (long)m_lib->items.size())
            selectedUid = m_lib->items[m_selectedLibIndex]->uid;

        m_list->Freeze();
        m_list->DeleteAllItems();
        m_selectedLibIndex = -1;
        long row = 0;
        for (size_t i = 0; i < m_lib->items.size(); ++i) {
            const Footprint& fp = *m_lib->items[i];
            if (!MatchesFilter(fp, query, category)) continue;
            long idx = m_list->InsertItem(row, fp.name);
            m_list->SetItem(idx, 1, fp.category);
            m_list->SetItem(idx, 2, wxString::Format("%d", (int)fp.pads.size()));
            m_list->SetItem(idx, 3, fp.description);
            m_list->SetItemData(idx, (long)i);
            if (!selectedUid.empty() && fp.uid == selectedUid) {
                m_list->SetItemState(idx, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                                     wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
                m_selectedLibIndex = (long)i;
            }
            ++row;
        }

        if (m_list->GetItemCount() > 0 && m_selectedLibIndex < 0) {
            m_list->SetItemState(0, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                                 wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
            m_selectedLibIndex = m_list->GetItemData(0);
        }
        m_list->Thaw();
        UpdatePreview();
        UpdateStatus();
    }

    Footprint* SelectedFootprint() const
    {
        if (!m_lib || m_selectedLibIndex < 0 || m_selectedLibIndex >= (long)m_lib->items.size()) return nullptr;
        return m_lib->items[m_selectedLibIndex].get();
    }

    std::shared_ptr<Footprint> SelectedShared() const
    {
        if (m_selectedLibIndex < 0 || m_selectedLibIndex >= (long)m_lib->items.size()) return nullptr;
        return m_lib->items[m_selectedLibIndex];
    }

    void UpdatePreview()
    {
        if (!m_list) return;
        long sel = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (sel == -1) {
            m_selectedLibIndex = -1;
            m_preview->SetFootprint(nullptr);
            m_info->SetValue("No footprint selected.");
            if (m_editBtn) m_editBtn->Enable(false);
            if (m_placeBtn) m_placeBtn->Enable(false);
            return;
        }
        m_selectedLibIndex = m_list->GetItemData(sel);
        if (m_selectedLibIndex < 0 || m_selectedLibIndex >= (long)m_lib->items.size()) return;
        std::shared_ptr<Footprint> fp = m_lib->items[m_selectedLibIndex];
        m_preview->SetFootprint(fp);
        wxString info;
        info << "Name:       " << fp->name << "\n";
        info << "Category:   " << (fp->category.empty() ? wxString("Uncategorised") : fp->category) << "\n";
        info << "Unit:       " << fp->unit << "\n";
        info << "Pads:       " << (int)fp->pads.size() << "\n";
        info << "Size:       " << fp->SizeText() << "\n";
        info << "Keywords:   " << fp->keywords << "\n";
        info << "UID:        " << fp->uid << "\n";
        if (!fp->description.empty()) info << "Description: " << fp->description << "\n";
        m_info->SetValue(info);
        m_editBtn->Enable(true);
        m_placeBtn->Enable(true);
    }

    bool ConfirmDiscardChanges()
    {
        if (!m_lib->modified) return true;
        int answer = wxMessageBox("Save changes to library '" + m_lib->name + "'?",
                                  "Unsaved changes", wxYES_NO | wxCANCEL | wxICON_QUESTION, this);
        if (answer == wxCANCEL) return false;
        if (answer == wxYES) return SaveLibrary(false);
        return true;
    }

    bool ValidateLibrary(wxArrayString& errors)
    {
        errors.Clear();
        std::set<wxString> names, uids;
        for (const auto& fp : m_lib->items) {
            wxArrayString fpErrors;
            fp->Validate(fpErrors);
            for (const auto& e : fpErrors) errors.Add(fp->name + ": " + e);
            wxString lowerName = fp->name.Lower();
            if (names.count(lowerName)) errors.Add("Duplicate footprint name: " + fp->name);
            names.insert(lowerName);
            if (uids.count(fp->uid)) errors.Add("Duplicate footprint identifier: " + fp->uid);
            uids.insert(fp->uid);
        }
        return errors.empty();
    }

    bool SaveLibrary(bool forceDialog)
    {
        if (m_lib->items.empty()) {
            wxMessageBox("The library does not contain any footprints.", "Nothing to save",
                         wxOK | wxICON_WARNING, this);
            return false;
        }
        wxArrayString errors;
        if (!ValidateLibrary(errors)) {
            wxString text;
            for (size_t i = 0; i < errors.size() && i < 15; ++i) text += "- " + errors[i] + "\n";
            if (errors.size() > 15) text += wxString::Format("... and %d more.\n", (int)errors.size() - 15);
            wxMessageDialog dlg(this, "The library contains problems and was not saved:\n\n" + text,
                                "Library validation failed", wxOK | wxICON_ERROR);
            dlg.ShowModal();
            return false;
        }

        wxString path = m_lib->filePath;
        if (forceDialog || path.empty()) {
            wxFileDialog dlg(this, "Save Component Library", "", path,
                             "EDA library (*.elib)|*.elib|XML files (*.xml)|*.xml|All files (*.*)|*.*",
                             wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
            if (dlg.ShowModal() != wxID_OK) return false;
            path = dlg.GetPath();
            if (wxFileName(path).GetExt().empty()) path += ".elib";
        }

        wxString err;
        if (!m_lib->Save(path, err)) {
            wxMessageBox(err, "Save failed", wxOK | wxICON_ERROR, this);
            return false;
        }
        m_lib->modified = false;
        RefreshAll();
        if (m_onChanged) m_onChanged();
        SetStatusText("Saved: " + path, 1);
        return true;
    }

    void NewLibrary()
    {
        if (!ConfirmDiscardChanges()) return;
        int answer = wxMessageBox("Create a library with the built-in sample footprints?\n\n"
                                  "Yes = sample library, No = empty library, Cancel = abort.",
                                  "New library", wxYES_NO | wxCANCEL | wxICON_QUESTION, this);
        if (answer == wxCANCEL) return;

        wxString name;
        if (answer == wxYES) {
            auto sample = Library::CreateSample();
            name = sample->name;
        }
        else {
            name = "Untitled Library";
        }
        wxTextEntryDialog nameDlg(this, "Library name:", "New library", name);
        if (nameDlg.ShowModal() != wxID_OK) return;
        name = TrimStr(nameDlg.GetValue());
        if (name.empty()) name = "Untitled Library";

        if (answer == wxYES) {
            auto sample = Library::CreateSample();
            sample->name = name;
            *m_lib = *sample;
        }
        else {
            Library fresh;
            fresh.name = name;
            *m_lib = fresh;
        }
        m_lib->modified = true;
        m_lib->filePath.clear();
        m_selectedLibIndex = -1;
        RefreshAll();
        if (m_onChanged) m_onChanged();
    }

    void OpenLibrary()
    {
        if (!ConfirmDiscardChanges()) return;
        wxFileDialog dlg(this, "Open Component Library", "", "",
                         "EDA library (*.elib)|*.elib|XML files (*.xml)|*.xml|All files (*.*)|*.*",
                         wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dlg.ShowModal() != wxID_OK) return;

        Library tmp;
        wxString warnings;
        if (!tmp.Load(dlg.GetPath(), warnings)) {
            wxMessageBox(warnings, "Open failed", wxOK | wxICON_ERROR, this);
            return;
        }
        *m_lib = tmp;
        m_selectedLibIndex = -1;
        RefreshAll();
        if (m_onChanged) m_onChanged();
        if (!warnings.empty()) {
            wxMessageBox("Library loaded with warnings:\n\n" + warnings, "Library loaded",
                         wxOK | wxICON_INFORMATION, this);
        }
        SetStatusText("Opened: " + m_lib->filePath, 1);
    }

    void OpenEditor(std::unique_ptr<Footprint> fp, bool isNew)
    {
        if (m_editor) {
            m_editor->Raise();
            return;
        }
        m_editor = new FootprintEditorFrame(this, std::move(fp),
            m_commitCb,
            [this]() {
                RefreshAll();
                if (m_onChanged) m_onChanged();
            },
            [this]() {
                m_editor = nullptr;
                RefreshAll();
            },
            isNew);
        m_editor->Show();
    }

    void NewFootprint()
    {
        if (m_editor) { m_editor->Raise(); return; }
        FootprintPropertiesDialog dlg(this, "New Footprint", nullptr);
        if (dlg.ShowModal() != wxID_OK) return;
        wxString name = dlg.GetName();
        if (name.empty()) return;
        if (m_lib->NameExists(name)) {
            wxMessageBox("A footprint named '" + name + "' already exists in this library.",
                         "Duplicate name", wxOK | wxICON_WARNING, this);
            return;
        }
        auto fp = std::make_unique<Footprint>();
        fp->uid = Library::NewUid();
        dlg.ApplyTo(*fp);
        OpenEditor(std::move(fp), true);
    }

    void EditSelected()
    {
        if (m_editor) { m_editor->Raise(); return; }
        std::shared_ptr<Footprint> selected = SelectedShared();
        if (!selected) return;
        OpenEditor(std::make_unique<Footprint>(*selected), false);
    }

    void RenameSelected()
    {
        if (m_editor) { m_editor->Raise(); return; }
        Footprint* fp = SelectedFootprint();
        if (!fp) return;
        wxString oldName = fp->name;
        wxTextEntryDialog dlg(this, "New footprint name:", "Rename footprint", oldName);
        if (dlg.ShowModal() != wxID_OK) return;
        wxString newName = TrimStr(dlg.GetValue());
        if (newName.empty()) {
            wxMessageBox("Footprint name must not be empty.", "Invalid name", wxOK | wxICON_WARNING, this);
            return;
        }
        if (newName.IsSameAs(oldName, false)) return;
        if (m_lib->NameExists(newName, fp->uid)) {
            wxMessageBox("A footprint named '" + newName + "' already exists in this library.",
                         "Duplicate name", wxOK | wxICON_WARNING, this);
            return;
        }
        int refs = m_refCountCb ? m_refCountCb(oldName) : 0;
        if (refs > 0) {
            int answer = wxMessageBox(wxString::Format(
                "This footprint is referenced by %d component instance(s) on the board.\n"
                "Renaming it will update those references. Continue?", refs),
                "Footprint is in use", wxYES_NO | wxICON_WARNING, this);
            if (answer != wxYES) return;
        }

        Footprint copy = *fp;
        copy.name = newName;
        wxString err;
        if (!m_commitCb || !m_commitCb(&copy, err)) {
            wxMessageBox(err.empty() ? wxString("Rename failed.") : err, "Rename failed", wxOK | wxICON_ERROR, this);
            return;
        }
        RefreshAll();
        if (m_onChanged) m_onChanged();
        SetStatusText("Renamed '" + oldName + "' to '" + newName + "'", 0);
    }

    void CopySelected()
    {
        if (m_editor) { m_editor->Raise(); return; }
        Footprint* fp = SelectedFootprint();
        if (!fp) return;
        wxString suggestion = m_lib->MakeUniqueName(fp->name + "_copy");
        wxTextEntryDialog dlg(this, "Name for the copied footprint:", "Copy footprint", suggestion);
        if (dlg.ShowModal() != wxID_OK) return;
        wxString name = TrimStr(dlg.GetValue());
        if (name.empty()) {
            wxMessageBox("Footprint name must not be empty.", "Invalid name", wxOK | wxICON_WARNING, this);
            return;
        }
        if (m_lib->NameExists(name)) {
            wxMessageBox("A footprint named '" + name + "' already exists in this library.",
                         "Duplicate name", wxOK | wxICON_WARNING, this);
            return;
        }
        Footprint copy = *fp;
        copy.uid = Library::NewUid();
        copy.name = name;
        wxString err;
        if (!m_commitCb || !m_commitCb(&copy, err)) {
            wxMessageBox(err.empty() ? wxString("Copy failed.") : err, "Copy failed", wxOK | wxICON_ERROR, this);
            return;
        }
        RefreshAll();
        if (m_onChanged) m_onChanged();
        SetStatusText("Copied footprint to '" + name + "'", 0);
    }

    void DeleteSelected()
    {
        if (m_editor) { m_editor->Raise(); return; }
        Footprint* fp = SelectedFootprint();
        if (!fp) return;
        int refs = m_refCountCb ? m_refCountCb(fp->name) : 0;
        wxString message = "Delete footprint '" + fp->name + "'?";
        if (refs > 0) {
            message += wxString::Format("\n\nWARNING: %d component instance(s) on the board reference this footprint.\n"
                                        "They will be shown as missing after deletion.", refs);
        }
        int answer = wxMessageBox(message, "Delete footprint", wxYES_NO | wxICON_WARNING, this);
        if (answer != wxYES) return;

        wxString uid = fp->uid;
        m_lib->items.erase(std::remove_if(m_lib->items.begin(), m_lib->items.end(),
            [&uid](const std::shared_ptr<Footprint>& f) { return f->uid == uid; }), m_lib->items.end());
        m_lib->modified = true;
        m_selectedLibIndex = -1;
        RefreshAll();
        if (m_onChanged) m_onChanged();
        SetStatusText("Deleted footprint '" + uid + "'", 0);
    }

    void CheckSelected()
    {
        Footprint* fp = SelectedFootprint();
        if (!fp) return;
        wxArrayString errors;
        fp->Validate(errors);
        if (m_lib->NameExists(fp->name, fp->uid)) errors.Add("Another footprint already uses this name.");
        if (errors.empty()) {
            wxMessageBox("Footprint '" + fp->name + "' passed validation.", "Footprint check",
                         wxOK | wxICON_INFORMATION, this);
        }
        else {
            wxString text;
            for (const auto& e : errors) text += "- " + e + "\n";
            wxMessageBox("Problems found:\n\n" + text, "Footprint check", wxOK | wxICON_WARNING, this);
        }
    }

    void PlaceSelected()
    {
        if (m_editor) { m_editor->Raise(); return; }
        Footprint* fp = SelectedFootprint();
        if (!fp) return;
        if (m_placeCb && m_placeCb(fp->name)) {
            SetStatusText("Placed '" + fp->name + "' on the board", 0);
            wxYield();
            Iconize(true);
        }
    }

    void OnTool(wxCommandEvent& e)
    {
        switch (e.GetId()) {
        case ID_LM_NEW_LIB: NewLibrary(); break;
        case ID_LM_OPEN_LIB: OpenLibrary(); break;
        case ID_LM_SAVE_LIB: SaveLibrary(false); break;
        case ID_LM_SAVEAS_LIB: SaveLibrary(true); break;
        case ID_LM_NEW_FP: NewFootprint(); break;
        case ID_LM_EDIT_FP: EditSelected(); break;
        case ID_LM_RENAME_FP: RenameSelected(); break;
        case ID_LM_COPY_FP: CopySelected(); break;
        case ID_LM_DELETE_FP: DeleteSelected(); break;
        case ID_LM_CHECK_FP: CheckSelected(); break;
        case ID_LM_PLACE_FP: PlaceSelected(); break;
        default: e.Skip(); break;
        }
    }

    void OnMenu(wxCommandEvent& e)
    {
        switch (e.GetId()) {
        case wxID_CLOSE: Close(); break;
        case wxID_ABOUT:
            wxMessageBox("Component Library Manager\n\n"
                         "Create, search, preview and maintain footprint libraries.\n"
                         "Each footprint has a unique name and identifier and can be checked\n"
                         "for duplicate pin numbers, invalid dimensions and missing pads.",
                         "About", wxOK | wxICON_INFORMATION, this);
            break;
        default: OnTool(e); break;
        }
    }

    void OnClose(wxCloseEvent& e)
    {
        if (m_editor) {
            m_editor->Close();
            if (m_editor) { e.Veto(); return; }
        }
        if (!ConfirmDiscardChanges()) { e.Veto(); return; }
        if (m_onClosed) m_onClosed();
        Destroy();
    }
};




// ============================================================================
// Main application frame
// ============================================================================
enum {
    ID_T_SEL = wxID_HIGHEST + 1000, ID_T_LINE, ID_T_RECT, ID_T_CIRCLE, ID_T_ARC,
    ID_T_POLY, ID_T_TEXT, ID_T_GRID, ID_T_SNAP, ID_T_ROT_CW, ID_T_ROT_CCW, ID_T_DELETE,
    ID_TOOLS_LIB, ID_TOOLS_FPE,
    ID_M_ALIGN_LEFT = wxID_HIGHEST + 1100, ID_M_ALIGN_RIGHT, ID_M_ALIGN_TOP, ID_M_ALIGN_BOTTOM,
    ID_M_ALIGN_CENTER_H, ID_M_ALIGN_CENTER_V, ID_M_ALIGN_DIST_H, ID_M_ALIGN_DIST_V,
    ID_M_FRONT, ID_M_BACK, ID_M_DEMO,
    ID_V_GRID = wxID_HIGHEST + 1200, ID_V_SNAP, ID_V_ZOOM_IN, ID_V_ZOOM_OUT, ID_V_ZOOM_FIT
};

class MainFrame : public wxFrame
{
public:
    MainFrame()
        : wxFrame(nullptr, wxID_ANY, "EDA Editor - PCB Design", wxDefaultPosition, wxSize(1200, 650))
    {
        m_library = Library::CreateSample();
        g_library = m_library.get();

        CreateMenuBar();
        CreateToolbar();

        CreateStatusBar(3);
        int widths[3] = { 200, 560, -1 };
        SetStatusWidths(3, widths);

        m_leftPanel = new LeftSidebarPanel(this);

        m_canvas = new DesignCanvas(this);
        m_propPanel = new BoardPropertyPanel(this);


        wxBoxSizer* topSizer = new wxBoxSizer(wxHORIZONTAL);
        topSizer->Add(m_leftPanel, 18, wxEXPAND);
        topSizer->Add(m_canvas, 59, wxEXPAND);
        topSizer->Add(m_propPanel, 23, wxEXPAND);
        SetSizer(topSizer);
        Layout();

        BindUiCallbacks();
        m_leftPanel->SetActiveLayer(m_canvas->GetActiveLayer());
        m_leftPanel->SyncChecks(m_canvas->GetSnap(), m_canvas->GetShowGrid());
        m_propPanel->UpdateFromShape(nullptr, 0);
        RefreshObjects();
        SetStatusText("0.000, 0.000 mm", 0);
        SetStatusText("Sample library loaded. Select a tool or open Tools > Component Library.", 1);
        SetStatusText("Nothing selected", 2);

        Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnClose, this);
        Bind(wxEVT_MENU, &MainFrame::OnMenu, this);
        Bind(wxEVT_TOOL, &MainFrame::OnTool, this);

        Centre();
    }

private:
    std::shared_ptr<Library> m_library;
    LibraryManagerFrame* m_libraryManager = nullptr;
    DesignCanvas* m_canvas = nullptr;
    LeftSidebarPanel* m_leftPanel = nullptr;
    BoardPropertyPanel* m_propPanel = nullptr;
    wxToolBar* m_toolbar = nullptr;
    wxMenuItem* m_gridMenuItem = nullptr;
    wxMenuItem* m_snapMenuItem = nullptr;
    bool m_refreshingObjects = false;

    void BindUiCallbacks()
    {
        m_canvas->OnSelectionChanged = [this]() {
            UpdateProperties();
        };
        m_canvas->OnContentChanged = [this]() {
            RefreshObjects();
            UpdateProperties();
        };
        m_canvas->OnStatus = [this](const wxString& text, const wxString& second) {
            if (!second.empty()) SetStatusText(second, 2);
            SetStatusText(text, 1);
        };
        m_canvas->OnCoords = [this](const wxString& coords) {
            SetStatusText(coords, 0);
        };
        m_canvas->OnViewChanged = [this]() { SyncViewState(); };

        m_leftPanel->OnLayerActivated = [this](int layer) {
            m_canvas->SetActiveLayer(layer);
            m_leftPanel->SetActiveLayer(layer);
            SetStatusText("Active layer: " + LayerName(layer), 2);
        };
        m_leftPanel->OnLayerVisibility = [this](int layer, bool visible) {
            m_canvas->SetLayerVisible(layer, visible);
            m_leftPanel->SetLayerVisible(layer, visible);
            SetStatusText((visible ? "Shown: " : "Hidden: ") + LayerName(layer), 2);
        };
        m_leftPanel->OnGridChanged = [this](double grid) {
            m_canvas->SetGridSize(grid);
            SetStatusText(wxString::Format("Grid step: %.2f mm", grid), 1);
        };
        m_leftPanel->OnSnapChanged = [this](bool snap) {
            m_canvas->SetSnap(snap);
            SetStatusText(snap ? "Grid snapping enabled" : "Grid snapping disabled", 1);
        };
        m_leftPanel->OnShowGridChanged = [this](bool show) {
            m_canvas->SetShowGrid(show);
            SetStatusText(show ? "Grid shown" : "Grid hidden", 1);
        };
        m_leftPanel->OnObjectSelected = [this](int uid) {
            if (m_refreshingObjects) return;
            m_canvas->SelectShapeByUid(uid);
            m_canvas->SetFocus();
        };
        m_leftPanel->OnFitRequested = [this]() {
            m_canvas->ZoomFit();
            SetStatusText("Zoomed to fit", 1);
        };

        m_propPanel->OnModified = [this]() {
            m_canvas->ShapeEditedExternally();
            SetStatusText("Property updated", 1);
        };
    }

    void SyncViewState()
    {
        bool showGrid = m_canvas->GetShowGrid();
        bool snap = m_canvas->GetSnap();
        if (m_gridMenuItem) m_gridMenuItem->Check(showGrid);
        if (m_snapMenuItem) m_snapMenuItem->Check(snap);
        if (m_toolbar) {
            m_toolbar->ToggleTool(ID_T_GRID, showGrid);
            m_toolbar->ToggleTool(ID_T_SNAP, snap);
        }
        m_leftPanel->SyncChecks(snap, showGrid);
        m_leftPanel->SyncGrid(m_canvas->GetGridSize());
    }

    void UpdateProperties()
    {
        m_propPanel->UpdateFromShape(m_canvas->GetSelectedShape(), m_canvas->GetSelectionCount());
        SetStatusText(m_canvas->SelectionDescription(), 2);
    }

    void RefreshObjects()
    {
        Shape* selected = m_canvas->GetSelectedShape();
        int uid = selected ? selected->uid : -1;
        m_refreshingObjects = true;
        m_leftPanel->SetObjects(m_canvas->GetObjectInfos(), uid);
        m_refreshingObjects = false;
        SetStatusText(wxString::Format("%d object(s) on the board", m_canvas->ShapeCount()), 2);
    }

    void CreateMenuBar()
    {
        wxMenuBar* mb = new wxMenuBar();

        wxMenu* fileMenu = new wxMenu();
        fileMenu->Append(wxID_NEW, "&New design\tCtrl+N");
        fileMenu->Append(ID_M_DEMO, "Load &demo design");
        fileMenu->AppendSeparator();
        fileMenu->Append(ID_TOOLS_LIB, "Component &library...");
        fileMenu->Append(ID_TOOLS_FPE, "&Footprint editor...");
        fileMenu->AppendSeparator();
        fileMenu->Append(wxID_EXIT, "E&xit");
        mb->Append(fileMenu, "&File");

        wxMenu* editMenu = new wxMenu();
        editMenu->Append(wxID_CUT, "Cu&t\tCtrl+X");
        editMenu->Append(wxID_COPY, "&Copy\tCtrl+C");
        editMenu->Append(wxID_PASTE, "&Paste\tCtrl+V");
        editMenu->Append(wxID_DUPLICATE, "&Duplicate\tCtrl+D");
        editMenu->Append(wxID_DELETE, "&Delete\tDel");
        editMenu->AppendSeparator();
        editMenu->Append(wxID_SELECTALL, "Select &all\tCtrl+A");
        editMenu->AppendSeparator();

        wxMenu* alignMenu = new wxMenu();
        alignMenu->Append(ID_M_ALIGN_LEFT, "Align left");
        alignMenu->Append(ID_M_ALIGN_RIGHT, "Align right");
        alignMenu->Append(ID_M_ALIGN_TOP, "Align top");
        alignMenu->Append(ID_M_ALIGN_BOTTOM, "Align bottom");
        alignMenu->Append(ID_M_ALIGN_CENTER_H, "Center horizontally");
        alignMenu->Append(ID_M_ALIGN_CENTER_V, "Center vertically");
        alignMenu->AppendSeparator();
        alignMenu->Append(ID_M_ALIGN_DIST_H, "Distribute horizontally");
        alignMenu->Append(ID_M_ALIGN_DIST_V, "Distribute vertically");
        editMenu->AppendSubMenu(alignMenu, "&Align");

        wxMenu* rotateMenu = new wxMenu();
        rotateMenu->Append(ID_T_ROT_CW, "Rotate 90 deg clockwise\tCtrl+R");
        rotateMenu->Append(ID_T_ROT_CCW, "Rotate 90 deg counter-clockwise");
        editMenu->AppendSubMenu(rotateMenu, "&Rotate");
        editMenu->AppendSeparator();
        editMenu->Append(ID_M_FRONT, "Bring to &front");
        editMenu->Append(ID_M_BACK, "Send to &back");
        mb->Append(editMenu, "&Edit");

        wxMenu* viewMenu = new wxMenu();
        m_gridMenuItem = viewMenu->AppendCheckItem(ID_V_GRID, "Show &grid");
        m_gridMenuItem->Check(true);
        m_snapMenuItem = viewMenu->AppendCheckItem(ID_V_SNAP, "&Snap to grid");
        m_snapMenuItem->Check(true);
        viewMenu->AppendSeparator();
        viewMenu->Append(ID_V_ZOOM_IN, "Zoom &in\tCtrl+=");
        viewMenu->Append(ID_V_ZOOM_OUT, "Zoom &out\tCtrl+-");
        viewMenu->Append(ID_V_ZOOM_FIT, "Zoom to &fit\tCtrl+0");
        mb->Append(viewMenu, "&View");

        wxMenu* toolsMenu = new wxMenu();
        toolsMenu->Append(ID_TOOLS_LIB, "Component Library Manager...");
        toolsMenu->Append(ID_TOOLS_FPE, "Footprint Editor...");
        mb->Append(toolsMenu, "&Tools");

        wxMenu* helpMenu = new wxMenu();
        helpMenu->Append(wxID_ABOUT, "&About EDA Editor...");
        mb->Append(helpMenu, "&Help");

        SetMenuBar(mb);
    }

    void CreateToolbar()
    {
        m_toolbar = CreateToolBar(wxTB_HORIZONTAL | wxTB_FLAT | wxTB_TEXT);
        m_toolbar->AddRadioTool(ID_T_SEL, "Sel", MakeToolIcon("select"));
        m_toolbar->AddRadioTool(ID_T_LINE, "Line", MakeToolIcon("line"));
        m_toolbar->AddRadioTool(ID_T_RECT, "Rect", MakeToolIcon("rect"));
        m_toolbar->AddRadioTool(ID_T_CIRCLE, "Circle", MakeToolIcon("circle"));
        m_toolbar->AddRadioTool(ID_T_ARC, "Arc", MakeToolIcon("arc"));
        m_toolbar->AddRadioTool(ID_T_POLY, "Poly", MakeToolIcon("poly"));
        m_toolbar->AddRadioTool(ID_T_TEXT, "Text", MakeToolIcon("text"));
        m_toolbar->AddSeparator();
        m_toolbar->AddCheckTool(ID_T_GRID, "Grid", MakeToolIcon("grid"));
        m_toolbar->AddCheckTool(ID_T_SNAP, "Snap", MakeToolIcon("snap"));
        m_toolbar->AddSeparator();
        m_toolbar->AddTool(ID_T_ROT_CCW, "Rot-90", MakeToolIcon("rotate"));
        m_toolbar->AddTool(ID_T_ROT_CW, "Rot+90", MakeToolIcon("rotate"));
        m_toolbar->AddTool(ID_T_DELETE, "Del", MakeToolIcon("delete"));
        m_toolbar->AddSeparator();
        m_toolbar->AddTool(ID_TOOLS_LIB, "Lib", MakeToolIcon("library"));
        m_toolbar->AddTool(ID_TOOLS_FPE, "FPE", MakeToolIcon("pad"));
        m_toolbar->Realize();
        m_toolbar->ToggleTool(ID_T_SEL, true);
        m_toolbar->ToggleTool(ID_T_GRID, true);
        m_toolbar->ToggleTool(ID_T_SNAP, true);
    }

    void OnTool(wxCommandEvent& e)
    {
        int id = e.GetId();
        if (id == ID_T_SEL) { m_canvas->SetTool(TOOL_SELECT); SetStatusText("Select tool: click, shift-click, drag to box-select. Drag handles to resize; circle handle to rotate.", 1); }
        else if (id == ID_T_LINE) { m_canvas->SetTool(TOOL_LINE); SetStatusText("Line: drag to draw. Hold Shift to constrain to 45 degrees.", 1); }
        else if (id == ID_T_RECT) { m_canvas->SetTool(TOOL_RECT); SetStatusText("Rectangle: drag to draw. Hold Shift for a square.", 1); }
        else if (id == ID_T_CIRCLE) { m_canvas->SetTool(TOOL_CIRCLE); SetStatusText("Circle / ellipse: drag to draw. Hold Shift for a circle.", 1); }
        else if (id == ID_T_ARC) { m_canvas->SetTool(TOOL_ARC); SetStatusText("Arc: click centre, then start point, then end point.", 1); }
        else if (id == ID_T_POLY) { m_canvas->SetTool(TOOL_POLY); SetStatusText("Polygon: click points, double-click / Enter / right-click to finish.", 1); }
        else if (id == ID_T_TEXT) { m_canvas->SetTool(TOOL_TEXT); SetStatusText("Text: click a position and type the text.", 1); }
        else if (id == ID_T_GRID) { m_canvas->SetShowGrid(e.IsChecked()); SetStatusText(e.IsChecked() ? "Grid shown" : "Grid hidden", 1); }
        else if (id == ID_T_SNAP) { m_canvas->SetSnap(e.IsChecked()); SetStatusText(e.IsChecked() ? "Grid snapping enabled" : "Grid snapping disabled", 1); }
        else if (id == ID_T_ROT_CW) { m_canvas->RotateSelection(90.0); SetStatusText("Rotated selection 90 degrees", 1); }
        else if (id == ID_T_ROT_CCW) { m_canvas->RotateSelection(-90.0); SetStatusText("Rotated selection -90 degrees", 1); }
        else if (id == ID_T_DELETE) m_canvas->DeleteSelection();
        else if (id == ID_TOOLS_LIB) OpenLibraryManager();
        else if (id == ID_TOOLS_FPE) OpenFootprintEditorForSelection();
        else e.Skip();
    }

    void OnMenu(wxCommandEvent& e)
    {
        switch (e.GetId()) {
        case wxID_NEW: NewDesign(); break;
        case ID_M_DEMO: LoadDemo(); break;
        case wxID_EXIT: Close(); break;
        case wxID_CUT: m_canvas->CutSelection(); break;
        case wxID_COPY: m_canvas->CopySelection(); break;
        case wxID_PASTE: m_canvas->PasteClipboard(); break;
        case wxID_DUPLICATE: m_canvas->DuplicateSelection(); break;
        case wxID_DELETE: m_canvas->DeleteSelection(); break;
        case wxID_SELECTALL: m_canvas->SelectAll(); break;
        case ID_M_ALIGN_LEFT: m_canvas->Align(ALIGN_LEFT); break;
        case ID_M_ALIGN_RIGHT: m_canvas->Align(ALIGN_RIGHT); break;
        case ID_M_ALIGN_TOP: m_canvas->Align(ALIGN_TOP); break;
        case ID_M_ALIGN_BOTTOM: m_canvas->Align(ALIGN_BOTTOM); break;
        case ID_M_ALIGN_CENTER_H: m_canvas->Align(ALIGN_CENTER_H); break;
        case ID_M_ALIGN_CENTER_V: m_canvas->Align(ALIGN_CENTER_V); break;
        case ID_M_ALIGN_DIST_H: m_canvas->Align(ALIGN_DISTRIBUTE_H); break;
        case ID_M_ALIGN_DIST_V: m_canvas->Align(ALIGN_DISTRIBUTE_V); break;
        case ID_M_FRONT: m_canvas->BringToFront(); break;
        case ID_M_BACK: m_canvas->SendToBack(); break;
        case ID_T_ROT_CW: m_canvas->RotateSelection(90.0); break;
        case ID_T_ROT_CCW: m_canvas->RotateSelection(-90.0); break;
        case ID_V_GRID:
            m_canvas->SetShowGrid(m_gridMenuItem->IsChecked());
            break;
        case ID_V_SNAP:
            m_canvas->SetSnap(m_snapMenuItem->IsChecked());
            break;
        case ID_V_ZOOM_IN: m_canvas->ZoomIn(); break;
        case ID_V_ZOOM_OUT: m_canvas->ZoomOut(); break;
        case ID_V_ZOOM_FIT: m_canvas->ZoomFit(); break;
        case ID_TOOLS_LIB: OpenLibraryManager(); break;
        case ID_TOOLS_FPE: OpenFootprintEditorForSelection(); break;
        case wxID_ABOUT:
            wxMessageBox(
                "EDA Editor - course PCB design tool\n\n"
                "Task B - drawing and editing:\n"
                "  line, arc, rectangle, circle, polygon and text objects; layer,\n"
                "  colour, line width and fill properties; grid, snapping and\n"
                "  alignment; selection, box selection, move, resize, rotate,\n"
                "  copy / paste, duplicate, delete and z-order.\n\n"
                "Task C - footprint editor:\n"
                "  SMD / through-hole pads, vias, silkscreen shapes and text;\n"
                "  pad numbers with overlap avoidance; multi-select, align and\n"
                "  copy; validation of names, duplicate pins and dimensions.\n\n"
                "Task D - component library:\n"
                "  new / open / save / save-as libraries, search by name,\n"
                "  category or keyword, thumbnail preview, rename / copy /\n"
                "  delete with reference warnings and unique names / UIDs.\n\n"
                "The built-in sample library contains SOIC-8, 0603, DIP-8,\n"
                "QFP-32, SOT-23, a 1x4 header, an 0805 LED and a test point.",
                "About EDA Editor", wxOK | wxICON_INFORMATION, this);
            break;
        default: e.Skip(); break;
        }
    }

    void LoadDemo()
    {
        m_canvas->BuildDemo();
        m_propPanel->UpdateFromShape(m_canvas->GetSelectedShape(), m_canvas->GetSelectionCount());
        RefreshObjects();
        SetStatusText("Demo design loaded: shapes, layers, and library components.", 1);
        SetStatusText(m_canvas->SelectionDescription(), 2);
    }

public:
    void ShowDemo(int stage)
    {
        LoadDemo();
        if (stage >= 1) OpenLibraryManager();
        if (stage >= 2 && m_libraryManager) m_libraryManager->EditByName("SOIC-8");
    }

private:
    void NewDesign()
    {
        if (m_canvas->ShapeCount() > 0) {
            int answer = wxMessageBox("Discard all objects in the current design?", "New design",
                                      wxYES_NO | wxICON_QUESTION, this);
            if (answer != wxYES) return;
        }
        m_canvas->NewDocument();
        m_propPanel->UpdateFromShape(nullptr, 0);
        RefreshObjects();
        SetStatusText("New design created", 1);
        SetStatusText("Nothing selected", 2);
        m_canvas->ZoomFit();
    }

    void OpenLibraryManager()
    {
        if (!m_libraryManager) {
            m_libraryManager = new LibraryManagerFrame(this, m_library,
                [this](Footprint* fp, wxString& err) { return CommitFootprint(fp, err); },
                [this](const wxString& name) { return m_canvas->CountComponents(name); },
                [this](const wxString& name) {
                    bool ok = m_canvas->PlaceComponent(name);
                    if (ok) {
                        Raise();
                        RefreshObjects();
                        SetStatusText("Footprint placed on the board", 1);
                    }
                    return ok;
                },
                [this]() {
                    m_canvas->OnLibraryChanged();
                    RefreshObjects();
                    UpdateProperties();
                },
                [this]() { m_libraryManager = nullptr; });
        }
        m_libraryManager->Show();
        m_libraryManager->Iconize(false);
        m_libraryManager->Raise();
    }

    void OpenFootprintEditorForSelection()
    {
        wxString footprintName;
        if (Shape* s = m_canvas->GetSelectedShape()) {
            if (s->TypeName() == "Component")
                footprintName = static_cast<ComponentShape*>(s)->footprintName;
        }
        OpenLibraryManager();
        if (m_libraryManager && !footprintName.empty())
            m_libraryManager->EditByName(footprintName);
    }

    bool CommitFootprint(Footprint* fp, wxString& err)
    {
        if (!fp) { err = "No footprint supplied."; return false; }
        if (TrimStr(fp->name).empty()) { err = "Footprint name must not be empty."; return false; }
        if (m_library->NameExists(fp->name, fp->uid)) {
            err = "A footprint named '" + fp->name + "' already exists. Names must be unique.";
            return false;
        }
        wxArrayString errors;
        if (!fp->Validate(errors)) {
            err = "Footprint validation failed:\n";
            for (size_t i = 0; i < errors.size() && i < 10; ++i) err += "- " + errors[i] + "\n";
            return false;
        }

        int index = m_library->IndexOfUid(fp->uid);
        if (index >= 0) {
            wxString oldName = m_library->items[index]->name;
            *m_library->items[index] = *fp;
            if (!oldName.IsSameAs(fp->name, false))
                m_canvas->RenameComponentFootprint(oldName, fp->name);
        }
        else {
            m_library->items.push_back(std::make_shared<Footprint>(*fp));
        }
        m_library->modified = true;
        if (m_libraryManager) m_libraryManager->RefreshAll();
        m_canvas->OnLibraryChanged();
        RefreshObjects();
        return true;
    }

    void OnClose(wxCloseEvent& e)
    {
        if (m_library && m_library->modified) {
            int answer = wxMessageBox("The component library has unsaved changes. Save before exiting?",
                                      "Unsaved library", wxYES_NO | wxCANCEL | wxICON_QUESTION, this);
            if (answer == wxCANCEL) { e.Veto(); return; }
            if (answer == wxYES) {
                OpenLibraryManager();
                if (!m_libraryManager->SaveLibraryNow()) { e.Veto(); return; }
            }
        }
        e.Skip();
    }
};

// ============================================================================
// Application entry point
// ============================================================================
class MyApp : public wxApp
{
public:
    bool OnInit() override
    {
        SetAppName("EDA Editor");
        SetVendorName("Course");
        MainFrame* frame = new MainFrame();
        frame->Show();

        int demoStage = 0;
        for (int i = 1; i < argc; ++i) {
            wxString arg = argv[i];
            if (arg == "--demo") demoStage = 0;
            else if (arg == "--demo-library") demoStage = 1;
            else if (arg == "--demo-editor") demoStage = 2;
        }
        if (demoStage >= 0 && argc > 1) {
            for (int i = 1; i < argc; ++i) {
                wxString arg = argv[i];
                if (arg.StartsWith("--demo")) {
                    frame->CallAfter([frame, demoStage]() { frame->ShowDemo(demoStage); });
                    break;
                }
            }
        }
        return true;
    }
};

IMPLEMENT_APP(MyApp)







