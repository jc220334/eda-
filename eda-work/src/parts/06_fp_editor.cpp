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







