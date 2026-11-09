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



