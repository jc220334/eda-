#pragma once

#include "core/Board.h"

#include <string>

namespace pcb {

struct LayoutResult {
    bool ok = false;
    std::size_t placedCount = 0;
    std::string message;
};

class LayoutService {
public:
    static LayoutResult autoPlace(
        Board& board,
        double margin = 3.0,
        double spacing = 2.0,
        double grid = 0.5);

    static bool rotateComponent(Board& board, const std::string& componentId, double deltaDegrees);
    static bool placeComponent(Board& board, const std::string& componentId, const Point& position);
};

} // namespace pcb
