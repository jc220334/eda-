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


