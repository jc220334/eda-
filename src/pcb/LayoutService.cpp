#include "pcb/LayoutService.h"

#include "pcb/Geometry.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <vector>

namespace pcb {

namespace {

struct PlacementItem {
    std::size_t componentIndex = 0;
    double width = 0;
    double height = 0;
    std::size_t degree = 0;
};

double snap(double value, double grid)
{
    return grid > kEps ? std::round(value / grid) * grid : value;
}

} // namespace

LayoutResult LayoutService::autoPlace(
    Board& board,
    double margin,
    double spacing,
    double grid)
{
    LayoutResult result;
    const Bounds boardBox = boardBounds(board);
    if (!boardBox.valid) {
        result.message = "board outline is empty";
        return result;
    }
    if (board.components.empty()) {
        result.ok = true;
        result.message = "no components to place";
        return result;
    }

    std::vector<PlacementItem> items;
    items.reserve(board.components.size());
    for (std::size_t index = 0; index < board.components.size(); ++index) {
        Component& component = board.components[index];
        const Bounds bounds = componentBounds(board, component);
        if (!bounds.valid) {
            result.message = "component " + component.reference + " has no footprint";
            return result;
        }
        const std::size_t degree = component.padNets.size();
        items.push_back({index, bounds.width(), bounds.height(), degree});
    }

    std::stable_sort(items.begin(), items.end(), [](const PlacementItem& a, const PlacementItem& b) {
        if (a.degree != b.degree) {
            return a.degree > b.degree;
        }
        return a.width * a.height > b.width * b.height;
    });

    const double left = boardBox.min.x + margin;
    const double right = boardBox.max.x - margin;
    const double top = boardBox.min.y + margin;
    const double bottom = boardBox.max.y - margin;
    double x = left;
    double y = top;
    double rowHeight = 0.0;

    std::vector<Point> placements(board.components.size());
    for (const PlacementItem& item : items) {
        if (item.width > right - left || item.height > bottom - top) {
            result.message = "component " + board.components[item.componentIndex].reference +
                             " does not fit inside the board margin";
            return result;
        }
        if (x + item.width > right && x > left) {
            x = left;
            y += rowHeight + spacing;
            rowHeight = 0.0;
        }
        if (y + item.height > bottom) {
            result.message = "not enough board space to place all components";
            return result;
        }

        const Point center{x + item.width / 2.0, y + item.height / 2.0};
        placements[item.componentIndex] = Point{
            snap(center.x, grid),
            snap(center.y, grid),
        };
        x += item.width + spacing;
        rowHeight = std::max(rowHeight, item.height);
    }

    Bounds placedBounds;
    for (const PlacementItem& item : items) {
        const Point& center = placements[item.componentIndex];
        placedBounds.include(Point{center.x - item.width / 2.0, center.y - item.height / 2.0});
        placedBounds.include(Point{center.x + item.width / 2.0, center.y + item.height / 2.0});
    }
    if (placedBounds.valid) {
        const double shiftX = boardBox.center().x - placedBounds.center().x;
        const double shiftY = boardBox.center().y - placedBounds.center().y;
        for (Point& placement : placements) {
            placement.x = snap(placement.x + shiftX, grid);
            placement.y = snap(placement.y + shiftY, grid);
        }
    }

    for (std::size_t index = 0; index < board.components.size(); ++index) {
        board.components[index].position = placements[index];
    }

    result.ok = true;
    result.placedCount = board.components.size();
    result.message = "placed " + std::to_string(result.placedCount) + " components";
    return result;
}

bool LayoutService::rotateComponent(
    Board& board,
    const std::string& componentId,
    double deltaDegrees)
{
    Component* component = board.findComponent(componentId);
    if (!component) {
        return false;
    }
    component->rotation = std::fmod(component->rotation + deltaDegrees + 360.0, 360.0);
    if (component->rotation < 0.0) {
        component->rotation += 360.0;
    }
    return true;
}

bool LayoutService::placeComponent(
    Board& board,
    const std::string& componentId,
    const Point& position)
{
    Component* component = board.findComponent(componentId);
    if (!component || !pointInsideBoard(board, position)) {
        return false;
    }
    const Bounds oldBounds = componentBounds(board, *component);
    const Bounds boardBox = boardBounds(board);
    const Bounds moved = Bounds{
        Point{oldBounds.min.x - component->position.x + position.x,
              oldBounds.min.y - component->position.y + position.y},
        Point{oldBounds.max.x - component->position.x + position.x,
              oldBounds.max.y - component->position.y + position.y},
        true,
    };
    if (moved.min.x < boardBox.min.x || moved.min.y < boardBox.min.y ||
        moved.max.x > boardBox.max.x || moved.max.y > boardBox.max.y) {
        return false;
    }
    component->position = position;
    return true;
}

} // namespace pcb
