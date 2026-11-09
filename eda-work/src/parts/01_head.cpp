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

