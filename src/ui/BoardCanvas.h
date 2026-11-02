#pragma once

#include "core/Project.h"
#include "pcb/Geometry.h"

#include <wx/wx.h>

#include <functional>
#include <string>
#include <vector>

class BoardCanvas : public wxPanel {
public:
    using StatusCallback = std::function<void(const wxString&)>;

    explicit BoardCanvas(wxWindow* parent);

    void SetProject(pcb::PcbProject* project);
    void SetActiveLayer(const std::string& layerId);
    void SetRouteWidth(double width) { m_routeWidth = width; }
    void ZoomIn();
    void ZoomOut();
    void SetComponentsVisible(bool visible) { m_showComponents = visible; Refresh(); }
    void SetPadsVisible(bool visible) { m_showPads = visible; Refresh(); }
    void SetViasVisible(bool visible) { m_showVias = visible; Refresh(); }
    void SetStatusCallback(StatusCallback callback);
    void RefreshBoard();
    void FitBoard();
    const std::string& SelectedComponentId() const { return m_selectedComponentId; }
    const std::string& LastPadNetId() const { return m_lastPadNetId; }
    pcb::Point LastMousePoint() const { return m_lastMousePoint; }

private:
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnLeftDown(wxMouseEvent& event);
    void OnLeftUp(wxMouseEvent& event);
    void OnMotion(wxMouseEvent& event);
    void OnRightDown(wxMouseEvent& event);
    void OnKeyDown(wxKeyEvent& event);

    [[nodiscard]] pcb::Bounds VisibleBoardBounds() const;
    [[nodiscard]] wxPoint ToScreen(const pcb::Point& point) const;
    [[nodiscard]] pcb::Point ToModel(const wxPoint& point) const;
    const pcb::Component* HitComponent(const pcb::Point& point) const;
    const pcb::PadLocation* HitPad(const pcb::Point& point) const;
    wxColour NetColour(const std::string& netId, int fallbackIndex = 0) const;
    void DrawGrid(wxDC& dc);
    void DrawBoard(wxDC& dc);
    void DrawCopperPours(wxDC& dc);
    void DrawRatsnest(wxDC& dc);
    void DrawTraces(wxDC& dc);
    void DrawPadsAndComponents(wxDC& dc);
    void DrawVias(wxDC& dc);
    void DrawRoutePreview(wxDC& dc);
    void Notify(const wxString& message);
    void FinishManualRoute(const pcb::Point& endPoint);

    pcb::PcbProject* m_project = nullptr;
    StatusCallback m_status;
    std::string m_activeLayer = "F.Cu";
    std::string m_selectedComponentId;
    std::string m_lastPadNetId;
    pcb::Point m_lastMousePoint;
    pcb::Point m_dragOffset;
    bool m_dragging = false;
    bool m_routing = false;
    bool m_showComponents = true;
    bool m_showPads = true;
    bool m_showVias = true;
    std::string m_routeNetId;
    double m_routeWidth = 0.30;
    std::vector<pcb::Point> m_routePoints;
    double m_scale = 1.0;
    wxPoint m_origin{30, 30};
};
