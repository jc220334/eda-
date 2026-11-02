#pragma once

#include "core/Board.h"

#include <string>
#include <utility>
#include <vector>

namespace pcb {

struct ManualRouteResult {
    bool ok = false;
    std::string traceId;
    std::string message;
};

struct RouteResult {
    bool ok = false;
    std::size_t routedNetCount = 0;
    std::size_t traceCount = 0;
    std::size_t viaCount = 0;
    std::vector<std::string> unresolvedNets;
    std::string message;
};

struct ValidationResult {
    bool ok = false;
    std::size_t connectedNetCount = 0;
    std::size_t unconnectedNetCount = 0;
    std::vector<std::string> unconnectedNets;
    std::vector<std::string> issues;
    [[nodiscard]] std::string summary() const;
};

class RoutingService {
public:
    static ManualRouteResult addManualTrace(
        Board& board,
        const std::string& netId,
        const std::string& layerId,
        double width,
        const std::vector<Point>& points,
        double clearance = 0.25);

    static bool placeVia(
        Board& board,
        const std::string& netId,
        const Point& position,
        const std::string& fromLayer,
        const std::string& toLayer,
        double diameter,
        double drill,
        std::string& error);

    static RouteResult autoRoute(Board& board, double clearance = 0.25);
    static bool addGroundPour(
        Board& board,
        const std::string& netId,
        const std::string& layerId,
        double margin,
        double clearance,
        std::string& error);

    static ValidationResult validate(const Board& board, double clearance = 0.25);
    static std::vector<std::pair<Point, Point>> ratsnest(const Board& board);
};

} // namespace pcb
