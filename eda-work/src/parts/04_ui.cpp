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


