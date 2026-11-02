#include "pcb/NetlistImporter.h"

#include "io/Json.h"
#include "pcb/Geometry.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <utility>

namespace pcb {

namespace {

const JsonValue& Member(const JsonValue& object, const std::string& key, const std::string& where)
{
    (void)where;
    return object.get(key);
}

std::string RequiredString(
    const JsonValue& object,
    const std::string& key,
    const std::string& where,
    NetlistImportResult& result,
    const std::string& code = "NET-003")
{
    const JsonValue& value = Member(object, key, where);
    if (value.type() != JsonValue::Type::String || value.asString().empty()) {
        result.diagnostics.push_back({code, where + "." + key + " must be a non-empty string"});
        return {};
    }
    return value.asString();
}

double OptionalNumber(
    const JsonValue& object,
    const std::string& key,
    double fallback,
    const std::string& where,
    NetlistImportResult& result)
{
    if (!object.has(key)) {
        return fallback;
    }
    const JsonValue& value = object.get(key);
    if (value.type() != JsonValue::Type::Number) {
        result.diagnostics.push_back({"NET-003", where + "." + key + " must be a number"});
        return fallback;
    }
    return value.asNumber(fallback);
}

bool ReadJsonFile(const std::string& path, JsonValue& value, NetlistImportResult& result)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        result.diagnostics.push_back({"NET-001", "cannot open file: " + path});
        return false;
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    try {
        value = JsonValue::parse(buffer.str());
    } catch (const JsonError& error) {
        result.diagnostics.push_back({
            "NET-001",
            path + ":" + std::to_string(error.line) + ":" + std::to_string(error.col) +
                " " + error.what(),
        });
        return false;
    }
    if (value.type() != JsonValue::Type::Object) {
        result.diagnostics.push_back({"NET-003", path + ": root must be an object"});
        return false;
    }
    return true;
}

bool ParseArrayPoint(const JsonValue& value, Point& point, const std::string& where,
                     NetlistImportResult& result)
{
    if (value.type() != JsonValue::Type::Array || value.size() != 2 ||
        value.at(0).type() != JsonValue::Type::Number ||
        value.at(1).type() != JsonValue::Type::Number) {
        result.diagnostics.push_back({"NET-003", where + " must be [number, number]"});
        return false;
    }
    point = Point{value.at(0).asNumber() / 1000.0, value.at(1).asNumber() / 1000.0};
    return true;
}

bool ParseLibrary(const JsonValue& root, std::vector<Footprint>& footprints,
                  NetlistImportResult& result)
{
    if (root.type() != JsonValue::Type::Object) {
        result.diagnostics.push_back({"NET-002", "footprint library root must be an object"});
        return false;
    }
    const std::string format = RequiredString(root, "format", "library", result, "NET-002");
    if (format != "eda-course-footprint-library") {
        result.diagnostics.push_back({"NET-002", "unsupported footprint library format: " + format});
        return false;
    }
    if (!root.has("version") || root.get("version").type() != JsonValue::Type::Number ||
        root.get("version").asNumber() != 1) {
        result.diagnostics.push_back({"NET-002", "unsupported footprint library version"});
        return false;
    }

    const JsonValue& list = Member(root, "footprints", "library");
    if (list.type() != JsonValue::Type::Array || list.size() == 0) {
        result.diagnostics.push_back({"NET-003", "library.footprints must be non-empty"});
        return false;
    }

    std::set<std::string> footprintIds;
    for (std::size_t index = 0; index < list.size(); ++index) {
        const JsonValue& item = list.at(index);
        const std::string where = "library.footprints[" + std::to_string(index) + "]";
        if (item.type() != JsonValue::Type::Object) {
            result.diagnostics.push_back({"NET-003", where + " must be an object"});
            continue;
        }

        Footprint footprint;
        footprint.id = RequiredString(item, "id", where, result);
        footprint.name = RequiredString(item, "name", where, result);
        footprint.description = item.has("description") ? item.get("description").asString() : "";
        if (footprint.id.empty()) {
            continue;
        }
        if (!footprintIds.insert(footprint.id).second) {
            result.diagnostics.push_back({"NET-004", "duplicate footprint ID: " + footprint.id});
            continue;
        }

        const JsonValue& pads = Member(item, "pads", where);
        if (pads.type() != JsonValue::Type::Array || pads.size() == 0) {
            result.diagnostics.push_back({"NET-003", where + ".pads must be non-empty"});
            continue;
        }

        std::set<std::string> padIds;
        for (std::size_t padIndex = 0; padIndex < pads.size(); ++padIndex) {
            const JsonValue& padValue = pads.at(padIndex);
            const std::string padWhere = where + ".pads[" + std::to_string(padIndex) + "]";
            if (padValue.type() != JsonValue::Type::Object) {
                result.diagnostics.push_back({"NET-003", padWhere + " must be an object"});
                continue;
            }
            const std::string padId = RequiredString(padValue, "id", padWhere, result);
            if (!padId.empty() && !padIds.insert(padId).second) {
                result.diagnostics.push_back({"NET-004", "duplicate pad ID: " + padId});
            }

            Pad pad;
            if (padValue.has("pin") && padValue.get("pin").type() == JsonValue::Type::String) {
                pad.number = normalizeIdentifier(padValue.get("pin").asString());
            }
            pad.name = padValue.has("name") ? padValue.get("name").asString() :
                (pad.number.empty() ? padId : pad.number);
            if (!padValue.has("center") || !ParseArrayPoint(
                    padValue.get("center"), pad.pos, padWhere + ".center", result)) {
                continue;
            }
            if (!padValue.has("size") || padValue.get("size").type() != JsonValue::Type::Array ||
                padValue.get("size").size() != 2) {
                result.diagnostics.push_back({"NET-003", padWhere + ".size must be [width, height]"});
                continue;
            }
            pad.width = padValue.get("size").at(0).asNumber() / 1000.0;
            pad.height = padValue.get("size").at(1).asNumber() / 1000.0;
            if (padValue.has("shape")) {
                parsePadShape(padValue.get("shape").asString(), pad.shape);
            }
            pad.drill = std::max(0.0, OptionalNumber(padValue, "drill_um", 0.0, padWhere, result)) /
                        1000.0;
            footprint.pads.push_back(std::move(pad));
        }
        if (!footprint.pads.empty()) {
            footprints.push_back(std::move(footprint));
        }
    }
    return result.diagnostics.empty();
}

struct LogicalComponent {
    std::string reference;
    std::string footprintId;
};

struct LogicalConnection {
    std::string reference;
    std::string pin;
};

struct LogicalNet {
    std::string name;
    std::vector<LogicalConnection> connections;
};

bool ParseNetlist(const JsonValue& root, std::vector<LogicalComponent>& components,
                  std::vector<LogicalNet>& nets, NetlistImportResult& result)
{
    if (root.type() != JsonValue::Type::Object) {
        result.diagnostics.push_back({"NET-002", "netlist root must be an object"});
        return false;
    }
    const std::string format = RequiredString(root, "format", "netlist", result, "NET-002");
    if (format != "eda-course-netlist") {
        result.diagnostics.push_back({"NET-002", "unsupported netlist format: " + format});
        return false;
    }
    if (!root.has("version") || root.get("version").type() != JsonValue::Type::Number ||
        root.get("version").asNumber() != 1) {
        result.diagnostics.push_back({"NET-002", "unsupported netlist version"});
        return false;
    }

    const JsonValue& componentList = Member(root, "components", "netlist");
    if (componentList.type() != JsonValue::Type::Array || componentList.size() == 0) {
        result.diagnostics.push_back({"NET-003", "netlist.components must be non-empty"});
        return false;
    }

    std::set<std::string> references;
    for (std::size_t index = 0; index < componentList.size(); ++index) {
        const JsonValue& item = componentList.at(index);
        const std::string where = "components[" + std::to_string(index) + "]";
        if (item.type() != JsonValue::Type::Object) {
            result.diagnostics.push_back({"NET-003", where + " must be an object"});
            continue;
        }
        LogicalComponent component;
        component.reference = normalizeIdentifier(RequiredString(item, "ref", where, result));
        component.footprintId = RequiredString(item, "footprint", where, result);
        if (component.reference.empty()) {
            continue;
        }
        if (!references.insert(component.reference).second) {
            result.diagnostics.push_back({
                "NET-004",
                "duplicate component reference: " + component.reference,
            });
        }
        components.push_back(std::move(component));
    }

    const JsonValue& netList = Member(root, "nets", "netlist");
    if (netList.type() != JsonValue::Type::Array || netList.size() == 0) {
        result.diagnostics.push_back({"NET-003", "netlist.nets must be non-empty"});
        return false;
    }

    std::set<std::string> netNames;
    for (std::size_t netIndex = 0; netIndex < netList.size(); ++netIndex) {
        const JsonValue& item = netList.at(netIndex);
        const std::string where = "nets[" + std::to_string(netIndex) + "]";
        if (item.type() != JsonValue::Type::Object) {
            result.diagnostics.push_back({"NET-003", where + " must be an object"});
            continue;
        }
        LogicalNet net;
        net.name = RequiredString(item, "name", where, result);
        if (net.name.empty()) {
            continue;
        }
        if (!netNames.insert(net.name).second) {
            result.diagnostics.push_back({"NET-005", "duplicate net name: " + net.name});
        }

        const JsonValue& connections = Member(item, "connections", where);
        if (connections.type() != JsonValue::Type::Array || connections.size() < 2) {
            result.diagnostics.push_back({
                "NET-006",
                "net " + net.name + " must have at least two connections",
            });
            continue;
        }
        for (std::size_t connectionIndex = 0; connectionIndex < connections.size();
             ++connectionIndex) {
            const JsonValue& connectionValue = connections.at(connectionIndex);
            const std::string connectionWhere =
                where + ".connections[" + std::to_string(connectionIndex) + "]";
            if (connectionValue.type() != JsonValue::Type::Object) {
                result.diagnostics.push_back({"NET-003", connectionWhere + " must be an object"});
                continue;
            }
            LogicalConnection connection;
            connection.reference = normalizeIdentifier(
                RequiredString(connectionValue, "ref", connectionWhere, result));
            connection.pin = normalizeIdentifier(
                RequiredString(connectionValue, "pin", connectionWhere, result));
            net.connections.push_back(std::move(connection));
        }
        nets.push_back(std::move(net));
    }
    return result.diagnostics.empty();
}

PcbProject BuildProject(
    const std::vector<Footprint>& footprints,
    const std::vector<LogicalComponent>& components,
    const std::vector<LogicalNet>& nets,
    const NetlistImportOptions& options,
    NetlistImportResult& result)
{
    PcbProject project;
    project.name = "Imported PCB";
    project.description = "Imported from eda-course-netlist";
    project.unit = Unit::Millimeter;
    Board& board = project.board;
    board.outline.points = {
        {0.0, 0.0},
        {options.boardWidth, 0.0},
        {options.boardWidth, options.boardHeight},
        {0.0, options.boardHeight},
    };

    board.layers.addLayer(Layer{"F.Cu", "Top Copper", LayerType::Copper, true, 10, Color{"#d14b45"}});
    board.layers.addLayer(Layer{"B.Cu", "Bottom Copper", LayerType::Copper, true, 20, Color{"#4b77d1"}});
    board.layers.addLayer(Layer{"F.Silk", "Top Silkscreen", LayerType::SilkScreen, true, 30, Color{"#e8e8e8"}});
    board.layers.addLayer(Layer{"Edge", "Board Edge", LayerType::Edge, true, 40, Color{"#e5c54d"}});
    board.layers.addGroup(LayerGroup{"Copper Layers", {"F.Cu", "B.Cu"}, true});
    board.layers.addGroup(LayerGroup{"Silk Layers", {"F.Silk"}, true});

    for (const Footprint& footprint : footprints) {
        board.addFootprint(footprint);
    }
    for (const LogicalNet& net : nets) {
        board.addNet(net.name);
    }

    std::map<std::string, std::string> footprintByReference;
    for (std::size_t index = 0; index < components.size(); ++index) {
        const LogicalComponent& source = components[index];
        const Footprint* footprint = board.findFootprint(source.footprintId);
        if (!footprint) {
            result.diagnostics.push_back({
                "NET-008",
                source.reference + " references unknown footprint " + source.footprintId,
            });
            continue;
        }

        Component component;
        component.id = "cmp:" + source.reference;
        component.reference = source.reference;
        component.footprintId = source.footprintId;
        const int column = options.columns > 0 ? static_cast<int>(index) % options.columns : 0;
        const int row = options.columns > 0 ? static_cast<int>(index) / options.columns : 0;
        component.position = Point{
            options.firstX + column * options.columnSpacing,
            options.firstY + row * options.rowSpacing,
        };
        board.addComponent(component);
        footprintByReference[source.reference] = source.footprintId;
    }

    std::map<std::pair<std::string, std::string>, std::string> pinOwner;
    for (const LogicalNet& net : nets) {
        std::set<std::pair<std::string, std::string>> connectionsInNet;
        for (const LogicalConnection& connection : net.connections) {
            const std::pair<std::string, std::string> key{
                connection.reference, connection.pin};
            if (!connectionsInNet.insert(key).second) {
                result.diagnostics.push_back({
                    "NET-011",
                    "duplicate connection " + connection.reference + "." + connection.pin +
                        " in net " + net.name,
                });
                continue;
            }
            const auto componentIterator = footprintByReference.find(connection.reference);
            if (componentIterator == footprintByReference.end()) {
                result.diagnostics.push_back({
                    "NET-007",
                    "net " + net.name + " references unknown component " + connection.reference,
                });
                continue;
            }
            const Footprint* footprint = board.findFootprint(componentIterator->second);
            if (!footprint || !findPad(*footprint, connection.pin)) {
                result.diagnostics.push_back({
                    "NET-009",
                    connection.reference + " has no pin " + connection.pin,
                });
                continue;
            }

            const auto previous = pinOwner.find(key);
            if (previous != pinOwner.end()) {
                result.diagnostics.push_back({
                    "NET-010",
                    "pin " + connection.reference + "." + connection.pin +
                        " appears in nets " + previous->second + " and " + net.name,
                });
                continue;
            }
            pinOwner.emplace(key, net.name);

            Component* component = board.findByReference(connection.reference);
            if (component) {
                component->padNets.push_back(PadNet{connection.pin, "net:" + net.name});
            }
        }
    }

    result.componentCount = board.components.size();
    result.netCount = board.nets.nets().size();
    result.matchedPinCount = pinOwner.size();
    if (result.diagnostics.empty() &&
        !board.checkIntegrity().empty()) {
        for (const std::string& problem : board.checkIntegrity()) {
            result.diagnostics.push_back({"NET-011", problem});
        }
    }
    return project;
}

} // namespace

std::string NetlistImportResult::summary() const
{
    if (ok) {
        return "components=" + std::to_string(componentCount) +
               ", nets=" + std::to_string(netCount) +
               ", matched pins=" + std::to_string(matchedPinCount);
    }
    std::ostringstream stream;
    stream << "netlist import failed";
    for (const ImportDiagnostic& diagnostic : diagnostics) {
        stream << "\n" << diagnostic.code << ": " << diagnostic.message;
    }
    return stream.str();
}

NetlistImportResult NetlistImporter::importFiles(
    const std::string& netlistPath,
    const std::string& libraryPath,
    PcbProject& output,
    const NetlistImportOptions& options)
{
    NetlistImportResult opening;
    JsonValue netlistRoot;
    JsonValue libraryRoot;
    if (!ReadJsonFile(netlistPath, netlistRoot, opening) ||
        !ReadJsonFile(libraryPath, libraryRoot, opening)) {
        return opening;
    }
    return importStrings(netlistRoot.dump(false), libraryRoot.dump(false), output, options);
}

NetlistImportResult NetlistImporter::importStrings(
    const std::string& netlistText,
    const std::string& libraryText,
    PcbProject& output,
    const NetlistImportOptions& options)
{
    NetlistImportResult result;
    try {
        const JsonValue netlistRoot = JsonValue::parse(netlistText);
        const JsonValue libraryRoot = JsonValue::parse(libraryText);

        std::vector<Footprint> footprints;
        std::vector<LogicalComponent> components;
        std::vector<LogicalNet> nets;
        if (!ParseLibrary(libraryRoot, footprints, result) ||
            !ParseNetlist(netlistRoot, components, nets, result)) {
            result.ok = false;
            return result;
        }

        PcbProject project = BuildProject(footprints, components, nets, options, result);
        result.ok = result.diagnostics.empty();
        if (result.ok) {
            output = std::move(project);
        }
        return result;
    } catch (const JsonError& error) {
        result.diagnostics.push_back({
            "NET-001",
            std::string(error.what()) + " at line " + std::to_string(error.line) +
                ", column " + std::to_string(error.col),
        });
        result.ok = false;
        return result;
    }
}

} // namespace pcb
