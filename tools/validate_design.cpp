#include "json_lite.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace {

namespace fs = std::filesystem;
using eda::json::Value;

class ValidationError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

[[noreturn]] void Fail(const std::string& message)
{
    throw ValidationError(message);
}

void Require(bool condition, const std::string& message)
{
    if (!condition) {
        Fail(message);
    }
}

std::string ReadFile(const fs::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        Fail(path.u8string() + ": cannot open file");
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

Value LoadObject(const fs::path& root, const fs::path& relativePath)
{
    const fs::path path = root / relativePath;
    try {
        Value value = eda::json::Parse(ReadFile(path));
        Require(value.IsObject(), relativePath.u8string() + ": root must be an object");
        return value;
    } catch (const eda::json::ParseError& error) {
        Fail(relativePath.u8string() + ": " + error.what());
    }
}

const Value& Member(const Value::Object& object, const std::string& key, const std::string& context)
{
    const auto iterator = object.find(key);
    if (iterator == object.end()) {
        Fail(context + "." + key + " is required");
    }
    return iterator->second;
}

const std::string& StringMember(
    const Value::Object& object,
    const std::string& key,
    const std::string& context)
{
    const Value& value = Member(object, key, context);
    if (!value.IsString()) {
        Fail(context + "." + key + " must be a string");
    }
    return value.AsString();
}

std::string Normalized(const Value& value, const std::string& context)
{
    if (!value.IsString()) {
        Fail(context + " must be a non-empty string");
    }

    std::string result;
    for (const unsigned char character : value.AsString()) {
        if (!std::isspace(character)) {
            result.push_back(static_cast<char>(std::toupper(character)));
        }
    }
    Require(!result.empty(), context + " must be a non-empty string");
    return result;
}

void RequireUnique(const std::vector<std::string>& values, const std::string& label)
{
    std::set<std::string> seen;
    for (const std::string& value : values) {
        Require(seen.insert(value).second, label + " contains duplicate: " + value);
    }
}

void RequirePoint(const Value& value, const std::string& context)
{
    Require(value.IsArray(), context + " must be [int, int]");
    const auto& coordinates = value.AsArray();
    Require(coordinates.size() == 2, context + " must be [int, int]");
    Require(coordinates[0].IsInteger(), context + "[0] must be an integer");
    Require(coordinates[1].IsInteger(), context + "[1] must be an integer");
}

void RequirePoints(const Value& value, const std::string& context, std::size_t minimum)
{
    Require(value.IsArray(), context + " must be an array");
    const auto& points = value.AsArray();
    Require(points.size() >= minimum, context + " needs at least " + std::to_string(minimum) + " points");
    for (std::size_t index = 0; index < points.size(); ++index) {
        RequirePoint(points[index], context + "[" + std::to_string(index) + "]");
    }
}

using PinsByFootprint = std::map<std::string, std::set<std::string>>;
using ConnectionKey = std::tuple<std::string, std::string, std::string>;
using PinKey = std::pair<std::string, std::string>;

PinsByFootprint ValidateLibrary(const Value& library)
{
    const auto& root = library.AsObject();
    Require(StringMember(root, "format", "library") == "eda-course-footprint-library",
            "library format mismatch");
    Require(Member(root, "version", "library").IsInteger() &&
                Member(root, "version", "library").AsInteger() == 1,
            "library version mismatch");
    Require(StringMember(root, "units", "library") == "um", "library units must be um");

    const Value& footprintsValue = Member(root, "footprints", "library");
    Require(footprintsValue.IsArray() && !footprintsValue.AsArray().empty(),
            "library footprints must be non-empty");

    PinsByFootprint result;
    for (std::size_t footprintIndex = 0; footprintIndex < footprintsValue.AsArray().size();
         ++footprintIndex) {
        const Value& footprintValue = footprintsValue.AsArray()[footprintIndex];
        Require(footprintValue.IsObject(), "footprint must be an object");
        const auto& footprint = footprintValue.AsObject();
        const std::string& footprintId = StringMember(footprint, "id", "footprint");
        Require(!footprintId.empty(), "footprint ID must be non-empty");
        Require(result.find(footprintId) == result.end(), "duplicate footprint ID: " + footprintId);

        RequirePoint(Member(footprint, "origin", footprintId), footprintId + ".origin");
        const Value& padsValue = Member(footprint, "pads", footprintId);
        const Value& graphicsValue = Member(footprint, "graphics", footprintId);
        Require(padsValue.IsArray(), footprintId + ".pads must be an array");
        Require(graphicsValue.IsArray(), footprintId + ".graphics must be an array");

        std::vector<std::string> padIds;
        std::set<std::string> pins;
        for (std::size_t padIndex = 0; padIndex < padsValue.AsArray().size(); ++padIndex) {
            const Value& padValue = padsValue.AsArray()[padIndex];
            Require(padValue.IsObject(), footprintId + ".pads must contain objects");
            const auto& pad = padValue.AsObject();
            const std::string& padId = StringMember(pad, "id", footprintId + ".pad");
            Require(!padId.empty(), footprintId + ".pad.id must be non-empty");
            padIds.push_back(padId);

            RequirePoint(Member(pad, "center", footprintId + "." + padId),
                         footprintId + "." + padId + ".center");
            RequirePoint(Member(pad, "size", footprintId + "." + padId),
                         footprintId + "." + padId + ".size");

            const Value& layers = Member(pad, "layer_set", footprintId + "." + padId);
            Require(layers.IsArray() && !layers.AsArray().empty(),
                    footprintId + "." + padId + ".layer_set must be non-empty");

            const Value& pinValue = Member(pad, "pin", footprintId + "." + padId);
            if (!pinValue.IsNull()) {
                pins.insert(Normalized(pinValue, footprintId + "." + padId + ".pin"));
            }

            const std::string& mount = StringMember(pad, "mount", footprintId + "." + padId);
            Require(mount == "smd" || mount == "plated_through" || mount == "non_plated",
                    footprintId + "." + padId + ".mount is invalid");
            const Value& drill = Member(pad, "drill_um", footprintId + "." + padId);
            Require(drill.IsInteger() && drill.AsInteger() >= 0,
                    footprintId + "." + padId + ".drill_um must be non-negative");
            if (mount == "smd") {
                Require(drill.AsInteger() == 0, footprintId + "." + padId + ": SMD drill must be zero");
            } else {
                Require(drill.AsInteger() > 0,
                        footprintId + "." + padId + ": through-hole drill must be positive");
            }
        }
        RequireUnique(padIds, "pad IDs in " + footprintId);
        result.emplace(footprintId, std::move(pins));
    }
    return result;
}

std::set<ConnectionKey> ValidateNetlist(
    const Value& netlist,
    const PinsByFootprint& pinsByFootprint)
{
    const auto& root = netlist.AsObject();
    Require(StringMember(root, "format", "netlist") == "eda-course-netlist",
            "netlist format mismatch");
    Require(Member(root, "version", "netlist").IsInteger() &&
                Member(root, "version", "netlist").AsInteger() == 1,
            "netlist version mismatch");

    const Value& componentsValue = Member(root, "components", "netlist");
    const Value& netsValue = Member(root, "nets", "netlist");
    Require(componentsValue.IsArray() && !componentsValue.AsArray().empty(),
            "netlist components must be non-empty");
    Require(netsValue.IsArray() && !netsValue.AsArray().empty(), "netlist nets must be non-empty");

    std::vector<std::string> refs;
    std::map<std::string, std::string> footprintByRef;
    for (std::size_t index = 0; index < componentsValue.AsArray().size(); ++index) {
        const Value& componentValue = componentsValue.AsArray()[index];
        Require(componentValue.IsObject(), "netlist component must be an object");
        const auto& component = componentValue.AsObject();
        const std::string ref = Normalized(
            Member(component, "ref", "component"),
            "components[" + std::to_string(index) + "].ref");
        const std::string& footprint =
            StringMember(component, "footprint", "components[" + std::to_string(index) + "]");
        Require(pinsByFootprint.find(footprint) != pinsByFootprint.end(),
                ref + " references unknown footprint " + footprint);
        refs.push_back(ref);
        footprintByRef.emplace(ref, footprint);
    }
    RequireUnique(refs, "component references");

    std::vector<std::string> netNames;
    for (std::size_t index = 0; index < netsValue.AsArray().size(); ++index) {
        const Value& netValue = netsValue.AsArray()[index];
        Require(netValue.IsObject(), "net must be an object");
        const auto& net = netValue.AsObject();
        const std::string& netName = StringMember(net, "name", "net");
        Require(!netName.empty(), "net name must be non-empty");
        netNames.push_back(netName);
    }
    RequireUnique(netNames, "net names");

    std::set<ConnectionKey> expectedConnections;
    std::map<PinKey, std::string> pinOwner;
    std::size_t totalConnectionCount = 0;
    for (std::size_t netIndex = 0; netIndex < netsValue.AsArray().size(); ++netIndex) {
        const auto& net = netsValue.AsArray()[netIndex].AsObject();
        const std::string& netName = StringMember(net, "name", "net");
        const Value& connections = Member(net, "connections", "net " + netName);
        Require(connections.IsArray() && connections.AsArray().size() >= 2,
                "net " + netName + " needs at least two connections");
        totalConnectionCount += connections.AsArray().size();

        for (std::size_t connectionIndex = 0; connectionIndex < connections.AsArray().size();
             ++connectionIndex) {
            const std::string context =
                "nets[" + std::to_string(netIndex) + "].connections[" +
                std::to_string(connectionIndex) + "]";
            const Value& connectionValue = connections.AsArray()[connectionIndex];
            Require(connectionValue.IsObject(), context + " must be an object");
            const auto& connection = connectionValue.AsObject();
            const std::string ref = Normalized(Member(connection, "ref", context), context + ".ref");
            const std::string pin = Normalized(Member(connection, "pin", context), context + ".pin");

            const auto footprintIterator = footprintByRef.find(ref);
            Require(footprintIterator != footprintByRef.end(),
                    "unknown component " + ref + " in net " + netName);
            const auto pinIterator = pinsByFootprint.find(footprintIterator->second);
            Require(pinIterator != pinsByFootprint.end(), "internal footprint lookup failed");
            Require(pinIterator->second.find(pin) != pinIterator->second.end(),
                    "component " + ref + " has no pin " + pin);

            const PinKey key{ref, pin};
            const auto owner = pinOwner.find(key);
            if (owner != pinOwner.end()) {
                Fail("pin " + ref + "." + pin + " appears in " + owner->second +
                     " and " + netName);
            }
            pinOwner.emplace(key, netName);
            expectedConnections.emplace(ref, pin, netName);
        }
    }
    Require(expectedConnections.size() == totalConnectionCount,
            "duplicate connection inside a net");
    return expectedConnections;
}

void ValidateBoard(
    const Value& board,
    const PinsByFootprint& pinsByFootprint,
    const std::set<ConnectionKey>& expectedConnections)
{
    const auto& root = board.AsObject();
    Require(StringMember(root, "format", "board") == "eda-course-board",
            "board format mismatch");
    Require(Member(root, "version", "board").IsInteger() &&
                Member(root, "version", "board").AsInteger() == 1,
            "board version mismatch");
    Require(StringMember(root, "units", "board") == "um", "board units must be um");

    const Value& outlineValue = Member(root, "outline", "board");
    RequirePoints(outlineValue, "board.outline", 4);
    const auto& outline = outlineValue.AsArray();
    const auto& first = outline.front().AsArray();
    const auto& last = outline.back().AsArray();
    Require(first[0].AsInteger() == last[0].AsInteger() &&
                first[1].AsInteger() == last[1].AsInteger(),
            "board outline must repeat its first point at the end");

    const Value& layersValue = Member(root, "layers", "board");
    Require(layersValue.IsArray() && !layersValue.AsArray().empty(),
            "board layers must be non-empty");
    std::vector<std::string> layerIds;
    for (const Value& layerValue : layersValue.AsArray()) {
        Require(layerValue.IsObject(), "board layer must be an object");
        const std::string& layerId = StringMember(layerValue.AsObject(), "id", "layer");
        Require(!layerId.empty(), "layer ID must be non-empty");
        layerIds.push_back(layerId);
    }
    RequireUnique(layerIds, "layer IDs");

    const Value& componentsValue = Member(root, "components", "board");
    const Value& netsValue = Member(root, "nets", "board");
    const Value& connectionsValue = Member(root, "connections", "board");
    Require(componentsValue.IsArray(), "board components must be an array");
    Require(netsValue.IsArray(), "board nets must be an array");
    Require(connectionsValue.IsArray(), "board connections must be an array");

    std::vector<std::string> componentIds;
    std::vector<std::string> refs;
    std::map<std::string, const Value::Object*> componentById;
    std::map<std::string, std::string> refByComponentId;
    for (std::size_t index = 0; index < componentsValue.AsArray().size(); ++index) {
        const Value& componentValue = componentsValue.AsArray()[index];
        Require(componentValue.IsObject(), "board component must be an object");
        const auto& component = componentValue.AsObject();
        const std::string& componentId = StringMember(component, "id", "board component");
        const std::string ref = Normalized(Member(component, "ref", "board component"),
                                           "board component ref");
        const std::string& footprint = StringMember(component, "footprint", "board component");
        Require(!componentId.empty(), "component ID must be non-empty");
        Require(pinsByFootprint.find(footprint) != pinsByFootprint.end(),
                componentId + " references unknown footprint " + footprint);

        const Value& rotation = Member(component, "rotation_mdeg", componentId);
        Require(rotation.IsInteger(), componentId + ".rotation_mdeg must be an integer");
        const std::int64_t degrees = rotation.AsInteger();
        Require(degrees == 0 || degrees == 90000 || degrees == 180000 || degrees == 270000,
                componentId + " has unsupported rotation");

        componentIds.push_back(componentId);
        refs.push_back(ref);
        componentById.emplace(componentId, &component);
        refByComponentId.emplace(componentId, ref);
    }
    RequireUnique(componentIds, "component instance IDs");
    RequireUnique(refs, "component references");

    std::vector<std::string> netIds;
    std::map<std::string, std::string> netNameById;
    for (const Value& netValue : netsValue.AsArray()) {
        Require(netValue.IsObject(), "board net must be an object");
        const auto& net = netValue.AsObject();
        const std::string& netId = StringMember(net, "id", "board net");
        const std::string& netName = StringMember(net, "name", "board net");
        Require(!netId.empty(), "net ID must be non-empty");
        netIds.push_back(netId);
        netNameById.emplace(netId, netName);
    }
    RequireUnique(netIds, "net IDs");

    std::set<ConnectionKey> actualConnections;
    std::set<PinKey> occupiedPins;
    for (std::size_t index = 0; index < connectionsValue.AsArray().size(); ++index) {
        const Value& connectionValue = connectionsValue.AsArray()[index];
        Require(connectionValue.IsObject(), "board connection must be an object");
        const auto& connection = connectionValue.AsObject();
        const std::string& componentId = StringMember(
            connection,
            "component",
            "connections[" + std::to_string(index) + "]");
        const auto componentIterator = componentById.find(componentId);
        Require(componentIterator != componentById.end(),
                "connections[" + std::to_string(index) + "] has unknown component");
        const std::string pin = Normalized(
            Member(connection, "pin", "connection"),
            "connections[" + std::to_string(index) + "].pin");
        const std::string ref = refByComponentId.at(componentId);
        const std::string& footprint = StringMember(
            *componentIterator->second,
            "footprint",
            componentId);
        const auto footprintIterator = pinsByFootprint.find(footprint);
        Require(footprintIterator != pinsByFootprint.end(), "internal footprint lookup failed");
        Require(footprintIterator->second.find(pin) != footprintIterator->second.end(),
                "connection " + ref + "." + pin + " references an unknown pin");

        const std::string& netId = StringMember(
            connection,
            "net",
            "connections[" + std::to_string(index) + "]");
        const auto netIterator = netNameById.find(netId);
        Require(netIterator != netNameById.end(),
                "connections[" + std::to_string(index) + "] has unknown net");

        const PinKey pinKey{componentId, pin};
        Require(occupiedPins.insert(pinKey).second,
                "pin " + ref + "." + pin + " is connected more than once");
        actualConnections.emplace(ref, pin, netIterator->second);
    }
    Require(actualConnections == expectedConnections,
            "board connections do not match the netlist");

    Require(Member(root, "tracks", "board").IsArray(), "board tracks must be an array");
    Require(Member(root, "vias", "board").IsArray(), "board vias must be an array");
    Require(Member(root, "copperZones", "board").IsArray(),
            "board copperZones must be an array");
}

fs::path FindRoot(const fs::path& start)
{
    fs::path current = start;
    if (!current.is_absolute()) {
        current = fs::absolute(current);
    }
    for (int depth = 0; depth < 32; ++depth) {
        if (fs::exists(current / "examples" / "netlist-basic" / "sample.net.json") &&
            fs::exists(current / "schemas" / "netlist.schema.json")) {
            return current;
        }
        if (!current.has_parent_path() || current.parent_path() == current) {
            break;
        }
        current = current.parent_path();
    }
    Fail("cannot locate repository root");
}

fs::path ResolveRoot(int argc, char* argv[])
{
    if (argc > 1) {
        return FindRoot(argv[1]);
    }
    try {
        return FindRoot(fs::current_path());
    } catch (const ValidationError&) {
        return FindRoot(fs::absolute(argv[0]).parent_path());
    }
}

} // namespace

int main(int argc, char* argv[])
{
    try {
        const fs::path root = ResolveRoot(argc, argv);
        const Value library = LoadObject(
            root,
            fs::path("examples") / "netlist-basic" / "sample.lib.json");
        const Value netlist = LoadObject(
            root,
            fs::path("examples") / "netlist-basic" / "sample.net.json");
        const Value board = LoadObject(
            root,
            fs::path("examples") / "netlist-basic" / "sample.board.json");

        for (const std::string schemaName : {"netlist", "footprint-library", "board"}) {
            const Value schema = LoadObject(
                root,
                fs::path("schemas") / (schemaName + ".schema.json"));
            Require(
                StringMember(schema.AsObject(), "$schema", schemaName + " schema") ==
                    "https://json-schema.org/draft/2020-12/schema",
                schemaName + " schema draft");
        }

        const PinsByFootprint pinsByFootprint = ValidateLibrary(library);
        const std::set<ConnectionKey> expectedConnections =
            ValidateNetlist(netlist, pinsByFootprint);
        ValidateBoard(board, pinsByFootprint, expectedConnections);

        std::cout << "Library footprints: "
                  << Member(library.AsObject(), "footprints", "library").AsArray().size()
                  << '\n';
        std::cout << "Netlist components: "
                  << Member(netlist.AsObject(), "components", "netlist").AsArray().size()
                  << '\n';
        std::cout << "Netlist nets: "
                  << Member(netlist.AsObject(), "nets", "netlist").AsArray().size()
                  << '\n';
        std::cout << "Matched pin connections: " << expectedConnections.size() << '\n';
        std::cout << "Design samples: OK\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
