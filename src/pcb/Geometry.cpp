#include "pcb/Geometry.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

namespace pcb {

namespace {

constexpr double kPi = 3.14159265358979323846;

Point subtract(const Point& a, const Point& b)
{
    return Point{a.x - b.x, a.y - b.y};
}

double dot(const Point& a, const Point& b)
{
    return a.x * b.x + a.y * b.y;
}

double cross(const Point& a, const Point& b)
{
    return a.x * b.y - a.y * b.x;
}

} // namespace

void Bounds::include(const Point& p)
{
    if (!valid) {
        min = max = p;
        valid = true;
        return;
    }
    min.x = std::min(min.x, p.x);
    min.y = std::min(min.y, p.y);
    max.x = std::max(max.x, p.x);
    max.y = std::max(max.y, p.y);
}

double Bounds::width() const
{
    return valid ? max.x - min.x : 0.0;
}

double Bounds::height() const
{
    return valid ? max.y - min.y : 0.0;
}

Point Bounds::center() const
{
    return valid ? Point{(min.x + max.x) / 2.0, (min.y + max.y) / 2.0} : Point{};
}

bool Bounds::contains(const Point& p) const
{
    return valid && p.x >= min.x - kEps && p.x <= max.x + kEps &&
           p.y >= min.y - kEps && p.y <= max.y + kEps;
}

bool Bounds::intersects(const Bounds& other) const
{
    return valid && other.valid && min.x < other.max.x - kEps &&
           max.x > other.min.x + kEps && min.y < other.max.y - kEps &&
           max.y > other.min.y + kEps;
}

Bounds Bounds::expanded(double amount) const
{
    if (!valid) {
        return {};
    }
    Bounds result;
    result.min = Point{min.x - amount, min.y - amount};
    result.max = Point{max.x + amount, max.y + amount};
    result.valid = true;
    return result;
}

Point rotatePoint(const Point& point, const Point& center, double degrees)
{
    const double radians = degrees * kPi / 180.0;
    const double cosine = std::cos(radians);
    const double sine = std::sin(radians);
    const Point local = subtract(point, center);
    return Point{center.x + local.x * cosine - local.y * sine,
                 center.y + local.x * sine + local.y * cosine};
}

Point padPosition(const Component& component, const Pad& pad)
{
    Point local = pad.pos;
    if (!component.onTop) {
        local.x = -local.x;
    }
    const Point rotated = rotatePoint(local, Point{}, component.rotation);
    return Point{rotated.x + component.position.x, rotated.y + component.position.y};
}

Bounds padBounds(const Component& component, const Pad& pad)
{
    const Point center = padPosition(component, pad);
    Bounds bounds;
    const double halfWidth = pad.width / 2.0;
    const double halfHeight = pad.height / 2.0;
    for (const Point& corner : {
             Point{-halfWidth, -halfHeight},
             Point{halfWidth, -halfHeight},
             Point{halfWidth, halfHeight},
             Point{-halfWidth, halfHeight},
         }) {
        const Point rotated = rotatePoint(corner, Point{}, component.rotation);
        bounds.include(Point{center.x + rotated.x, center.y + rotated.y});
    }
    return bounds;
}

Bounds componentBounds(const Board& board, const Component& component)
{
    Bounds bounds;
    const Footprint* footprint = board.findFootprint(component.footprintId);
    if (!footprint) {
        return bounds;
    }
    for (const Pad& pad : footprint->pads) {
        const Bounds padBox = padBounds(component, pad);
        bounds.include(padBox.min);
        bounds.include(padBox.max);
    }
    if (!bounds.valid) {
        bounds.include(component.position);
    }
    return bounds;
}

Bounds boardBounds(const Board& board)
{
    Bounds bounds;
    for (const Point& point : board.outline.points) {
        bounds.include(point);
    }
    return bounds;
}

const Pad* findPad(const Footprint& footprint, const std::string& number)
{
    const std::string normalized = normalizeIdentifier(number);
    const auto iterator = std::find_if(
        footprint.pads.begin(), footprint.pads.end(),
        [&](const Pad& pad) { return normalizeIdentifier(pad.number) == normalized; });
    return iterator == footprint.pads.end() ? nullptr : &*iterator;
}

double distancePointToSegment(const Point& p, const Point& a, const Point& b)
{
    const Point ab = subtract(b, a);
    const double lengthSquared = dot(ab, ab);
    if (lengthSquared <= kEps) {
        return p.distanceTo(a);
    }
    const double t = std::clamp(dot(subtract(p, a), ab) / lengthSquared, 0.0, 1.0);
    return p.distanceTo(Point{a.x + ab.x * t, a.y + ab.y * t});
}

double distanceSegmentToSegment(const Point& a, const Point& b, const Point& c, const Point& d)
{
    const Point r = subtract(b, a);
    const Point s = subtract(d, c);
    const Point ca = subtract(c, a);
    const double denominator = cross(r, s);
    if (std::fabs(denominator) > kEps) {
        const double t = cross(ca, s) / denominator;
        const double u = cross(ca, r) / denominator;
        if (t >= 0 && t <= 1 && u >= 0 && u <= 1) {
            return 0.0;
        }
    }
    return std::min({
        distancePointToSegment(a, c, d),
        distancePointToSegment(b, c, d),
        distancePointToSegment(c, a, b),
        distancePointToSegment(d, a, b),
    });
}

bool pointOnSegment(const Point& p, const Point& a, const Point& b, double tolerance)
{
    return distancePointToSegment(p, a, b) <= tolerance &&
           p.x >= std::min(a.x, b.x) - tolerance &&
           p.x <= std::max(a.x, b.x) + tolerance &&
           p.y >= std::min(a.y, b.y) - tolerance &&
           p.y <= std::max(a.y, b.y) + tolerance;
}

bool pointInPolygon(const Point& p, const std::vector<Point>& polygon)
{
    if (polygon.size() < 3) {
        return false;
    }
    bool inside = false;
    for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const Point& a = polygon[i];
        const Point& b = polygon[j];
        if (pointOnSegment(p, a, b)) {
            return true;
        }
        const bool crosses = ((a.y > p.y) != (b.y > p.y)) &&
            (p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x);
        if (crosses) {
            inside = !inside;
        }
    }
    return inside;
}

bool polygonContains(const Point& p, const std::vector<Point>& outer,
                     const std::vector<std::vector<Point>>& holes)
{
    if (!pointInPolygon(p, outer)) {
        return false;
    }
    return !std::any_of(
        holes.begin(), holes.end(),
        [&](const std::vector<Point>& hole) { return pointInPolygon(p, hole); });
}

bool pointInsideBoard(const Board& board, const Point& point, double margin)
{
    if (!pointInPolygon(point, board.outline.points)) {
        return false;
    }
    const Bounds bounds = boardBounds(board);
    return point.x >= bounds.min.x + margin && point.x <= bounds.max.x - margin &&
           point.y >= bounds.min.y + margin && point.y <= bounds.max.y - margin;
}

double polygonArea(const std::vector<Point>& polygon)
{
    if (polygon.size() < 3) {
        return 0.0;
    }
    double twiceArea = 0.0;
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const Point& a = polygon[i];
        const Point& b = polygon[(i + 1) % polygon.size()];
        twiceArea += a.x * b.y - b.x * a.y;
    }
    return std::fabs(twiceArea) / 2.0;
}

double traceLength(const Trace& trace)
{
    double total = 0.0;
    for (std::size_t i = 1; i < trace.points.size(); ++i) {
        total += trace.points[i - 1].distanceTo(trace.points[i]);
    }
    return total;
}

std::vector<PadLocation> collectPads(const Board& board, const std::string& netId)
{
    std::vector<PadLocation> result;
    for (const Component& component : board.components) {
        const Footprint* footprint = board.findFootprint(component.footprintId);
        if (!footprint) {
            continue;
        }
        for (const Pad& pad : footprint->pads) {
            const auto binding = std::find_if(
                component.padNets.begin(), component.padNets.end(),
                [&](const PadNet& item) {
                    return normalizeIdentifier(item.padNumber) == normalizeIdentifier(pad.number);
                });
            if (binding == component.padNets.end()) {
                continue;
            }
            if (!netId.empty() && binding->netId != netId) {
                continue;
            }
            result.push_back(PadLocation{
                component.id,
                component.reference,
                pad.number,
                binding->netId,
                component.onTop ? "F.Cu" : "B.Cu",
                padPosition(component, pad),
                pad.width,
                pad.height,
                pad.drill,
            });
        }
    }
    return result;
}

std::string normalizeIdentifier(const std::string& value)
{
    std::string result;
    for (const unsigned char character : value) {
        if (!std::isspace(character)) {
            result.push_back(static_cast<char>(std::toupper(character)));
        }
    }
    return result;
}

} // namespace pcb
