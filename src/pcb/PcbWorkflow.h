#pragma once

#include "core/Project.h"
#include "pcb/LayoutService.h"
#include "pcb/NetlistImporter.h"
#include "pcb/RoutingService.h"

#include <string>
#include <vector>

namespace pcb {

struct WorkflowResult {
    bool ok = false;
    NetlistImportResult import;
    LayoutResult layout;
    RouteResult routing;
    ValidationResult validation;
    std::vector<std::string> log;
    std::string message;
};

class PcbWorkflow {
public:
    static WorkflowResult buildSample(
        const std::string& netlistPath,
        const std::string& libraryPath,
        PcbProject& project);

    static ValidationResult validateAll(const Board& board, double clearance = 0.25);
};

} // namespace pcb
