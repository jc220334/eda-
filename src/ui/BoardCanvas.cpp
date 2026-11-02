#include "ui/BoardCanvas.h"

#include "pcb/Geometry.h"
#include "pcb/LayoutService.h"
#include "pcb/RoutingService.h"

#include <wx/dcbuffer.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

wxColour ToColour(const pcb::Color& color, const wxColour& fallback)
{
    const wxColour parsed(wxString::FromUTF8(color.hex));
    return parsed.IsOk() ? parsed : fallback;
}

} // namespace

BoardCanvas::BoardCanvas(wxWindow* parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
              wxBORDER_SIMPLE | wxFULL_REPAINT_ON_RESIZE)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(wxColour(252, 252, 252));
    Bind(wxEVT_PAINT, &BoardCanvas::OnPaint, this);
    Bind(wxEVT_SIZE, &BoardCanvas::OnSize, this);
    Bind(wxEVT_LEFT_DOWN, &BoardCanvas::OnLeftDown, this);
    Bind(wxEVT_LEFT_UP, &BoardCanvas::OnLeftUp, this);
    Bind(wxEVT_MOTION, &BoardCanvas::OnMotion, this);
    Bind(wxEVT_RIGHT_DOWN, &BoardCanvas::OnRightDown, this);
    Bind(wxEVT_KEY_DOWN, &BoardCanvas::OnKeyDown, this);
    SetFocus();
}

void BoardCanvas::SetProject(pcb::PcbProject* project)
{
    m_project = project;
    m_routing = false;
    m_routePoints.clear();
    m_selectedComponentId.clear();
    FitBoard();
    Refresh();
}

void BoardCanvas::SetActiveLayer(const std::string& layerId)
{
    m_activeLayer = layerId;
    Notify("Active layer: " + wxString::FromUTF8(layerId));
}

void BoardCanvas::ZoomIn()
{
    const wxSize client = GetClientSize();
    const wxPoint center{client.x / 2, client.y / 2};
    m_scale = std::min(160.0, m_scale * 1.20);
    m_origin.x = center.x + static_cast<int>((m_origin.x - center.x) * 1.20);
    m_origin.y = center.y + static_cast<int>((m_origin.y - center.y) * 1.20);
    Refresh();
}

void BoardCanvas::ZoomOut()
{
    const wxSize client = GetClientSize();
    const wxPoint center{client.x / 2, client.y / 2};
    m_scale = std::max(0.3, m_scale / 1.20);
    m_origin.x = center.x + static_cast<int>((m_origin.x - center.x) / 1.20);
    m_origin.y = center.y + static_cast<int>((m_origin.y - center.y) / 1.20);
    Refresh();
}

void BoardCanvas::SetStatusCallback(StatusCallback callback)
{
    m_status = std::move(callback);
}

void BoardCanvas::RefreshBoard()
{
    Refresh();
}

pcb::Bounds BoardCanvas::VisibleBoardBounds() const
{
    if (m_project) {
        const pcb::Bounds bounds = pcb::boardBounds(m_project->board);
        if (bounds.valid && bounds.width() > 0.1 && bounds.height() > 0.1) {
            return bounds;
        }
    }
    pcb::Bounds fallback;
    fallback.include(pcb::Point{0, 0});
    fallback.include(pcb::Point{60, 40});
    return fallback;
}

void BoardCanvas::FitBoard()
{
    const wxSize size = GetClientSize();
    const pcb::Bounds bounds = VisibleBoardBounds();
    const double availableWidth = std::max(100, size.x - 60);
    const double availableHeight = std::max(100, size.y - 60);
    m_scale = std::min(availableWidth / bounds.width(), availableHeight / bounds.height());
    m_scale = std::clamp(m_scale, 0.5, 80.0);
    const double boardWidth = bounds.width() * m_scale;
    const double boardHeight = bounds.height() * m_scale;
    m_origin = wxPoint{
        static_cast<int>((size.x - boardWidth) / 2.0),
        static_cast<int>((size.y - boardHeight) / 2.0),
    };
    Refresh();
}

wxPoint BoardCanvas::ToScreen(const pcb::Point& point) const
{
    const pcb::Bounds bounds = VisibleBoardBounds();
    return wxPoint{
        m_origin.x + static_cast<int>(std::lround((point.x - bounds.min.x) * m_scale)),
        m_origin.y + static_cast<int>(std::lround((bounds.max.y - point.y) * m_scale)),
    };
}

pcb::Point BoardCanvas::ToModel(const wxPoint& point) const
{
    const pcb::Bounds bounds = VisibleBoardBounds();
    return pcb::Point{
        bounds.min.x + (point.x - m_origin.x) / m_scale,
        bounds.max.y - (point.y - m_origin.y) / m_scale,
    };
}

const pcb::Component* BoardCanvas::HitComponent(const pcb::Point& point) const
{
    if (!m_project) {
        return nullptr;
    }
    for (const pcb::Component& component : m_project->board.components) {
        if (pcb::componentBounds(m_project->board, component).expanded(0.5).contains(point)) {
            return &component;
        }
    }
    return nullptr;
}

const pcb::PadLocation* BoardCanvas::HitPad(const pcb::Point& point) const
{
    if (!m_project) {
        return nullptr;
    }
    static pcb::PadLocation hit;
    const auto pads = pcb::collectPads(m_project->board);
    for (const pcb::PadLocation& pad : pads) {
        if (pad.position.distanceTo(point) <=
            std::max(pad.width, pad.height) / 2.0 + 0.35) {
            hit = pad;
            return &hit;
        }
    }
    return nullptr;
}

wxColour BoardCanvas::NetColour(const std::string& netId, int fallbackIndex) const
{
    static const wxColour palette[] = {
        wxColour(224, 91, 85),
        wxColour(85, 188, 116),
        wxColour(88, 158, 238),
        wxColour(233, 179, 76),
        wxColour(183, 126, 230),
        wxColour(80, 200, 210),
    };
    if (m_project) {
        const auto& nets = m_project->board.nets.nets();
        for (std::size_t index = 0; index < nets.size(); ++index) {
            if (nets[index].id == netId) {
                return palette[index % (sizeof(palette) / sizeof(palette[0]))];
            }
        }
    }
    return palette[fallbackIndex % (sizeof(palette) / sizeof(palette[0]))];
}

void BoardCanvas::Notify(const wxString& message)
{
    if (m_status) {
        m_status(message);
    }
}

void BoardCanvas::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(wxColour(252, 252, 252)));
    dc.Clear();
    DrawGrid(dc);
    DrawBoard(dc);
    DrawCopperPours(dc);
    DrawRatsnest(dc);
    DrawTraces(dc);
    DrawPadsAndComponents(dc);
    DrawVias(dc);
    DrawRoutePreview(dc);
}

void BoardCanvas::DrawGrid(wxDC& dc)
{
    const pcb::Bounds bounds = VisibleBoardBounds();
    dc.SetPen(wxPen(wxColour(235, 235, 235), 1));
    const int step = 5;
    for (int x = static_cast<int>(std::ceil(bounds.min.x / step)) * step;
         x <= static_cast<int>(bounds.max.x); x += step) {
        dc.DrawLine(ToScreen({static_cast<double>(x), bounds.min.y}),
                    ToScreen({static_cast<double>(x), bounds.max.y}));
    }
    for (int y = static_cast<int>(std::ceil(bounds.min.y / step)) * step;
         y <= static_cast<int>(bounds.max.y); y += step) {
        dc.DrawLine(ToScreen({bounds.min.x, static_cast<double>(y)}),
                    ToScreen({bounds.max.x, static_cast<double>(y)}));
    }
}

void BoardCanvas::DrawBoard(wxDC& dc)
{
    if (!m_project || m_project->board.outline.points.size() < 3) {
        return;
    }
    std::vector<wxPoint> points;
    for (const pcb::Point& point : m_project->board.outline.points) {
        points.push_back(ToScreen(point));
    }
    dc.SetBrush(wxBrush(wxColour(250, 251, 249)));
    const pcb::Layer* edge = m_project->board.layers.findLayer("Edge");
    dc.SetPen(wxPen(edge ? ToColour(edge->color, wxColour(80, 126, 105))
                         : wxColour(80, 126, 105), 2));
    dc.DrawPolygon(static_cast<int>(points.size()), points.data());
}

void BoardCanvas::DrawCopperPours(wxDC& dc)
{
    if (!m_project) {
        return;
    }
    for (const pcb::CopperPour& pour : m_project->board.copperPours) {
        const pcb::Layer* layer = m_project->board.layers.findLayer(pour.layerId);
        if (layer && !layer->visible) {
            continue;
        }
        std::vector<wxPoint> outer;
        for (const pcb::Point& point : pour.outline) {
            outer.push_back(ToScreen(point));
        }
        dc.SetPen(wxPen(wxColour(102, 150, 116), 1));
        dc.SetBrush(wxBrush(wxColour(177, 215, 188), wxBRUSHSTYLE_FDIAGONAL_HATCH));
        dc.DrawPolygon(static_cast<int>(outer.size()), outer.data());
        dc.SetBrush(wxBrush(wxColour(250, 251, 249)));
        dc.SetPen(wxPen(wxColour(150, 180, 160), 1));
        for (const auto& hole : pour.holes) {
            std::vector<wxPoint> holePoints;
            for (const pcb::Point& point : hole) {
                holePoints.push_back(ToScreen(point));
            }
            if (holePoints.size() >= 3) {
                dc.DrawPolygon(static_cast<int>(holePoints.size()), holePoints.data());
            }
        }
    }
}

void BoardCanvas::DrawRatsnest(wxDC& dc)
{
    if (!m_project) {
        return;
    }
    wxPen pen(wxColour(152, 157, 160), 1, wxPENSTYLE_SHORT_DASH);
    dc.SetPen(pen);
    for (const auto& [a, b] : pcb::RoutingService::ratsnest(m_project->board)) {
        dc.DrawLine(ToScreen(a), ToScreen(b));
    }
}

void BoardCanvas::DrawTraces(wxDC& dc)
{
    if (!m_project) {
        return;
    }
    for (const pcb::Trace& trace : m_project->board.traces) {
        const pcb::Layer* layer = m_project->board.layers.findLayer(trace.layerId);
        if (layer && !layer->visible) {
            continue;
        }
        wxPen pen(NetColour(trace.netId), std::max(2, static_cast<int>(trace.width * m_scale)));
        dc.SetPen(pen);
        for (std::size_t index = 1; index < trace.points.size(); ++index) {
            dc.DrawLine(ToScreen(trace.points[index - 1]), ToScreen(trace.points[index]));
        }
    }
}

void BoardCanvas::DrawPadsAndComponents(wxDC& dc)
{
    if (!m_project) {
        return;
    }
    if (!m_showComponents && !m_showPads) {
        return;
    }
    const pcb::Bounds boardBox = pcb::boardBounds(m_project->board);
    dc.SetFont(wxFont(9, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
    for (const pcb::Component& component : m_project->board.components) {
        const pcb::Footprint* footprint = m_project->board.findFootprint(component.footprintId);
        if (!footprint) {
            continue;
        }
        const pcb::Bounds componentBox = pcb::componentBounds(m_project->board, component);
        if (m_showComponents && componentBox.valid) {
            const bool selected = component.id == m_selectedComponentId;
            dc.SetPen(wxPen(selected ? wxColour(208, 132, 32) : wxColour(112, 120, 118),
                           selected ? 2 : 1));
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            const wxPoint topLeft = ToScreen(pcb::Point{componentBox.min.x, componentBox.max.y});
            const wxPoint bottomRight = ToScreen(pcb::Point{componentBox.max.x, componentBox.min.y});
            dc.DrawRectangle(topLeft, wxSize{
                bottomRight.x - topLeft.x,
                bottomRight.y - topLeft.y,
            });
        }
        for (const pcb::Pad& pad : footprint->pads) {
            if (!m_showPads) {
                break;
            }
            const pcb::Point center = pcb::padPosition(component, pad);
            const wxPoint screen = ToScreen(center);
            const bool connected = std::any_of(
                component.padNets.begin(), component.padNets.end(),
                [&](const pcb::PadNet& binding) {
                    return pcb::normalizeIdentifier(binding.padNumber) ==
                           pcb::normalizeIdentifier(pad.number);
                });
            const auto binding = std::find_if(
                component.padNets.begin(), component.padNets.end(),
                [&](const pcb::PadNet& item) {
                    return pcb::normalizeIdentifier(item.padNumber) ==
                           pcb::normalizeIdentifier(pad.number);
                });
            const wxColour fill = binding != component.padNets.end()
                ? NetColour(binding->netId)
                : wxColour(194, 164, 94);
            dc.SetPen(wxPen(connected ? wxColour(245, 220, 150) : wxColour(120, 110, 78), 1));
            dc.SetBrush(wxBrush(fill));
            const wxSize size{
                std::max(3, static_cast<int>(pad.width * m_scale)),
                std::max(3, static_cast<int>(pad.height * m_scale)),
            };
            dc.DrawEllipse(screen.x - size.x / 2, screen.y - size.y / 2, size.x, size.y);
            if (!pad.number.empty() && m_scale > 6.0) {
                dc.SetTextForeground(wxColour(50, 56, 54));
                dc.DrawText(wxString::FromUTF8(pad.number), screen.x + 3, screen.y - 14);
            }
        }
        if (m_showComponents) {
            const wxPoint label = ToScreen(component.position);
            dc.SetTextForeground(wxColour(35, 40, 38));
            dc.DrawText(wxString::FromUTF8(component.reference), label.x - 10, label.y - 9);
        }
    }
    (void)boardBox;
}

void BoardCanvas::DrawVias(wxDC& dc)
{
    if (!m_project || !m_showVias) {
        return;
    }
    for (const pcb::Via& via : m_project->board.vias) {
        const wxPoint center = ToScreen(via.position);
        const int outer = std::max(4, static_cast<int>(via.diameter * m_scale));
        const int inner = std::max(2, static_cast<int>(via.drill * m_scale));
        dc.SetPen(wxPen(NetColour(via.netId), 1));
        dc.SetBrush(wxBrush(wxColour(212, 212, 212)));
        dc.DrawCircle(center, outer / 2);
        dc.SetBrush(wxBrush(wxColour(252, 252, 252)));
        dc.DrawCircle(center, inner / 2);
    }
}

void BoardCanvas::DrawRoutePreview(wxDC& dc)
{
    if (!m_routing || m_routePoints.empty()) {
        return;
    }
    dc.SetPen(wxPen(wxColour(255, 205, 79), 2, wxPENSTYLE_DOT));
    pcb::Point previous = m_routePoints.back();
    dc.DrawLine(ToScreen(previous), ToScreen(m_lastMousePoint));
    for (std::size_t index = 1; index < m_routePoints.size(); ++index) {
        dc.DrawLine(ToScreen(m_routePoints[index - 1]), ToScreen(m_routePoints[index]));
    }
}

void BoardCanvas::FinishManualRoute(const pcb::Point& endPoint)
{
    if (!m_project || !m_routing) {
        return;
    }
    m_routePoints.push_back(endPoint);
    const pcb::ManualRouteResult result = pcb::RoutingService::addManualTrace(
        m_project->board, m_routeNetId, m_activeLayer, m_routeWidth, m_routePoints, 0.25);
    Notify(result.ok
        ? wxString("Manual trace created")
        : wxString::FromUTF8(result.message));
    m_routing = false;
    m_routePoints.clear();
    m_routeNetId.clear();
    Refresh();
}

void BoardCanvas::OnLeftDown(wxMouseEvent& event)
{
    SetFocus();
    const pcb::Point model = ToModel(event.GetPosition());
    m_lastMousePoint = model;
    if (!m_project) {
        return;
    }
    if (m_routing) {
        const pcb::PadLocation* pad = HitPad(model);
        if (pad && pad->netId == m_routeNetId) {
            FinishManualRoute(pad->position);
        } else {
            m_routePoints.push_back(model);
            Notify("Routing point added. Click a matching pad to finish.");
            Refresh();
        }
        return;
    }

    if (const pcb::PadLocation* pad = HitPad(model)) {
        m_lastPadNetId = pad->netId;
        if (!pad->netId.empty()) {
            m_routing = true;
            m_routeNetId = pad->netId;
            m_routePoints = {pad->position};
            Notify("Manual route started from " +
                   wxString::FromUTF8(pad->reference + "." + pad->padNumber) +
                   ". Click points, then a matching pad.");
        }
        event.Skip();
        return;
    }
    if (const pcb::Component* component = HitComponent(model)) {
        m_selectedComponentId = component->id;
        m_dragOffset = pcb::Point{
            component->position.x - model.x,
            component->position.y - model.y,
        };
        m_dragging = true;
        Notify("Selected " + wxString::FromUTF8(component->reference));
        Refresh();
    } else {
        m_selectedComponentId.clear();
        Refresh();
    }
}

void BoardCanvas::OnLeftUp(wxMouseEvent&)
{
    m_dragging = false;
}

void BoardCanvas::OnMotion(wxMouseEvent& event)
{
    const pcb::Point model = ToModel(event.GetPosition());
    m_lastMousePoint = model;
    if (m_project && m_dragging && !m_selectedComponentId.empty()) {
        const pcb::Component* component = m_project->board.findComponent(m_selectedComponentId);
        if (component && !component->locked) {
            const pcb::Point target{
                std::round((model.x + m_dragOffset.x) * 2.0) / 2.0,
                std::round((model.y + m_dragOffset.y) * 2.0) / 2.0,
            };
            pcb::LayoutService::placeComponent(
                m_project->board, m_selectedComponentId, target);
        }
    }
    Refresh();
}

void BoardCanvas::OnRightDown(wxMouseEvent& event)
{
    if (!m_project) {
        return;
    }
    if (const pcb::Component* component = HitComponent(ToModel(event.GetPosition()))) {
        pcb::LayoutService::rotateComponent(m_project->board, component->id, 90.0);
        m_selectedComponentId = component->id;
        Notify("Rotated " + wxString::FromUTF8(component->reference) + " by 90 degrees");
        Refresh();
    }
}

void BoardCanvas::OnKeyDown(wxKeyEvent& event)
{
    if (event.GetKeyCode() == WXK_ESCAPE) {
        m_routing = false;
        m_routePoints.clear();
        m_routeNetId.clear();
        Notify("Manual route cancelled");
        Refresh();
        return;
    }
    event.Skip();
}

void BoardCanvas::OnSize(wxSizeEvent& event)
{
    FitBoard();
    event.Skip();
}
