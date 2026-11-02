#include "core/Project.h"
#include "io/ProjectIO.h"
#include "pcb/Geometry.h"
#include "pcb/LayoutService.h"
#include "pcb/NetlistImporter.h"
#include "pcb/PcbWorkflow.h"
#include "pcb/RoutingService.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

namespace fs = std::filesystem;
using namespace pcb;

int gPassed = 0;
int gFailed = 0;

void Check(bool condition, const std::string& message)
{
    if (condition) {
        ++gPassed;
        std::cout << "[PASS] " << message << '\n';
    } else {
        ++gFailed;
        std::cout << "[FAIL] " << message << '\n';
    }
}

std::string ReadFile(const fs::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

bool HasDiagnostic(const NetlistImportResult& result, const std::string& code)
{
    return std::any_of(
        result.diagnostics.begin(), result.diagnostics.end(),
        [&](const ImportDiagnostic& diagnostic) { return diagnostic.code == code; });
}

bool ReplaceFirst(std::string& text, const std::string& from, const std::string& to)
{
    const std::size_t position = text.find(from);
    if (position == std::string::npos) {
        return false;
    }
    text.replace(position, from.size(), to);
    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    const fs::path root = argc > 1 ? fs::path(argv[1]) : fs::current_path();
    const fs::path netlist = root / "examples" / "netlist-basic" / "sample.net.json";
    const fs::path library = root / "examples" / "netlist-basic" / "sample.lib.json";

    PcbProject imported;
    const NetlistImportResult import =
        NetlistImporter::importFiles(netlist.string(), library.string(), imported);
    Check(import.ok, "netlist imports successfully");
    Check(import.componentCount == 4, "netlist creates four components");
    Check(import.netCount == 4, "netlist creates four nets");
    Check(import.matchedPinCount == 13, "netlist matches thirteen pins");
    Check(imported.board.checkIntegrity().empty(), "imported board has no dangling references");

    const LayoutResult layout = LayoutService::autoPlace(imported.board, 3.0, 2.0, 0.5);
    Check(layout.ok, "automatic placement succeeds");
    Check(layout.placedCount == 4, "automatic placement moves four components");
    for (const Component& component : imported.board.components) {
        Check(pointInsideBoard(imported.board, component.position),
              component.reference + " remains inside the board");
    }
    for (std::size_t a = 0; a < imported.board.components.size(); ++a) {
        for (std::size_t b = a + 1; b < imported.board.components.size(); ++b) {
            Check(!componentBounds(imported.board, imported.board.components[a]).intersects(
                      componentBounds(imported.board, imported.board.components[b])),
                  imported.board.components[a].reference + " does not overlap " +
                      imported.board.components[b].reference);
        }
    }

    Component* u1 = imported.board.findByReference("U1");
    const Footprint* u1Footprint = u1 ? imported.board.findFootprint(u1->footprintId) : nullptr;
    Check(u1 != nullptr && u1Footprint != nullptr, "U1 resolves to its footprint");
    if (u1 && u1Footprint && findPad(*u1Footprint, "1")) {
        const Point before = padPosition(*u1, *findPad(*u1Footprint, "1"));
        Check(LayoutService::rotateComponent(imported.board, u1->id, 90.0),
              "component rotation succeeds");
        const Point after = padPosition(*u1, *findPad(*u1Footprint, "1"));
        Check(before.distanceTo(after) > 0.1, "rotation updates pad position");
        LayoutService::rotateComponent(imported.board, u1->id, 270.0);
    }

    const RouteResult routing = RoutingService::autoRoute(imported.board, 0.25);
    Check(routing.traceCount > 0, "automatic routing creates traces");
    std::cout << "Routing detail: " << routing.message
              << ", traces=" << routing.traceCount
              << ", vias=" << routing.viaCount << '\n';

    const Net* ground = imported.board.findNetByName("GND");
    Check(ground != nullptr, "GND net exists");
    std::string pourError;
    Check(ground && RoutingService::addGroundPour(
                        imported.board, ground->id, "B.Cu", 1.0, 0.30, pourError),
          "GND copper pour is created");
    Check(!imported.board.copperPours.empty(), "copper pour exists in the board");
    if (!imported.board.copperPours.empty()) {
        Check(imported.board.copperPours.front().clearance > 0.0,
              "copper pour records clearance");
        Check(!imported.board.copperPours.front().holes.empty(),
              "copper pour contains avoidance holes");
    }

    const ValidationResult validation = RoutingService::validate(imported.board, 0.25);
    std::cout << validation.summary() << '\n';
    Check(validation.connectedNetCount == 4, "all four nets are connected");
    Check(validation.unconnectedNetCount == 0, "no net remains unconnected");
    Check(validation.issues.empty(), "no connectivity or clearance issue remains");

    const std::string saved = ProjectIO::saveToString(imported);
    PcbProject reloaded;
    const LoadResult load = ProjectIO::loadFromString(saved, reloaded);
    Check(load.ok, "completed PCB project reloads from JSON");
    Check(reloaded == imported, "completed PCB project round-trips exactly");

    PcbProject manualProject;
    const NetlistImportResult manualImport =
        NetlistImporter::importFiles(netlist.string(), library.string(), manualProject);
    Check(manualImport.ok, "manual routing sample imports");
    const Net* signalInput = manualProject.board.findNetByName("SIG_IN");
    if (signalInput) {
        const auto signalPads = collectPads(manualProject.board, signalInput->id);
        if (signalPads.size() >= 2) {
            const ManualRouteResult manual = RoutingService::addManualTrace(
                manualProject.board,
                signalInput->id,
                "F.Cu",
                0.30,
                {signalPads[0].position, signalPads[1].position},
                0.25);
            Check(manual.ok, "manual trace is accepted when same-net path is legal");
        }
        const Net* vcc = manualProject.board.findNetByName("VCC");
        std::string viaError;
        Check(vcc && RoutingService::placeVia(
                         manualProject.board,
                         vcc->id,
                         Point{30.0, 20.0},
                         "F.Cu",
                         "B.Cu",
                         0.6,
                         0.3,
                         viaError),
              "manual via is placed on a free area");
    }

    const std::string netlistText = ReadFile(netlist);
    const std::string libraryText = ReadFile(library);
    std::string brokenNetlist = netlistText;
    ReplaceFirst(brokenNetlist, "\"SOIC-8_3.9x4.9mm_P1.27mm\"", "\"MISSING_FOOTPRINT\"");
    PcbProject invalidProject;
    const NetlistImportResult invalid = NetlistImporter::importStrings(
        brokenNetlist, libraryText, invalidProject);
    Check(!invalid.ok, "unknown footprint rejects the import");
    Check(HasDiagnostic(invalid, "NET-008"), "unknown footprint reports NET-008");

    std::string unknownPinNetlist = netlistText;
    ReplaceFirst(unknownPinNetlist, "\"pin\": \"8\"", "\"pin\": \"99\"");
    PcbProject invalidPinProject;
    const NetlistImportResult invalidPin = NetlistImporter::importStrings(
        unknownPinNetlist, libraryText, invalidPinProject);
    Check(!invalidPin.ok && HasDiagnostic(invalidPin, "NET-009"),
          "unknown pin reports NET-009");

    std::string duplicateRefNetlist = netlistText;
    ReplaceFirst(duplicateRefNetlist, "\"ref\": \"R1\"", "\"ref\": \"U1\"");
    PcbProject duplicateRefProject;
    const NetlistImportResult duplicateRef = NetlistImporter::importStrings(
        duplicateRefNetlist, libraryText, duplicateRefProject);
    Check(!duplicateRef.ok && HasDiagnostic(duplicateRef, "NET-004"),
          "duplicate reference reports NET-004");

    std::string duplicatePinNetlist = netlistText;
    ReplaceFirst(
        duplicatePinNetlist,
        "\"ref\": \"R1\", \"pin\": \"1\"",
        "\"ref\": \"R1\", \"pin\": \"2\"");
    PcbProject duplicatePinProject;
    const NetlistImportResult duplicatePin = NetlistImporter::importStrings(
        duplicatePinNetlist, libraryText, duplicatePinProject);
    Check(!duplicatePin.ok && HasDiagnostic(duplicatePin, "NET-010"),
          "pin used by two nets reports NET-010");

    PcbProject sample;
    const WorkflowResult workflow =
        PcbWorkflow::buildSample(netlist.string(), library.string(), sample);
    Check(workflow.ok, "complete PCB sample workflow succeeds");

    std::cout << "\nMember C tests: " << gPassed << " passed, " << gFailed << " failed\n";
    return gFailed == 0 ? 0 : 1;
}
