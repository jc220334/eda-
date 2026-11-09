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







