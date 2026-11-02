#include "pcb/RoutingService.h"

#include "pcb/Geometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <numeric>
#include <queue>
#include <set>
#include <sstream>
#include <unordered_map>

namespace pcb {

namespace {

class DisjointSet {
public:
    explicit DisjointSet(std::size_t size) : m_parent(size), m_rank(size, 0) {
        std::iota(m_parent.begin(), m_parent.end(), 0);
    }

    std::size_t find(std::size_t node) {
        if (m_parent[node] != node) {
            m_parent[node] = find(m_parent[node]);
        }
        return m_parent[node];
    }

    void unite(std::size_t a, std::size_t b) {
        a = find(a);
        b = find(b);
        if (a == b) {
            return;
        }
        if (m_rank[a] < m_rank[b]) {
            std::swap(a, b);
        }
        m_parent[b] = a;
        if (m_rank[a] == m_rank[b]) {
            ++m_rank[a];
        }
    }

private:
    std::vector<std::size_t> m_parent;
    std::vector<int> m_rank;
};

bool isCopperLayer(const Board& board, const std::string& layerId)
{
    const Layer* layer = board.layers.findLayer(layerId);
    return layer && layer->type == LayerType::Copper;
}

bool padLayerMatches(const PadLocation& pad, const std::string& layerId)
{
    return pad.layerId == layerId || pad.drill > 0.0;
}

double pathLength(const std::vector<Point>& points)
{
    double total = 0;
    for (std::size_t index = 1; index < points.size(); ++index) {
        total += points[index - 1].distanceTo(points[index]);
    }
    return total;
}

std::vector<Point> removeDuplicatePoints(const std::vector<Point>& points)
{
    std::vector<Point> result;
    for (const Point& point : points) {
        if (result.empty() || result.back().distanceTo(point) > 1e-5) {
            result.push_back(point);
        }
    }
    return result;
}

bool traceTouchesPad(const Trace& trace, const PadLocation& pad, double tolerance = 1e-4)
{
    if (!padLayerMatches(pad, trace.layerId)) {
        return false;
    }
    const double radius = std::max(pad.width, pad.height) / 2.0;
    for (std::size_t index = 1; index < trace.points.size(); ++index) {
        if (distancePointToSegment(pad.position, trace.points[index - 1], trace.points[index]) <=
            radius + tolerance) {
            return true;
        }
    }
    return false;
}

bool tracesTouch(const Trace& a, const Trace& b, double tolerance = 1e-4)
{
    if (a.netId != b.netId || a.layerId != b.layerId) {
        return false;
    }
    for (std::size_t i = 1; i < a.points.size(); ++i) {
        for (std::size_t j = 1; j < b.points.size(); ++j) {
            if (distanceSegmentToSegment(
                    a.points[i - 1], a.points[i],
                    b.points[j - 1], b.points[j]) <= tolerance) {
                return true;
            }
        }
    }
    return false;
}

bool traceTouchesVia(const Trace& trace, const Via& via, double tolerance = 1e-4)
{
    if (trace.netId != via.netId ||
        (trace.layerId != via.fromLayer && trace.layerId != via.toLayer)) {
        return false;
    }
    for (std::size_t index = 1; index < trace.points.size(); ++index) {
        if (distancePointToSegment(via.position, trace.points[index - 1], trace.points[index]) <=
            tolerance) {
            return true;
        }
    }
    return false;
}

bool traceTouchesPour(const Trace& trace, const CopperPour& pour)
{
    if (trace.netId != pour.netId || trace.layerId != pour.layerId) {
        return false;
    }
    for (const Point& point : trace.points) {
        if (polygonContains(point, pour.outline, pour.holes)) {
            return true;
        }
    }
    for (std::size_t index = 1; index < trace.points.size(); ++index) {
        for (std::size_t edge = 0; edge < pour.outline.size(); ++edge) {
            if (distanceSegmentToSegment(
                    trace.points[index - 1], trace.points[index],
                    pour.outline[edge], pour.outline[(edge + 1) % pour.outline.size()]) <= 1e-5) {
                return true;
            }
        }
    }
    return false;
}

bool tracePathClear(
    const Board& board,
    const std::string& netId,
    const std::string& layerId,
    double width,
    const std::vector<Point>& points,
    double clearance,
    const std::vector<PadLocation>& pads)
{
    for (const Point& point : points) {
        if (!pointInsideBoard(board, point)) {
            return false;
        }
    }

    for (std::size_t index = 1; index < points.size(); ++index) {
        const Point& a = points[index - 1];
        const Point& b = points[index];
        for (const PadLocation& pad : pads) {
            if (pad.netId == netId || !padLayerMatches(pad, layerId)) {
                continue;
            }
            const double radius = std::max(pad.width, pad.height) / 2.0;
            if (distancePointToSegment(pad.position, a, b) <
                radius + width / 2.0 + clearance - 1e-5) {
                return false;
            }
        }
        for (const Trace& trace : board.traces) {
            if (trace.netId == netId || trace.layerId != layerId) {
                continue;
            }
            for (std::size_t traceIndex = 1; traceIndex < trace.points.size(); ++traceIndex) {
                const double distance = distanceSegmentToSegment(
                    a, b, trace.points[traceIndex - 1], trace.points[traceIndex]);
                if (distance < width / 2.0 + trace.width / 2.0 + clearance - 1e-5) {
                    return false;
                }
            }
        }
        for (const Via& via : board.vias) {
            if (via.netId == netId) {
                continue;
            }
            if (distancePointToSegment(via.position, a, b) <
                via.diameter / 2.0 + width / 2.0 + clearance - 1e-5) {
                return false;
            }
        }
    }
    return true;
}

std::vector<std::vector<Point>> routeCandidates(
    const Point& start,
    const Point& end,
    std::size_t laneIndex,
    const Board& board)
{
    const Bounds bounds = boardBounds(board);
    std::vector<std::vector<Point>> candidates;
    candidates.push_back({start, end});
    candidates.push_back({start, Point{end.x, start.y}, end});
    candidates.push_back({start, Point{start.x, end.y}, end});

    for (std::size_t step = 1; step <= 10; ++step) {
        const double offset = static_cast<double>(step);
        for (double sign : {1.0, -1.0}) {
            const double laneY = std::clamp(
                (start.y + end.y) / 2.0 + sign * offset + static_cast<double>(laneIndex % 3) * 0.25,
                bounds.min.y + 1.0,
                bounds.max.y - 1.0);
            candidates.push_back({start, Point{start.x, laneY}, Point{end.x, laneY}, end});

            const double laneX = std::clamp(
                (start.x + end.x) / 2.0 + sign * offset + static_cast<double>(laneIndex % 3) * 0.25,
                bounds.min.x + 1.0,
                bounds.max.x - 1.0);
            candidates.push_back({start, Point{laneX, start.y}, Point{laneX, end.y}, end});
        }
    }

    for (std::vector<Point>& candidate : candidates) {
        candidate = removeDuplicatePoints(candidate);
    }
    std::stable_sort(
        candidates.begin(), candidates.end(),
        [](const std::vector<Point>& a, const std::vector<Point>& b) {
            return pathLength(a) < pathLength(b);
        });
    return candidates;
}

bool gridPointPassable(
    const Board& board,
    const std::string& netId,
    const std::string& layerId,
    double width,
    const Point& point,
    double clearance,
    const std::vector<PadLocation>& pads)
{
    if (!pointInsideBoard(board, point, width / 2.0 + clearance)) {
        return false;
    }
    for (const PadLocation& pad : pads) {
        if (pad.netId == netId || !padLayerMatches(pad, layerId)) {
            continue;
        }
        const double radius = std::max(pad.width, pad.height) / 2.0;
        if (pad.position.distanceTo(point) < radius + width / 2.0 + clearance - 1e-5) {
            return false;
        }
    }
    for (const Trace& trace : board.traces) {
        if (trace.netId == netId || trace.layerId != layerId) {
            continue;
        }
        for (std::size_t index = 1; index < trace.points.size(); ++index) {
            if (distancePointToSegment(point, trace.points[index - 1], trace.points[index]) <
                trace.width / 2.0 + width / 2.0 + clearance - 1e-5) {
                return false;
            }
        }
    }
    for (const Via& via : board.vias) {
        if (via.netId == netId) {
            continue;
        }
        if (via.position.distanceTo(point) <
            via.diameter / 2.0 + width / 2.0 + clearance - 1e-5) {
            return false;
        }
    }
    return true;
}

std::vector<Point> simplifyPolyline(const std::vector<Point>& points)
{
    if (points.size() <= 2) {
        return points;
    }
    std::vector<Point> result;
    result.push_back(points.front());
    for (std::size_t index = 1; index + 1 < points.size(); ++index) {
        const Point ab{points[index].x - points[index - 1].x,
                       points[index].y - points[index - 1].y};
        const Point bc{points[index + 1].x - points[index].x,
                       points[index + 1].y - points[index].y};
        const double cross = ab.x * bc.y - ab.y * bc.x;
        if (std::fabs(cross) > 1e-6) {
            result.push_back(points[index]);
        }
    }
    result.push_back(points.back());
    return result;
}

std::vector<Point> gridRoute(
    const Board& board,
    const std::string& netId,
    const std::string& layerId,
    const Point& start,
    const Point& end,
    double width,
    double clearance,
    const std::vector<PadLocation>& pads)
{
    constexpr double cell = 0.5;
    const Bounds bounds = boardBounds(board);
    if (!bounds.valid) {
        return {};
    }

    const int columns = static_cast<int>(std::ceil((bounds.width() + cell) / cell));
    const int rows = static_cast<int>(std::ceil((bounds.height() + cell) / cell));
    if (columns <= 0 || rows <= 0) {
        return {};
    }

    const auto toCell = [&](const Point& point) {
        return std::pair<int, int>{
            std::clamp(static_cast<int>(std::lround((point.x - bounds.min.x) / cell)), 0, columns - 1),
            std::clamp(static_cast<int>(std::lround((point.y - bounds.min.y) / cell)), 0, rows - 1),
        };
    };
    const auto toPoint = [&](int x, int y) {
        return Point{bounds.min.x + static_cast<double>(x) * cell,
                     bounds.min.y + static_cast<double>(y) * cell};
    };
    const auto indexOf = [&](int x, int y) { return y * columns + x; };

    const auto [startX, startY] = toCell(start);
    const auto [endX, endY] = toCell(end);
    const int startIndex = indexOf(startX, startY);
    const int endIndex = indexOf(endX, endY);
    const int cellCount = columns * rows;
    const double infinity = std::numeric_limits<double>::max();
    std::vector<double> distance(static_cast<std::size_t>(cellCount), infinity);
    std::vector<int> parent(static_cast<std::size_t>(cellCount), -1);
    std::vector<unsigned char> closed(static_cast<std::size_t>(cellCount), 0);

    using QueueItem = std::pair<double, int>;
    std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> open;
    distance[static_cast<std::size_t>(startIndex)] = 0.0;
    open.push({0.0, startIndex});

    const std::array<std::pair<int, int>, 8> directions = {
        std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1},
        std::pair{1, 1}, std::pair{1, -1}, std::pair{-1, 1}, std::pair{-1, -1},
    };

    while (!open.empty()) {
        const int current = open.top().second;
        open.pop();
        if (closed[static_cast<std::size_t>(current)] != 0) {
            continue;
        }
        closed[static_cast<std::size_t>(current)] = 1;
        if (current == endIndex) {
            break;
        }

        const int x = current % columns;
        const int y = current / columns;
        for (const auto& [dx, dy] : directions) {
            const int nx = x + dx;
            const int ny = y + dy;
            if (nx < 0 || nx >= columns || ny < 0 || ny >= rows) {
                continue;
            }
            const int next = indexOf(nx, ny);
            if (closed[static_cast<std::size_t>(next)] != 0) {
                continue;
            }
            const Point nextPoint = toPoint(nx, ny);
            if ((next != startIndex && next != endIndex &&
                 !gridPointPassable(
                     board, netId, layerId, width, nextPoint, clearance, pads))) {
                continue;
            }
            if (dx != 0 && dy != 0 &&
                (!gridPointPassable(
                     board, netId, layerId, width, toPoint(x + dx, y), clearance, pads) ||
                 !gridPointPassable(
                     board, netId, layerId, width, toPoint(x, y + dy), clearance, pads))) {
                continue;
            }
            const double step = (dx == 0 || dy == 0) ? cell : std::sqrt(2.0) * cell;
            const double candidate = distance[static_cast<std::size_t>(current)] + step;
            if (candidate + 1e-9 < distance[static_cast<std::size_t>(next)]) {
                distance[static_cast<std::size_t>(next)] = candidate;
                parent[static_cast<std::size_t>(next)] = current;
                const double heuristic =
                    std::hypot(static_cast<double>(endX - nx), static_cast<double>(endY - ny)) *
                    cell;
                open.push({candidate + heuristic, next});
            }
        }
    }

    if (parent[static_cast<std::size_t>(endIndex)] < 0 && startIndex != endIndex) {
        return {};
    }

    std::vector<Point> points;
    for (int current = endIndex; current >= 0; current = parent[static_cast<std::size_t>(current)]) {
        const int x = current % columns;
        const int y = current / columns;
        points.push_back(toPoint(x, y));
        if (current == startIndex) {
            break;
        }
    }
    std::reverse(points.begin(), points.end());
    if (points.empty()) {
        points.push_back(start);
    } else {
        points.front() = start;
    }
    points.push_back(end);
    return simplifyPolyline(removeDuplicatePoints(points));
}

std::string nextTraceId(const Board& board)
{
    std::size_t index = board.traces.size() + 1;
    while (board.findTrace("trc:auto:" + std::to_string(index))) {
        ++index;
    }
    return "trc:auto:" + std::to_string(index);
}

std::string nextViaId(const Board& board)
{
    std::size_t index = board.vias.size() + 1;
    while (board.findVia("via:auto:" + std::to_string(index))) {
        ++index;
    }
    return "via:auto:" + std::to_string(index);
}

bool addViaInternal(
    Board& board,
    const std::string& netId,
    const Point& position,
    const std::string& fromLayer,
    const std::string& toLayer,
    double diameter,
    double drill,
    std::string& error)
{
    if (!board.findNetById(netId)) {
        error = "net does not exist: " + netId;
        return false;
    }
    if (!isCopperLayer(board, fromLayer) || !isCopperLayer(board, toLayer) ||
        fromLayer == toLayer) {
        error = "via must connect two different copper layers";
        return false;
    }
    if (!pointInsideBoard(board, position)) {
        error = "via is outside the board";
        return false;
    }
    if (drill <= 0.0 || diameter <= drill) {
        error = "via diameter must be larger than its drill";
        return false;
    }
    for (const Via& other : board.vias) {
        if (other.netId != netId &&
            other.position.distanceTo(position) <
                (other.diameter + diameter) / 2.0 + 0.2) {
            error = "via conflicts with via " + other.id;
            return false;
        }
    }
    for (const Trace& trace : board.traces) {
        if (trace.netId == netId) {
            continue;
        }
        for (std::size_t index = 1; index < trace.points.size(); ++index) {
            if (distancePointToSegment(
                    position, trace.points[index - 1], trace.points[index]) <
                diameter / 2.0 + trace.width / 2.0 + 0.2) {
                error = "via conflicts with trace " + trace.id;
                return false;
            }
        }
    }

    Via via;
    via.id = nextViaId(board);
    via.netId = netId;
    via.position = position;
    via.fromLayer = fromLayer;
    via.toLayer = toLayer;
    via.diameter = diameter;
    via.drill = drill;
    board.addVia(via);
    return true;
}

struct NearestEdge {
    std::size_t a = 0;
    std::size_t b = 0;
    double distance = 0;
};

std::vector<NearestEdge> minimumSpanningEdges(const std::vector<PadLocation>& pads)
{
    std::vector<NearestEdge> edges;
    if (pads.size() < 2) {
        return edges;
    }
    std::vector<bool> used(pads.size(), false);
    used[0] = true;
    while (edges.size() + 1 < pads.size()) {
        NearestEdge best;
        best.distance = std::numeric_limits<double>::max();
        for (std::size_t a = 0; a < pads.size(); ++a) {
            if (!used[a]) {
                continue;
            }
            for (std::size_t b = 0; b < pads.size(); ++b) {
                if (used[b]) {
                    continue;
                }
                const double distance = pads[a].position.distanceTo(pads[b].position);
                if (distance < best.distance) {
                    best = {a, b, distance};
                }
            }
        }
        if (best.distance == std::numeric_limits<double>::max()) {
            break;
        }
        used[best.b] = true;
        edges.push_back(best);
    }
    return edges;
}

} // namespace

ManualRouteResult RoutingService::addManualTrace(
    Board& board,
    const std::string& netId,
    const std::string& layerId,
    double width,
    const std::vector<Point>& points,
    double clearance)
{
    ManualRouteResult result;
    if (!board.findNetById(netId)) {
        result.message = "net does not exist";
        return result;
    }
    if (!isCopperLayer(board, layerId)) {
        result.message = "trace layer is not copper";
        return result;
    }
    if (width <= 0.0 || points.size() < 2) {
        result.message = "trace requires positive width and at least two points";
        return result;
    }
    const auto pads = collectPads(board);
    if (!tracePathClear(board, netId, layerId, width, points, clearance, pads)) {
        result.message = "trace violates board bounds or clearance";
        return result;
    }

    Trace trace;
    trace.id = nextTraceId(board);
    trace.netId = netId;
    trace.layerId = layerId;
    trace.width = width;
    trace.points = removeDuplicatePoints(points);
    if (trace.points.size() < 2) {
        result.message = "trace points collapse to a single position";
        return result;
    }
    board.addTrace(trace);
    result.ok = true;
    result.traceId = trace.id;
    result.message = "trace created";
    return result;
}

bool RoutingService::placeVia(
    Board& board,
    const std::string& netId,
    const Point& position,
    const std::string& fromLayer,
    const std::string& toLayer,
    double diameter,
    double drill,
    std::string& error)
{
    return addViaInternal(board, netId, position, fromLayer, toLayer, diameter, drill, error);
}

RouteResult RoutingService::autoRoute(Board& board, double clearance)
{
    RouteResult result;
    const std::vector<PadLocation> allPads = collectPads(board);
    std::vector<std::pair<std::string, std::string>> netList;
    for (const Net& net : board.nets.nets()) {
        netList.emplace_back(net.id, net.name);
    }
    for (const auto& netEntry : netList) {
        const std::string& netId = netEntry.first;
        const std::string& netName = netEntry.second;
        const std::vector<PadLocation> pads = collectPads(board, netId);
        if (pads.size() < 2) {
            ++result.routedNetCount;
            continue;
        }

        bool netComplete = true;
        const std::vector<NearestEdge> edges = minimumSpanningEdges(pads);
        for (std::size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex) {
            const PadLocation& startPad = pads[edges[edgeIndex].a];
            const PadLocation& endPad = pads[edges[edgeIndex].b];
            if (startPad.position.distanceTo(endPad.position) <= 1e-4) {
                continue;
            }

            bool routed = false;
            const std::vector<std::string> layers = {
                startPad.layerId,
                startPad.layerId == "F.Cu" ? "B.Cu" : "F.Cu",
            };
            for (std::size_t layerIndex = 0; layerIndex < layers.size() && !routed; ++layerIndex) {
                const std::string& layer = layers[layerIndex];
                auto candidates = routeCandidates(
                    startPad.position, endPad.position, edgeIndex + layerIndex * 4, board);
                std::vector<Point> gridPath = gridRoute(
                    board, netId, layer, startPad.position, endPad.position,
                    0.30, clearance, allPads);
                if (gridPath.size() >= 2) {
                    candidates.push_back(std::move(gridPath));
                }
                for (const std::vector<Point>& path : candidates) {
                    if (!tracePathClear(
                            board, netId, layer, 0.30, path, clearance, allPads)) {
                        continue;
                    }

                    Board trial = board;
                    Trace trace;
                    trace.id = nextTraceId(trial);
                    trace.netId = netId;
                    trace.layerId = layer;
                    trace.width = 0.30;
                    trace.points = path;
                    trial.addTrace(trace);

                    bool viasOk = true;
                    std::size_t newViaCount = 0;
                    if (layer != startPad.layerId) {
                        std::string error;
                        if (addViaInternal(
                                trial, netId, startPad.position, startPad.layerId, layer,
                                0.6, 0.3, error)) {
                            ++newViaCount;
                        } else {
                            viasOk = false;
                        }
                    }
                    if (layer != endPad.layerId) {
                        std::string error;
                        if (addViaInternal(
                                trial, netId, endPad.position, endPad.layerId, layer,
                                0.6, 0.3, error)) {
                            ++newViaCount;
                        } else {
                            viasOk = false;
                        }
                    }
                    if (!viasOk) {
                        continue;
                    }
                    board = std::move(trial);
                    ++result.traceCount;
                    result.viaCount += newViaCount;
                    routed = true;
                    break;
                }
            }
            if (!routed) {
                netComplete = false;
                break;
            }
        }

        if (netComplete) {
            ++result.routedNetCount;
        } else {
            result.unresolvedNets.push_back(netName);
        }
    }

    result.ok = result.unresolvedNets.empty();
    result.message = result.ok
        ? "all nets routed"
        : std::to_string(result.unresolvedNets.size()) + " nets could not be routed";
    return result;
}

bool RoutingService::addGroundPour(
    Board& board,
    const std::string& netId,
    const std::string& layerId,
    double margin,
    double clearance,
    std::string& error)
{
    if (!board.findNetById(netId) || !isCopperLayer(board, layerId)) {
        error = "invalid net or copper layer";
        return false;
    }
    const Bounds bounds = boardBounds(board);
    if (!bounds.valid || bounds.width() <= margin * 2.0 || bounds.height() <= margin * 2.0) {
        error = "board is too small for copper pour";
        return false;
    }

    CopperPour pour;
    pour.id = "pour:auto:" + netId;
    pour.netId = netId;
    pour.layerId = layerId;
    pour.clearance = clearance;
    pour.outline = {
        {bounds.min.x + margin, bounds.min.y + margin},
        {bounds.max.x - margin, bounds.min.y + margin},
        {bounds.max.x - margin, bounds.max.y - margin},
        {bounds.min.x + margin, bounds.max.y - margin},
    };

    const auto addRectHole = [&](const Bounds& source) {
        const Bounds hole = source.expanded(clearance);
        if (hole.max.x <= pour.outline[0].x || hole.min.x >= pour.outline[1].x ||
            hole.max.y <= pour.outline[0].y || hole.min.y >= pour.outline[3].y) {
            return;
        }
        pour.holes.push_back({
            {hole.min.x, hole.min.y},
            {hole.max.x, hole.min.y},
            {hole.max.x, hole.max.y},
            {hole.min.x, hole.max.y},
        });
    };

    for (const Component& component : board.components) {
        const Footprint* footprint = board.findFootprint(component.footprintId);
        if (!footprint || (!component.onTop && layerId != "B.Cu")) {
            continue;
        }
        for (const Pad& pad : footprint->pads) {
            const auto binding = std::find_if(
                component.padNets.begin(), component.padNets.end(),
                [&](const PadNet& item) {
                    return normalizeIdentifier(item.padNumber) == normalizeIdentifier(pad.number);
                });
            if (binding == component.padNets.end() || binding->netId == netId) {
                continue;
            }
            if (padLayerMatches(
                    PadLocation{"", "", "", binding->netId, component.onTop ? "F.Cu" : "B.Cu",
                                Point{}, pad.width, pad.height, pad.drill},
                    layerId)) {
                addRectHole(padBounds(component, pad));
            }
        }
    }
    for (const Trace& trace : board.traces) {
        if (trace.netId == netId || trace.layerId != layerId) {
            continue;
        }
        for (std::size_t index = 1; index < trace.points.size(); ++index) {
            Bounds segment;
            segment.include(trace.points[index - 1]);
            segment.include(trace.points[index]);
            segment = segment.expanded(trace.width / 2.0);
            addRectHole(segment);
        }
    }
    for (const Via& via : board.vias) {
        if (via.netId == netId) {
            continue;
        }
        Bounds viaBounds;
        viaBounds.include(Point{via.position.x - via.diameter / 2.0,
                                via.position.y - via.diameter / 2.0});
        viaBounds.include(Point{via.position.x + via.diameter / 2.0,
                                via.position.y + via.diameter / 2.0});
        addRectHole(viaBounds);
    }

    if (CopperPour* old = board.findCopperPour(pour.id)) {
        *old = pour;
    } else {
        board.addCopperPour(std::move(pour));
    }
    return true;
}

ValidationResult RoutingService::validate(const Board& board, double clearance)
{
    ValidationResult result;
    const auto pads = collectPads(board);

    for (const Net& net : board.nets.nets()) {
        std::vector<PadLocation> netPads;
        for (const PadLocation& pad : pads) {
            if (pad.netId == net.id) {
                netPads.push_back(pad);
            }
        }
        if (netPads.size() <= 1) {
            ++result.connectedNetCount;
            continue;
        }

        std::vector<Trace> netTraces;
        for (const Trace& trace : board.traces) {
            if (trace.netId == net.id) {
                netTraces.push_back(trace);
            }
        }
        std::vector<Via> netVias;
        for (const Via& via : board.vias) {
            if (via.netId == net.id) {
                netVias.push_back(via);
            }
        }
        std::vector<CopperPour> pours;
        for (const CopperPour& pour : board.copperPours) {
            if (pour.netId == net.id) {
                pours.push_back(pour);
            }
        }

        const std::size_t padOffset = 0;
        const std::size_t traceOffset = padOffset + netPads.size();
        const std::size_t viaOffset = traceOffset + netTraces.size();
        const std::size_t pourOffset = viaOffset + netVias.size();
        DisjointSet groups(pourOffset + pours.size());

        for (std::size_t padIndex = 0; padIndex < netPads.size(); ++padIndex) {
            for (std::size_t traceIndex = 0; traceIndex < netTraces.size(); ++traceIndex) {
                if (traceTouchesPad(netTraces[traceIndex], netPads[padIndex])) {
                    groups.unite(padOffset + padIndex, traceOffset + traceIndex);
                }
            }
            for (std::size_t viaIndex = 0; viaIndex < netVias.size(); ++viaIndex) {
                if (padLayerMatches(netPads[padIndex], netVias[viaIndex].fromLayer) &&
                    netPads[padIndex].position.distanceTo(netVias[viaIndex].position) <=
                        std::max(netPads[padIndex].width, netPads[padIndex].height) / 2.0 + 1e-4) {
                    groups.unite(padOffset + padIndex, viaOffset + viaIndex);
                }
            }
            for (std::size_t pourIndex = 0; pourIndex < pours.size(); ++pourIndex) {
                if (padLayerMatches(netPads[padIndex], pours[pourIndex].layerId) &&
                    polygonContains(
                        netPads[padIndex].position, pours[pourIndex].outline,
                        pours[pourIndex].holes)) {
                    groups.unite(padOffset + padIndex, pourOffset + pourIndex);
                }
            }
        }
        for (std::size_t a = 0; a < netTraces.size(); ++a) {
            for (std::size_t b = a + 1; b < netTraces.size(); ++b) {
                if (tracesTouch(netTraces[a], netTraces[b])) {
                    groups.unite(traceOffset + a, traceOffset + b);
                }
            }
            for (std::size_t viaIndex = 0; viaIndex < netVias.size(); ++viaIndex) {
                if (traceTouchesVia(netTraces[a], netVias[viaIndex])) {
                    groups.unite(traceOffset + a, viaOffset + viaIndex);
                }
            }
            for (std::size_t pourIndex = 0; pourIndex < pours.size(); ++pourIndex) {
                if (traceTouchesPour(netTraces[a], pours[pourIndex])) {
                    groups.unite(traceOffset + a, pourOffset + pourIndex);
                }
            }
        }
        for (std::size_t a = 0; a < netVias.size(); ++a) {
            for (std::size_t b = a + 1; b < netVias.size(); ++b) {
                if (netVias[a].position.distanceTo(netVias[b].position) <= 1e-4) {
                    groups.unite(viaOffset + a, viaOffset + b);
                }
            }
            for (std::size_t pourIndex = 0; pourIndex < pours.size(); ++pourIndex) {
                if ((netVias[a].fromLayer == pours[pourIndex].layerId ||
                     netVias[a].toLayer == pours[pourIndex].layerId) &&
                    polygonContains(netVias[a].position, pours[pourIndex].outline,
                                    pours[pourIndex].holes)) {
                    groups.unite(viaOffset + a, pourOffset + pourIndex);
                }
            }
        }

        const std::size_t root = groups.find(padOffset);
        bool connected = true;
        for (std::size_t index = 1; index < netPads.size(); ++index) {
            if (groups.find(padOffset + index) != root) {
                connected = false;
                break;
            }
        }
        if (connected) {
            ++result.connectedNetCount;
        } else {
            ++result.unconnectedNetCount;
            result.unconnectedNets.push_back(net.name);
            result.issues.push_back("NET " + net.name + " is not fully connected");
        }
    }

    for (const Trace& trace : board.traces) {
        for (const Point& point : trace.points) {
            if (!pointInsideBoard(board, point)) {
                result.issues.push_back("trace " + trace.id + " leaves the board");
                break;
            }
        }
    }
    for (std::size_t a = 0; a < board.traces.size(); ++a) {
        const Trace& first = board.traces[a];
        for (std::size_t b = a + 1; b < board.traces.size(); ++b) {
            const Trace& second = board.traces[b];
            if (first.netId == second.netId || first.layerId != second.layerId) {
                continue;
            }
            for (std::size_t i = 1; i < first.points.size(); ++i) {
                for (std::size_t j = 1; j < second.points.size(); ++j) {
                    const double distance = distanceSegmentToSegment(
                        first.points[i - 1], first.points[i],
                        second.points[j - 1], second.points[j]);
                    if (distance < first.width / 2.0 + second.width / 2.0 + clearance - 1e-5) {
                        result.issues.push_back(
                            "clearance: traces " + first.id + " and " + second.id);
                    }
                }
            }
        }
    }
    for (const Trace& trace : board.traces) {
        for (const PadLocation& pad : pads) {
            if (trace.netId == pad.netId || !padLayerMatches(pad, trace.layerId)) {
                continue;
            }
            const double radius = std::max(pad.width, pad.height) / 2.0;
            for (std::size_t index = 1; index < trace.points.size(); ++index) {
                if (distancePointToSegment(pad.position, trace.points[index - 1],
                                           trace.points[index]) <
                    radius + trace.width / 2.0 + clearance - 1e-5) {
                    result.issues.push_back(
                        "clearance: trace " + trace.id + " and pad " +
                        pad.reference + "." + pad.padNumber);
                }
            }
        }
        for (const Via& via : board.vias) {
            if (trace.netId == via.netId) {
                continue;
            }
            for (std::size_t index = 1; index < trace.points.size(); ++index) {
                if (distancePointToSegment(via.position, trace.points[index - 1],
                                           trace.points[index]) <
                    via.diameter / 2.0 + trace.width / 2.0 + clearance - 1e-5) {
                    result.issues.push_back(
                        "clearance: trace " + trace.id + " and via " + via.id);
                }
            }
        }
    }
    for (const CopperPour& pour : board.copperPours) {
        for (const Trace& trace : board.traces) {
            if (trace.netId == pour.netId || trace.layerId != pour.layerId) {
                continue;
            }
            for (const Point& point : trace.points) {
                if (polygonContains(point, pour.outline, pour.holes)) {
                    result.issues.push_back(
                        "clearance: copper pour " + pour.id + " contains trace " + trace.id);
                }
            }
        }
        for (const PadLocation& pad : pads) {
            if (pad.netId == pour.netId || !padLayerMatches(pad, pour.layerId)) {
                continue;
            }
            if (polygonContains(pad.position, pour.outline, pour.holes)) {
                result.issues.push_back(
                    "clearance: copper pour " + pour.id + " contains pad " +
                    pad.reference + "." + pad.padNumber);
            }
        }
    }

    result.ok = result.issues.empty() && result.unconnectedNetCount == 0;
    return result;
}

std::vector<std::pair<Point, Point>> RoutingService::ratsnest(const Board& board)
{
    std::vector<std::pair<Point, Point>> result;
    for (const Net& net : board.nets.nets()) {
        const std::vector<PadLocation> pads = collectPads(board, net.id);
        const std::vector<NearestEdge> edges = minimumSpanningEdges(pads);
        for (const NearestEdge& edge : edges) {
            result.emplace_back(pads[edge.a].position, pads[edge.b].position);
        }
    }
    return result;
}

std::string ValidationResult::summary() const
{
    std::ostringstream stream;
    stream << "connected nets: " << connectedNetCount
           << ", unconnected nets: " << unconnectedNetCount
           << ", issues: " << issues.size();
    for (const std::string& issue : issues) {
        stream << "\n- " << issue;
    }
    return stream.str();
}

} // namespace pcb
