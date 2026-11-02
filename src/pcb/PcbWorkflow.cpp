#include "pcb/PcbWorkflow.h"

#include <algorithm>

namespace pcb {

namespace {

const Net* findNetByDisplayName(const Board& board, const std::string& name)
{
    return board.findNetByName(name);
}

} // namespace

WorkflowResult PcbWorkflow::buildSample(
    const std::string& netlistPath,
    const std::string& libraryPath,
    PcbProject& project)
{
    WorkflowResult result;
    result.import = NetlistImporter::importFiles(netlistPath, libraryPath, project);
    result.log.push_back("Import: " + result.import.summary());
    if (!result.import.ok) {
        result.message = "netlist import failed";
        return result;
    }

    result.layout = LayoutService::autoPlace(project.board, 3.0, 2.0, 0.5);
    result.log.push_back("Layout: " + result.layout.message);
    if (!result.layout.ok) {
        result.message = "automatic placement failed";
        return result;
    }

    result.routing = RoutingService::autoRoute(project.board, 0.25);
    result.log.push_back(
        "Routing: " + std::to_string(result.routing.traceCount) + " traces, " +
        std::to_string(result.routing.viaCount) + " vias");
    for (const std::string& net : result.routing.unresolvedNets) {
        result.log.push_back("Unrouted net: " + net);
    }

    if (const Net* ground = findNetByDisplayName(project.board, "GND")) {
        std::string error;
        if (RoutingService::addGroundPour(
                project.board, ground->id, "B.Cu", 1.0, 0.30, error)) {
            result.log.push_back("Copper pour: GND on B.Cu");
        } else {
            result.log.push_back("Copper pour failed: " + error);
        }
    }

    result.validation = RoutingService::validate(project.board, 0.25);
    result.log.push_back("Validation: " + result.validation.summary());
    result.ok = result.validation.ok;
    result.message = result.ok
        ? "PCB sample workflow completed"
        : "PCB sample workflow completed with validation errors";
    return result;
}

ValidationResult PcbWorkflow::validateAll(const Board& board, double clearance)
{
    return RoutingService::validate(board, clearance);
}

} // namespace pcb
