#pragma once

#include "core/Board.h"

#include <string>
#include <vector>

namespace pcb {

struct Bounds {
    Point min;
    Point max;
    bool valid = false;

    void include(const Point& p);
    [[nodiscard]] double width() const;
    [[nodiscard]] double height() const;
    [[nodiscard]] Point center() const;
    [[nodiscard]] bool contains(const Point& p) const;
    [[nodiscard]] bool intersects(const Bounds& other) const;
    [[nodiscard]] Bounds expanded(double amount) const;
};

struct PadLocation {
    std::string componentId;
    std::string reference;
    std::string padNumber;
    std::string netId;
    std::string layerId;
    Point position;
    double width = 0;
    double height = 0;
    double drill = 0;
};

Point rotatePoint(const Point& point, const Point& center, double degrees);
Point padPosition(const Component& component, const Pad& pad);
Bounds padBounds(const Component& component, const Pad& pad);
Bounds componentBounds(const Board& board, const Component& component);
Bounds boardBounds(const Board& board);
const Pad* findPad(const Footprint& footprint, const std::string& number);
double distancePointToSegment(const Point& p, const Point& a, const Point& b);
double distanceSegmentToSegment(const Point& a, const Point& b, const Point& c, const Point& d);
bool pointOnSegment(const Point& p, const Point& a, const Point& b, double tolerance = 1e-4);
bool pointInPolygon(const Point& p, const std::vector<Point>& polygon);
bool polygonContains(const Point& p, const std::vector<Point>& outer,
                     const std::vector<std::vector<Point>>& holes);
bool pointInsideBoard(const Board& board, const Point& point, double margin = 0.0);
double polygonArea(const std::vector<Point>& polygon);
double traceLength(const Trace& trace);
std::vector<PadLocation> collectPads(const Board& board, const std::string& netId = "");
std::string normalizeIdentifier(const std::string& value);

} // namespace pcb
